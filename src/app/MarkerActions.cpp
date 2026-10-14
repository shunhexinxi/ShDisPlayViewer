#include "MarkerActions.h"
#include "app/AppPaths.h"
#include "ui/SkinnedDialog.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#if defined(ZQXXXT_ENABLE_VIDEO)
#include <ShVideoSDK/VideoPlayer.h>
#endif

namespace Zqxxxt::MarkerActions {
QString currentPlatform()
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows");
#else
    return QStringLiteral("linux");
#endif
}

QJsonObject normalize(const QJsonObject &source)
{
    const auto type = source.value(QStringLiteral("type")).toString();
    if (type != "video" && type != "web" && type != "program") return {};
    QString target = source.value(QStringLiteral("target")).toString().trimmed();
    if (type == "video" && target == "__custom_rtsp__")
        target = source.value(QStringLiteral("parameter")).toString().trimmed();
    const QString trigger = source.value("trigger").toString() == "auto"
        ? QStringLiteral("auto") : QStringLiteral("single");
    const int requestedRecovery = source.value("recoverySeconds").toInt();
    const int recovery = type != "web" && (requestedRecovery == 10
        || requestedRecovery == 30 || requestedRecovery == 60) ? requestedRecovery : 0;
    return {{"type", type}, {"trigger", trigger}, {"recoverySeconds", recovery},
        {"enabled", source.value("enabled").toBool(true)},
        {"triggerName", source.value("triggerName").toString()},
        {"target", target}, {"platformTargets", source.value("platformTargets").toObject()},
        {"parameter", type == "program" ? source.value("parameter").toString() : QString()}};
}

QString target(const QJsonObject &action, const QString &platform)
{
    const QString commonTarget = action.value("target").toString().trimmed();
    if (!commonTarget.isEmpty()) return commonTarget;
    if (action.value("type").toString() == "program") {
        const auto paths = action.value("platformTargets").toObject();
        if (!paths.isEmpty()) return paths.value(platform.isEmpty() ? currentPlatform() : platform).toString().trimmed();
    }
    return action.value("target").toString().trimmed();
}

bool validate(const QJsonObject &action, QString *error)
{
    const auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    const auto normalized = normalize(action);
    if (normalized.isEmpty()) return fail(QStringLiteral("只支持 RTSP 视频、网页和本地程序。"));
    const auto type = normalized.value("type").toString();
    if (type == "program") {
        const auto paths = normalized.value("platformTargets").toObject();
        if (normalized.value("target").toString().isEmpty()
            && paths.value("windows").toString().isEmpty() && paths.value("linux").toString().isEmpty())
            return fail(QStringLiteral("请填写程序路径。"));
        return true;
    }
    const QUrl url(target(normalized), QUrl::StrictMode);
    const auto scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()) return fail(QStringLiteral("请填写完整且有效的网络地址。"));
    if (type == "video" && scheme != "rtsp" && scheme != "rtsps")
        return fail(QStringLiteral("视频仅支持 rtsp:// 或 rtsps:// 地址。"));
    if (type == "web" && scheme != "http" && scheme != "https")
        return fail(QStringLiteral("网页仅支持 http:// 或 https:// 地址。"));
    return true;
}

bool execute(const QJsonObject &source, QWidget *parent)
{
    return execute(source, parent, false, nullptr);
}

