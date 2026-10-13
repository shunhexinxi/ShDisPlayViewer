#include "modules/zqbs/MapAnalysis.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double EarthRadiusMeters = 6378137.0;

double radians(double degrees)
{
    return degrees * Pi / 180.0;
}

struct Point3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Point3 localPoint(const Zqxxxt::GeoCoordinate &point,
                  const Zqxxxt::GeoCoordinate &origin)
{
    const double longitudeScale = EarthRadiusMeters * std::cos(radians(origin.latitude));
    return {radians(point.longitude - origin.longitude) * longitudeScale,
            radians(point.latitude - origin.latitude) * EarthRadiusMeters,
            point.altitude - origin.altitude};
}

Point3 cross(const Point3 &left, const Point3 &right)
{
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

double norm(const Point3 &value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

namespace Zqxxxt {

QStringList measurementKinds()
{
    return {
        QStringLiteral("space-distance"),
        QStringLiteral("surface-distance"),
        QStringLiteral("height-distance"),
        QStringLiteral("space-area"),
        QStringLiteral("shadow-area"),
        QStringLiteral("line-visible"),
        QStringLiteral("radial-line-visible"),
        QStringLiteral("profile-analysis"),
        QStringLiteral("gradient-analysis"),
        QStringLiteral("flood-analysis"),
        QStringLiteral("fill-cut-analysis"),
        QStringLiteral("contour-measure")
    };
}

QString measurementDisplayName(const QString &kind)
{
    const QHash<QString, QString> names{
        {QStringLiteral("space-distance"), QStringLiteral("空间距离")},
        {QStringLiteral("surface-distance"), QStringLiteral("地表距离")},
        {QStringLiteral("height-distance"), QStringLiteral("高差分析")},
        {QStringLiteral("space-area"), QStringLiteral("空间面积")},
        {QStringLiteral("shadow-area"), QStringLiteral("投影面积")},
        {QStringLiteral("line-visible"), QStringLiteral("通视分析")},
        {QStringLiteral("radial-line-visible"), QStringLiteral("视域分析")},
        {QStringLiteral("profile-analysis"), QStringLiteral("剖面分析")},
        {QStringLiteral("gradient-analysis"), QStringLiteral("坡度分析")},
        {QStringLiteral("flood-analysis"), QStringLiteral("淹没分析")},
        {QStringLiteral("fill-cut-analysis"), QStringLiteral("填挖方分析")},
        {QStringLiteral("contour-measure"), QStringLiteral("等高线分析")},
        {QStringLiteral("distance"), QStringLiteral("空间距离")},
        {QStringLiteral("area"), QStringLiteral("投影面积")}
    };
    return names.value(kind, kind);
}

bool measurementIsArea(const QString &kind)
{
    return kind == QStringLiteral("space-area")
        || kind == QStringLiteral("shadow-area")
        || kind == QStringLiteral("flood-analysis")
        || kind == QStringLiteral("fill-cut-analysis")
        || kind == QStringLiteral("contour-measure")
        || kind == QStringLiteral("area");
}

bool measurementHasAnalysisStyle(const QString &kind)
{
    return kind == QStringLiteral("height-distance")
        || kind == QStringLiteral("line-visible")
        || kind == QStringLiteral("radial-line-visible")
        || kind == QStringLiteral("fill-cut-analysis")
        || kind == QStringLiteral("contour-measure");
}

int measurementMinimumPoints(const QString &kind)
{
    if (kind == QStringLiteral("space-area")
        || kind == QStringLiteral("shadow-area")
        || kind == QStringLiteral("flood-analysis")
        || kind == QStringLiteral("fill-cut-analysis")
        || kind == QStringLiteral("area"))
        return 3;
    return 2;
}

int measurementMaximumPoints(const QString &kind)
{
    return kind == QStringLiteral("height-distance")
        || kind == QStringLiteral("radial-line-visible")
        || kind == QStringLiteral("gradient-analysis")
        || kind == QStringLiteral("contour-measure") ? 2 : 0;
}

int measurementAutoFinishPoints(const QString &kind)
{
    Q_UNUSED(kind);
    // CDefaultMeasureEventHandler commits a point on click release and ends
    // only on a right-button click release.  Double-click and Enter are not
    // completion gestures in the reference measurement system.
    return 0;
}

bool measurementNeedsTerrain(const QString &kind)
{
    static const QSet<QString> values{
        QStringLiteral("line-visible"), QStringLiteral("radial-line-visible"),
        QStringLiteral("profile-analysis"),
        QStringLiteral("gradient-analysis"), QStringLiteral("flood-analysis"),
        QStringLiteral("fill-cut-analysis"), QStringLiteral("contour-measure")
    };
    return values.contains(kind);
}

double measurementDefaultParameter(const QString &kind)
{
    if (kind == QStringLiteral("profile-analysis"))
        return 100.0;
    if (kind == QStringLiteral("flood-analysis"))
        return 10.0; // used only until the sampled min/max derive (max-min)/20
    if (kind == QStringLiteral("contour-measure"))
        return 100.0;
    return 0.0; // fill/cut: automatic midpoint of sampled min/max
}

double horizontalDistance(const GeoCoordinate &left, const GeoCoordinate &right)
{
    const double lat1 = radians(left.latitude);
    const double lat2 = radians(right.latitude);
    const double deltaLat = lat2 - lat1;
    const double deltaLon = radians(right.longitude - left.longitude);
    const double a = std::sin(deltaLat / 2.0) * std::sin(deltaLat / 2.0)
        + std::cos(lat1) * std::cos(lat2)
        * std::sin(deltaLon / 2.0) * std::sin(deltaLon / 2.0);
    return 2.0 * EarthRadiusMeters * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

double polylineDistance(const QVector<GeoCoordinate> &points, bool includeHeight)
{
    double result = 0.0;
    for (qsizetype index = 1; index < points.size(); ++index) {
        const double horizontal = horizontalDistance(points.at(index - 1), points.at(index));
        const double vertical = includeHeight
            ? points.at(index).altitude - points.at(index - 1).altitude : 0.0;
        result += std::hypot(horizontal, vertical);
    }
    return result;
}

double rhumbPolylineDistance(const QVector<GeoCoordinate> &points)
{
    double result = 0.0;
    for (qsizetype index = 1; index < points.size(); ++index) {
        const double lat1 = radians(points.at(index - 1).latitude);
        const double lat2 = radians(points.at(index).latitude);
        const double deltaLatitude = lat2 - lat1;
        double deltaLongitude = radians(points.at(index).longitude
            - points.at(index - 1).longitude);
        if (std::abs(deltaLongitude) > Pi)
            deltaLongitude += deltaLongitude > 0.0 ? -2.0 * Pi : 2.0 * Pi;
        const double deltaPsi = std::log(std::tan(Pi / 4.0 + lat2 / 2.0)
            / std::tan(Pi / 4.0 + lat1 / 2.0));
        const double q = std::abs(deltaPsi) > 1e-12
            ? deltaLatitude / deltaPsi : std::cos(lat1);
        result += std::hypot(deltaLatitude, q * deltaLongitude)
            * EarthRadiusMeters;
    }
    return result;
}

double projectedPolygonArea(const QVector<GeoCoordinate> &points)
{
    if (points.size() < 3)
        return 0.0;
    const GeoCoordinate origin = points.constFirst();
    double twiceArea = 0.0;
    for (qsizetype index = 0; index < points.size(); ++index) {
        const Point3 a = localPoint(points.at(index), origin);
        const Point3 b = localPoint(points.at((index + 1) % points.size()), origin);
        twiceArea += a.x * b.y - b.x * a.y;
    }
    return std::abs(twiceArea) * 0.5;
}

double spatialPolygonArea(const QVector<GeoCoordinate> &points)
{
    if (points.size() < 3)
        return 0.0;
    const auto ecef = [](const GeoCoordinate &point) {
        const double longitude = radians(point.longitude);
        const double latitude = radians(point.latitude);
        const double radius = EarthRadiusMeters + point.altitude;
        return Point3{radius * std::cos(latitude) * std::cos(longitude),
                      radius * std::cos(latitude) * std::sin(longitude),
                      radius * std::sin(latitude)};
    };
    const Point3 first = ecef(points.constFirst());
    double area = 0.0;
    for (qsizetype index = 1; index + 1 < points.size(); ++index) {
        const Point3 a = ecef(points.at(index));
        const Point3 b = ecef(points.at(index + 1));
        const Point3 left{a.x - first.x, a.y - first.y, a.z - first.z};
        const Point3 right{b.x - first.x, b.y - first.y, b.z - first.z};
        area += norm(cross(left, right)) * 0.5;
    }
    return area;
}

QVector<GeoCoordinate> densifyPolyline(const QVector<GeoCoordinate> &points,
                                       int maximumSamples)
{
    if (points.size() < 2 || maximumSamples <= points.size())
        return points;
    QVector<double> lengths;
    double total = 0.0;
    for (qsizetype i = 1; i < points.size(); ++i) {
        const double value = horizontalDistance(points.at(i - 1), points.at(i));
        lengths.append(value);
        total += value;
    }
    QVector<GeoCoordinate> result;
    result.append(points.constFirst());
    for (qsizetype segment = 0; segment < lengths.size(); ++segment) {
        const int count = std::max(1, int(std::round((maximumSamples - 1)
            * (total > 0.0 ? lengths.at(segment) / total
                           : 1.0 / lengths.size()))));
        const GeoCoordinate &a = points.at(segment);
        const GeoCoordinate &b = points.at(segment + 1);
        for (int step = 1; step <= count && result.size() < maximumSamples; ++step) {
            const double t = double(step) / count;
            result.append({a.longitude + (b.longitude - a.longitude) * t,
                           a.latitude + (b.latitude - a.latitude) * t,
                           a.altitude + (b.altitude - a.altitude) * t});
        }
    }
    if (result.constLast().longitude != points.constLast().longitude
        || result.constLast().latitude != points.constLast().latitude) {
        if (result.size() >= maximumSamples)
            result.last() = points.constLast();
        else
            result.append(points.constLast());
    }
    return result;
}

bool pointInPolygon(const GeoCoordinate &point,
                    const QVector<GeoCoordinate> &polygon)
{
    bool inside = false;
    for (qsizetype i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const GeoCoordinate &a = polygon.at(i);
        const GeoCoordinate &b = polygon.at(j);
        const bool crossing = ((a.latitude > point.latitude) != (b.latitude > point.latitude))
            && (point.longitude < (b.longitude - a.longitude)
                * (point.latitude - a.latitude)
                / ((b.latitude - a.latitude) == 0.0 ? 1e-12 : (b.latitude - a.latitude))
                + a.longitude);
        if (crossing)
            inside = !inside;
    }
    return inside;
}

QString formatMeasurementValue(const QString &kind, double value)
{
    if (measurementIsArea(kind)) {
        return value >= 1000000.0
            ? QStringLiteral("%1 km²").arg(value / 1000000.0, 0, 'f', 2)
            : QStringLiteral("%1 m²").arg(value, 0, 'f', 4);
    }
    return value >= 1000.0
        ? QStringLiteral("%1 km").arg(value / 1000.0, 0, 'f', 2)
        : QStringLiteral("%1 m").arg(value, 0, 'f', 4);
}

QString measurementMapLabel(const QString &kind, double value)
{
    const QHash<QString, QString> prefixes{
        {QStringLiteral("space-distance"), QStringLiteral("直线距离：")},
        {QStringLiteral("surface-distance"), QStringLiteral("地表距离：")},
        {QStringLiteral("height-distance"), QStringLiteral("高差分析：")},
        {QStringLiteral("space-area"), QStringLiteral("空间面积：")},
        {QStringLiteral("shadow-area"), QStringLiteral("投影面积：")}
    };
    return prefixes.value(kind, measurementDisplayName(kind) + QStringLiteral("："))
        + formatMeasurementValue(kind, value);
}

LineOfSightAnalysis analyzeLineOfSightTerrain(
    const QVector<GeoCoordinate> &samples, double observerHeightMeters)
{
    LineOfSightAnalysis result;
    if (samples.size() < 2)
        return result;
    for (const GeoCoordinate &sample : samples) {
        if (!std::isfinite(sample.longitude) || !std::isfinite(sample.latitude)
            || !std::isfinite(sample.altitude)) {
            return result;
        }
    }

    GeoCoordinate observer = samples.constFirst();
    observer.altitude += observerHeightMeters;
    const GeoCoordinate target = samples.constLast();
    int obstruction = -1;
    for (qsizetype index = 1; index + 1 < samples.size(); ++index) {
        const double t = double(index) / double(samples.size() - 1);
        const double lineHeight = observer.altitude
            + (target.altitude - observer.altitude) * t;
        if (samples.at(index).altitude > lineHeight) {
            obstruction = int(index);
            break;
        }
    }

    result.valid = true;
    if (obstruction < 0) {
        result.targetVisible = true;
        result.segments = {observer, target};
        result.segmentStates = {1};
        result.visibleDistance = polylineDistance({observer, target}, true);
        return result;
    }

    const double t = double(obstruction) / double(samples.size() - 1);
    GeoCoordinate split{
        observer.longitude + (target.longitude - observer.longitude) * t,
        observer.latitude + (target.latitude - observer.latitude) * t,
        observer.altitude + (target.altitude - observer.altitude) * t};
    result.segments = {observer, split, split, target};
    result.segmentStates = {1, 0};
    result.visibleDistance = polylineDistance({observer, split}, true);
    result.blockedDistance = polylineDistance({split, target}, true);
    return result;
}

ViewshedAnalysis analyzeViewshedTerrain(const QVector<GeoCoordinate> &samples,
                                        int rayCount, int detectCount)
{
    ViewshedAnalysis result;
    if (rayCount <= 0 || detectCount <= 0
        || samples.size() != rayCount * (detectCount + 1)) {
        return result;
    }
    for (const GeoCoordinate &sample : samples) {
        if (!std::isfinite(sample.longitude) || !std::isfinite(sample.latitude)
            || !std::isfinite(sample.altitude)) {
            return result;
        }
    }

    QVector<QVector<int>> pointStates(rayCount);
    result.raySegmentCounts.fill(detectCount, rayCount);
    result.radialSegments.reserve(rayCount * detectCount * 2);
    result.radialSegmentStates.reserve(rayCount * detectCount);

    for (int ray = 0; ray < rayCount; ++ray) {
        const int offset = ray * (detectCount + 1);
        const GeoCoordinate groundObserver = samples.at(offset);
        GeoCoordinate observer = groundObserver;
        observer.altitude += ViewshedObserverHeightMeters;
        QVector<int> &states = pointStates[ray];
        states.fill(1, detectCount + 1);

        for (int targetIndex = 1; targetIndex <= detectCount; ++targetIndex) {
            GeoCoordinate target = samples.at(offset + targetIndex);
            target.altitude += ViewshedTargetHeightMeters;
            bool visible = true;
            for (int sampleIndex = 1; sampleIndex < targetIndex; ++sampleIndex) {
                const double t = double(sampleIndex) / double(targetIndex);
                const double lineHeight = observer.altitude
                    + (target.altitude - observer.altitude) * t;
                if (samples.at(offset + sampleIndex).altitude > lineHeight) {
                    visible = false;
                    break;
                }
            }
            states[targetIndex] = visible ? 1 : 0;
        }
        if (states.constLast() != 0)
            ++result.visibleRayCount;

        // The old MultiGeometry changes colour between point i and i+1
        // using point i's state, so the transition sample belongs to the
        // preceding (visible) segment.
        for (int index = 1; index <= detectCount; ++index) {
            result.radialSegments += {samples.at(offset + index - 1),
                                      samples.at(offset + index)};
            result.radialSegmentStates.append(states.at(index - 1));
        }
    }

    result.circularSegments.reserve(rayCount * detectCount * 2);
    result.circularSegmentStates.reserve(rayCount * detectCount);
    for (int ring = 1; ring <= detectCount; ++ring) {
        for (int ray = 0; ray < rayCount; ++ray) {
            const int next = (ray + 1) % rayCount;
            result.circularSegments += {
                samples.at(ray * (detectCount + 1) + ring),
                samples.at(next * (detectCount + 1) + ring)};
            result.circularSegmentStates.append(pointStates.at(ray).at(ring));
        }
    }
    result.valid = true;
    return result;
}

int viewshedSweepStartRay(const QVector<GeoCoordinate> &samples,
                          const GeoCoordinate &radiusPoint,
                          int rayCount, int detectCount)
{
    if (rayCount <= 0 || detectCount <= 0
        || samples.size() != rayCount * (detectCount + 1)) {
        return 0;
    }
    int nearest = 0;
    double nearestSquared = std::numeric_limits<double>::max();
    for (int ray = 0; ray < rayCount; ++ray) {
        const GeoCoordinate &edge = samples.at(
            ray * (detectCount + 1) + detectCount);
        const double dx = edge.longitude - radiusPoint.longitude;
        const double dy = edge.latitude - radiusPoint.latitude;
        const double squared = dx * dx + dy * dy;
        if (squared < nearestSquared) {
            nearestSquared = squared;
            nearest = ray;
        }
    }
    return nearest;
}

void ViewshedSweep::reset(int rayCount, int startRay)
{
    m_rayCount = std::max(0, rayCount);
    m_startRay = m_rayCount > 0
        ? ((startRay % m_rayCount) + m_rayCount) % m_rayCount : 0;
    m_revealed = 0;
    m_gridVisible = false;
    m_stopped = m_rayCount == 0;
}

void ViewshedSweep::stop()
{
    m_stopped = true;
}

bool ViewshedSweep::tick()
{
    if (m_stopped)
        return false;
    if (m_revealed < m_rayCount) {
        ++m_revealed;
        return true;
    }
    if (!m_gridVisible) {
        m_gridVisible = true;
        m_stopped = true;
        return true;
    }
    m_stopped = true;
    return false;
}

int ViewshedSweep::revealedRayAt(int order) const noexcept
{
    if (order < 0 || order >= m_revealed || m_rayCount <= 0)
        return -1;
    return (m_startRay + order) % m_rayCount;
}

bool ViewshedSweep::running() const noexcept
{
    return !m_stopped;
}

bool isInternalLabelPlaceholder(const QString &text)
{
    return text.trimmed().compare(QStringLiteral("LabelItem"),
                                  Qt::CaseInsensitive) == 0;
}

} // namespace Zqxxxt
