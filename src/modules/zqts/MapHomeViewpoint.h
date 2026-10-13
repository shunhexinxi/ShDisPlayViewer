#pragma once

#include <QDomDocument>
#include <QFile>
#include <cmath>

namespace Zqxxxt {

// Reference: old/bin/Data/Config/systemConfig.xml -> viewPointEnd.
// hei is focal-point altitude, NOT camera range; both must be preserved.
struct MapHomeViewpoint {
    double longitude = 107.205;
    double latitude = 27.645;
    double altitude = 10383909.0;
    double heading = -7.91417;
    double pitch = -89.8888;
    double range = 10383909.0;
    // FreeMainWindow sets the manipulator home duration to 3 seconds,
    // independently of the serialized viewpoint's "time" attribute.
    static constexpr double flightSeconds = 3.0;
};

inline bool readMapHomeViewpoint(QIODevice *device, MapHomeViewpoint &result)
{
    QDomDocument document;
    if (!device || !document.setContent(device))
        return false;
    const QDomElement system = document.documentElement();
    if (system.tagName() != QStringLiteral("system"))
        return false;
    const QDomElement end = system.firstChildElement(QStringLiteral("viewPoints"))
        .firstChildElement(QStringLiteral("viewPointEnd"));
    if (end.isNull())
        return false;
    MapHomeViewpoint candidate;
    const auto number = [&](const QString &name, double &value) {
        bool ok = false;
        value = end.attribute(name).toDouble(&ok);
        return ok && std::isfinite(value);
    };
    if (!number(QStringLiteral("lon"), candidate.longitude)
        || !number(QStringLiteral("lat"), candidate.latitude)
        || !number(QStringLiteral("hei"), candidate.altitude)
        || !number(QStringLiteral("heading"), candidate.heading)
        || !number(QStringLiteral("pitch"), candidate.pitch)
        || !number(QStringLiteral("range"), candidate.range)
        || std::abs(candidate.longitude) > 180.0
        || std::abs(candidate.latitude) > 90.0
        || std::abs(candidate.pitch) > 90.0 || candidate.range <= 0.0)
        return false;
    result = candidate;
    return true;
}

inline MapHomeViewpoint loadMapHomeViewpoint(const QString &path, bool *loaded = nullptr)
{
    MapHomeViewpoint result;
    QFile file(path);
    const bool ok = file.open(QIODevice::ReadOnly) && readMapHomeViewpoint(&file, result);
    if (loaded)
        *loaded = ok;
    return result;
}

}
