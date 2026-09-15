#include "window.h"

#include "filmstrip.h"
#include "imageutil.h"
#include "panel.h"
#include "presets.h"
#include "rawio.h"
#include "viewer.h"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

class ToastLabel : public QLabel {
public:
    explicit ToastLabel(QWidget *parent) : QLabel(parent) {
        setStyleSheet(QStringLiteral(
            "background: rgba(20,20,20,210); color: white; border-radius: 10px; padding: 8px 14px;"));
        hide();
        timer_.setSingleShot(true);
        connect(&timer_, &QTimer::timeout, this, &QWidget::hide);
    }
    void showText(const QString &t) {
        setText(t);
        adjustSize();
        auto *p = parentWidget();
        if (p)
            move((p->width() - width()) / 2, p->height() - height() - 48);
        show();
        raise();
        timer_.start(2000);
    }
private:
    QTimer timer_;
};

namespace {

QSet<QString> allExts() {
    QSet<QString> s = kImageExts;
    s.unite(kRawExts);
    return s;
}

QIcon themeIcon(const QString &name, QStyle::StandardPixmap fb) {
    QIcon i = QIcon::fromTheme(name);
    if (i.isNull())
        i = QIcon::fromTheme(name + QStringLiteral("-symbolic"));
    if (i.isNull())
        i = qApp->style()->standardIcon(fb);
    return i;
}

} // namespace

