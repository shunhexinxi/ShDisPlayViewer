#pragma once
#include <QJsonObject>
#include <QString>
class QWidget;
namespace Zqxxxt::MarkerActions {
QJsonObject normalize(const QJsonObject &action);
QString target(const QJsonObject &action, const QString &platform = {});
bool validate(const QJsonObject &action, QString *error = nullptr);
QString currentPlatform();
// Automatic playback reports failures without blocking the rehearsal clock.
bool execute(const QJsonObject &action, QWidget *parent);
bool execute(const QJsonObject &action, QWidget *parent,
             bool automatic, QString *error = nullptr);
}
