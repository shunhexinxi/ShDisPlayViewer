#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

namespace {
QString g_applicationDir;
QString g_dataDir;
QString g_cacheDir;
QString g_temporaryDir;
QString g_earthCacheDir;
QString g_configDir;
QString g_logDir;

void setEnvironmentPathIfUnset(const char *name, const QString &path)
{
    if (!qEnvironmentVariableIsSet(name) && QFileInfo::exists(path)) {
        qputenv(name, QDir::toNativeSeparators(path).toLocal8Bit());
    }
}

bool ensureWritableDirectory(const QString &path)
{
    if (!QDir().mkpath(path) || !QFileInfo(path).isDir()) return false;
    QTemporaryFile probe(QDir(path).filePath(QStringLiteral(".shdisplay-write-XXXXXX")));
    return probe.open();
}
}

namespace Zqxxxt::AppPaths {

QString defaultProjectDir(const QString &applicationDirectory)
{
    const QDir directory(applicationDirectory.isEmpty()
        ? QCoreApplication::applicationDirPath() : applicationDirectory);
    if (QFileInfo(directory.filePath(QStringLiteral("shdisplay-project.json"))).isFile())
        return directory.absolutePath();
    return directory.absoluteFilePath(QStringLiteral("Projects/default"));
}

bool initialize(const QString &applicationDirectory, const QString &projectDirectory,
                QString *errorMessage)
{
    if (errorMessage) errorMessage->clear();
    const QString applicationRoot = applicationDirectory.isEmpty()
        ? QCoreApplication::applicationDirPath()
        : QDir(applicationDirectory).absolutePath();
    const QDir directory(applicationRoot);
    const QString cache = directory.filePath(QStringLiteral("cache"));
    const QString earthCache = directory.filePath(QStringLiteral("cache/earth"));
    const QString temporary = directory.filePath(QStringLiteral("cache/temporary"));
    const QString settings = directory.filePath(QStringLiteral("settings"));
    const QString logs = directory.filePath(QStringLiteral("log"));
    for (const QString &path : {earthCache, temporary,
                               directory.filePath(QStringLiteral("cache/fontconfig")), settings, logs}) {
        if (!ensureWritableDirectory(path)) {
            if (errorMessage) *errorMessage = QStringLiteral(
                "无法创建或写入程序数据目录：%1\n请将完整程序目录放到当前用户可写的位置。")
                .arg(QDir::toNativeSeparators(path));
            return false;
        }
    }
    const QString project = projectDirectory.isEmpty()
        ? defaultProjectDir(applicationRoot) : projectDirectory;
    g_applicationDir = applicationRoot;
    g_dataDir = QDir(project).absoluteFilePath(QStringLiteral("Data"));
    g_cacheDir = cache;
    g_temporaryDir = temporary;
    g_earthCacheDir = earthCache;
    g_configDir = settings;
    g_logDir = logs;
    // All generated data stays with this installation; never fall back to a system user directory.
    qputenv("OSGEARTH_CACHE_PATH", QFile::encodeName(QDir::toNativeSeparators(g_earthCacheDir)));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(g_cacheDir));

    setEnvironmentPathIfUnset("GDAL_DATA", QDir(g_applicationDir).filePath(QStringLiteral("gdal-data")));
    setEnvironmentPathIfUnset("PROJ_DATA", QDir(g_applicationDir).filePath(QStringLiteral("proj-data")));
    setEnvironmentPathIfUnset(
        "FONTCONFIG_FILE", QDir(g_applicationDir).filePath(QStringLiteral("fontconfig/fonts.conf")));
    setEnvironmentPathIfUnset("FONTCONFIG_PATH", QDir(g_applicationDir).filePath(QStringLiteral("fontconfig")));
    setEnvironmentPathIfUnset("OPENSSL_MODULES", g_applicationDir);
    if (!qEnvironmentVariableIsSet("OSGEARTH_CACHE_DRIVER")) {
        qputenv("OSGEARTH_CACHE_DRIVER", QByteArrayLiteral("filesystem"));
    }
    if (!qEnvironmentVariableIsSet("OSGEARTH_HTTP_CONNECTTIMEOUT")) {
        qputenv("OSGEARTH_HTTP_CONNECTTIMEOUT", QByteArrayLiteral("5"));
    }
    if (!qEnvironmentVariableIsSet("OSGEARTH_HTTP_TIMEOUT")) {
        qputenv("OSGEARTH_HTTP_TIMEOUT", QByteArrayLiteral("15"));
    }

    const QStringList pluginDirectories = QDir(g_applicationDir).entryList(
        QStringList{QStringLiteral("osgPlugins-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (!pluginDirectories.isEmpty()) {
        setEnvironmentPathIfUnset(
            "OSG_LIBRARY_PATH", QDir(g_applicationDir).filePath(pluginDirectories.constFirst()));
    }
    return true;
}

QString assetPath(const QString &relative) {
    if (relative.startsWith(QStringLiteral(":")) || QFileInfo(relative).isAbsolute()) return relative;
    const auto local = QDir(g_dataDir).filePath(relative);
    if (QFileInfo::exists(local)) return local;
    return QDir(g_applicationDir).filePath(QStringLiteral("Data/") + relative);
}
QString applicationDir() { return g_applicationDir; }
QString dataDir() { return g_dataDir; }
QString cacheDir() { return g_cacheDir; }
QString temporaryDir() { return g_temporaryDir; }
QString earthCacheDir() { return g_earthCacheDir; }
QString configDir() { return g_configDir; }
QString logDir() { return g_logDir; }

}