PhotoWindow::PhotoWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("Photo"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("camera-photo")));
    resize(1400, 900);
    setAcceptDrops(true);
    pool_.setMaxThreadCount(3);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("header"));
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(8, 6, 8, 6);
    hl->setSpacing(4);

    auto *openBtn = iconBtn(QStringLiteral("folder-open"), QStringLiteral("Open folder"), [this] { openFolder(); });
    hl->addWidget(openBtn);

    auto *titles = new QWidget;
    auto *tl = new QVBoxLayout(titles);
    tl->setContentsMargins(12, 0, 12, 0);
    tl->setSpacing(0);
    title_ = new QLabel(QStringLiteral("Photo"));
    QFont tf = title_->font();
    tf.setBold(true);
    title_->setFont(tf);
    title_->setAlignment(Qt::AlignCenter);
    subtitle_ = new QLabel(QStringLiteral("Open a folder of photos"));
    subtitle_->setAlignment(Qt::AlignCenter);
    QPalette sp = subtitle_->palette();
    sp.setColor(QPalette::WindowText, sp.color(QPalette::PlaceholderText));
    subtitle_->setPalette(sp);
    tl->addWidget(title_);
    tl->addWidget(subtitle_);
    hl->addWidget(titles, 1);

    auto addEnd = [&](QWidget *w) { hl->addWidget(w); };
    addEnd(iconBtn(QStringLiteral("zoom-fit-best"), QStringLiteral("Fit"), [this] { viewer_->fit(); updateStatus(); }));
    addEnd(iconBtn(QStringLiteral("zoom-original"), QStringLiteral("100%"), [this] { viewer_->actual(); updateStatus(); }));
    cropBtn_ = iconBtn(QStringLiteral("edit-select-all"), QStringLiteral("Crop"), {});
    cropBtn_->setCheckable(true);
    connect(cropBtn_, &QToolButton::toggled, this, [this](bool on) { toggleCrop(on); });
    addEnd(cropBtn_);
    addEnd(iconBtn(QStringLiteral("object-rotate-left"), QStringLiteral("Rotate left"), [this] { rotateBy(-90); }));
    addEnd(iconBtn(QStringLiteral("object-rotate-right"), QStringLiteral("Rotate right"), [this] { rotateBy(90); }));
    addEnd(iconBtn(QStringLiteral("document-save-as"), QStringLiteral("Save all"), [this] { saveAll(); }));
    addEnd(iconBtn(QStringLiteral("document-save"), QStringLiteral("Save"), [this] { saveCurrent(); }));
    root->addWidget(header);

    auto *body = new QWidget;
    auto *bl = new QHBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    auto *main = new QWidget;
    auto *ml = new QVBoxLayout(main);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);

    stack_ = new QStackedWidget;
    auto *empty = new QWidget;
    auto *el = new QVBoxLayout(empty);
    el->addStretch();
    auto *emptyIcon = new QLabel;
    emptyIcon->setPixmap(themeIcon(QStringLiteral("folder-pictures"), QStyle::SP_DirIcon).pixmap(96, 96));
    emptyIcon->setAlignment(Qt::AlignCenter);
    auto *emptyTitle = new QLabel(QStringLiteral("Open a folder of photos"));
    QFont et = emptyTitle->font();
    et.setPointSize(et.pointSize() + 4);
    et.setBold(true);
    emptyTitle->setFont(et);
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto *emptyDesc = new QLabel(QStringLiteral("Adjust, crop, rotate, then save. Arrow keys move between shots."));
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyDesc->setWordWrap(true);
    auto *emptyBtn = new QPushButton(QStringLiteral("Open folder"));
    emptyBtn->setObjectName(QStringLiteral("suggested"));
    emptyBtn->setCursor(Qt::PointingHandCursor);
    emptyBtn->setFixedHeight(36);
    connect(emptyBtn, &QPushButton::clicked, this, [this] { openFolder(); });
    auto *btnWrap = new QHBoxLayout;
    btnWrap->addStretch();
    btnWrap->addWidget(emptyBtn);
    btnWrap->addStretch();
    el->addWidget(emptyIcon);
    el->addWidget(emptyTitle);
    el->addWidget(emptyDesc);
    el->addSpacing(12);
    el->addLayout(btnWrap);
    el->addStretch();
    stack_->addWidget(empty);

    auto *viewerPage = new QWidget;
    auto *gl = new QGridLayout(viewerPage);
    gl->setContentsMargins(0, 0, 0, 0);
    viewer_ = new PhotoViewer;
    viewer_->onCropChanged = [this](const std::optional<QRectF> &c) { cropApplied(c); };
    viewer_->onViewChanged = [this] { updateStatus(); };
    gl->addWidget(viewer_, 0, 0);
    cropBar_ = new QWidget;
    auto *cbl = new QHBoxLayout(cropBar_);
    cbl->setContentsMargins(8, 8, 8, 16);
    cbl->setSpacing(8);
    auto *cancelC = new QPushButton(QStringLiteral("Cancel"));
    auto *applyC = new QPushButton(QStringLiteral("Apply crop"));
    applyC->setObjectName(QStringLiteral("suggested"));
    connect(cancelC, &QPushButton::clicked, this, [this] { finishCrop(false); });
    connect(applyC, &QPushButton::clicked, this, [this] { finishCrop(true); });
    cbl->addWidget(cancelC);
    cbl->addWidget(applyC);
    cropBar_->setVisible(false);
    gl->addWidget(cropBar_, 0, 0, Qt::AlignBottom | Qt::AlignHCenter);
    stack_->addWidget(viewerPage);
    stack_->setCurrentIndex(0);

    film_ = new Filmstrip;
    film_->onSelect = [this](int i) { showIndex(i, false); };
    film_->hide();

    status_ = new QLabel;
    status_->setContentsMargins(12, 4, 12, 6);
    QPalette stp = status_->palette();
    stp.setColor(QPalette::WindowText, stp.color(QPalette::PlaceholderText));
    status_->setPalette(stp);

    ml->addWidget(stack_, 1);
    ml->addWidget(film_);
    ml->addWidget(status_);

    panel_ = new AdjustPanel;
    panel_->onChange = [this](const QString &k, const QVariant &v) { adjChanged(k, v); };
    panel_->onReset = [this] { resetAdjustments(); };
    panel_->setEnabled(false);

    bl->addWidget(main, 1);
    bl->addWidget(panel_);
    root->addWidget(body, 1);

    toast_ = new ToastLabel(this);

    setStyleSheet(QStringLiteral(
        "#photoCanvas { background: #141414; }"
        "#filmstrip { border-top: 1px solid rgba(128,128,128,0.35); }"
        "#adjustPanel { border-left: 1px solid rgba(128,128,128,0.35); }"
        "#header { border-bottom: 1px solid rgba(128,128,128,0.35); }"
        "QPushButton#suggested { background: palette(highlight); color: palette(highlighted-text);"
        " border: none; border-radius: 16px; padding: 6px 18px; font-weight: 600; }"
        "QPushButton#suggested:hover { opacity: 0.9; }"));
}

PhotoWindow::~PhotoWindow() {
    ++loadId_;
    ++procId_;
    ++folderGen_;
    pool_.waitForDone();
}

QToolButton *PhotoWindow::iconBtn(const QString &theme, const QString &tip, std::function<void()> cb) {
    auto *b = new QToolButton;
    b->setIcon(themeIcon(theme, QStyle::SP_FileIcon));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setIconSize(QSize(18, 18));
    if (cb)
        connect(b, &QToolButton::clicked, this, std::move(cb));
    return b;
}

