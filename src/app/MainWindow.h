#pragma once
#include <QMainWindow>
#include <QMargins>
#include <QPointer>
#include <QVector>
#include <memory>
class QTemporaryDir;
class QAction;
class QVBoxLayout;
class QMenuBar;
class QToolButton;
namespace Zqxxxt {
class SceneWidget;
class ZqbsPage;
class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(const QString &project);
    ~MainWindow() override;
    bool ready() const { return m_page != nullptr; }
    bool earthReady() const;
    int activeDynamicCount() const;
private:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
    bool openProject(const QString &path);
    bool save();
    void exportProject(bool openCopy);
    void togglePresentation();
    void message(const QString &error);
    void showAbout();
    void setMapFullscreen(bool fullScreen);
    void toggleWindowFullscreen();
    QString m_project;
    QString m_editProject;
    std::unique_ptr<QTemporaryDir> m_presentation;
    SceneWidget *m_scene = nullptr;
    ZqbsPage *m_page = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
    QAction *m_presentationAction = nullptr;
    QMenuBar *m_mainMenuBar = nullptr;
    QWidget *m_windowHeader = nullptr;
    QToolButton *m_maximizeButton = nullptr;
    QRect m_windowFullscreenGeometry;
    Qt::WindowStates m_windowFullscreenPreviousState;
    QVector<QPair<QPointer<QWidget>, bool>> m_mapFullscreenVisibility;
    QMargins m_mapFullscreenMargins;
    QRect m_mapFullscreenGeometry;
    Qt::WindowStates m_mapFullscreenPreviousState;
    int m_mapFullscreenSpacing = 0;
    bool m_mapFullscreen = false;
};
}
