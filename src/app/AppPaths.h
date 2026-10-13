#pragma once

#include <QString>

namespace Zqxxxt::AppPaths {

bool initialize(const QString &applicationDirectory = {}, const QString &projectDirectory = {},
                QString *errorMessage = nullptr);
// Keep projects and generated data beside the executable, independent of the working directory.
QString defaultProjectDir(const QString &applicationDirectory = {});
QString assetPath(const QString &relative);
QString applicationDir();
QString dataDir();
QString cacheDir();
QString temporaryDir();
QString earthCacheDir();
QString configDir();
QString logDir();

}
