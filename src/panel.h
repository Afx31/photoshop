#pragma once

#include "adjustments.h"

#include <QHash>
#include <QVariant>
#include <QWidget>
#include <functional>

class QComboBox;
class QLabel;
class QSlider;

class AdjustPanel : public QWidget {
    Q_OBJECT
public:
    explicit AdjustPanel(QWidget *parent = nullptr);

    void setAdjustments(const Adjustments &adj);
    void reloadPresets(const QString &select = {});

    std::function<void(const QString &, const QVariant &)> onChange;
    std::function<void()> onReset;

private:
    QWidget *presetsBox();
    QWidget *cropBox();
    QWidget *row(const QString &key, const QString &name, double lo, double hi, double step);
    QString fmt(const QString &key, double value) const;
    QString selectedName() const;
    void pickRatio(const QString &value);
    void showInstagramInfo();

    bool syncing_ = false;
    QHash<QString, QSlider *> scales_;
    QHash<QString, QLabel *> values_;
    QHash<QString, double> scaleMul_;
    QComboBox *presetDrop_ = nullptr;
};
