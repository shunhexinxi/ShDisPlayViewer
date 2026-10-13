#include "modules/zqbs/MapAnalysis.h"
#include "modules/zqbs/PlotGeometry.h"

#include <QCoreApplication>
#include <QSet>

#include <cmath>
#include <iostream>
#include <limits>

namespace {

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QVector<Zqxxxt::GeoCoordinate> controls{
        {121.4000, 31.1800, 10.0}, {121.4050, 31.1810, 12.0},
        {121.4100, 31.1860, 18.0}, {121.4150, 31.1790, 9.0}
    };
    const QStringList plotKinds{
        QStringLiteral("plot-straight-arrow"),
        QStringLiteral("plot-straight-multi-arrow"),
        QStringLiteral("plot-dovetail-diagonal-arrow"),
        QStringLiteral("plot-dovetail-diagonal-multi-arrow"),
        QStringLiteral("plot-dovetail-straight-arrow"),
        QStringLiteral("plot-dovetail-straight-multi-arrow"),
        QStringLiteral("plot-diagonal-arrow"),
        QStringLiteral("plot-diagonal-multi-arrow"),
        QStringLiteral("plot-double-arrow"),
        QStringLiteral("plot-closed-curve"),
        QStringLiteral("plot-gathering-place"),
        QStringLiteral("plot-rounded-rect"),
        QStringLiteral("plot-bezier-arrow"),
        QStringLiteral("plot-polyline-arrow"),
        QStringLiteral("plot-parallel-search"),
        QStringLiteral("plot-sector-search"),
        QStringLiteral("plot-cardinal-arrow"),
        QStringLiteral("plot-straight-line-arrow")
    };
    const QSet<QString> referenceLinePlots{
        QStringLiteral("plot-bezier-arrow"),
        QStringLiteral("plot-polyline-arrow"),
        QStringLiteral("plot-parallel-search"),
        QStringLiteral("plot-sector-search"),
        QStringLiteral("plot-cardinal-arrow"),
        QStringLiteral("plot-straight-line-arrow")
    };
    QSet<QString> geometryFingerprints;
    for (const QString &kind : plotKinds) {
        if (!require(Zqxxxt::isPlotAnnotation(kind), "plot type not recognized"))
            return 1;
        const auto geometry = Zqxxxt::expandAnnotationGeometry(kind, controls);
        const QString expectedType = referenceLinePlots.contains(kind)
            ? QStringLiteral("line") : QStringLiteral("polygon");
        if (!require(geometry.points.size() >= 2, "plot geometry expansion failed")
            || !require(geometry.geometryType == expectedType,
                        "plot geometry kind does not match reference implementation"))
            return 2;
        QString fingerprint = geometry.geometryType + QLatin1Char('|')
            + QString::number(geometry.points.size());
        const qsizetype sampleCount = std::min<qsizetype>(geometry.points.size(), 8);
        for (qsizetype index = 0; index < sampleCount; ++index) {
            fingerprint += QStringLiteral("|%1,%2")
                .arg(geometry.points.at(index).longitude, 0, 'f', 9)
                .arg(geometry.points.at(index).latitude, 0, 'f', 9);
        }
        if (!require(!geometryFingerprints.contains(fingerprint),
                     "two plot menu types collapsed to the same geometry"))
            return 2;
        geometryFingerprints.insert(fingerprint);
    }