Photo *PhotoWindow::current() {
    if (index_ >= 0 && index_ < photos_.size())
        return &photos_[index_];
    return nullptr;
}

void PhotoWindow::openFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Open folder"));
    if (!dir.isEmpty())
        loadPath(dir);
}

void PhotoWindow::loadPath(const QString &path) {
    QFileInfo info(path);
    QString select;
    QDir folder;
    if (info.isFile()) {
        select = info.absoluteFilePath();
        folder = info.absoluteDir();
    } else {
        folder = QDir(info.absoluteFilePath());
    }
    const auto exts = allExts();
    QFileInfoList files = folder.entryInfoList(QDir::Files, QDir::Name);
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.fileName().toLower() < b.fileName().toLower();
    });
    QVector<Photo> photos;
    for (const auto &fi : files) {
        if (exts.contains(fi.suffix().toLower()))
            photos.append(Photo{fi.absoluteFilePath(), Adjustments{}});
    }
    if (photos.isEmpty()) {
        toast(QStringLiteral("No photos in that folder"));
        return;
    }
    ++folderGen_;
    photos_ = photos;
    film_->setPhotos(photos_);
    film_->show();
    panel_->setEnabled(true);
    stack_->setCurrentIndex(1);
    int idx = 0;
    if (!select.isEmpty()) {
        for (int i = 0; i < photos_.size(); ++i) {
            if (QFileInfo(photos_[i].path).canonicalFilePath() == QFileInfo(select).canonicalFilePath()) {
                idx = i;
                break;
            }
        }
    }
    showIndex(idx);
    const int gen = folderGen_;
    QVector<QString> paths;
    paths.reserve(photos_.size());
    for (const auto &p : photos_)
        paths.append(p.path);
    QPointer<PhotoWindow> self(this);
    pool_.start([self, paths, gen] {
        for (int i = 0; i < paths.size(); ++i) {
            if (!self || gen != self->folderGen_)
                return;
            QImage pb = loadImage(paths[i], qMax(kThumbW, kThumbH) * 2);
            if (pb.isNull())
                continue;
            QMetaObject::invokeMethod(self, [self, i, pb, gen] {
                if (!self || gen != self->folderGen_)
                    return;
                self->film_->setThumb(i, pb);
            }, Qt::QueuedConnection);
        }
    });
}

void PhotoWindow::showIndex(int index, bool syncStrip) {
    if (photos_.isEmpty())
        return;
    if (cropping_)
        finishCrop(false);
    index = qBound(0, index, photos_.size() - 1);
    index_ = index;
    Photo *photo = current();
    if (syncStrip)
        film_->select(index);
    panel_->setAdjustments(photo->adj);
    viewer_->setFocus();
    title_->setText(QFileInfo(photo->path).fileName());
    subtitle_->setText(QFileInfo(photo->path).absolutePath());
    updateStatus();
    const int lid = ++loadId_;
    const QString path = photo->path;
    QPointer<PhotoWindow> self(this);
    pool_.start([self, path, lid] {
        QImage pb = loadImage(path);
        if (!self)
            return;
        if (pb.isNull()) {
            QMetaObject::invokeMethod(self, [self, path] {
                if (self)
                    self->toast(QStringLiteral("Could not open %1").arg(QFileInfo(path).fileName()));
            }, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(self, [self, pb, lid, path] {
            if (!self || lid != self->loadId_)
                return;
            Photo *photo = self->current();
            if (!photo || photo->path != path)
                return;
            self->original_ = pb;
            photo->width = pb.width();
            photo->height = pb.height();
            self->rebuildPreview(false);
            self->updateStatus();
        }, Qt::QueuedConnection);
    });
}

void PhotoWindow::rebuildPreview(bool keepView) {
    Photo *photo = current();
    if (!photo || original_.isNull())
        return;
    const QImage rotated = rotateImage(original_, photo->adj.rotation);
    basePreview_ = scalePreview(rotated);
    cropPreview_ = cropping_ ? basePreview_ : cropImage(basePreview_, photo->adj.crop);
    requestProcess(keepView);
}

void PhotoWindow::requestProcess(bool keepView) {
    Photo *photo = current();
    if (!photo || cropPreview_.isNull())
        return;
    pending_ = Pending{photo->adj.copy(), keepView, cropPreview_.copy()};
    if (!busy_)
        kick();
}

void PhotoWindow::kick() {
    if (!pending_)
        return;
    Pending p = *pending_;
    pending_.reset();
    busy_ = true;
    const int pid = ++procId_;
    QPointer<PhotoWindow> self(this);
    pool_.start([self, p, pid] {
        QImage out = applyColor(p.src, p.adj);
        QMetaObject::invokeMethod(self, [self, out, p, pid] {
            if (!self)
                return;
            self->busy_ = false;
            if (pid == self->procId_) {
                if (out.isNull())
                    self->toast(QStringLiteral("Could not process photo"));
                else
                    self->viewer_->setImage(out, p.keepView);
            }
            if (self->pending_)
                self->kick();
            self->updateStatus();
        }, Qt::QueuedConnection);
    });
}

void PhotoWindow::adjChanged(const QString &key, const QVariant &value) {
    Photo *photo = current();
    if (!photo)
        return;
    if (key == QLatin1String("__preset__")) {
        presets::apply(value.toString(), photo->adj);
        panel_->setAdjustments(photo->adj);
        markDirty();
        requestProcess();
        return;
    }
    if (key == QLatin1String("__save_preset__")) {
        askPresetName();
        return;
    }
    if (key == QLatin1String("__crop_ratio__")) {
        const QString v = value.toString();
        std::optional<double> aspect;
        bool original = false;
        if (v == QLatin1String("original")) {
            original = true;
        } else if (v != QLatin1String("free")) {
            const QStringList parts = v.split(QLatin1Char(':'));
            if (parts.size() == 2 && parts[1].toDouble() != 0)
                aspect = parts[0].toDouble() / parts[1].toDouble();
        }
        if (v != QLatin1String("free") && !cropping_)
            cropBtn_->setChecked(true);
        viewer_->setCropAspect(aspect, original);
        return;
    }
    photo->adj.set(key, value.toDouble());
    markDirty();
    requestProcess();
}

void PhotoWindow::askPresetName() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Save preset"),
                                               QStringLiteral("Name this look"), QLineEdit::Normal,
                                               {}, &ok).trimmed();
    Photo *photo = current();
    if (!ok || name.isEmpty() || !photo)
        return;
    presets::save(name, photo->adj);
    panel_->reloadPresets(name);
    toast(QStringLiteral("Saved preset %1").arg(name));
}

