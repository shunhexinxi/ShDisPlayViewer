#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <QStringList>

namespace Zqxxxt {

// Canonical measurement inventory exposed by the reference viewer.  The old
// Buildings Visible action exists in source but is commented out of its menu,
// so it is intentionally not part of this list.
[[nodiscard]] QStringList measurementKinds();
[[nodiscard]] QString measurementDisplayName(const QString &kind);
[[nodiscard]] bool measurementIsArea(const QString &kind);
[[nodiscard]] bool measurementHasAnalysisStyle(const QString &kind);
[[nodiscard]] int measurementMinimumPoints(const QString &kind);
[[nodiscard]] int measurementMaximumPoints(const QString &kind);
[[nodiscard]] int measurementAutoFinishPoints(const QString &kind);
[[nodiscard]] bool measurementNeedsTerrain(const QString &kind);
[[nodiscard]] double measurementDefaultParameter(const QString &kind);
[[nodiscard]] QString measurementMapLabel(const QString &kind, double value);
[[nodiscard]] double horizontalDistance(const GeoCoordinate &left,
                                        const GeoCoordinate &right);
[[nodiscard]] double polylineDistance(const QVector<GeoCoordinate> &points,
                                      bool includeHeight);
[[nodiscard]] double rhumbPolylineDistance(const QVector<GeoCoordinate> &points);
[[nodiscard]] double projectedPolygonArea(const QVector<GeoCoordinate> &points);
[[nodiscard]] double spatialPolygonArea(const QVector<GeoCoordinate> &points);
[[nodiscard]] QVector<GeoCoordinate> densifyPolyline(
    const QVector<GeoCoordinate> &points, int maximumSamples);
[[nodiscard]] bool pointInPolygon(const GeoCoordinate &point,
                                  const QVector<GeoCoordinate> &polygon);
[[nodiscard]] QString formatMeasurementValue(const QString &kind, double value);

// RadialLineOfSightNode reference constants.  The original node advances one
// spoke on each OSG update traversal; there is deliberately no wall-clock
// duration or separate millisecond timer in the legacy implementation.
inline constexpr int ViewshedRayCount = 150;
inline constexpr int ViewshedDetectCount = 50;
inline constexpr double ViewshedObserverHeightMeters = 3.0;
inline constexpr double ViewshedTargetHeightMeters = 10.0;
inline constexpr double LineOfSightObserverHeightMeters = 2.0;

struct LineOfSightAnalysis
{
    QVector<GeoCoordinate> segments;
    QVector<int> segmentStates;
    double visibleDistance = 0.0;
    double blockedDistance = 0.0;
    bool targetVisible = false;
    bool valid = false;
};

// `samples` must be a complete, finite terrain profile ordered from observer
// to target.  Invalid/NoData profiles return valid=false and no geometry.
[[nodiscard]] LineOfSightAnalysis analyzeLineOfSightTerrain(
    const QVector<GeoCoordinate> &samples,
    double observerHeightMeters = LineOfSightObserverHeightMeters);

struct ViewshedAnalysis
{
    // Pairwise line segments, first grouped by ray and then the completed
    // circular grid. One state (1 visible, 0 blocked) belongs to each pair.
    QVector<GeoCoordinate> radialSegments;
    QVector<int> radialSegmentStates;
    QVector<int> raySegmentCounts;
    QVector<GeoCoordinate> circularSegments;
    QVector<int> circularSegmentStates;
    int visibleRayCount = 0;
    bool valid = false;
};

// Builds the old 150 x 50 radial/circular visibility geometry from one
// already-sampled terrain array. It never performs terrain IO itself.
[[nodiscard]] ViewshedAnalysis analyzeViewshedTerrain(
    const QVector<GeoCoordinate> &samples,
    int rayCount = ViewshedRayCount,
    int detectCount = ViewshedDetectCount);

[[nodiscard]] int viewshedSweepStartRay(
    const QVector<GeoCoordinate> &samples, const GeoCoordinate &radiusPoint,
    int rayCount = ViewshedRayCount,
    int detectCount = ViewshedDetectCount);

class ViewshedSweep
{
public:
    void reset(int rayCount, int startRay);
    void stop();
    bool tick();
    [[nodiscard]] int rayCount() const noexcept { return m_rayCount; }
    [[nodiscard]] int revealedRayCount() const noexcept { return m_revealed; }
    [[nodiscard]] int revealedRayAt(int order) const noexcept;
    [[nodiscard]] bool circularGridVisible() const noexcept { return m_gridVisible; }
    [[nodiscard]] bool running() const noexcept;

private:
    int m_rayCount = 0;
    int m_startRay = 0;
    int m_revealed = 0;
    bool m_gridVisible = false;
    bool m_stopped = true;
};

[[nodiscard]] bool isInternalLabelPlaceholder(const QString &text);

} // namespace Zqxxxt