    const QVector<Zqxxxt::GeoCoordinate> shapeControls{
        {121.4000, 31.1800, 10.0}, {121.4060, 31.1830, 10.0},
        {121.3980, 31.1850, 10.0}
    };
    const auto rectangle = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("rectangle"), shapeControls.mid(0, 2));
    const auto circle = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("circle"), shapeControls.mid(0, 2));
    const auto ellipse = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("ellipse"), shapeControls);
    const auto arc = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("arc"), shapeControls);
    const auto sector = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("sector"), shapeControls);
    if (!require(rectangle.geometryType == QStringLiteral("polygon")
                 && rectangle.points.size() == 4,
                 "rectangle reference geometry invalid")
        || !require(circle.geometryType == QStringLiteral("polygon")
                    && circle.points.size() >= 64,
                    "circle collapsed to a rectangle")
        || !require(ellipse.geometryType == QStringLiteral("polygon")
                    && ellipse.points.size() >= 64,
                    "ellipse collapsed to a rectangle")
        || !require(arc.geometryType == QStringLiteral("line")
                    && arc.points.size() >= 12,
                    "arc collapsed to an area")
        || !require(sector.geometryType == QStringLiteral("polygon")
                    && sector.points.size() >= 13,
                    "sector collapsed to a rectangle")
        || !require(Zqxxxt::annotationMinimumPoints(QStringLiteral("line")) == 2,
                    "line mark must accept two control points")
        || !require(Zqxxxt::annotationMinimumPoints(QStringLiteral("model")) == 1,
                    "model mark must accept one position"))
        return 2;
    const QStringList localizedMarks{
        QStringLiteral("point"), QStringLiteral("model"),
        QStringLiteral("billboard"), QStringLiteral("overlay"),
        QStringLiteral("arc"), QStringLiteral("ellipse"),
        QStringLiteral("rectangle"), QStringLiteral("sector"),
        QStringLiteral("circle"), QStringLiteral("text"),
        QStringLiteral("tilt-model"), QStringLiteral("particle"),
        QStringLiteral("label"), QStringLiteral("legacy-point")
    };
    for (const QString &kind : localizedMarks) {
        if (!require(Zqxxxt::annotationAutoFinishPoints(kind) == 1,
                     "localized marker did not finish after one click"))
            return 3;
    }
    if (!require(Zqxxxt::annotationAutoFinishPoints(QStringLiteral("line")) == 0,
                    "line must retain multi-point completion")
        || !require(Zqxxxt::annotationAutoFinishPoints(
                        QStringLiteral("plot-straight-arrow")) == 0,
                    "plot must wait for right-click completion")
        || !require(Zqxxxt::measurementAutoFinishPoints(
                        QStringLiteral("height-distance")) == 0,
                    "measurement must wait for right-click completion"))
        return 3;

    const QStringList expectedMeasurementKinds{
        QStringLiteral("space-distance"), QStringLiteral("surface-distance"),
        QStringLiteral("height-distance"), QStringLiteral("space-area"),
        QStringLiteral("shadow-area"), QStringLiteral("line-visible"),
        QStringLiteral("radial-line-visible"), QStringLiteral("profile-analysis"),
        QStringLiteral("gradient-analysis"), QStringLiteral("flood-analysis"),
        QStringLiteral("fill-cut-analysis"), QStringLiteral("contour-measure")};
    if (!require(Zqxxxt::measurementKinds() == expectedMeasurementKinds,
                 "exposed measurement inventory differs from reference"))
        return 33;
    const QSet<QString> polygonMeasurements{
        QStringLiteral("space-area"), QStringLiteral("shadow-area"),
        QStringLiteral("flood-analysis"), QStringLiteral("fill-cut-analysis")};
    const QSet<QString> twoPointMeasurements{
        QStringLiteral("height-distance"), QStringLiteral("radial-line-visible"),
        QStringLiteral("gradient-analysis"), QStringLiteral("contour-measure")};
    for (const QString &kind : expectedMeasurementKinds) {
        const int expectedMinimum = polygonMeasurements.contains(kind) ? 3 : 2;
        const int expectedMaximum = twoPointMeasurements.contains(kind) ? 2 : 0;
        if (!require(Zqxxxt::measurementMinimumPoints(kind) == expectedMinimum,
                     "measurement minimum point contract mismatch")
            || !require(Zqxxxt::measurementMaximumPoints(kind) == expectedMaximum,
                        "measurement maximum point contract mismatch")
            || !require(Zqxxxt::measurementAutoFinishPoints(kind) == 0,
                        "measurement must end only on right-click release"))
            return 34;
    }
    if (!require(qFuzzyCompare(Zqxxxt::measurementDefaultParameter(
                    QStringLiteral("profile-analysis")), 100.0),
                 "profile reference sample count mismatch")
        || !require(qFuzzyCompare(Zqxxxt::measurementDefaultParameter(
                    QStringLiteral("contour-measure")), 100.0),
                    "contour reference interval mismatch")
        || !require(qFuzzyCompare(Zqxxxt::measurementDefaultParameter(
                    QStringLiteral("fill-cut-analysis")) + 1.0, 1.0),
                    "fill/cut must derive its initial midpoint")
        || !require(Zqxxxt::formatMeasurementValue(
                    QStringLiteral("space-distance"), 999.0)
                    == QStringLiteral("999.0000 m"),
                    "reference metre precision mismatch")
        || !require(Zqxxxt::formatMeasurementValue(
                    QStringLiteral("space-distance"), 1000.0)
                    == QStringLiteral("1.00 km"),
                    "reference kilometre precision mismatch")
        || !require(Zqxxxt::measurementMapLabel(
                    QStringLiteral("height-distance"), -12.5)
                    == QStringLiteral("高差分析：-12.5000 m"),
                    "signed height label mismatch"))
        return 35;

    const QVector<Zqxxxt::GeoCoordinate> center{{121.4, 31.18, 10.0}};
    const auto defaultEllipse = Zqxxxt::referenceMarkerControlPoints(
        QStringLiteral("ellipse"), center, 10000.0);
    const auto defaultArc = Zqxxxt::referenceMarkerControlPoints(
        QStringLiteral("arc"), center, 10000.0);
    const auto defaultSector = Zqxxxt::referenceMarkerControlPoints(
        QStringLiteral("sector"), center, 10000.0);
    if (!require(defaultEllipse.size() == 3, "ellipse defaults missing")
        || !require(defaultArc.size() == 3, "arc defaults missing")
        || !require(defaultSector.size() == 3, "sector defaults missing"))
        return 31;
    const double majorRadius = Zqxxxt::polylineDistance(
        {defaultEllipse.at(0), defaultEllipse.at(1)}, false);
    const double minorRadius = Zqxxxt::polylineDistance(
        {defaultEllipse.at(0), defaultEllipse.at(2)}, false);
    if (!require(majorRadius > minorRadius * 1.9
                 && majorRadius < minorRadius * 2.1,
                 "ellipse default radii collapsed into a circle"))
        return 31;
    const QVector<Zqxxxt::GeoCoordinate> reversedArcControls{
        center.constFirst(), {121.4, 31.19, 10.0}, {121.41, 31.18, 10.0}
    };
    const auto minorSector = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("sector"), reversedArcControls);
    const auto minorArc = Zqxxxt::expandAnnotationGeometry(
        QStringLiteral("arc"), reversedArcControls);
    if (!require(minorSector.points.size() <= 39,
                 "sector selected the major arc over 180 degrees")
        || !require(minorArc.points.size() <= 38,
                    "arc selected the major arc over 180 degrees"))
        return 32;

    const QVector<Zqxxxt::GeoCoordinate> line{
        {121.0, 31.0, 10.0}, {121.001, 31.0, 20.0}
    };
    const double horizontal = Zqxxxt::polylineDistance(line, false);
    const double spatial = Zqxxxt::polylineDistance(line, true);
    if (!require(horizontal > 90.0 && horizontal < 110.0,
                 "horizontal geodesic distance outside expected range")
        || !require(spatial > horizontal, "height not included in spatial distance"))
        return 4;
    if (!require(Zqxxxt::rhumbPolylineDistance(line) > 90.0
                 && Zqxxxt::rhumbPolylineDistance(line) < 110.0,
                 "reference rhumb-line surface distance failed")
        || !require(!Zqxxxt::measurementNeedsTerrain(
                        QStringLiteral("surface-distance")),
                    "surface distance incorrectly depends on DEM readiness"))
        return 41;

    const QVector<Zqxxxt::GeoCoordinate> polygon{
        {121.0, 31.0, 0.0}, {121.001, 31.0, 0.0},
        {121.001, 31.001, 0.0}, {121.0, 31.001, 0.0}
    };
    if (!require(Zqxxxt::projectedPolygonArea(polygon) > 9000.0,
                 "projected area calculation failed")
        || !require(Zqxxxt::pointInPolygon({121.0005, 31.0005, 0.0}, polygon),
                    "point-in-polygon calculation failed")
        || !require(Zqxxxt::densifyPolyline(line, 64).size() > 2,
                    "profile densification failed"))
        return 5;

    struct MeasurementStyleExpectation {
        const char *kind;
        const char *lineColor;
        double lineWidth;
        double lineOpacity;
        double fillOpacity;
        const char *analysisColor;
        double analysisWidth;
        double analysisOpacity;
        bool area;
        bool analysis;
    };
    const MeasurementStyleExpectation measurementStyles[] = {
        {"space-distance", "#ffff00", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, false, false},
        {"surface-distance", "#ffff00", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, false, false},
        {"height-distance", "#ffff00", 2.0, 1.0, 0.5, "#ff0000", 1.2, 1.0, false, true},
        {"space-area", "#80e680", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, true, false},
        {"shadow-area", "#80e680", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, true, false},
        {"line-visible", "#ffff00", 2.0, 1.0, 0.5, "#00ff00", 3.0, 1.0, false, true},
        {"radial-line-visible", "#ffff00", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, false, true},
        {"profile-analysis", "#ffff00", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, false, false},
        {"gradient-analysis", "#ffff00", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, false, false},
        {"flood-analysis", "#e6e600", 2.0, 1.0, 0.5, "#00ff00", 2.0, 1.0, true, false},
        {"fill-cut-analysis", "#e6e600", 2.0, 1.0, 0.5, "#80e680", 2.0, 1.0, true, true},
        {"contour-measure", "#ffff00", 2.0, 0.8, 0.0, "#00ff00", 1.0, 0.8, true, true}
    };
    for (const MeasurementStyleExpectation &expected : measurementStyles) {
        Zqxxxt::DeploymentMeasurement measurement;
        measurement.kind = QString::fromLatin1(expected.kind);
        Zqxxxt::applyLegacyMeasurementStyleDefaults(measurement);
        const bool valid = measurement.lineColor
                == QColor(QString::fromLatin1(expected.lineColor))
            && qFuzzyCompare(measurement.lineWidth, expected.lineWidth)
            && qFuzzyCompare(measurement.lineOpacity, expected.lineOpacity)
            && std::abs(measurement.fillOpacity - expected.fillOpacity) < 0.0001
            && measurement.analysisColor
                == QColor(QString::fromLatin1(expected.analysisColor))
            && qFuzzyCompare(measurement.analysisLineWidth, expected.analysisWidth)
            && qFuzzyCompare(measurement.analysisOpacity, expected.analysisOpacity)
            && Zqxxxt::measurementIsArea(measurement.kind) == expected.area
            && Zqxxxt::measurementHasAnalysisStyle(measurement.kind) == expected.analysis;
        if (!require(valid, expected.kind))
            return 6;
    }

    if (!require(Zqxxxt::formatMeasurementValue(
                    QStringLiteral("shadow-area"), 999.0)
                    == QStringLiteral("999.0000 m²"),
                 "area label must use a UTF-8 superscript unit")
        || !require(Zqxxxt::formatMeasurementValue(
                        QStringLiteral("space-area"), 1000000.0)
                        == QStringLiteral("1.00 km²"),
                    "square-kilometre label precision mismatch")
        || !require(!Zqxxxt::measurementMapLabel(
                        QStringLiteral("space-area"), 50.0)
                        .contains(QStringLiteral("LabelItem")),
                    "measurement label exposed an internal object name")
        || !require(Zqxxxt::isInternalLabelPlaceholder(
                        QStringLiteral(" labelitem ")),
                    "placeholder filtering is not case insensitive")
        || !require(QString::fromUtf8(Zqxxxt::measurementMapLabel(
                        QStringLiteral("surface-distance"), 12.0).toUtf8())
                        == QStringLiteral("地表距离：12.0000 m"),
                    "Chinese measurement text is not valid UTF-8"))
        return 7;

    const QVector<Zqxxxt::GeoCoordinate> obstructedProfile{
        {121.0000, 31.0000, 0.0}, {121.0001, 31.0000, 0.0},
        {121.0002, 31.0000, 30.0}, {121.0003, 31.0000, 0.0},
        {121.0004, 31.0000, 0.0}};
    const auto obstructed = Zqxxxt::analyzeLineOfSightTerrain(obstructedProfile);
    if (!require(obstructed.valid && !obstructed.targetVisible,
                 "terrain obstruction did not block line of sight")
        || !require(obstructed.segments.size() == 4
                    && obstructed.segmentStates == QVector<int>{1, 0},
                    "line of sight did not split into green/red segments")
        || !require(std::abs(obstructed.segments.constFirst().altitude - 2.0)
                        < 0.0001,
                    "observer height offset differs from the reference")
        || !require(obstructed.visibleDistance > 0.0
                    && obstructed.blockedDistance > 0.0,
                    "line-of-sight distance labels lost a segment"))
        return 8;
    QVector<Zqxxxt::GeoCoordinate> clearProfile = obstructedProfile;
    for (Zqxxxt::GeoCoordinate &sample : clearProfile)
        sample.altitude = 0.0;
    const auto clearSight = Zqxxxt::analyzeLineOfSightTerrain(clearProfile);
    if (!require(clearSight.valid && clearSight.targetVisible
                 && clearSight.segmentStates == QVector<int>{1},
                 "clear terrain did not produce one green segment"))
        return 8;
    QVector<Zqxxxt::GeoCoordinate> noDataProfile = clearProfile;
    noDataProfile[2].altitude = std::numeric_limits<double>::quiet_NaN();
    const auto noDataSight = Zqxxxt::analyzeLineOfSightTerrain(noDataProfile);
    if (!require(!noDataSight.valid && noDataSight.segments.isEmpty(),
                 "NoData terrain created fake zero-height LOS geometry"))
        return 8;

    constexpr int TestRays = 4;
    constexpr int TestDetect = 3;
    QVector<Zqxxxt::GeoCoordinate> viewshedSamples;
    for (int ray = 0; ray < TestRays; ++ray) {
        const double angle = 2.0 * 3.14159265358979323846 * ray / TestRays;
        for (int step = 0; step <= TestDetect; ++step) {
            viewshedSamples.append({121.0 + std::cos(angle) * step * 0.001,
                                    31.0 + std::sin(angle) * step * 0.001,
                                    ray == 0 && step == 1 ? 30.0 : 0.0});
        }
    }
    const auto viewshed = Zqxxxt::analyzeViewshedTerrain(
        viewshedSamples, TestRays, TestDetect);
    if (!require(viewshed.valid && viewshed.visibleRayCount == 3,
                 "deterministic viewshed visibility classification failed")
        || !require(viewshed.raySegmentCounts
                        == QVector<int>({TestDetect, TestDetect,
                                         TestDetect, TestDetect}),
                    "viewshed ray grouping differs from the reference")
        || !require(viewshed.radialSegments.size()
                        == TestRays * TestDetect * 2
                    && viewshed.circularSegments.size()
                        == TestRays * TestDetect * 2,
                    "viewshed radial/circular geometry count mismatch")
        || !require(viewshed.radialSegmentStates.contains(0)
                    && viewshed.radialSegmentStates.contains(1)
                    && viewshed.circularSegmentStates.contains(0)
                    && viewshed.circularSegmentStates.contains(1),
                    "viewshed did not preserve red/green segment colors")
        || !require(Zqxxxt::viewshedSweepStartRay(
                        viewshedSamples,
                        viewshedSamples.at(2 * (TestDetect + 1) + TestDetect),
                        TestRays, TestDetect) == 2,
                    "viewshed sweep did not start at the radius endpoint"))
        return 9;
    QVector<Zqxxxt::GeoCoordinate> noDataViewshed = viewshedSamples;
    noDataViewshed[5].altitude = std::numeric_limits<double>::quiet_NaN();
    if (!require(!Zqxxxt::analyzeViewshedTerrain(
                        noDataViewshed, TestRays, TestDetect).valid,
                 "NoData viewshed samples were accepted"))
        return 9;

    Zqxxxt::ViewshedSweep sweep;
    sweep.reset(TestRays, 2);
    if (!require(sweep.running() && sweep.revealedRayCount() == 0
                 && !sweep.circularGridVisible(),
                 "viewshed sweep initial state mismatch"))
        return 10;
    const int expectedOrder[]{2, 3, 0, 1};
    for (int tick = 0; tick < TestRays; ++tick) {
        if (!require(sweep.tick(), "viewshed ray tick stopped early")
            || !require(sweep.revealedRayCount() == tick + 1,
                        "viewshed ray count is not monotonic")
            || !require(sweep.revealedRayAt(tick) == expectedOrder[tick],
                        "viewshed sweep ordering mismatch")
            || !require(!sweep.circularGridVisible(),
                        "circular grid appeared before all spokes"))
            return 10;
    }
    if (!require(sweep.tick() && sweep.circularGridVisible()
                 && !sweep.running(),
                 "viewshed circular completion tick mismatch"))
        return 10;
    sweep.reset(TestRays, 0);
    sweep.stop();
    if (!require(!sweep.tick() && sweep.revealedRayCount() == 0,
                 "cancelled viewshed sweep retained a callback tick"))
        return 10;
    return 0;
}