void PhotoWindow::resetAdjustments() {
    Photo *photo = current();
    if (!photo)
        return;
    const int rot = photo->adj.rotation;
    const auto crop = photo->adj.crop;
    photo->adj = Adjustments{};
    photo->adj.rotation = rot;
    photo->adj.crop = crop;
    panel_->setAdjustments(photo->adj);
    markDirty();
    requestProcess();
}

void PhotoWindow::rotateBy(int delta) {
    Photo *photo = current();
    if (!photo)
        return;
    photo->adj.rotated(delta);
    markDirty();
    rebuildPreview(false);
}

void PhotoWindow::toggleCrop(bool on) {
    Photo *photo = current();
    if (!photo) {
        cropBtn_->blockSignals(true);
        cropBtn_->setChecked(false);
        cropBtn_->blockSignals(false);
        return;
    }
    if (on) {
        cropping_ = true;
        cropBar_->setVisible(true);
        rebuildPreview(false);
        viewer_->beginCrop(photo->adj.crop);
    } else if (cropping_) {
        finishCrop(false);
    }
}

void PhotoWindow::finishCrop(bool apply) {
    cropping_ = false;
    cropBar_->setVisible(false);
    if (cropBtn_->isChecked()) {
        cropBtn_->blockSignals(true);
        cropBtn_->setChecked(false);
        cropBtn_->blockSignals(false);
    }
    viewer_->endCrop(apply);
}

void PhotoWindow::cropApplied(const std::optional<QRectF> &crop) {
    Photo *photo = current();
    if (!photo)
        return;
    photo->adj.crop = crop;
    markDirty();
    rebuildPreview(false);
}

void PhotoWindow::markDirty() {
    Photo *photo = current();
    if (!photo)
        return;
    photo->dirty = !photo->adj.isDefault();
    film_->setDirty(index_, photo->dirty);
    updateStatus();
}

