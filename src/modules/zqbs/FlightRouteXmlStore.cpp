#include "modules/zqbs/FlightRouteXmlStore.h"

#include <QDomDocument>
#include <QFile>
#include <QSaveFile>
#include <cmath>

namespace Zqxxxt {
namespace {
bool numberAttribute(const QDomElement &element, const QString &name, double &value)
{
    bool ok = false;
    value = element.attribute(name).toDouble(&ok);
    return ok && std::isfinite(value);
}

QString childText(const QDomElement &parent, const QString &name)
{
    return parent.firstChildElement(name).text();
}
}

bool FlightRouteXmlStore::load(const QString &filePath, FlightRouteDocument &document,
                               QString &errorMessage)
{
    errorMessage.clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = QStringLiteral("无法打开航线 XML：%1").arg(file.errorString());
        return false;
    }
    QDomDocument xml;
    const QDomDocument::ParseResult parsed = xml.setContent(&file);
    if (!parsed) {
        errorMessage = QStringLiteral("航线 XML 第 %1 行第 %2 列格式错误：%3")
            .arg(parsed.errorLine).arg(parsed.errorColumn).arg(parsed.errorMessage);
        return false;
    }
    const QDomElement root = xml.documentElement();
    if (root.tagName() != QStringLiteral("flightRoutes")
        || root.attribute(QStringLiteral("version")) != QStringLiteral("1.0")) {
        errorMessage = QStringLiteral("不是受支持的航线 XML（需要 flightRoutes 1.0）");
        return false;
    }

    FlightRouteDocument candidate;
    for (QDomElement routeElement = root.firstChildElement(QStringLiteral("route"));
         !routeElement.isNull();
         routeElement = routeElement.nextSiblingElement(QStringLiteral("route"))) {
        if (candidate.routes.size() >= 3) {
            errorMessage = QStringLiteral("航线 XML 最多只能包含 3 条航线");
            return false;
        }
        FlightRouteDefinition route;
        route.id = routeElement.attribute(QStringLiteral("id"),
                                           QString::number(candidate.routes.size() + 1));
        route.name = routeElement.attribute(QStringLiteral("name"),
            QStringLiteral("航线 %1").arg(candidate.routes.size() + 1));
        route.color = QColor(routeElement.attribute(QStringLiteral("color"),
                                                      QStringLiteral("#19e7ed")));
        if (!route.color.isValid()) {
            errorMessage = QStringLiteral("航线“%1”的颜色无效").arg(route.name);
            return false;
        }
        bool widthOk = false;
        route.lineWidth = routeElement.attribute(QStringLiteral("lineWidth"),
                                                  QStringLiteral("2")).toDouble(&widthOk);
        if (!widthOk || !std::isfinite(route.lineWidth)
            || route.lineWidth < 0.5 || route.lineWidth > 10.0) {
            errorMessage = QStringLiteral("航线“%1”的线宽必须为 0.5～10").arg(route.name);
            return false;
        }
        route.markerStyle = routeElement.attribute(QStringLiteral("markerStyle"),
                                                    QStringLiteral("circle"));
        route.visible = routeElement.attribute(QStringLiteral("visible"),
                                               QStringLiteral("true")) != QStringLiteral("false");
        for (QDomElement pointElement = routeElement.firstChildElement(QStringLiteral("waypoint"));
             !pointElement.isNull();
             pointElement = pointElement.nextSiblingElement(QStringLiteral("waypoint"))) {
            if (route.waypoints.size() >= 10000) {
                errorMessage = QStringLiteral("航线“%1”的航点超过 10000 个").arg(route.name);
                return false;
            }
            FlightRouteWaypoint waypoint;
            if (!numberAttribute(pointElement, QStringLiteral("longitude"),
                                 waypoint.coordinate.longitude)
                || !numberAttribute(pointElement, QStringLiteral("latitude"),
                                    waypoint.coordinate.latitude)
                || !numberAttribute(pointElement, QStringLiteral("altitude"),
                                    waypoint.coordinate.altitude)
                || std::abs(waypoint.coordinate.longitude) > 180.0
                || std::abs(waypoint.coordinate.latitude) > 90.0) {
                errorMessage = QStringLiteral("航线“%1”的第 %2 个航点坐标无效")
                    .arg(route.name).arg(route.waypoints.size() + 1);
                return false;
            }
            bool speedOk = false;
            waypoint.speed = pointElement.attribute(QStringLiteral("speed"),
                                                     QStringLiteral("15")).toDouble(&speedOk);
            if (!speedOk || !std::isfinite(waypoint.speed)
                || waypoint.speed < 1.0 || waypoint.speed > 999.0) {
                errorMessage = QStringLiteral("航线“%1”的第 %2 个航点速度无效")
                    .arg(route.name).arg(route.waypoints.size() + 1);
                return false;
            }
            waypoint.annotation = pointElement.attribute(QStringLiteral("annotation"));
            const QDomElement linkage = pointElement.firstChildElement(QStringLiteral("linkage"));
            if (!linkage.isNull()) {
                waypoint.linkage.type = linkage.attribute(QStringLiteral("type"));
                if (!waypoint.linkage.type.isEmpty()
                    && !flightLinkageTypes().contains(waypoint.linkage.type)) {
                    errorMessage = QStringLiteral("航线“%1”的第 %2 个航点联动类型无效")
                        .arg(route.name).arg(route.waypoints.size() + 1);
                    return false;
                }
                waypoint.linkage.content = childText(linkage, QStringLiteral("content"));
                waypoint.linkage.parameter = childText(linkage, QStringLiteral("parameter"));
            }
            route.waypoints.append(waypoint);
        }
        candidate.routes.append(route);
    }
    while (candidate.routes.size() < 3) {
        const int index = candidate.routes.size();
        FlightRouteDocument defaults = newFlightRouteDocument();
        candidate.routes.append(defaults.routes.at(index));
    }
    document = candidate;
    return true;
}

