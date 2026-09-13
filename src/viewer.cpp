#include "viewer.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <cmath>

namespace {
constexpr double kHandle = 8;
constexpr double kHit = 14;
}

PhotoViewer::PhotoViewer(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("photoCanvas"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 200);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x14, 0x14, 0x14));
    setPalette(pal);
}

void PhotoViewer::setImage(const QImage &image, bool keepView) {
    image_ = image;
    texW_ = qMax(1, image.width());
    texH_ = qMax(1, image.height());
    if (!keepView || fitted_)
        fit();
    else
        update();
    notify();
}

void PhotoViewer::clear() {
    image_ = {};
    update();
}

void PhotoViewer::fit() {
    fitted_ = true;
    const int w = qMax(1, width());
    const int h = qMax(1, height());
    fitZoom_ = qMin(double(w) / texW_, double(h) / texH_);
    zoom_ = fitZoom_;
    pan_ = QPoint();
    update();
    notify();
}

void PhotoViewer::actual() {
    fitted_ = false;
    zoom_ = 1.0;
    pan_ = QPointF((width() - texW_ * zoom_) / 2.0, (height() - texH_ * zoom_) / 2.0);
    clampPan();
    update();
    notify();
}

void PhotoViewer::clampPan() {
    const double dw = texW_ * zoom_;
    const double dh = texH_ * zoom_;
    if (dw <= width())
        pan_.setX((width() - dw) / 2.0);
    else
        pan_.setX(qBound(double(width()) - dw, pan_.x(), 0.0));
    if (dh <= height())
        pan_.setY((height() - dh) / 2.0);
    else
        pan_.setY(qBound(double(height()) - dh, pan_.y(), 0.0));
}

void PhotoViewer::zoomBy(double factor) {
    zoomBy(factor, lastPos_.x() < 0 ? QPointF(width() / 2.0, height() / 2.0) : lastPos_);
}

void PhotoViewer::zoomBy(double factor, QPointF anchor) {
    if (image_.isNull())
        return;
    const int w = qMax(1, width());
    const int h = qMax(1, height());
    fitZoom_ = qMin(double(w) / texW_, double(h) / texH_);
    const double nz = qMin(16.0, qMax(fitZoom_ * 0.25, zoom_ * factor));
    if (nz <= fitZoom_ * 1.02) {
        fit();
        return;
    }
    const QRectF ir = imageRect();
    const double oldZ = ir.width() / double(texW_);
    const double ix = (anchor.x() - ir.x()) / oldZ;
    const double iy = (anchor.y() - ir.y()) / oldZ;
    fitted_ = false;
    zoom_ = nz;
    pan_ = QPointF(anchor.x() - ix * zoom_, anchor.y() - iy * zoom_);
    clampPan();
    update();
    notify();
}

void PhotoViewer::beginCrop(const std::optional<QRectF> &crop) {
    cropMode_ = true;
    crop_ = crop.value_or(QRectF(0, 0, 1, 1));
    fitCropToAspect(false);
    fit();
    update();
}

void PhotoViewer::setCropAspect(std::optional<double> aspect, bool original) {
    cropAspect_ = aspect;
    cropAspectOriginal_ = original;
    if (cropMode_)
        fitCropToAspect(true);
    update();
}

double PhotoViewer::pixelAspect() const {
    if (cropAspectOriginal_)
        return double(texW_) / double(texH_);
    return cropAspect_.value_or(0);
}

double PhotoViewer::normAspect() const {
    const double a = pixelAspect();
    if (a <= 0)
        return 0;
    return a * double(texH_) / double(texW_);
}

void PhotoViewer::fitCropToAspect(bool forceLargest) {
    const double na = normAspect();
    if (na <= 0)
        return;
    const double cur = crop_.height() > 1e-9 ? crop_.width() / crop_.height() : 0;
    if (!forceLargest && std::abs(cur - na) < 0.012)
        return;
    double w, h;
    if (na >= 1.0) {
        w = 1.0;
        h = w / na;
    } else {
        h = 1.0;
        w = h * na;
    }
    const double x = qBound(0.0, crop_.center().x() - w / 2.0, 1.0 - w);
    const double y = qBound(0.0, crop_.center().y() - h / 2.0, 1.0 - h);
    crop_ = QRectF(x, y, w, h);
}

void PhotoViewer::endCrop(bool apply) {
    cropMode_ = false;
    dragKind_.clear();
    update();
    if (apply && onCropChanged) {
        const QRectF c = crop_;
        if (c.x() <= 0.001 && c.y() <= 0.001 && c.right() >= 0.998 && c.bottom() >= 0.998)
            onCropChanged(std::nullopt);
        else
            onCropChanged(c);
    }
}

QRectF PhotoViewer::imageRect() const {
    const double w = qMax(1, width());
    const double h = qMax(1, height());
    if (fitted_) {
        const double scale = qMin(w / texW_, h / texH_);
        const double dw = texW_ * scale;
        const double dh = texH_ * scale;
        return QRectF((w - dw) / 2.0, (h - dh) / 2.0, dw, dh);
    }
    return QRectF(pan_.x(), pan_.y(), texW_ * zoom_, texH_ * zoom_);
}

