#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QWidget>
#include <functional>
#include <optional>

class PhotoViewer : public QWidget {
    Q_OBJECT
public:
    explicit PhotoViewer(QWidget *parent = nullptr);

    void setImage(const QImage &image, bool keepView = false);
    void clear();
    void fit();
    void actual();
    void zoomBy(double factor);
    void zoomBy(double factor, QPointF anchor);
    double zoom() const { return zoom_; }

    void beginCrop(const std::optional<QRectF> &crop);
    void endCrop(bool apply);
    void setCropAspect(std::optional<double> aspect, bool original = false);
    QRectF currentCrop() const { return crop_; }

    std::function<void(const std::optional<QRectF> &)> onCropChanged;
    std::function<void()> onViewChanged;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QRectF imageRect() const;
    QRectF cropView() const;
    QVector<QPointF> handles(const QRectF &r) const;
    QString hit(const QPointF &p) const;
    void resizeCrop(const QString &kind, const QRectF &orig, const QPointF &pos);
    void fitCropToAspect(bool forceLargest);
    double pixelAspect() const;
    double normAspect() const;
    void notify();
    void setCursorFor(const QString &h);
    void clampPan();

    QImage image_;
    int texW_ = 1;
    int texH_ = 1;
    double zoom_ = 1.0;
    double fitZoom_ = 1.0;
    bool fitted_ = true;
    QPointF pan_;
    bool cropMode_ = false;
    QRectF crop_{0, 0, 1, 1};
    std::optional<double> cropAspect_;
    bool cropAspectOriginal_ = false;
    QPointF lastPos_{-1, -1};
    QString dragKind_;
    QPoint dragStart_;
    QRectF dragCrop_;
    QPointF dragPan_;
};
