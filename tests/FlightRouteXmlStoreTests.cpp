#include "modules/zqbs/FlightRouteXmlStore.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

using namespace Zqxxxt;

static bool check(bool value, const char *message)
{
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "temporary directory")) return 1;
    FlightRouteDocument document = newFlightRouteDocument();
    FlightRouteWaypoint point;
    point.coordinate = {120.12345678, 36.87654321, 123.5};
    point.speed = 28.0;
    point.annotation = QStringLiteral("观察点");
    point.linkage = {QStringLiteral("打开网页"),
                     QStringLiteral("https://example.com/a?x=1&y=2"),
                     QStringLiteral("新窗口打开")};
    document.routes[0].name = QStringLiteral("测试航线");
    document.routes[0].lineWidth = 3.5;
    document.routes[0].waypoints = {point, FlightRouteWaypoint{
        {120.22345678, 36.97654321, 125.0}, 30.0, QStringLiteral("终点"), {}}};
    const QString path = temporary.filePath(QStringLiteral("routes.xml"));
    QString error;
    if (!check(FlightRouteXmlStore::save(path, document, error),
               error.toUtf8().constData())) return 1;
    FlightRouteDocument loaded;
    if (!check(FlightRouteXmlStore::load(path, loaded, error),
               error.toUtf8().constData())) return 1;
    if (!check(loaded.routes.size() == 3, "exactly three routes")
        || !check(loaded.routes[0].waypoints.size() == 2, "waypoint count")
        || !check(loaded.routes[0].name == QStringLiteral("测试航线"), "UTF-8 route name")
        || !check(loaded.routes[0].waypoints[0].linkage.type == QStringLiteral("打开网页"),
                  "linkage type")
        || !check(loaded.routes[0].waypoints[0].linkage.content
                      == QStringLiteral("https://example.com/a?x=1&y=2"),
                  "CDATA linkage content")
        || !check(loaded.routes[0].waypoints[0].linkage.parameter
                      == QStringLiteral("新窗口打开"),
                  "linkage parameter")) return 1;

    QFile malformed(temporary.filePath(QStringLiteral("bad.xml")));
    if (!check(malformed.open(QIODevice::WriteOnly), "open malformed XML fixture")) return 1;
    malformed.write("<flightRoutes version=\"1.0\"><route color=\"bad\">");
    malformed.close();
    if (!check(!FlightRouteXmlStore::load(malformed.fileName(), loaded, error),
               "malformed XML rejected")) return 1;
    std::cout << "PASS flight route XML\n";
    return 0;
}