void PhotoWindow::saveCurrent() {
    Photo *photo = current();
    if (!photo)
        return;
    const QImage orig = original_.copy();
    const Adjustments adj = photo->adj.copy();
    const QString path = photo->path;
    const QString dest = savePathFor(path);
    QPointer<PhotoWindow> self(this);
    pool_.start([self, orig, adj, path, dest] {
        QImage pb = orig.isNull() ? loadImage(path) : orig;
        QImage out = develop(pb, adj, false);
        const bool ok = !out.isNull() && saveImage(out, dest);
        const QImage thumb = ok ? scalePreview(out, qMax(kThumbW, kThumbH) * 2) : QImage();
        QMetaObject::invokeMethod(self, [self, path, dest, ok, out, thumb] {
            if (!self)
                return;
            if (!ok) {
                self->toast(QStringLiteral("Save failed"));
                return;
            }
            Photo *photo = nullptr;
            int i = -1;
            for (int n = 0; n < self->photos_.size(); ++n) {
                if (self->photos_[n].path == path) {
                    photo = &self->photos_[n];
                    i = n;
                    break;
                }
            }
            if (!photo)
                return;
            const bool raw = isRaw(photo->path);
            if (!raw)
                photo->adj = Adjustments{};
            photo->dirty = false;
            if (!out.isNull() && !raw) {
                photo->width = out.width();
                photo->height = out.height();
            }
            self->film_->setDirty(i, false);
            if (!thumb.isNull())
                self->film_->setThumb(i, thumb);
            if (photo == self->current() && !raw) {
                self->panel_->setAdjustments(photo->adj);
                if (!out.isNull()) {
                    self->original_ = out;
                    self->rebuildPreview(false);
                }
            }
            self->toast(QStringLiteral("Saved %1").arg(QFileInfo(dest).fileName()));
            self->updateStatus();
        }, Qt::QueuedConnection);
    });
}