QRectF PhotoViewer::cropView() const {
    const QRectF ir = imageRect();
    return QRectF(ir.x() + crop_.x() * ir.width(),
                  ir.y() + crop_.y() * ir.height(),
                  crop_.width() * ir.width(),
                  crop_.height() * ir.height());
}

QVector<QPointF> PhotoViewer::handles(const QRectF &r) const {
    return {
        r.topLeft(),
        QPointF(r.center().x(), r.top()),
        r.topRight(),
        QPointF(r.right(), r.center().y()),
        r.bottomRight(),
        QPointF(r.center().x(), r.bottom()),
        r.bottomLeft(),
        QPointF(r.left(), r.center().y()),
    };
}

QString PhotoViewer::hit(const QPointF &p) const {
    const QRectF r = cropView();
    static const char *names[] = {"nw", "n", "ne", "e", "se", "s", "sw", "w"};
    const auto hs = handles(r);
    for (int i = 0; i < hs.size(); ++i) {
        if (std::abs(p.x() - hs[i].x()) <= kHit && std::abs(p.y() - hs[i].y()) <= kHit)
            return QString::fromLatin1(names[i]);
    }
    if (r.contains(p))
        return QStringLiteral("move");
    return {};
}

void PhotoViewer::notify() {
    if (onViewChanged)
        onViewChanged();
}

void PhotoViewer::setCursorFor(const QString &h) {
    if (h == QLatin1String("move"))
        setCursor(Qt::SizeAllCursor);
    else if (h == QLatin1String("n") || h == QLatin1String("s"))
        setCursor(Qt::SizeVerCursor);
    else if (h == QLatin1String("e") || h == QLatin1String("w"))
        setCursor(Qt::SizeHorCursor);
    else if (h == QLatin1String("ne") || h == QLatin1String("sw"))
        setCursor(Qt::SizeBDiagCursor);
    else if (h == QLatin1String("nw") || h == QLatin1String("se"))
        setCursor(Qt::SizeFDiagCursor);
    else if (cropMode_)
        setCursor(Qt::ArrowCursor);
    else
        setCursor(Qt::OpenHandCursor);
}

void PhotoViewer::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x14, 0x14, 0x14));
    if (image_.isNull())
        return;
    const QRectF ir = imageRect();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(ir, image_);
    if (!cropMode_)
        return;
    const QRectF cv = cropView();
    const QColor dim(0, 0, 0, int(0.55 * 255));
    p.fillRect(QRectF(0, 0, width(), qMax(0.0, cv.top())), dim);
    p.fillRect(QRectF(0, cv.bottom(), width(), qMax(0.0, height() - cv.bottom())), dim);
    p.fillRect(QRectF(0, cv.top(), qMax(0.0, cv.left()), cv.height()), dim);
    p.fillRect(QRectF(cv.right(), cv.top(), qMax(0.0, width() - cv.right()), cv.height()), dim);
    p.setPen(Qt::NoPen);
    const QColor line(255, 255, 255, int(0.85 * 255));
    p.fillRect(QRectF(cv.left(), cv.top(), cv.width(), 1), line);
    p.fillRect(QRectF(cv.left(), cv.bottom() - 1, cv.width(), 1), line);
    p.fillRect(QRectF(cv.left(), cv.top(), 1, cv.height()), line);
    p.fillRect(QRectF(cv.right() - 1, cv.top(), 1, cv.height()), line);
    const QColor grid(255, 255, 255, int(0.28 * 255));
    p.fillRect(QRectF(cv.left() + cv.width() / 3.0, cv.top(), 1, cv.height()), grid);
    p.fillRect(QRectF(cv.left() + 2.0 * cv.width() / 3.0, cv.top(), 1, cv.height()), grid);
    p.fillRect(QRectF(cv.left(), cv.top() + cv.height() / 3.0, cv.width(), 1), grid);
    p.fillRect(QRectF(cv.left(), cv.top() + 2.0 * cv.height() / 3.0, cv.width(), 1), grid);
    for (const auto &hp : handles(cv))
        p.fillRect(QRectF(hp.x() - kHandle / 2.0, hp.y() - kHandle / 2.0, kHandle, kHandle), Qt::white);
}

void PhotoViewer::resizeEvent(QResizeEvent *) {
    if (fitted_ && !image_.isNull())
        fit();
}

void PhotoViewer::wheelEvent(QWheelEvent *event) {
    lastPos_ = event->position();
    zoomBy(event->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1, lastPos_);
    event->accept();
}

