#include "filmstrip.h"

#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

Filmstrip::Filmstrip(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("filmstrip"));
    setFixedHeight(118);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->installEventFilter(this);
    scroll_->viewport()->installEventFilter(this);
    row_ = new QWidget;
    rowLay_ = new QHBoxLayout(row_);
    rowLay_->setContentsMargins(10, 8, 10, 8);
    rowLay_->setSpacing(8);
    rowLay_->addStretch();
    scroll_->setWidget(row_);
    root->addWidget(scroll_);
}

bool Filmstrip::eventFilter(QObject *obj, QEvent *event) {
    if ((obj == scroll_ || obj == scroll_->viewport()) && event->type() == QEvent::Wheel) {
        auto *e = static_cast<QWheelEvent *>(event);
        QScrollBar *bar = scroll_->horizontalScrollBar();
        bar->setValue(bar->value() - e->angleDelta().y());
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void Filmstrip::setPhotos(const QVector<Photo> &photos) {
    while (QLayoutItem *it = rowLay_->takeAt(0)) {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    buttons_.clear();
    dots_.clear();
    index_ = -1;
    for (int i = 0; i < photos.size(); ++i) {
        auto *wrap = new QWidget;
        wrap->setFixedSize(kThumbW + 8, kThumbH + 8);
        auto *btn = new QToolButton(wrap);
        btn->setObjectName(QStringLiteral("thumb"));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setAutoRaise(true);
        btn->setIconSize(QSize(kThumbW, kThumbH));
        btn->setGeometry(2, 2, kThumbW + 4, kThumbH + 4);
        btn->setToolTip(QFileInfo(photos[i].path).fileName());
        btn->setCheckable(true);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { border: 2px solid transparent; border-radius: 6px; padding: 0; }"
            "QToolButton:checked { border: 2px solid palette(highlight); }"));
        connect(btn, &QToolButton::clicked, this, [this, i] {
            if (onSelect)
                onSelect(i);
        });
        auto *dot = new QLabel(wrap);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QStringLiteral("background: palette(highlight); border-radius: 4px;"));
        dot->move(wrap->width() - 14, 6);
        dot->setAttribute(Qt::WA_TransparentForMouseEvents);
        dot->setVisible(photos[i].dirty);
        buttons_.append(btn);
        dots_.append(dot);
    }
    for (auto *b : buttons_)
        rowLay_->addWidget(b->parentWidget());
    rowLay_->addStretch();
}

void Filmstrip::setThumb(int index, const QImage &image) {
    if (index < 0 || index >= buttons_.size() || image.isNull())
        return;
    const QImage scaled = image.scaled(kThumbW, kThumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    buttons_[index]->setIcon(QIcon(QPixmap::fromImage(scaled)));
}

void Filmstrip::setDirty(int index, bool dirty) {
    if (index >= 0 && index < dots_.size())
        dots_[index]->setVisible(dirty);
}

void Filmstrip::select(int index, bool scroll) {
    if (buttons_.isEmpty())
        return;
    if (index_ >= 0 && index_ < buttons_.size())
        buttons_[index_]->setChecked(false);
    index_ = index;
    if (index >= 0 && index < buttons_.size()) {
        buttons_[index]->setChecked(true);
        if (scroll)
            scrollTo(index);
    }
}

void Filmstrip::scrollTo(int index) {
    if (index < 0 || index >= buttons_.size())
        return;
    QWidget *w = buttons_[index]->parentWidget();
    const int x = w->mapTo(row_, QPoint(0, 0)).x();
    const int ww = w->width();
    QScrollBar *bar = scroll_->horizontalScrollBar();
    const int val = bar->value();
    const int page = scroll_->viewport()->width();
    if (x < val)
        bar->setValue(qMax(0, x - 16));
    else if (x + ww > val + page)
        bar->setValue(x + ww - page + 16);
}