bool execute(const QJsonObject &source, QWidget *parent, bool automatic, QString *error)
{
    const auto action = normalize(source);
    QString validationError;
    const auto fail = [&](const QString &message) {
        if (error) *error = message;
        if (!automatic) QMessageBox::warning(parent, QStringLiteral("标记关联动作"), message);
        return false;
    };
    if (error) error->clear();
    if (!validate(action, &validationError)) return fail(validationError);
    if (!action.value("enabled").toBool(true)) return false;
    const auto type = action.value("type").toString();
    const auto destination = target(action);
    const int recovery = action.value("recoverySeconds").toInt();
    if (type == "web") {
        if (!QDesktopServices::openUrl(QUrl(destination))) return fail(QStringLiteral("无法启动系统浏览器。"));
        return true;
    }
    if (type == "program") {
        if (destination.isEmpty()) return fail(QStringLiteral("此动作未配置当前平台的程序路径。"));
        const QFileInfo file(QDir(AppPaths::dataDir()).absoluteFilePath(destination));
        if (!file.isFile() || !file.isExecutable()) return fail(QStringLiteral("当前平台的程序不存在或不可执行。"));
        // Mouse actions confirm here; automatic programs are approved together
        // before the user starts the plan, so playback needs no per-action modal UI.
        if (!automatic && QMessageBox::question(parent, QStringLiteral("打开本地程序"),
            QStringLiteral("将运行：%1\n参数：%2\n请仅运行可信方案中的程序。")
                .arg(file.absoluteFilePath(), action.value("parameter").toString()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return false;
        if (recovery == 0) {
            if (!QProcess::startDetached(file.absoluteFilePath(),
                QProcess::splitCommand(action.value("parameter").toString()), file.absolutePath()))
                return fail(QStringLiteral("程序启动失败。"));
            return true;
        }
        // Own only this invocation. Never search for or kill an existing process
        // by application name; single-instance applications may hand off and exit.
        auto *process = new QProcess(parent);
        process->setObjectName(QStringLiteral("markerActionProgram"));
        process->setProgram(file.absoluteFilePath());
        process->setArguments(QProcess::splitCommand(action.value("parameter").toString()));
        process->setWorkingDirectory(file.absolutePath());
        process->setStandardOutputFile(QProcess::nullDevice());
        process->setStandardErrorFile(QProcess::nullDevice());
        QObject::connect(process, &QProcess::started, process, [process, recovery] {
            QTimer::singleShot(recovery * 1000, Qt::PreciseTimer, process, [process] {
                if (process->state() == QProcess::NotRunning) return;
                process->terminate();
                QTimer::singleShot(1000, process, [process] {
                    if (process->state() != QProcess::NotRunning) process->kill();
                });
            });
        });
        QObject::connect(process, &QProcess::finished, process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError code) {
            if (code == QProcess::FailedToStart) process->deleteLater();
        });
        process->start();
        if (!process->waitForStarted(1000)) return fail(QStringLiteral("程序启动失败。"));
        return true;
    }
#if defined(ZQXXXT_ENABLE_VIDEO)
    auto *dialog = new SkinnedDialog(automatic ? QStringLiteral("RTSP 视频 · 自动触发")
        : QStringLiteral("RTSP 视频 · 演练已暂停，可手动继续"), parent);
    dialog->setObjectName(QStringLiteral("markerActionVideoDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(900, 580);
    auto *player = new VideoSDK::VideoPlayer(dialog);
    auto *status = new QLabel(QStringLiteral("正在连接视频…"), dialog);
    dialog->contentLayout()->addWidget(player, 1);
    dialog->contentLayout()->addWidget(status);
    if (recovery > 0) {
        auto *remaining = new QLabel(QStringLiteral("%1 秒后自动关闭").arg(recovery), dialog);
        remaining->setObjectName(QStringLiteral("markerActionRecoveryStatus"));
        dialog->contentLayout()->addWidget(remaining);
        auto *clock = new QTimer(dialog);
        clock->setInterval(1000);
        QObject::connect(clock, &QTimer::timeout, remaining, [remaining, seconds = recovery]() mutable {
            remaining->setText(QStringLiteral("%1 秒后自动关闭").arg(qMax(0, --seconds)));
        });
        clock->start();
        // Timer ownership cancels it when the user closes this window early.
        QTimer::singleShot(recovery * 1000, Qt::PreciseTimer, dialog, &QDialog::close);
    }
    QObject::connect(dialog, &QDialog::finished, player, &VideoSDK::VideoPlayer::stop);
    QObject::connect(player, &VideoSDK::VideoPlayer::playbackStarted, status, [status](int) {
        status->setText(QStringLiteral("RTSP 已连接"));
    });
    QObject::connect(player, &VideoSDK::VideoPlayer::playbackError, status, [status](int code) {
        status->setText(QStringLiteral("RTSP 连接或解码失败（%1），请检查地址、网络和编码。" ).arg(code));
    });
    VideoSDK::OpenOptions options;
    options.mediaUrl = destination;
    options.transport = QStringLiteral("tcp");
    options.connectTimeoutMs = 8000;
    options.readTimeoutMs = 10000;
    dialog->show();
    if (!player->open(options)) status->setText(QStringLiteral("无法打开 RTSP 流。"));
    return true;
#else
    return fail(QStringLiteral("当前构建未包含视频组件。"));
#endif
}
}
