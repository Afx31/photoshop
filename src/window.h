#pragma once

#include "adjustments.h"

#include <QImage>
#include <QThreadPool>
#include <QWidget>
#include <functional>
#include <optional>

class AdjustPanel;
class Filmstrip;
class PhotoViewer;
class QLabel;
class QPushButton;
class QStackedWidget;
class QToolButton;
class ToastLabel;

class PhotoWindow : public QWidget {
    Q_OBJECT
public:
    explicit PhotoWindow(QWidget *parent = nullptr);
    ~PhotoWindow() override;

    void loadPath(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    Photo *current();
    void openFolder();
    void showIndex(int index);
    void rebuildPreview(bool keepView);
    void requestProcess(bool keepView = true);
    void kick();
    void adjChanged(const QString &key, const QVariant &value);
    void askPresetName();
    void resetAdjustments();
    void rotateBy(int delta);
    void toggleCrop(bool on);
    void finishCrop(bool apply);
    void cropApplied(const std::optional<QRectF> &crop);
    void markDirty();
    void saveCurrent();
    void saveAll();
    void updateStatus();
    void toast(const QString &text);
    QToolButton *iconBtn(const QString &theme, const QString &tip, std::function<void()> cb);

    QVector<Photo> photos_;
    int index_ = -1;
    QImage original_;
    QImage basePreview_;
    QImage cropPreview_;
    bool busy_ = false;
    struct Pending {
        Adjustments adj;
        bool keepView = true;
        QImage src;
    };
    std::optional<Pending> pending_;
    int loadId_ = 0;
    int procId_ = 0;
    int folderGen_ = 0;
    bool cropping_ = false;
    QThreadPool pool_;

    PhotoViewer *viewer_ = nullptr;
    Filmstrip *film_ = nullptr;
    AdjustPanel *panel_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QLabel *title_ = nullptr;
    QLabel *subtitle_ = nullptr;
    QLabel *status_ = nullptr;
    QToolButton *cropBtn_ = nullptr;
    QWidget *cropBar_ = nullptr;
    ToastLabel *toast_ = nullptr;
};