bool FlightRouteXmlStore::save(const QString &filePath,
                               const FlightRouteDocument &document,
                               QString &errorMessage)
{
    errorMessage.clear();
    if (document.routes.size() != 3) {
        errorMessage = QStringLiteral("航线文档必须包含 3 个航线位置");
        return false;
    }
    QDomDocument xml;
    QDomProcessingInstruction declaration =
        xml.createProcessingInstruction(QStringLiteral("xml"),
                                        QStringLiteral("version=\"1.0\" encoding=\"UTF-8\""));
    xml.appendChild(declaration);
    QDomElement root = xml.createElement(QStringLiteral("flightRoutes"));
    root.setAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.appendChild(root);
    for (const FlightRouteDefinition &route : document.routes) {
        QDomElement routeElement = xml.createElement(QStringLiteral("route"));
        routeElement.setAttribute(QStringLiteral("id"), route.id);
        routeElement.setAttribute(QStringLiteral("name"), route.name);
        routeElement.setAttribute(QStringLiteral("visible"),
                                  route.visible ? QStringLiteral("true") : QStringLiteral("false"));
        routeElement.setAttribute(QStringLiteral("color"), route.color.name(QColor::HexRgb));
        routeElement.setAttribute(QStringLiteral("lineWidth"),
                                  QString::number(route.lineWidth, 'f', 1));
        routeElement.setAttribute(QStringLiteral("markerStyle"), route.markerStyle);
        for (const FlightRouteWaypoint &waypoint : route.waypoints) {
            QDomElement pointElement = xml.createElement(QStringLiteral("waypoint"));
            pointElement.setAttribute(QStringLiteral("longitude"),
                                      QString::number(waypoint.coordinate.longitude, 'f', 8));
            pointElement.setAttribute(QStringLiteral("latitude"),
                                      QString::number(waypoint.coordinate.latitude, 'f', 8));
            pointElement.setAttribute(QStringLiteral("altitude"),
                                      QString::number(waypoint.coordinate.altitude, 'f', 3));
            pointElement.setAttribute(QStringLiteral("speed"),
                                      QString::number(waypoint.speed, 'f', 1));
            if (!waypoint.annotation.isEmpty())
                pointElement.setAttribute(QStringLiteral("annotation"), waypoint.annotation);
            if (!waypoint.linkage.type.isEmpty()) {
                QDomElement linkage = xml.createElement(QStringLiteral("linkage"));
                linkage.setAttribute(QStringLiteral("type"), waypoint.linkage.type);
                QDomElement content = xml.createElement(QStringLiteral("content"));
                content.appendChild(xml.createCDATASection(waypoint.linkage.content));
                linkage.appendChild(content);
                QDomElement parameter = xml.createElement(QStringLiteral("parameter"));
                parameter.appendChild(xml.createCDATASection(waypoint.linkage.parameter));
                linkage.appendChild(parameter);
                pointElement.appendChild(linkage);
            }
            routeElement.appendChild(pointElement);
        }
        root.appendChild(routeElement);
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMessage = QStringLiteral("无法写入航线 XML：%1").arg(file.errorString());
        return false;
    }
    if (file.write(xml.toByteArray(2)) < 0 || !file.commit()) {
        errorMessage = QStringLiteral("保存航线 XML 失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

}
