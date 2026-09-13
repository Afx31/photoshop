#include "panel.h"
#include "presets.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <tuple>

namespace {

struct SliderRow {
    const char *key;
    const char *name;
    double lo;
    double hi;
    double step;
};

const SliderRow kLight[] = {
    {"exposure", "Exposure", -4.0, 4.0, 0.05},
    {"contrast", "Contrast", -100.0, 100.0, 1.0},
    {"highlights", "Highlights", -100.0, 100.0, 1.0},
    {"shadows", "Shadows", -100.0, 100.0, 1.0},
    {"whites", "Whites", -100.0, 100.0, 1.0},
    {"blacks", "Blacks", -100.0, 100.0, 1.0},
};

const SliderRow kColor[] = {
    {"temperature", "Temperature", -100.0, 100.0, 1.0},
    {"tint", "Tint", -100.0, 100.0, 1.0},
    {"vibrance", "Vibrance", -100.0, 100.0, 1.0},
    {"saturation", "Saturation", -100.0, 100.0, 1.0},
};

class DblClickFilter : public QObject {
public:
    QSlider *slider = nullptr;
    explicit DblClickFilter(QSlider *s) : QObject(s), slider(s) {}
protected:
    bool eventFilter(QObject *, QEvent *e) override {
        if (e->type() == QEvent::MouseButtonDblClick) {
            slider->setValue(0);
            return true;
        }
        return false;
    }
};

} // namespace

AdjustPanel::AdjustPanel(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("adjustPanel"));
    setFixedWidth(300);
    auto *scroller = new QScrollArea(this);
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    auto *lay = new QVBoxLayout(inner);
    lay->setContentsMargins(14, 12, 14, 12);
    lay->setSpacing(14);
    scroller->setWidget(inner);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroller);

    lay->addWidget(presetsBox());

    auto addGroup = [&](const QString &title, const SliderRow *rows, int n) {
        auto *lab = new QLabel(title);
        QFont f = lab->font();
        f.setBold(true);
        lab->setFont(f);
        lay->addWidget(lab);
        auto *group = new QWidget;
        auto *g = new QVBoxLayout(group);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(6);
        for (int i = 0; i < n; ++i)
            g->addWidget(row(QLatin1String(rows[i].key), QLatin1String(rows[i].name), rows[i].lo, rows[i].hi, rows[i].step));
        lay->addWidget(group);
    };
    addGroup(QStringLiteral("Light"), kLight, int(sizeof(kLight) / sizeof(kLight[0])));
    addGroup(QStringLiteral("Color"), kColor, int(sizeof(kColor) / sizeof(kColor[0])));
    lay->addWidget(cropBox());

    auto *reset = new QPushButton(QStringLiteral("Reset"));
    reset->setFlat(true);
    connect(reset, &QPushButton::clicked, this, [this] {
        if (onReset)
            onReset();
    });
    lay->addWidget(reset);
    lay->addStretch();
}

QWidget *AdjustPanel::presetsBox() {
    auto *box = new QWidget;
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);
    auto *lab = new QLabel(QStringLiteral("Presets"));
    QFont f = lab->font();
    f.setBold(true);
    lab->setFont(f);
    lay->addWidget(lab);
    presetDrop_ = new QComboBox;
    presetDrop_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(presetDrop_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (syncing_)
            return;
        const QString name = selectedName();
        if (!name.isEmpty() && onChange)
            onChange(QStringLiteral("__preset__"), name);
    });
    auto *roww = new QWidget;
    auto *rl = new QHBoxLayout(roww);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    auto *save = new QPushButton(QStringLiteral("Save"));
    connect(save, &QPushButton::clicked, this, [this] {
        if (onChange)
            onChange(QStringLiteral("__save_preset__"), QString());
    });
    auto *del = new QPushButton(QStringLiteral("Delete"));
    del->setFlat(true);
    connect(del, &QPushButton::clicked, this, [this] {
        const QString name = selectedName();
        if (name.isEmpty())
            return;
        presets::remove(name);
        reloadPresets();
    });
    rl->addWidget(presetDrop_, 1);
    rl->addWidget(save);
    rl->addWidget(del);
    lay->addWidget(roww);
    reloadPresets();
    return box;
}

