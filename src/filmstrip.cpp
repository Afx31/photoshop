#include "filmstrip.h"

#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>

Filmstrip::Filmstrip(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("filmstrip"));
    setFixedHeight(148);
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
    rowLay_->setContentsMargins(10, 6, 10, 6);
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
    nums_.clear();
    selected_.clear();
    index_ = -1;
    anchor_ = -1;
    for (int i = 0; i < photos.size(); ++i) {
        auto *wrap = new QWidget;
        wrap->setFixedWidth(kThumbW + 8);
        auto *vl = new QVBoxLayout(wrap);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(2);

        auto *thumbBox = new QWidget;
        thumbBox->setFixedSize(kThumbW + 8, kThumbH + 8);
        auto *btn = new QToolButton(thumbBox);
        btn->setObjectName(QStringLiteral("thumb"));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setAutoRaise(true);
        btn->setAutoExclusive(false);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setIconSize(QSize(kThumbW, kThumbH));
        btn->setGeometry(1, 1, kThumbW + 6, kThumbH + 6);
        const QString fn = QFileInfo(photos[i].path).fileName();
        btn->setToolTip(fn);
        btn->setCheckable(true);
        btn->setStyleSheet(QStringLiteral(
            "QToolButton { border: 3px solid transparent; border-radius: 6px; padding: 0; }"
            "QToolButton:checked { border: 3px solid #ff0000; }"));
        connect(btn, &QToolButton::clicked, this, [this] {
            auto *b = qobject_cast<QToolButton *>(sender());
            handleClick(buttons_.indexOf(b));
        });
        auto *dot = new QLabel(thumbBox);
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QStringLiteral("background: palette(highlight); border-radius: 4px;"));
        dot->move(thumbBox->width() - 14, 6);
        dot->setAttribute(Qt::WA_TransparentForMouseEvents);
        dot->setVisible(photos[i].dirty);

        auto *num = new QLabel(QString::number(i + 1));
        num->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        num->setFixedHeight(14);
        QFont nf = num->font();
        nf.setPointSizeF(qMax(8.0, nf.pointSizeF() - 1));
        num->setFont(nf);

        auto *name = new QLabel;
        name->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        name->setFixedHeight(16);
        name->setFont(nf);
        QPalette np = name->palette();
        np.setColor(QPalette::WindowText, np.color(QPalette::PlaceholderText));
        name->setPalette(np);
        num->setPalette(np);
        name->setText(QFontMetrics(nf).elidedText(fn, Qt::ElideMiddle, kThumbW + 8));
        name->setToolTip(fn);

        vl->addWidget(thumbBox, 0, Qt::AlignHCenter);
        vl->addWidget(num);
        vl->addWidget(name);

        buttons_.append(btn);
        dots_.append(dot);
        nums_.append(num);
        rowLay_->addWidget(wrap);
    }
    rowLay_->addStretch();
}

QWidget *Filmstrip::wrapAt(int index) const {
    if (index < 0 || index >= buttons_.size())
        return nullptr;
    QWidget *w = buttons_[index]->parentWidget();
    while (w && w->parentWidget() != row_)
        w = w->parentWidget();
    return w;
}

void Filmstrip::removeAt(int index) {
    if (index < 0 || index >= buttons_.size())
        return;
    QWidget *wrap = wrapAt(index);
    if (wrap)
        rowLay_->removeWidget(wrap);
    buttons_.removeAt(index);
    dots_.removeAt(index);
    nums_.removeAt(index);
    if (wrap)
        wrap->deleteLater();
    QSet<int> next;
    for (int s : selected_) {
        if (s == index)
            continue;
        next.insert(s > index ? s - 1 : s);
    }
    selected_ = next;
    if (index_ == index)
        index_ = -1;
    else if (index_ > index)
        --index_;
    if (anchor_ == index)
        anchor_ = index_;
    else if (anchor_ > index)
        --anchor_;
    updateNumbers();
}

void Filmstrip::updateNumbers() {
    for (int i = 0; i < nums_.size(); ++i)
        nums_[i]->setText(QString::number(i + 1));
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
    selected_.clear();
    index_ = index;
    anchor_ = index;
    if (index >= 0 && index < buttons_.size())
        selected_.insert(index);
    applyChecked();
    if (scroll)
        scrollTo(index);
}

QVector<int> Filmstrip::selected() const {
    QVector<int> out;
    out.reserve(selected_.size());
    for (int i : selected_)
        out.append(i);
    std::sort(out.begin(), out.end());
    return out;
}

void Filmstrip::applyChecked() {
    for (int i = 0; i < buttons_.size(); ++i)
        buttons_[i]->setChecked(selected_.contains(i));
}

void Filmstrip::handleClick(int index) {
    if (index < 0 || index >= buttons_.size())
        return;
    const auto mods = QGuiApplication::keyboardModifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);
    if (shift && (anchor_ >= 0 || index_ >= 0)) {
        const int from = anchor_ >= 0 ? anchor_ : index_;
        const int a = qMin(from, index);
        const int b = qMax(from, index);
        if (!ctrl)
            selected_.clear();
        for (int i = a; i <= b; ++i)
            selected_.insert(i);
        selected_.insert(index);
        index_ = index;
        applyChecked();
        scrollTo(index);
        if (onSelect)
            onSelect(index);
        return;
    }
    if (ctrl) {
        if (selected_.contains(index) && selected_.size() > 1 && index != index_) {
            selected_.remove(index);
            applyChecked();
            return;
        }
        selected_.insert(index);
        index_ = index;
        anchor_ = index;
        applyChecked();
        scrollTo(index);
        if (onSelect)
            onSelect(index);
        return;
    }
    selected_.clear();
    selected_.insert(index);
    index_ = index;
    anchor_ = index;
    applyChecked();
    scrollTo(index);
    if (onSelect)
        onSelect(index);
}

void Filmstrip::scrollTo(int index) {
    QWidget *w = wrapAt(index);
    if (!w)
        return;
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