void PhotoWindow::saveAll() {
    QVector<Photo *> dirty;
    for (auto &p : photos_) {
        if (p.dirty)
            dirty.append(&p);
    }
    if (dirty.isEmpty()) {
        toast(QStringLiteral("Nothing to save"));
        return;
    }
    Photo *cur = current();
    struct Job {
        QImage orig;
        Adjustments adj;
        QString path;
        QString dest;
        bool keep;
    };
    QVector<Job> jobs;
    for (Photo *photo : dirty) {
        jobs.append({photo == cur ? original_.copy() : QImage(), photo->adj.copy(), photo->path,
                     savePathFor(photo->path), photo == cur});
    }
    QPointer<PhotoWindow> self(this);
    pool_.start([self, jobs] {
        int ok = 0;
        for (const Job &job : jobs) {
            if (!self)
                return;
            QImage pb = job.orig.isNull() ? loadImage(job.path) : job.orig;
            QImage out = develop(pb, job.adj, false);
            if (out.isNull() || !saveImage(out, job.dest))
                continue;
            const QImage thumb = scalePreview(out, qMax(kThumbW, kThumbH) * 2);
            ++ok;
            QMetaObject::invokeMethod(self, [self, job, out, thumb] {
                if (!self)
                    return;
                Photo *photo = nullptr;
                int i = -1;
                for (int n = 0; n < self->photos_.size(); ++n) {
                    if (self->photos_[n].path == job.path) {
                        photo = &self->photos_[n];
                        i = n;
                        break;
                    }
                }
                if (!photo)
                    return;
                const bool raw = isRaw(photo->path);
                if (!raw)
                    photo->adj = Adjustments{};
                photo->dirty = false;
                if (!out.isNull() && !raw) {
                    photo->width = out.width();
                    photo->height = out.height();
                }
                self->film_->setDirty(i, false);
                if (!thumb.isNull())
                    self->film_->setThumb(i, thumb);
                if (photo == self->current() && !raw) {
                    self->panel_->setAdjustments(photo->adj);
                    if (job.keep && !out.isNull()) {
                        self->original_ = out;
                        self->rebuildPreview(false);
                    }
                }
                self->updateStatus();
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(self, [self, ok] {
            if (self)
                self->toast(QStringLiteral("Saved %1 photo%2").arg(ok).arg(ok == 1 ? QString() : QStringLiteral("s")));
        }, Qt::QueuedConnection);
    });
}

void PhotoWindow::deleteSelected() {
    QVector<int> sel = film_->selected();
    if (sel.isEmpty() && index_ >= 0)
        sel.append(index_);
    if (sel.isEmpty())
        return;
    if (cropping_)
        finishCrop(false);
    std::sort(sel.begin(), sel.end());
    const int pivot = sel.first();
    int deleted = 0;
    QString lastName;
    for (int n = sel.size() - 1; n >= 0; --n) {
        const int gone = sel[n];
        if (gone < 0 || gone >= photos_.size())
            continue;
        const QString path = photos_[gone].path;
        const QString name = QFileInfo(path).fileName();
        if (!QFile::moveToTrash(path) && !QFile::remove(path)) {
            toast(QStringLiteral("Could not delete %1").arg(name));
            continue;
        }
        lastName = name;
        photos_.removeAt(gone);
        film_->removeAt(gone);
        ++deleted;
        if (index_ == gone)
            index_ = -1;
        else if (index_ > gone)
            --index_;
    }
    if (deleted == 0)
        return;
    if (deleted == 1)
        toast(QStringLiteral("Deleted %1").arg(lastName));
    else
        toast(QStringLiteral("Deleted %1 photos").arg(deleted));
    if (photos_.isEmpty()) {
        ++loadId_;
        ++procId_;
        ++folderGen_;
        pending_.reset();
        index_ = -1;
        original_ = {};
        basePreview_ = {};
        cropPreview_ = {};
        viewer_->clear();
        film_->hide();
        panel_->setEnabled(false);
        stack_->setCurrentIndex(0);
        title_->setText(QStringLiteral("Photo"));
        subtitle_->setText(QStringLiteral("Open a folder of photos"));
        status_->clear();
        return;
    }
    showIndex(qBound(0, pivot, photos_.size() - 1));
}

void PhotoWindow::updateStatus() {
    Photo *photo = current();
    if (!photo) {
        status_->clear();
        return;
    }
    int dirty = 0;
    for (const auto &p : photos_) {
        if (p.dirty)
            ++dirty;
    }
    const int zoom = int(std::lround(viewer_->zoom() * 100));
    QStringList bits;
    bits << QStringLiteral("%1 / %2").arg(index_ + 1).arg(photos_.size());
    bits << QFileInfo(photo->path).fileName();
    if (photo->width)
        bits << QStringLiteral("%1×%2").arg(photo->width).arg(photo->height);
    bits << QStringLiteral("%1%").arg(zoom);
    if (dirty)
        bits << QStringLiteral("%1 unsaved").arg(dirty);
    status_->setText(bits.join(QStringLiteral("  ·  ")));
}

void PhotoWindow::toast(const QString &text) {
    toast_->showText(text);
}

void PhotoWindow::keyPressEvent(QKeyEvent *event) {
    if (qobject_cast<QLineEdit *>(focusWidget())) {
        QWidget::keyPressEvent(event);
        return;
    }
    const int key = event->key();
    if (qobject_cast<QSlider *>(focusWidget()) &&
        (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Home || key == Qt::Key_End)) {
        QWidget::keyPressEvent(event);
        return;
    }
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    if ((key == Qt::Key_Left || key == Qt::Key_PageUp) && !ctrl) {
        showIndex(index_ - 1);
        return;
    }
    if ((key == Qt::Key_Right || key == Qt::Key_PageDown) && !ctrl) {
        showIndex(index_ + 1);
        return;
    }
    if (key == Qt::Key_Delete && !ctrl) {
        deleteSelected();
        return;
    }
    if (key == Qt::Key_Plus || key == Qt::Key_Equal) {
        viewer_->zoomBy(1.25);
        updateStatus();
        return;
    }
    if (key == Qt::Key_Minus) {
        viewer_->zoomBy(1.0 / 1.25);
        updateStatus();
        return;
    }
    if (key == Qt::Key_0) {
        viewer_->fit();
        updateStatus();
        return;
    }
    if (key == Qt::Key_1) {
        viewer_->actual();
        updateStatus();
        return;
    }
    if (key == Qt::Key_BracketLeft) {
        rotateBy(-90);
        return;
    }
    if (key == Qt::Key_BracketRight) {
        rotateBy(90);
        return;
    }
    if (key == Qt::Key_C && !ctrl) {
        cropBtn_->toggle();
        return;
    }
    if (key == Qt::Key_Escape && cropping_) {
        finishCrop(false);
        return;
    }
    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && cropping_) {
        finishCrop(true);
        return;
    }
    if (ctrl && key == Qt::Key_S) {
        if (event->modifiers() & Qt::ShiftModifier)
            saveAll();
        else
            saveCurrent();
        return;
    }
    if (ctrl && key == Qt::Key_O) {
        openFolder();
        return;
    }
    if (ctrl && key == Qt::Key_R) {
        resetAdjustments();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PhotoWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void PhotoWindow::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    const QString path = urls.first().toLocalFile();
    if (!path.isEmpty())
        loadPath(path);
}

void PhotoWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (toast_ && toast_->isVisible())
        toast_->move((width() - toast_->width()) / 2, height() - toast_->height() - 48);
}
