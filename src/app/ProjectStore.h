#pragma once
#include <QStringList>
#include "modules/zqbs/DeploymentTypes.h"
namespace Zqxxxt::ProjectStore {
QString manifestName();
QStringList configFiles();
bool isProject(const QString &root, QString &error);
bool create(const QString &root, QString &error, bool demo = true);
bool clone(const QString &source, const QString &destination, QString &error);
bool importLegacy(const QString &configDirectory, const QString &destination, QString &error);
bool importAsset(const QString &root, const QString &source, QString &relative, QString &error);
bool setEarth(const QString &root, const QString &source, QString &error);
QStringList validateResources(const DeploymentData &data);
}
