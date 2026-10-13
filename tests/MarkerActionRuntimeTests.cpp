#include "app/AppPaths.h"
#include "app/MarkerActions.h"
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLabel>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

using namespace Zqxxxt;
static void advance(int milliseconds)
{ QEventLoop loop; QTimer::singleShot(milliseconds, Qt::PreciseTimer, &loop, &QEventLoop::quit); loop.exec(); }

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().contains("--action-child")) {
        QTimer::singleShot(45000, &app, &QApplication::quit);
        return app.exec();
    }
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << message << '\n'; }
    };
    QTemporaryDir root;
    AppPaths::initialize({}, root.path());
    QWidget host;
    QString error;
    // The same executable is a benign long-lived process, with no user data or shell commands.
    QProcess unrelated;
    unrelated.setProgram(QCoreApplication::applicationFilePath());
    unrelated.setArguments({"--action-child"});
    unrelated.start();
    check(unrelated.waitForStarted(2000), "unrelated test helper failed to start");
    QElapsedTimer elapsed;
    elapsed.start();
    check(MarkerActions::execute({{"type", "program"}, {"trigger", "auto"},
        {"target", QCoreApplication::applicationFilePath()}, {"parameter", "--action-child"},
        {"recoverySeconds", 10}}, &host, true, &error), "timed program did not start");
    QPointer<QProcess> managed = host.findChild<QProcess *>("markerActionProgram");
    check(managed && managed->state() != QProcess::NotRunning, "program invocation is not owned");
#if defined(ZQXXXT_ENABLE_VIDEO)
    const QJsonObject video{{"type", "video"}, {"trigger", "auto"},
        {"target", "rtsp://127.0.0.1:1/closed-test-stream"}, {"recoverySeconds", 10}};
    check(MarkerActions::execute(video, &host, true, &error), "automatic RTSP window failed to open");
    QPointer<QDialog> timedVideo = host.findChild<QDialog *>("markerActionVideoDialog");
    check(timedVideo && timedVideo->isVisible()
        && timedVideo->findChild<QLabel *>("markerActionRecoveryStatus"), "video recovery status is missing");
    // Closing early must cancel its timer and never close a subsequent no-recovery window.
    check(MarkerActions::execute(video, &host, true, &error), "second RTSP window failed to open");
    const auto dialogs = host.findChildren<QDialog *>("markerActionVideoDialog");
    check(dialogs.size() == 2, "video actions unexpectedly reused a window");
    if (dialogs.size() == 2) dialogs.last()->close();
    advance(20);
    auto persistent = video;
    persistent.insert("recoverySeconds", 0);
    check(MarkerActions::execute(persistent, &host, true, &error), "no-recovery video failed to open");
#endif
    advance(qMax(0, 9500 - int(elapsed.elapsed())));
    check(managed && managed->state() != QProcess::NotRunning, "program closed before its deadline");
#if defined(ZQXXXT_ENABLE_VIDEO)
    check(timedVideo && timedVideo->isVisible(), "video closed before its deadline");
#endif
    advance(qMax(0, 12300 - int(elapsed.elapsed())));
    check(!managed || managed->state() == QProcess::NotRunning, "program did not close after ten seconds");
    check(unrelated.state() != QProcess::NotRunning, "recovery closed an unrelated process");
#if defined(ZQXXXT_ENABLE_VIDEO)
    check(!timedVideo || !timedVideo->isVisible(), "video did not close after ten seconds");
    const auto remaining = host.findChildren<QDialog *>("markerActionVideoDialog");
    check(remaining.size() == 1 && remaining.first()->isVisible(), "early-close timer affected the no-recovery video");
    for (auto *dialog : remaining) dialog->close();
    advance(20);
#endif
    unrelated.kill();
    unrelated.waitForFinished(2000);
    std::cout << "Timed program/video recovery and invocation isolation: " << failures << " failures\n";
    return failures ? 1 : 0;
}
