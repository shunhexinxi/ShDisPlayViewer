#include "app/AppPaths.h"
#include "app/MainWindow.h"
#include "app/ProjectStore.h"
#include "modules/zqbs/ZqbsPage.h"
#include "modules/zqts/SceneWidget.h"
#include "ui/AppTheme.h"
#include "ui/AppIcon.h"
#include <QApplication>
#include <QAction>
#include <QCommandLineParser>
#include <QDir>
#include <QSurfaceFormat>
#include <QTimer>
#include <QPushButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>
#if defined(ZQXXXT_ENABLE_EARTH) && defined(__linux__)
#include <osgDB/Registry>
#endif
#include <iostream>

int main(int argc, char **argv)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3); format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setDepthBufferSize(24); format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ShDisplayViewer"));
    QCoreApplication::setApplicationName(QStringLiteral("ShDisplayViewer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.2"));
    app.setWindowIcon(Zqxxxt::applicationIcon());
    app.setDesktopFileName(QStringLiteral("ShDisplayViewer"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("独立部署与步骤演练客户端"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({QStringLiteral("project"), QStringLiteral("项目目录"), QStringLiteral("directory")});
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("打开演练界面并验证地图初始化后退出")});
    parser.addOption({QStringLiteral("smoke-wait-ms"), QStringLiteral("冒烟测试截图前等待的毫秒数（默认 12000）"), QStringLiteral("milliseconds")});
    parser.addOption({QStringLiteral("smoke-screenshot"), QStringLiteral("冒烟测试界面截图路径"), QStringLiteral("file")});
    parser.addOption({QStringLiteral("smoke-report"), QStringLiteral("冒烟测试结果 JSON 路径"), QStringLiteral("file")});
    parser.addOption({QStringLiteral("smoke-fullscreen-cycle"), QStringLiteral("冒烟测试地图全屏进入与退出")});
    parser.addOption({QStringLiteral("smoke-window-ui-cycle"), QStringLiteral("冒烟测试项目菜单和标题栏全屏/还原")});
    parser.addOption({QStringLiteral("smoke-open-manual"), QStringLiteral("冒烟测试通过帮助菜单打开本地 PDF 手册")});
    parser.addOption({QStringLiteral("create-demo"), QStringLiteral("在 --project 空目录生成示例并退出")});
    parser.process(app);
    if ((parser.isSet("smoke-fullscreen-cycle") || parser.isSet("smoke-window-ui-cycle"))
        && !parser.isSet("smoke-test")) {
        std::cerr << "fullscreen/UI cycles require --smoke-test\n";
        return 1;
    }
    int smokeWaitMs = 12000;
    if (parser.isSet("smoke-test") && parser.isSet("smoke-wait-ms")) {
        bool valid = false;
        const int requested = parser.value("smoke-wait-ms").toInt(&valid);
        if (!valid || requested < 1500 || requested > 120000) {
            std::cerr << "--smoke-wait-ms must be between 1500 and 120000\n";
            return 1;
        }
        smokeWaitMs = requested;
    }
    if (parser.isSet("smoke-fullscreen-cycle") && smokeWaitMs < 5000) {
        std::cerr << "--smoke-fullscreen-cycle requires --smoke-wait-ms >= 5000\n";
        return 1;
    }
    if (parser.isSet("smoke-window-ui-cycle") && smokeWaitMs < 7500) {
        std::cerr << "--smoke-window-ui-cycle requires --smoke-wait-ms >= 7500\n";
        return 1;
    }
    if (parser.isSet("smoke-open-manual") && (!parser.isSet("smoke-test") || smokeWaitMs < 6000)) {
        std::cerr << "--smoke-open-manual requires --smoke-test and --smoke-wait-ms >= 6000\n";
        return 1;
    }
    const auto root = parser.isSet("project") ? parser.value("project")
        : Zqxxxt::AppPaths::defaultProjectDir();
    QString pathError;
    if (!Zqxxxt::AppPaths::initialize({}, root, &pathError)) {
        std::cerr << pathError.toStdString() << '\n';
        QMessageBox::critical(nullptr, QStringLiteral("ShDisplayViewer"), pathError);
        return 1;
    }
    if (parser.isSet("create-demo")) {
        QString error;
        if (!Zqxxxt::ProjectStore::create(root, error)) { std::cerr << error.toStdString() << '\n'; return 1; }
        return 0;
    }
    Zqxxxt::AppTheme::apply(app);
    int exitCode = 0;
    {
        Zqxxxt::MainWindow window(root);
        if (!window.ready()) {
            exitCode = 1;
        } else {
            window.show();
            bool startupBasicScenePassed = false;
            bool startupScreenshotSaved = true;
            bool windowUiCyclePassed = false;
            bool manualOpenPassed = false;
            if (parser.isSet("smoke-open-manual")) QTimer::singleShot(4500, &app, [&] {
                if (auto *manual = window.findChild<QAction *>(QStringLiteral("helpManual"))) {
                    manual->trigger();
                    manualOpenPassed = window.property("manualOpenSucceeded").toBool();
                }
            });
            if (parser.isSet("smoke-window-ui-cycle")) QTimer::singleShot(500, &app, [&] {
                auto *menu = window.findChild<QMenu *>(QStringLiteral("projectMenu"));
                auto *save = window.findChild<QAction *>(QStringLiteral("projectSave"));
                auto *presentation = window.findChild<QAction *>(QStringLiteral("projectPresentation"));
                auto *maximize = window.findChild<QToolButton *>(QStringLiteral("viewerMaximize"));
                auto *header = window.findChild<QWidget *>(QStringLiteral("viewerWindowHeader"));
                if (!menu || !save || !presentation || !maximize || !header) return;
                const bool controls = window.findChildren<QToolBar *>().isEmpty()
                    && menu->actions().contains(save) && menu->actions().contains(presentation)
                    && window.windowFlags().testFlag(Qt::FramelessWindowHint);
                const QString originalDataDir = Zqxxxt::AppPaths::dataDir();
                save->trigger();
                presentation->trigger();
                const bool enteredPresentation = presentation->isChecked()
                    && Zqxxxt::AppPaths::dataDir() != originalDataDir
                    && Zqxxxt::AppPaths::dataDir().startsWith(
                        Zqxxxt::AppPaths::temporaryDir() + QLatin1Char('/'));
                presentation->trigger();
                const bool restoredProject = !presentation->isChecked()
                    && Zqxxxt::AppPaths::dataDir() == originalDataDir;
                const QRect originalGeometry = window.geometry();
                maximize->click();
                QTimer::singleShot(500, &app, [&, maximize, header, controls,
                                                enteredPresentation, restoredProject, originalGeometry] {
                    const bool entered = window.isFullScreen() && header->isVisible()
                        && maximize->isVisible() && maximize->toolTip() == QStringLiteral("还原");
                    maximize->click();
                    QTimer::singleShot(500, &app, [&, entered, header, controls,
                                                    enteredPresentation, restoredProject, originalGeometry] {
                        windowUiCyclePassed = controls && enteredPresentation && restoredProject
                            && entered && !window.isFullScreen() && header->isVisible()
                            && window.geometry() == originalGeometry;
                    });
                });
            });
            if (parser.isSet("smoke-test")) QTimer::singleShot(1500, &app, [&] {
                auto *page = window.findChild<Zqxxxt::ZqbsPage *>();
                auto *title = window.findChild<QLabel *>(QStringLiteral("deploymentSectionTitle"));
                QPushButton *fixed = nullptr;
                QPushButton *dynamic = nullptr;
                if (page) for (auto *button : page->topPanel()->findChildren<QPushButton *>()) {
                    if (!button->property("deploymentDomain").toBool()) continue;
                    if (button->property("domainIndex").toInt() == 0) fixed = button;
                    if (button->property("domainIndex").toInt() == 2) dynamic = button;
                }
                startupBasicScenePassed = fixed && dynamic && fixed->isChecked()
                    && !dynamic->isChecked() && title && title->text() == QStringLiteral("场景列表");
                if (parser.isSet("smoke-screenshot")) startupScreenshotSaved = window.grab().save(
                    parser.value("smoke-screenshot") + QStringLiteral(".startup.png"));
                // Rehearsal smoke checks must explicitly enter the rehearsal module.
                if (dynamic) dynamic->click();
                if (auto *play = window.findChild<QPushButton *>(QStringLiteral("dynamicPlayPause"))) play->click();
            });
            bool fullscreenCyclePassed = false;
            if (parser.isSet("smoke-fullscreen-cycle")) QTimer::singleShot(3000, &app, [&] {
                auto *button = window.findChild<QPushButton *>(QStringLiteral("deploymentFullScreenButton"));
                auto *page = window.findChild<Zqxxxt::ZqbsPage *>();
                auto *scene = window.findChild<Zqxxxt::SceneWidget *>();
                if (!button || !page || !scene) return;
                const QRect originalGeometry = window.geometry();
                const QSize originalSceneSize = scene->size();
                const bool leftVisible = page->leftPanel()->isVisible();
                const bool rightVisible = page->rightPanel()->isVisible();
                button->click();
                QTimer::singleShot(500, &app, [&, button, page, scene,
                                                originalGeometry, originalSceneSize,
                                                leftVisible, rightVisible] {
                    const bool entered = window.isFullScreen()
                        && scene->width() >= window.width() - 2
                        && scene->height() >= window.height() - 2
                        && !page->leftPanel()->isVisible()
                        && !page->rightPanel()->isVisible();
                    button->click();
                    QTimer::singleShot(500, &app, [&, entered, button, page, scene,
                                                    originalGeometry, originalSceneSize,
                                                    leftVisible, rightVisible] {
                        fullscreenCyclePassed = entered && !window.isFullScreen()
                            && window.geometry() == originalGeometry
                            && scene->size() == originalSceneSize
                            && page->leftPanel()->isVisible() == leftVisible
                            && page->rightPanel()->isVisible() == rightVisible
                            && button->text() == QStringLiteral("地图全屏");
                    });
                });
            });
            if (parser.isSet("smoke-test")) QTimer::singleShot(smokeWaitMs, &app, [&] {
                bool ok = window.earthReady() && window.activeDynamicCount() > 0
                    && startupBasicScenePassed && startupScreenshotSaved;
                if (parser.isSet("smoke-fullscreen-cycle")) ok = ok && fullscreenCyclePassed;
                if (parser.isSet("smoke-window-ui-cycle")) ok = ok && windowUiCyclePassed;
                if (parser.isSet("smoke-open-manual")) ok = ok && manualOpenPassed;
                bool screenshot = !parser.isSet("smoke-screenshot") || window.grab().save(parser.value("smoke-screenshot"));
                ok = ok && screenshot;
                if (parser.isSet("smoke-report")) {
                    QFile report(parser.value("smoke-report"));
                    if (report.open(QIODevice::WriteOnly)) report.write(QJsonDocument(QJsonObject{
                        {"uiReady", window.ready()}, {"earthReady", window.earthReady()},
                        {"startupBasicScenePassed", startupBasicScenePassed},
                        {"startupScreenshotSaved", startupScreenshotSaved},
                        {"projectDirectory", QDir(root).absolutePath()},
                        {"earthCacheDirectory", Zqxxxt::AppPaths::earthCacheDir()},
                        {"temporaryDirectory", Zqxxxt::AppPaths::temporaryDir()},
                        {"settingsDirectory", Zqxxxt::AppPaths::configDir()},
                        {"logDirectory", Zqxxxt::AppPaths::logDir()},
                        {"activeDynamicMarkers", window.activeDynamicCount()},
                        {"mapFullscreenCyclePassed", fullscreenCyclePassed},
                        {"windowUiCyclePassed", windowUiCyclePassed},
                        {"manualOpenPassed", manualOpenPassed},
                        {"screenshotSaved", screenshot}, {"passed", ok}}).toJson());
                }
                std::cout << (ok ? "ShDisplayViewer UI and FreeEarth initialized\n" : "FreeEarth initialization failed\n");
                app.exit(ok ? 0 : 2);
            });
            exitCode = app.exec();
        }
    }
#if defined(ZQXXXT_ENABLE_EARTH) && defined(__linux__)
    // OSG keeps plugin ReaderWriters in its process-wide registry. Release them
    // while their shared libraries are still loaded; OSG closes those libraries
    // during registry destruction on Linux.
    osgDB::Registry::instance()->getReaderWriterList().clear();
#endif
    return exitCode;
}
