#pragma once

#include "adjustments.h"

#include <QSet>
#include <QVector>
#include <QWidget>
#include <functional>

class QHBoxLayout;
class QLabel;
class QScrollArea;
class QToolButton;

constexpr int kThumbW = 112;
constexpr int kThumbH = 74;

class Filmstrip : public QWidget {
    Q_OBJECT
public:
    explicit Filmstrip(QWidget *parent = nullptr);

    void setPhotos(const QVector<Photo> &photos);
    void removeAt(int index);
    void setThumb(int index, const QImage &image);
    void setDirty(int index, bool dirty);
    void select(int index, bool scroll = true);
    QVector<int> selected() const;

    std::function<void(int)> onSelect;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void scrollTo(int index);
    void handleClick(int index);
    void applyChecked();
    void updateNumbers();
    QWidget *wrapAt(int index) const;

    QScrollArea *scroll_ = nullptr;
    QWidget *row_ = nullptr;
    QHBoxLayout *rowLay_ = nullptr;
    QVector<QToolButton *> buttons_;
    QVector<QLabel *> dots_;
    QVector<QLabel *> nums_;
    QSet<int> selected_;
    int index_ = -1;
    int anchor_ = -1;
};