QWidget *AdjustPanel::cropBox() {
    auto *box = new QWidget;
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *head = new QWidget;
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    auto *lab = new QLabel(QStringLiteral("Crop"));
    QFont f = lab->font();
    f.setBold(true);
    lab->setFont(f);
    auto *info = new QToolButton;
    info->setText(QStringLiteral("i"));
    info->setToolTip(QStringLiteral("Instagram posting ratios"));
    info->setAutoRaise(true);
    info->setFixedSize(22, 22);
    info->setStyleSheet(QStringLiteral(
        "QToolButton { border: 1px solid palette(mid); border-radius: 11px; font-weight: 600; }"
        "QToolButton:hover { background: palette(midlight); }"));
    connect(info, &QToolButton::clicked, this, &AdjustPanel::showInstagramInfo);
    hl->addWidget(lab);
    hl->addStretch();
    hl->addWidget(info);
    lay->addWidget(head);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);

    auto makeBtn = [&](const QString &label, const QString &value, const QString &tip) {
        auto *b = new QToolButton;
        b->setText(label);
        b->setCheckable(true);
        b->setToolTip(tip);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setStyleSheet(QStringLiteral(
            "QToolButton { padding: 4px 2px; border: 1px solid palette(mid); border-radius: 6px; }"
            "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }"));
        group->addButton(b);
        connect(b, &QToolButton::clicked, this, [this, value] { pickRatio(value); });
        return b;
    };

    auto addGrid = [&](const QString &title, const std::initializer_list<std::tuple<const char *, const char *, const char *>> &items) {
        if (!title.isEmpty()) {
            auto *t = new QLabel(title);
            QPalette p = t->palette();
            p.setColor(QPalette::WindowText, p.color(QPalette::PlaceholderText));
            t->setPalette(p);
            lay->addWidget(t);
        }
        auto *gridW = new QWidget;
        auto *g = new QGridLayout(gridW);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(6);
        int i = 0;
        for (const auto &[label, value, tip] : items) {
            g->addWidget(makeBtn(QString::fromUtf8(label), QString::fromUtf8(value), QString::fromUtf8(tip)), i / 4, i % 4);
            ++i;
        }
        lay->addWidget(gridW);
    };

    addGrid({}, {
        {"Free", "free", "No aspect lock"},
        {"Original", "original", "Match the photo’s aspect ratio"},
        {"1:1", "1:1", "Square · Instagram feed"},
    });
    addGrid(QStringLiteral("Landscape"), {
        {"3:2", "3:2", "35mm stills"},
        {"4:3", "4:3", "Classic / Micro Four Thirds"},
        {"16:9", "16:9", "Widescreen"},
        {"5:4", "5:4", "Large format"},
        {"1.91:1", "1.91:1", "Instagram feed landscape"},
        {"7:5", "7:5", "5×7 print"},
    });
    addGrid(QStringLiteral("Portrait"), {
        {"2:3", "2:3", "35mm portrait"},
        {"3:4", "3:4", "Classic portrait"},
        {"4:5", "4:5", "Instagram feed portrait"},
        {"9:16", "9:16", "Instagram Stories & Reels"},
        {"5:7", "5:7", "5×7 print"},
    });

    if (!group->buttons().isEmpty())
        group->buttons().first()->setChecked(true);
    return box;
}

void AdjustPanel::pickRatio(const QString &value) {
    if (onChange)
        onChange(QStringLiteral("__crop_ratio__"), value);
}

void AdjustPanel::showInstagramInfo() {
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Instagram ratios"));
    box.setIcon(QMessageBox::Information);
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<p><b>Feed</b></p>"
        "<p>Posts are cropped to sit between <b>1.91:1</b> (landscape) and <b>4:5</b> (portrait).</p>"
        "<ul>"
        "<li><b>1:1</b> — square, always safe (1080×1080)</li>"
        "<li><b>4:5</b> — recommended portrait (1080×1350)</li>"
        "<li><b>1.91:1</b> — landscape (1080×566)</li>"
        "</ul>"
        "<p><b>Stories, Reels &amp; Highlights</b></p>"
        "<ul>"
        "<li><b>9:16</b> — full screen (1080×1920)</li>"
        "</ul>"
        "<p>4:5 fills more of the portrait feed than 1:1. Anything wider than 1.91:1 or taller than 4:5 is cropped on the feed. Stories and Reels letterbox other ratios.</p>"));
    box.exec();
}

QWidget *AdjustPanel::row(const QString &key, const QString &name, double lo, double hi, double step) {
    auto *col = new QWidget;
    auto *lay = new QVBoxLayout(col);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    auto *top = new QWidget;
    auto *tl = new QHBoxLayout(top);
    tl->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(name);
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *value = new QLabel(QStringLiteral("0"));
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setMinimumWidth(48);
    QPalette pal = value->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::PlaceholderText));
    value->setPalette(pal);
    tl->addWidget(title);
    tl->addWidget(value);
    const double mul = step < 1.0 ? 100.0 : 1.0;
    auto *scale = new QSlider(Qt::Horizontal);
    scale->setRange(int(std::lround(lo * mul)), int(std::lround(hi * mul)));
    scale->setSingleStep(int(std::lround(step * mul)));
    scale->setPageStep(int(std::lround(step * mul * 10)));
    scale->setValue(0);
    scale->installEventFilter(new DblClickFilter(scale));
    connect(scale, &QSlider::valueChanged, this, [this, key, mul](int iv) {
        const double v = iv / mul;
        if (values_.contains(key))
            values_[key]->setText(fmt(key, v));
        if (syncing_ || !onChange)
            return;
        onChange(key, v);
    });
    lay->addWidget(top);
    lay->addWidget(scale);
    scales_[key] = scale;
    values_[key] = value;
    scaleMul_[key] = mul;
    return col;
}

QString AdjustPanel::fmt(const QString &key, double value) const {
    if (key == QLatin1String("exposure"))
        return QString::asprintf("%+.2f", value);
    return QString::asprintf("%+.0f", value);
}

void AdjustPanel::setAdjustments(const Adjustments &adj) {
    syncing_ = true;
    for (const auto &key : kColorKeys) {
        const double val = adj.get(key);
        const double mul = scaleMul_.value(key, 1.0);
        if (QSlider *s = scales_.value(key))
            s->setValue(int(std::lround(val * mul)));
        if (QLabel *l = values_.value(key))
            l->setText(fmt(key, val));
    }
    syncing_ = false;
}

void AdjustPanel::reloadPresets(const QString &select) {
    if (!presetDrop_)
        return;
    syncing_ = true;
    presetDrop_->clear();
    presetDrop_->addItem(QStringLiteral("None"));
    int chosen = 0;
    const auto names = presets::names();
    for (int i = 0; i < names.size(); ++i) {
        presetDrop_->addItem(names[i]);
        if (!select.isEmpty() && names[i] == select)
            chosen = i + 1;
    }
    presetDrop_->setCurrentIndex(chosen);
    syncing_ = false;
}

QString AdjustPanel::selectedName() const {
    if (!presetDrop_ || presetDrop_->currentIndex() <= 0)
        return {};
    return presetDrop_->currentText();
}