void PhotoViewer::mousePressEvent(QMouseEvent *event) {
    setFocus();
    lastPos_ = event->position();
    if (cropMode_) {
        const QString h = hit(event->position());
        if (!h.isEmpty()) {
            dragKind_ = h;
            dragStart_ = event->pos();
            dragCrop_ = crop_;
        } else {
            dragKind_.clear();
        }
        return;
    }
    if (!fitted_) {
        dragKind_ = QStringLiteral("pan");
        dragStart_ = event->pos();
        dragPan_ = pan_;
        setCursor(Qt::ClosedHandCursor);
    }
}

void PhotoViewer::mouseMoveEvent(QMouseEvent *event) {
    lastPos_ = event->position();
    if (dragKind_.isEmpty()) {
        if (cropMode_)
            setCursorFor(hit(event->position()));
        else
            setCursor(fitted_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
        return;
    }
    if (dragKind_ == QLatin1String("pan")) {
        pan_ = dragPan_ + (event->pos() - dragStart_);
        clampPan();
        update();
        return;
    }
    resizeCrop(dragKind_, dragCrop_, event->position());
    update();
}

void PhotoViewer::mouseReleaseEvent(QMouseEvent *) {
    dragKind_.clear();
    if (!cropMode_)
        setCursor(fitted_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
}

void PhotoViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    setFocus();
    if (!cropMode_ && !image_.isNull()) {
        if (fitted_)
            actual();
        else
            fit();
        event->accept();
    }
}

void PhotoViewer::resizeCrop(const QString &kind, const QRectF &orig, const QPointF &pos) {
    const QRectF ir = imageRect();
    if (ir.width() < 1 || ir.height() < 1)
        return;
    const double minW = 32.0 / ir.width();
    const double minH = 32.0 / ir.height();
    double l = orig.x();
    double t = orig.y();
    double r = orig.x() + orig.width();
    double b = orig.y() + orig.height();
    const double w = orig.width();
    const double h = orig.height();
    if (kind == QLatin1String("move")) {
        const double dl = (pos.x() - dragStart_.x()) / ir.width();
        const double dt = (pos.y() - dragStart_.y()) / ir.height();
        l = qBound(0.0, orig.x() + dl, 1.0 - w);
        t = qBound(0.0, orig.y() + dt, 1.0 - h);
        crop_ = QRectF(l, t, w, h);
        return;
    }
    const double nx = (pos.x() - ir.x()) / ir.width();
    const double ny = (pos.y() - ir.y()) / ir.height();
    const double na = normAspect();
    if (na <= 0) {
        if (kind.contains(QLatin1Char('w')))
            l = qMin(qMax(0.0, nx), r - minW);
        if (kind.contains(QLatin1Char('e')))
            r = qMax(qMin(1.0, nx), l + minW);
        if (kind.contains(QLatin1Char('n')))
            t = qMin(qMax(0.0, ny), b - minH);
        if (kind.contains(QLatin1Char('s')))
            b = qMax(qMin(1.0, ny), t + minH);
        crop_ = QRectF(l, t, r - l, b - t);
        return;
    }
    const double minWa = qMax(minW, na * minH);
    auto setFrom = [&](double L, double T, double W, double H) {
        if (W < minWa) {
            W = minWa;
            H = W / na;
        }
        if (H < minWa / na) {
            H = minWa / na;
            W = H * na;
        }
        if (W > 1.0) {
            W = 1.0;
            H = W / na;
        }
        if (H > 1.0) {
            H = 1.0;
            W = H * na;
        }
        L = qBound(0.0, L, 1.0 - W);
        T = qBound(0.0, T, 1.0 - H);
        crop_ = QRectF(L, T, W, H);
    };
    if (kind == QLatin1String("se")) {
        const double W = qMax(minWa, qMax(nx - l, (ny - t) * na));
        setFrom(l, t, W, W / na);
        return;
    }
    if (kind == QLatin1String("nw")) {
        const double W = qMax(minWa, qMax(r - nx, (b - ny) * na));
        setFrom(r - W, b - W / na, W, W / na);
        return;
    }
    if (kind == QLatin1String("ne")) {
        const double W = qMax(minWa, qMax(nx - l, (b - ny) * na));
        setFrom(l, b - W / na, W, W / na);
        return;
    }
    if (kind == QLatin1String("sw")) {
        const double W = qMax(minWa, qMax(r - nx, (ny - t) * na));
        setFrom(r - W, t, W, W / na);
        return;
    }
    if (kind == QLatin1String("e") || kind == QLatin1String("w")) {
        double W = kind == QLatin1String("e") ? nx - l : r - nx;
        W = qMax(minWa, W);
        double H = W / na;
        const double cy = orig.center().y();
        double L = kind == QLatin1String("e") ? l : r - W;
        setFrom(L, cy - H / 2.0, W, H);
        return;
    }
    if (kind == QLatin1String("n") || kind == QLatin1String("s")) {
        double H = kind == QLatin1String("s") ? ny - t : b - ny;
        H = qMax(minWa / na, H);
        double W = H * na;
        const double cx = orig.center().x();
        double T = kind == QLatin1String("s") ? t : b - H;
        setFrom(cx - W / 2.0, T, W, H);
    }
}
