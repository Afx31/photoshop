#pragma once

#include "adjustments.h"

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
    void setThumb(int index, const QImage &image);
    void setDirty(int index, bool dirty);
    void select(int index, bool scroll = true);

    std::function<void(int)> onSelect;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void scrollTo(int index);

    QScrollArea *scroll_ = nullptr;
    QWidget *row_ = nullptr;
    QHBoxLayout *rowLay_ = nullptr;
    QVector<QToolButton *> buttons_;
    QVector<QLabel *> dots_;
    int index_ = -1;
};
