#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Zqxxxt {

struct FlightWaypointLinkage
{
    QString type;
    QString content;
    QString parameter;
};

struct FlightRouteWaypoint
{
    GeoCoordinate coordinate;
    double speed = 15.0;
    QString annotation;
    FlightWaypointLinkage linkage;
};

struct FlightRouteDefinition
{
    QString id;
    QString name;
    QColor color = QColor(QStringLiteral("#19e7ed"));
    double lineWidth = 2.0;
    QString markerStyle = QStringLiteral("circle");
    bool visible = true;
    QVector<FlightRouteWaypoint> waypoints;
};

struct FlightRouteDocument
{
    QVector<FlightRouteDefinition> routes;
};

inline QStringList flightLinkageTypes()
{
    return {QStringLiteral("自定义视频"),
            QStringLiteral("打开网页"),
            QStringLiteral("打开程序")};
}

inline FlightRouteDocument newFlightRouteDocument()
{
    FlightRouteDocument document;
    const QColor colors[] = {QColor(QStringLiteral("#19e7ed")),
                             QColor(QStringLiteral("#ffe600")),
                             QColor(QStringLiteral("#ef55dc"))};
    for (int index = 0; index < 3; ++index) {
        FlightRouteDefinition route;
        route.id = QString::number(index + 1);
        route.name = QStringLiteral("航线 %1").arg(index + 1);
        route.color = colors[index];
        document.routes.append(route);
    }
    return document;
}

}
