#include "modules/zqbs/DeploymentSceneOverlay.h"

#include "modules/zqts/SceneWidget.h"
#include "modules/zqbs/MapAnalysis.h"
#include "modules/zqbs/PlotGeometry.h"

#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {

constexpr double EarthRadiusMeters = 6378137.0;

double controlAxisLength(const QWidget *widget)
{
    // CExternNodeEditing scales the reference manipulator to one quarter of
    // the viewport height. Keep a small-window floor, but do not cap it: the
    // old upper cap made the editor visibly too small on a full-screen map.
    return qMax(96.0, widget ? widget->height() / 4.0 : 128.0);
}

QPointF scaledAxisPoint(const QPointF &origin, const QPointF &end, double scale)
{
    return origin + (end - origin) * scale;
}

void drawTranslateDragger(QPainter &painter, const QPointF &origin,
                          double axisLength)
{
    const QPointF xEnd = origin + QPointF(axisLength, 0.0);
    const QPointF yEnd = origin + QPointF(-axisLength * 0.68,
                                          -axisLength * 0.74);
    const QPointF zEnd = origin + QPointF(0.0, -axisLength);

    // FeTranslateDragger uses 0.25-sized XY/XZ/YZ plane handles with the
    // corresponding axis color and 0.6 alpha.
    const auto drawPlane = [&painter, &origin](const QPointF &first,
                                               const QPointF &second,
                                               const QColor &color) {
        QPolygonF plane;
        plane << origin << first << first + second - origin << second;
        QColor fill = color;
        fill.setAlphaF(0.6f);
        painter.setPen(QPen(color, 1.0));
        painter.setBrush(fill);
        painter.drawPolygon(plane);
    };
    drawPlane(scaledAxisPoint(origin, xEnd, 0.25),
              scaledAxisPoint(origin, zEnd, 0.25), QColor(255, 0, 0));
    drawPlane(scaledAxisPoint(origin, xEnd, 0.25),
              scaledAxisPoint(origin, yEnd, 0.25), QColor(0, 255, 0));
    drawPlane(scaledAxisPoint(origin, yEnd, 0.25),
              scaledAxisPoint(origin, zEnd, 0.25), QColor(0, 0, 255));

    const auto drawAxis = [&painter, axisLength](const QPointF &start,
                                                  const QPointF &end,
                                                  const QColor &color) {
        painter.setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(color);
        painter.drawLine(start, end);
        const QLineF line(start, end);
        const double angle = std::atan2(-line.dy(), line.dx());
        const double coneLength = axisLength * 0.20;
        QPolygonF cone;
        cone << end
             << end - QPointF(std::cos(angle + 0.46) * coneLength,
                              -std::sin(angle + 0.46) * coneLength)
             << end - QPointF(std::cos(angle - 0.46) * coneLength,
                              -std::sin(angle - 0.46) * coneLength);
        painter.drawPolygon(cone);
    };
    drawAxis(origin, xEnd, QColor(255, 0, 0));
    drawAxis(origin, yEnd, QColor(0, 255, 0));
    drawAxis(origin, zEnd, QColor(0, 0, 255));
}

double radians(double degrees)
{
    return degrees * 3.14159265358979323846 / 180.0;
}

double distance(const Zqxxxt::GeoCoordinate &left,
                const Zqxxxt::GeoCoordinate &right)
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

double polygonArea(const QVector<Zqxxxt::GeoCoordinate> &points)
{
    if (points.size() < 3)
        return 0.0;
    const double referenceLatitude = radians(points.constFirst().latitude);
    double twiceArea = 0.0;
    for (qsizetype index = 0; index < points.size(); ++index) {
        const Zqxxxt::GeoCoordinate &a = points.at(index);
        const Zqxxxt::GeoCoordinate &b = points.at((index + 1) % points.size());
        const double ax = EarthRadiusMeters * radians(a.longitude)
            * std::cos(referenceLatitude);
        const double ay = EarthRadiusMeters * radians(a.latitude);
        const double bx = EarthRadiusMeters * radians(b.longitude)
            * std::cos(referenceLatitude);
        const double by = EarthRadiusMeters * radians(b.latitude);
        twiceArea += ax * by - bx * ay;
    }
    return std::abs(twiceArea) * 0.5;
}

QString formatValue(const QString &kind, double value)
{
    return Zqxxxt::formatMeasurementValue(kind, value);
}

double distanceToSegment(const QPointF &point, const QPointF &start,
                         const QPointF &end)
{
    const QPointF delta = end - start;
    const double lengthSquared = delta.x() * delta.x() + delta.y() * delta.y();
    if (lengthSquared <= 0.0001)
        return QLineF(point, start).length();
    const QPointF relative = point - start;
    const double projection = qBound(0.0,
        (relative.x() * delta.x() + relative.y() * delta.y()) / lengthSquared,
        1.0);
    return QLineF(point, start + delta * projection).length();
}

} // namespace

namespace Zqxxxt {

DeploymentSceneOverlay::DeploymentSceneOverlay(SceneWidget *scene)
    : QWidget(scene), m_scene(scene)
{
    setObjectName(QStringLiteral("deploymentSceneOverlay"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setFocusPolicy(Qt::StrongFocus);
}

void DeploymentSceneOverlay::beginMarkerPlacement()
{
    beginAnnotationPlacement(QStringLiteral("legacy-point"));
}

void DeploymentSceneOverlay::beginAnnotationPlacement(const QString &kind)
{
    m_interaction = Interaction::Annotation;
    m_annotationKind = kind.isEmpty() ? QStringLiteral("point") : kind;
    m_draftPoints.clear();
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setCursor(Qt::CrossCursor);
    setFocus();
    publishInstruction();
    update();
}

void DeploymentSceneOverlay::beginMeasurement(const QString &kind)
{
    m_interaction = Interaction::Measurement;
    m_measurementKind = kind.isEmpty() ? QStringLiteral("space-distance") : kind;
    m_draftPoints.clear();
    m_measurementStarted = false;
    m_suppressMeasurementRelease = false;
    m_measurementPressButton = Qt::NoButton;
    setMouseTracking(true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setCursor(Qt::CrossCursor);
    setFocus();
    publishInstruction();
    update();
}

void DeploymentSceneOverlay::beginBlackboard(const QString &tool,
                                              const QColor &color)
{
    m_blackboardVisible = true;
    m_blackboardTool = tool.isEmpty() ? QStringLiteral("freehand") : tool;
    if (color.isValid())
        m_blackboardColor = color;
    m_interaction = Interaction::Blackboard;
    m_draftPoints.clear();
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setCursor(Qt::CrossCursor);
    setFocus();
    publishInstruction();
    update();
}

void DeploymentSceneOverlay::setBlackboardTool(const QString &tool)
{
    if (!tool.isEmpty())
        m_blackboardTool = tool;
    if (m_blackboardVisible) {
        m_interaction = Interaction::Blackboard;
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setCursor(Qt::CrossCursor);
        setFocus();
        publishInstruction();
    }
}

void DeploymentSceneOverlay::setBlackboardColor(const QColor &color)
{
    if (color.isValid())
        m_blackboardColor = color;
}

void DeploymentSceneOverlay::clearBlackboard()
{
    m_blackboardShapes.clear();
    m_blackboardDraft = {};
    m_blackboardDrawing = false;
    update();
}

void DeploymentSceneOverlay::closeBlackboard()
{
    m_blackboardVisible = false;
    m_blackboardDrawing = false;
    m_blackboardDraft = {};
    cancelInteraction();
}

void DeploymentSceneOverlay::cancelInteraction()
{
    m_interaction = Interaction::Navigate;
    m_draftPoints.clear();
    m_measurementStarted = false;
    m_suppressMeasurementRelease = false;
    m_measurementPressButton = Qt::NoButton;
    setMouseTracking(false);
    unsetCursor();
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    publishInstruction();
    update();
}

void DeploymentSceneOverlay::refresh()
{
    update();
}

bool DeploymentSceneOverlay::measurementInteractionActive() const noexcept
{
    return m_interaction == Interaction::Measurement;
}

bool DeploymentSceneOverlay::navigationInteractionActive() const noexcept
{
    return m_interaction == Interaction::Navigate;
}

QVector<GeoCoordinate> DeploymentSceneOverlay::measurementDraftPoints() const
{
    return m_interaction == Interaction::Measurement
        ? m_draftPoints : QVector<GeoCoordinate>{};
}

QVector<QPair<GeoCoordinate, GeoCoordinate>>
DeploymentSceneOverlay::measurementPreviewSegments() const
{
    QVector<QPair<GeoCoordinate, GeoCoordinate>> result;
    if (m_interaction != Interaction::Measurement || m_draftPoints.size() < 2)
        return result;
    if (m_measurementKind == QStringLiteral("line-visible")
        || m_measurementKind == QStringLiteral("radial-line-visible")) {
        result.reserve(m_draftPoints.size() - 1);
        for (qsizetype index = 1; index < m_draftPoints.size(); ++index)
            result.append({m_draftPoints.constFirst(), m_draftPoints.at(index)});
        return result;
    }
    result.reserve(m_draftPoints.size() - 1);
    for (qsizetype index = 1; index < m_draftPoints.size(); ++index)
        result.append({m_draftPoints.at(index - 1), m_draftPoints.at(index)});
    return result;
}

void DeploymentSceneOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // Never paint screen-space weather over the native three-dimensional scene.
    if (!m_scene->earthReady()) drawEnvironment(painter);
    drawMeasurements(painter);
    drawMarkers(painter);
    if (!m_scene->m_hoveredMarkerId.isEmpty()
        && m_scene->m_hoveredMarkerBounds.isValid()) {
        painter.save();
        painter.setPen(QPen(QColor(QStringLiteral("#65e5ff")), 2));
        painter.setBrush(QColor(75, 205, 255, 35));
        painter.drawRoundedRect(m_scene->m_hoveredMarkerBounds, 3, 3);
        painter.restore();
    }
    drawBlackboard(painter);
    drawDraft(painter);
    drawControlPointDraggers(painter);
}

void DeploymentSceneOverlay::drawControlPointDraggers(QPainter &painter)
{
    if (!m_scene->m_markerPointEditing)
        return;
    const QVector<GeoCoordinate> controls = m_scene->m_markerEditPreview.points.isEmpty()
        ? QVector<GeoCoordinate>{m_scene->m_markerEditPreview.position}
        : m_scene->m_markerEditPreview.points;
    if (controls.isEmpty())
        return;
    if (m_activeMarkerId != m_scene->m_markerEditPreview.id) {
        m_activeMarkerId = m_scene->m_markerEditPreview.id;
        m_activeControlPointIndex = int(controls.size() - 1);
    }
    m_activeControlPointIndex = qBound(0, m_activeControlPointIndex,
                                      int(controls.size() - 1));
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    for (qsizetype index = 0; index < controls.size(); ++index) {
        QPointF origin;
        if (!m_scene->screenPosition(controls.at(index), origin))
            continue;
        const bool active = int(index) == m_activeControlPointIndex;
        // Exact reference edit-point colors: normal (0.6,1.0,0.5,1),
        // active (1.0,0.2,0.1,1). The old sphere is auto-scaled to 6 px.
        painter.setPen(Qt::NoPen);
        painter.setBrush(active ? QColor(255, 51, 26)
                                : QColor(153, 255, 128));
        painter.drawEllipse(origin, 6.0, 6.0);
        if (active)
            drawTranslateDragger(painter, origin, controlAxisLength(this));
    }
    painter.restore();
}

bool DeploymentSceneOverlay::beginControlPointDrag(const QPointF &position)
{
    if (!m_scene->m_markerPointEditing)
        return false;
    const QVector<GeoCoordinate> controls = m_scene->m_markerEditPreview.points.isEmpty()
        ? QVector<GeoCoordinate>{m_scene->m_markerEditPreview.position}
        : m_scene->m_markerEditPreview.points;
    if (controls.isEmpty())
        return false;
    if (m_activeMarkerId != m_scene->m_markerEditPreview.id) {
        m_activeMarkerId = m_scene->m_markerEditPreview.id;
        m_activeControlPointIndex = int(controls.size() - 1);
    }
    m_activeControlPointIndex = qBound(0, m_activeControlPointIndex,
                                      int(controls.size() - 1));
    const double axisLength = controlAxisLength(this);
    const double hitTolerance = qBound(12.0, axisLength * 0.08, 18.0);
    // As in the reference editor, only the active edit point owns a three-axis
    // manipulator. Check those axes before the point spheres.
    {
        QPointF origin;
        if (!m_scene->screenPosition(controls.at(m_activeControlPointIndex), origin))
            return false;
        DraggerAxis axis = DraggerAxis::None;
        if (distanceToSegment(position, origin,
                              origin + QPointF(axisLength, 0.0)) <= hitTolerance)
            axis = DraggerAxis::X;
        else if (distanceToSegment(position, origin,
                                   origin + QPointF(-axisLength * 0.68,
                                                    -axisLength * 0.74)) <= hitTolerance)
            axis = DraggerAxis::Y;
        else if (distanceToSegment(position, origin,
                                   origin + QPointF(0.0, -axisLength)) <= hitTolerance)
            axis = DraggerAxis::Z;
        if (axis != DraggerAxis::None) {
            m_dragAxis = axis;
            m_dragPointIndex = m_activeControlPointIndex;
            m_dragPressPosition = position;
            m_dragOriginScreen = origin;
            m_dragOriginCoordinate = controls.at(m_activeControlPointIndex);
            setCursor(axis == DraggerAxis::Z ? Qt::SizeVerCursor
                                             : Qt::SizeAllCursor);
            return true;
        }
    }
    // Clicking any other sphere activates it and immediately permits free
    // dragging, matching CExFeatureNode::ActiveEditPoint behaviour.
    for (qsizetype index = controls.size(); index > 0; --index) {
        QPointF origin;
        if (!m_scene->screenPosition(controls.at(index - 1), origin)
            || QLineF(position, origin).length() > 13.0) {
            continue;
        }
        m_activeControlPointIndex = int(index - 1);
        m_dragAxis = DraggerAxis::Free;
        m_dragPointIndex = m_activeControlPointIndex;
        m_dragPressPosition = position;
        m_dragOriginScreen = origin;
        m_dragOriginCoordinate = controls.at(index - 1);
        setCursor(Qt::SizeAllCursor);
        update();
        return true;
    }
    return false;
}

bool DeploymentSceneOverlay::updateControlPointDrag(const QPointF &position)
{
    if (m_dragAxis == DraggerAxis::None || m_dragPointIndex < 0
        || !m_scene->m_markerPointEditing) {
        return false;
    }
    GeoCoordinate changed = m_dragOriginCoordinate;
    GeoCoordinate geographic;
    if (m_dragAxis == DraggerAxis::Free) {
        if (m_scene->coordinateAtScreen(position, geographic)) {
            changed.longitude = geographic.longitude;
            changed.latitude = geographic.latitude;
        }
    } else if (m_dragAxis == DraggerAxis::X) {
        if (m_scene->coordinateAtScreen(
                QPointF(position.x(), m_dragOriginScreen.y()), geographic)) {
            changed.longitude = geographic.longitude;
        }
    } else if (m_dragAxis == DraggerAxis::Y) {
        if (m_scene->coordinateAtScreen(
                QPointF(m_dragOriginScreen.x(), position.y()), geographic)) {
            changed.latitude = geographic.latitude;
        }
    } else if (m_dragAxis == DraggerAxis::Z) {
        const SceneViewpoint view = m_scene->currentViewpoint();
        const double metersPerPixel = qMax(0.05,
            view.range / qMax(1, height()) * 0.08);
        changed.altitude = m_dragOriginCoordinate.altitude
            + (m_dragPressPosition.y() - position.y()) * metersPerPixel;
    }

    QVector<GeoCoordinate> &points = m_scene->m_markerEditPreview.points;
    if (points.isEmpty())
        points.append(m_scene->m_markerEditPreview.position);
    if (m_dragPointIndex >= points.size())
        return false;
    points[m_dragPointIndex] = changed;
    m_scene->m_markerEditPreview.position = points.constFirst();
    m_scene->synchronizeEarthAnnotations();
    if (m_scene->m_markerEditPointsChanged)
        m_scene->m_markerEditPointsChanged(points);
    update();
    return true;
}

bool DeploymentSceneOverlay::endControlPointDrag()
{
    if (m_dragAxis == DraggerAxis::None)
        return false;
    m_dragAxis = DraggerAxis::None;
    m_dragPointIndex = -1;
    unsetCursor();
    update();
    return true;
}

void DeploymentSceneOverlay::drawEnvironment(QPainter &painter)
{
    const EnvironmentSettings &environment = m_scene->m_environment;
    const int hour = qBound(0, environment.timeOfDay, 23);
    const int nightDarkness = (hour < 6 || hour >= 20) ? 95
        : ((hour < 8 || hour >= 18) ? 45 : 0);
    const int darkness = qBound(0, (100 - environment.brightness) * 2 + nightDarkness, 190);
    if (environment.atmosphereVisible && hour >= 6 && hour < 19) {
        QColor atmosphere = environment.atmosphereColor.isValid()
            ? environment.atmosphereColor : QColor(QStringLiteral("#3c91d2"));
        atmosphere.setAlphaF(qBound(0.0, environment.atmosphereOpacity, 1.0)
                             * qBound(0.0, environment.ambientIntensity, 100.0) / 100.0);
        painter.fillRect(rect(), atmosphere);
    }
    if (darkness > 0)
        painter.fillRect(rect(), QColor(0, 5, 24, darkness));
    if (environment.fogVisible && environment.fog > 0) {
        QColor fog = environment.fogColor.isValid()
            ? environment.fogColor : QColor(QStringLiteral("#a8b3cf"));
        fog.setAlphaF(qBound(0.0, environment.fogOpacity, 1.0)
                      * qBound(0, environment.fog, 100) / 100.0);
        painter.fillRect(rect(), fog);
    }

    if (environment.starsVisible && (hour < 6 || hour >= 19)) {
        painter.setPen(Qt::NoPen);
        QColor star = environment.starColor.isValid()
            ? environment.starColor : QColor(Qt::white);
        star.setAlphaF(qBound(0.0, environment.starOpacity, 1.0));
        painter.setBrush(star);
        const int count = environment.nebulaVisible ? 130 : 75;
        const double baseSize = qBound(0.5, environment.starSize, 8.0);
        for (int index = 0; index < count; ++index) {
            const int x = (index * 97 + 23) % qMax(1, width());
            const int y = (index * 53 + 19) % qMax(1, qMax(1, height() * 2 / 3));
            const double size = baseSize * (index % 9 == 0 ? 1.8 : 1.0);
            painter.drawEllipse(QPointF(x, y), size, size);
        }
    }
    if (environment.sunVisible && hour >= 6 && hour < 19) {
        const double phase = (hour + environment.minuteOfHour / 60.0 - 6.0) / 13.0;
        const QPointF sun(40.0 + phase * qMax(1, width() - 80),
            45.0 + std::abs(phase - 0.5) * 90.0);
        painter.setPen(Qt::NoPen);
        QColor color = environment.sunColor.isValid()
            ? environment.sunColor : QColor(QStringLiteral("#ffe073"));
        color.setAlphaF(qBound(0.0, environment.sunOpacity, 1.0));
        painter.setBrush(color);
        const double size = qBound(2.0, environment.sunSize, 80.0);
        painter.drawEllipse(sun, size, size);
    } else if (environment.moonVisible) {
        painter.setPen(Qt::NoPen);
        QColor color = environment.moonColor.isValid()
            ? environment.moonColor : QColor(QStringLiteral("#dcebff"));
        color.setAlphaF(qBound(0.0, environment.moonOpacity, 1.0));
        painter.setBrush(color);
        const double size = qBound(2.0, environment.moonSize, 80.0);
        painter.drawEllipse(QPointF(width() * 0.78, 70), size, size);
    }

    if (environment.cloudVisible && environment.cloudDensity > 0) {
        painter.setPen(Qt::NoPen);
        QColor cloud = environment.cloudColor.isValid()
            ? environment.cloudColor : QColor(QStringLiteral("#e1ebf2"));
        cloud.setAlphaF(qBound(0, environment.cloudOpacity, 100) / 100.0);
        painter.setBrush(cloud);
        const int count = qMin(40, qMax(1, environment.cloudDensity / 3));
        for (int index = 0; index < count; ++index) {
            const int x = (index * 137 + environment.cloudDirectionDegrees * 3) % qMax(1, width());
            const int y = 25 + (index * 61) % qMax(1, height() / 2);
            painter.drawEllipse(QRectF(x - 36, y - 10, 85, 25));
        }
    }

    const auto setWeatherClip = [this, &painter](bool global, double longitude,
                                                  double latitude, double radius) {
        if (global)
            return;
        QPointF center;
        GeoCoordinate value{longitude, latitude, 0.0};
        if (!m_scene->screenPosition(value, center)) {
            painter.setClipRect(QRect());
            return;
        }
        const SceneViewpoint view = m_scene->currentViewpoint();
        const double pixels = qBound(25.0, radius / qMax(1.0, view.range)
            * qMax(width(), height()) * 1.5, double(qMax(width(), height())));
        QPainterPath clip;
        clip.addEllipse(center, pixels, pixels);
        painter.setClipPath(clip, Qt::IntersectClip);
    };

    painter.save();
    setWeatherClip(environment.rainGlobal, environment.rainLongitude,
                   environment.rainLatitude, environment.rainRadiusMeters);
    QColor rain = environment.rainColor.isValid()
        ? environment.rainColor : QColor(QStringLiteral("#8c8c8c"));
    rain.setAlphaF(qBound(0.0, environment.rainOpacity, 1.0));
    painter.setPen(QPen(rain, qBound(0.5, environment.rainLineWidth, 10.0)));
    if (environment.rainVisible && environment.rain > 0) {
        const int count = qMin(240, environment.rain * 3);
        for (int index = 0; index < count; ++index) {
            const int x = (index * 71 + 17) % qMax(1, width());
            const int y = (index * 43 + 29) % qMax(1, height());
            painter.drawLine(x, y, x - 4, y + 10);
        }
    }
    painter.restore();
    painter.save();
    setWeatherClip(environment.snowGlobal, environment.snowLongitude,
                   environment.snowLatitude, environment.snowRadiusMeters);
    if (environment.snowVisible && environment.snow > 0) {
        QColor snow = environment.snowColor.isValid()
            ? environment.snowColor : QColor(QStringLiteral("#cccccc"));
        snow.setAlphaF(qBound(0.0, environment.snowOpacity, 1.0));
        painter.setBrush(snow);
        painter.setPen(Qt::NoPen);
        const int count = qMin(180, environment.snow * 2);
        const double particleSize = qBound(0.5, environment.snowParticleSize, 10.0);
        for (int index = 0; index < count; ++index) {
            const int x = (index * 83 + 11) % qMax(1, width());
            const int y = (index * 47 + 37) % qMax(1, height());
            painter.drawEllipse(QPointF(x, y), particleSize, particleSize);
        }
    }
    painter.restore();
}

void DeploymentSceneOverlay::drawMeasurements(QPainter &painter)
{
    if (m_scene->earthReady())
        return;
    for (const DeploymentMeasurement &measurement : m_scene->m_deploymentMeasurements) {
        if (!measurement.visible || measurement.points.size() < 2)
            continue;
        QPolygonF polygon;
        for (const GeoCoordinate &point : measurement.points) {
            QPointF screen;
            if (m_scene->screenPosition(point, screen))
                polygon.append(screen);
        }
        if (polygon.size() < 2)
            continue;
        QColor lineColor = measurement.lineColor.isValid()
            ? measurement.lineColor : QColor(QStringLiteral("#ffff00"));
        lineColor.setAlphaF(qBound(0.0, measurement.lineOpacity, 1.0));
        painter.setPen(QPen(lineColor, measurement.lineWidth));
        QColor fillColor = measurement.fillColor.isValid()
            ? measurement.fillColor : QColor(QStringLiteral("#e6e600"));
        fillColor.setAlphaF(qBound(0.0, measurement.fillOpacity, 1.0));
        if (measurementIsArea(measurement.kind))
            painter.setBrush(fillColor);
        else
            painter.setBrush(Qt::NoBrush);
        if (measurementIsArea(measurement.kind) && polygon.size() >= 3)
            painter.drawPolygon(polygon);
        else
            painter.drawPolyline(polygon);
        QColor analysisColor = measurement.analysisColor.isValid()
            ? measurement.analysisColor : QColor(QStringLiteral("#00ff00"));
        analysisColor.setAlphaF(qBound(0.0, measurement.analysisOpacity, 1.0));
        painter.setPen(QPen(analysisColor, measurement.analysisLineWidth));
        for (qsizetype index = 1; index < measurement.analysisSegments.size(); index += 2) {
            const int stateIndex = static_cast<int>(index / 2);
            QColor stateColor = stateIndex < measurement.analysisSegmentStates.size()
                    && measurement.analysisSegmentStates.at(stateIndex) == 0
                ? QColor(QStringLiteral("#ff0000")) : analysisColor;
            stateColor.setAlphaF(qBound(0.0, measurement.analysisOpacity, 1.0));
            painter.setPen(QPen(stateColor, measurement.analysisLineWidth));
            QPointF a;
            QPointF b;
            if (m_scene->screenPosition(measurement.analysisSegments.at(index - 1), a)
                && m_scene->screenPosition(measurement.analysisSegments.at(index), b))
                painter.drawLine(a, b);
        }
        painter.setBrush(QColor(6, 35, 62, 215));
        painter.setPen(QPen(QColor(QStringLiteral("#62f2ff")), 1.5));
        const QString valueText = measurement.resultText.isEmpty()
            ? formatValue(measurement.kind, measurement.value)
            : measurement.resultText.section(QLatin1Char('\n'), 0, 0);
        const QString label = measurement.name + QStringLiteral("  ") + valueText;
        const QRectF labelRect(polygon.constLast() + QPointF(8, -27), QSizeF(260, 24));
        painter.drawRoundedRect(labelRect, 3, 3);
        painter.drawText(labelRect.adjusted(7, 0, -4, 0), Qt::AlignVCenter, label);
    }
}

void DeploymentSceneOverlay::drawMarkers(QPainter &painter)
{
    const bool earth = m_scene->earthReady();
    for (const DeploymentMarker &marker : m_scene->m_deploymentMarkers) {
        if (!marker.visible || (marker.associationHideFlash && !marker.associationFlashOn))
            continue;
        if (earth) {
            if (marker.associationAlert && !marker.associationFlashOn) {
                QPointF center;
                if (m_scene->screenPosition(marker.position, center)) {
                    painter.setPen(QPen(QColor(QStringLiteral("#ff5151")), 3));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(center, 15, 15);
                }
            }
            continue;
        }
        const QVector<GeoCoordinate> controls = marker.points.isEmpty()
            ? QVector<GeoCoordinate>{marker.position} : marker.points;
        const ExpandedAnnotationGeometry geometry = expandAnnotationGeometry(
            marker.geometryType, controls);
        QColor color = marker.color.isValid() ? marker.color
                                               : QColor(QStringLiteral("#37d6ff"));
        if (marker.live && (marker.liveState == QStringLiteral("offline")
                            || marker.liveState == QStringLiteral("disabled"))) {
            color = QColor(QStringLiteral("#8d9baa"));
        }
        if (marker.bindingType == QStringLiteral("alarm") || marker.associationAlert)
            color = QColor(QStringLiteral("#ff4e42"));
        if (marker.associationAlert && !marker.associationFlashOn)
            color = QColor(QStringLiteral("#ffd15d"));
        if (marker.associationColor.isValid()) color = marker.associationColor;

        QPolygonF screenGeometry;
        for (const GeoCoordinate &coordinate : geometry.points) {
            QPointF value;
            if (m_scene->screenPosition(coordinate, value))
                screenGeometry.append(value);
        }
        if (screenGeometry.isEmpty())
            continue;
        const QPointF point = screenGeometry.constFirst();
        QColor lineColor = color;
        lineColor.setAlphaF(qBound(0.0, marker.lineOpacity, 1.0));
        const QColor markerFill = marker.associationColor.isValid()
            ? marker.associationColor : marker.fillColor.isValid() ? marker.fillColor : color;
        painter.setPen(QPen(lineColor, marker.lineWidth));
        painter.setBrush(annotationIsArea(marker.geometryType)
            ? QColor(markerFill.red(), markerFill.green(), markerFill.blue(),
                     qBound(0, qRound(marker.fillOpacity * 255.0), 255))
            : Qt::NoBrush);
        if (geometry.geometryType == QStringLiteral("polygon") && screenGeometry.size() >= 3)
            painter.drawPolygon(screenGeometry);
        else if (geometry.geometryType == QStringLiteral("line") && screenGeometry.size() >= 2)
            painter.drawPolyline(screenGeometry);
        else {
            painter.setPen(QPen(QColor(3, 25, 46, 220), 2));
            painter.setBrush(color);
            painter.drawEllipse(point, marker.live ? 7 : 6, marker.live ? 7 : 6);
            painter.setPen(QPen(color, 2));
            painter.drawLine(point + QPointF(0, 7), point + QPointF(0, 16));
        }

        const QString state = marker.liveState.isEmpty()
            ? QString() : QStringLiteral(" · %1").arg(marker.liveState);
        const QString label = marker.associationText.isEmpty()
            ? marker.name + state : marker.associationText;
        QFontMetrics metrics(painter.font());
        const int labelWidth = qBound(72, metrics.horizontalAdvance(label) + 18, 230);
        const QRectF labelRect(point.x() + 10, point.y() - 15, labelWidth, 24);
        painter.setPen(color.lighter(150));
        painter.setBrush(QColor(4, 28, 51, 220));
        painter.drawRoundedRect(labelRect, 3, 3);
        painter.setPen(Qt::white);
        painter.drawText(labelRect.adjusted(8, 0, -6, 0),
                         Qt::AlignVCenter | Qt::AlignLeft, label);
    }
}

void DeploymentSceneOverlay::drawBlackboard(QPainter &painter)
{
    if (!m_blackboardVisible)
        return;
    const auto drawShape = [&painter](const BlackboardShape &shape) {
        if (shape.points.isEmpty())
            return;
        painter.setPen(QPen(shape.color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        if (shape.kind == QStringLiteral("circle") && shape.points.size() >= 2) {
            const qreal radius = QLineF(shape.points.at(0), shape.points.at(1)).length();
            painter.drawEllipse(shape.points.at(0), radius, radius);
        } else if (shape.kind == QStringLiteral("rectangle") && shape.points.size() >= 2) {
            painter.drawRect(QRectF(shape.points.at(0), shape.points.at(1)).normalized());
        } else if (shape.points.size() >= 2) {
            painter.drawPolyline(QPolygonF(shape.points));
        } else {
            painter.drawPoint(shape.points.constFirst());
        }
    };
    for (const BlackboardShape &shape : m_blackboardShapes)
        drawShape(shape);
    if (m_blackboardDrawing)
        drawShape(m_blackboardDraft);
}

void DeploymentSceneOverlay::drawDraft(QPainter &painter)
{
    if (m_draftPoints.isEmpty())
        return;
    const bool annotation = m_interaction == Interaction::Annotation;

    if (!annotation && m_measurementKind == QStringLiteral("line-visible")) {
        // BuildingsLinesMeasure treats every point after the observer as an
        // independent target. Keep the moving last point as a rubber-band
        // ray; joining targets into a polyline is factually wrong.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QStringLiteral("#00ffff")), 3.0,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (const auto &segment : measurementPreviewSegments()) {
            QPointF observer;
            QPointF target;
            if (m_scene->screenPosition(segment.first, observer)
                && m_scene->screenPosition(segment.second, target)) {
                painter.drawLine(observer, target);
            }
        }
        return;
    }

    if (!annotation
        && m_measurementKind == QStringLiteral("radial-line-visible")) {
        QPointF centerScreen;
        if (!m_scene->screenPosition(m_draftPoints.constFirst(), centerScreen))
            return;
        if (m_draftPoints.size() >= 2) {
            QPointF edgeScreen;
            if (m_scene->screenPosition(m_draftPoints.constLast(), edgeScreen)) {
                // Exact reference pending-radius line: cyan, 3 pixels.
                painter.setPen(QPen(QColor(QStringLiteral("#00ffff")), 3.0));
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(centerScreen, edgeScreen);
            }

            const GeoCoordinate &center = m_draftPoints.constFirst();
            const double radius = horizontalDistance(
                center, m_draftPoints.constLast());
            constexpr double Pi = 3.14159265358979323846;
            const double latitudeScale = 111320.0;
            const double longitudeScale = std::max(1000.0,
                latitudeScale * std::cos(center.latitude * Pi / 180.0));
            QPolygonF outline;
            outline.reserve(ViewshedRayCount + 1);
            for (int index = 0; index < ViewshedRayCount; ++index) {
                const double angle = 2.0 * Pi * index / ViewshedRayCount;
                const GeoCoordinate edge{
                    center.longitude + radius * std::cos(angle) / longitudeScale,
                    center.latitude + radius * std::sin(angle) / latitudeScale,
                    center.altitude};
                QPointF screen;
                if (m_scene->screenPosition(edge, screen))
                    outline.append(screen);
            }
            if (outline.size() >= 3) {
                outline.append(outline.constFirst());
                // RadialLineOfSightNode's perimeter is yellow, 3 pixels.
                painter.setPen(QPen(QColor(QStringLiteral("#ffff00")), 3.0));
                painter.drawPolyline(outline);
            }
        }
        // The reference view-height control is a magenta auto-scaled sphere.
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#ff00ff")));
        painter.drawEllipse(centerScreen, 5.0, 5.0);
        return;
    }

    QVector<GeoCoordinate> measurementGeometry = m_draftPoints;
    const bool rectangularMeasurement = !annotation && m_draftPoints.size() >= 2
        && (m_measurementKind == QStringLiteral("gradient-analysis")
            || m_measurementKind == QStringLiteral("contour-measure"));
    if (rectangularMeasurement) {
        const GeoCoordinate &a = m_draftPoints.constFirst();
        const GeoCoordinate &b = m_draftPoints.constLast();
        measurementGeometry = {{a.longitude, a.latitude, a.altitude},
                               {b.longitude, a.latitude, a.altitude},
                               {b.longitude, b.latitude, b.altitude},
                               {a.longitude, b.latitude, b.altitude}};
    }
    const ExpandedAnnotationGeometry geometry = annotation
        ? m_scene->expandedAnnotationGeometry(m_annotationKind, m_draftPoints)
        : ExpandedAnnotationGeometry{
            (measurementIsArea(m_measurementKind) || rectangularMeasurement)
                ? QStringLiteral("polygon") : QStringLiteral("line"),
            measurementGeometry};
    QPolygonF polygon;
    for (const GeoCoordinate &point : geometry.points) {
        QPointF screen;
        if (m_scene->screenPosition(point, screen))
            polygon.append(screen);
    }
    const bool annotationArea = annotation
        && geometry.geometryType == QStringLiteral("polygon");
    const bool filledArea = (m_interaction == Interaction::Measurement
                             && (measurementIsArea(m_measurementKind)
                                 || rectangularMeasurement))
        || annotationArea;
    if (annotation) {
        // MarkVisitProcessor reference defaults: opaque green, one-pixel
        // solid outline; area markers use half-transparent yellow fill.
        // PlotVisitProcessor deliberately keeps its separate yellow/red style.
        const bool plot = isPlotAnnotation(m_annotationKind);
        painter.setPen(QPen(plot ? QColor(255, 255, 0) : QColor(0, 255, 0),
                            plot ? 2.0 : 1.0, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(annotationArea
            ? (plot ? QColor(255, 0, 0, 204) : QColor(255, 255, 0, 128))
            : Qt::NoBrush);
    } else {
        DeploymentMeasurement referenceStyle;
        referenceStyle.kind = m_measurementKind;
        applyLegacyMeasurementStyleDefaults(referenceStyle);
        QColor line = referenceStyle.lineColor;
        line.setAlphaF(referenceStyle.lineOpacity);
        QColor fill = referenceStyle.fillColor;
        fill.setAlphaF(referenceStyle.fillOpacity);
        // Native GL measurement strokes in the reference renderer use the
        // platform line rasterizer (square/default caps), not expanded round
        // quads. Keep the preview's pixels identical to the committed path.
        painter.setPen(QPen(line, referenceStyle.lineWidth));
        painter.setBrush(filledArea ? fill : Qt::NoBrush);
    }
    if (filledArea && polygon.size() >= 3)
        painter.drawPolygon(polygon);
    else if (polygon.size() >= 2)
        painter.drawPolyline(polygon);
    else if (polygon.size() == 1)
        painter.drawEllipse(polygon.constFirst(), 6, 6);
    if (annotation) {
        for (qsizetype index = 0; index < m_draftPoints.size(); ++index) {
            const GeoCoordinate &control = m_draftPoints.at(index);
            QPointF point;
            if (!m_scene->screenPosition(control, point))
                continue;
            painter.setPen(Qt::NoPen);
            const bool active = index == m_draftPoints.size() - 1;
            painter.setBrush(active ? QColor(255, 51, 26)
                                    : QColor(153, 255, 128));
            painter.drawEllipse(point, 6.0, 6.0);
            if (active)
                drawTranslateDragger(painter, point, controlAxisLength(this));
        }
    }
}

void DeploymentSceneOverlay::mousePressEvent(QMouseEvent *event)
{
    if (m_interaction == Interaction::Measurement
        && (event->button() == Qt::LeftButton
            || event->button() == Qt::RightButton)) {
        // The reference MeasureSys commits clicks on RELEASE only, provided
        // the pointer moved less than three physical pixels.  This prevents a
        // navigation drag from also creating a measurement control point.
        m_measurementPressPosition = event->position();
        m_measurementPressButton = event->button();
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        if (m_interaction == Interaction::Annotation)
            finishAnnotation();
        else
            cancelInteraction();
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    if (m_interaction == Interaction::Blackboard) {
        m_blackboardDrawing = true;
        m_blackboardDraft = {m_blackboardTool, m_blackboardColor,
                             {event->position()}};
        update();
        event->accept();
        return;
    }
    GeoCoordinate coordinate;
    if (!m_scene->coordinateAtScreen(event->position(), coordinate))
        return;
    if (m_interaction == Interaction::Annotation) {
        m_draftPoints.append(coordinate);
        const int autoFinish = annotationAutoFinishPoints(m_annotationKind);
        if (autoFinish > 0 && m_draftPoints.size() >= autoFinish) {
            finishAnnotation();
        } else {
            update();
        }
    }
    event->accept();
}

void DeploymentSceneOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_interaction == Interaction::Measurement) {
        // CDefaultMeasureEventHandler has no double-click action.  Consume it
        // so it neither completes the tool nor reaches ACTION_GOTO; suppress
        // the paired release so it cannot become a second single click.
        m_suppressMeasurementRelease = true;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton
        && m_interaction == Interaction::Annotation) {
        finishAnnotation();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void DeploymentSceneOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (m_interaction == Interaction::Measurement) {
        if (m_measurementStarted) {
            GeoCoordinate coordinate;
            if (m_scene->coordinateAtScreen(event->position(), coordinate)) {
                if (m_draftPoints.size() == 1)
                    m_draftPoints.append(coordinate);
                else if (!m_draftPoints.isEmpty())
                    m_draftPoints.last() = coordinate;
                update();
            }
        }
        event->accept();
        return;
    }
    if (m_interaction == Interaction::Blackboard && m_blackboardDrawing
        && event->buttons().testFlag(Qt::LeftButton)) {
        if (m_blackboardTool == QStringLiteral("freehand")
            || m_blackboardTool == QStringLiteral("polyline")) {
            m_blackboardDraft.points.append(event->position());
        } else if (m_blackboardDraft.points.size() == 1) {
            m_blackboardDraft.points.append(event->position());
        } else {
            m_blackboardDraft.points.last() = event->position();
        }
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DeploymentSceneOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_interaction == Interaction::Measurement
        && (event->button() == Qt::LeftButton
            || event->button() == Qt::RightButton)) {
        if (m_suppressMeasurementRelease && event->button() == Qt::LeftButton) {
            m_suppressMeasurementRelease = false;
            m_measurementPressButton = Qt::NoButton;
            event->accept();
            return;
        }
        const bool click = m_measurementPressButton == event->button()
            && std::abs(event->position().x() - m_measurementPressPosition.x()) < 3.0
            && std::abs(event->position().y() - m_measurementPressPosition.y()) < 3.0;
        m_measurementPressButton = Qt::NoButton;
        if (click && event->button() == Qt::RightButton) {
            // CDefaultMeasureEventHandler performs one final UpdatePoint on
            // right release before EndMeasure, so commit the current cursor
            // instead of retaining an older mouse-move coordinate.
            GeoCoordinate coordinate;
            if (m_measurementStarted && m_draftPoints.size() >= 2
                && m_scene->coordinateAtScreen(event->position(), coordinate)) {
                m_draftPoints.last() = coordinate;
            }
            finishMeasurement();
        } else if (click) {
            GeoCoordinate coordinate;
            if (m_scene->coordinateAtScreen(event->position(), coordinate)) {
                if (!m_measurementStarted) {
                    m_draftPoints.clear();
                    m_draftPoints.append(coordinate);
                    m_measurementStarted = true;
                } else if (measurementMaximumPoints(m_measurementKind) == 2
                           && m_draftPoints.size() >= 2) {
                    m_draftPoints.last() = coordinate;
                } else {
                    m_draftPoints.append(coordinate);
                }
                update();
            }
        }
        event->accept();
        return;
    }
    if (m_interaction == Interaction::Blackboard && m_blackboardDrawing
        && event->button() == Qt::LeftButton) {
        if (m_blackboardDraft.points.size() == 1)
            m_blackboardDraft.points.append(event->position());
        m_blackboardShapes.append(m_blackboardDraft);
        m_blackboardDraft = {};
        m_blackboardDrawing = false;
        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void DeploymentSceneOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelInteraction();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_interaction == Interaction::Annotation)
            finishAnnotation();
        else if (m_interaction == Interaction::Measurement) {
            event->accept();
            return;
        }
        else
            cancelInteraction();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void DeploymentSceneOverlay::finishMeasurement()
{
    const QString kind = m_measurementKind;
    const int minimum = measurementMinimumPoints(kind);
    if (m_draftPoints.size() >= minimum)
        emit m_scene->measurementCompleted(kind, m_draftPoints, measurementValue());
    cancelInteraction();
}

void DeploymentSceneOverlay::finishAnnotation()
{
    const int minimum = annotationMinimumPoints(m_annotationKind);
    if (m_draftPoints.size() >= minimum) {
        if (m_annotationKind == QStringLiteral("legacy-point"))
            emit m_scene->mapPointSelected(m_draftPoints.constFirst());
        else
            emit m_scene->annotationCompleted(m_annotationKind, m_draftPoints);
    }
    cancelInteraction();
}

void DeploymentSceneOverlay::publishInstruction()
{
    emit m_scene->interactionInstructionChanged(instruction());
}

QString DeploymentSceneOverlay::instruction() const
{
    switch (m_interaction) {
    case Interaction::Annotation:
        if (m_annotationKind == QStringLiteral("line"))
            return QStringLiteral("依次单击折线节点；双击、回车或右键完成；Esc 取消");
        if (m_annotationKind == QStringLiteral("polygon"))
            return QStringLiteral("依次单击多边形顶点；双击、回车或右键完成；Esc 取消");
        if (isPlotAnnotation(m_annotationKind))
            return QStringLiteral("%1：依次单击控制点；双击、回车或右键完成；Esc 取消")
                .arg(annotationDisplayName(m_annotationKind));
        if (annotationAutoFinishPoints(m_annotationKind) == 1)
            return QStringLiteral("单击地图确定中心位置；随后在属性窗体调整尺寸和样式；Esc 取消");
        return QStringLiteral("依次单击控制点；双击、回车或右键完成并设置属性；Esc 取消");
    case Interaction::Measurement:
        return QStringLiteral("%1：单击添加控制点，移动鼠标实时预览；右键完成；Esc 取消")
            .arg(measurementDisplayName(m_measurementKind));
    case Interaction::Blackboard:
        return QStringLiteral("黑板批注：按住左键绘制；Esc 结束绘制并保留内容");
    case Interaction::Navigate:
        return {};
    }
    return {};
}

double DeploymentSceneOverlay::measurementValue() const
{
    if (measurementIsArea(m_measurementKind))
        return polygonArea(m_draftPoints);
    if (m_measurementKind == QStringLiteral("line-visible")
        && m_draftPoints.size() >= 2) {
        double result = 0.0;
        for (qsizetype index = 1; index < m_draftPoints.size(); ++index)
            result += distance(m_draftPoints.constFirst(), m_draftPoints.at(index));
        return result;
    }
    double result = 0.0;
    for (qsizetype index = 1; index < m_draftPoints.size(); ++index)
        result += distance(m_draftPoints.at(index - 1), m_draftPoints.at(index));
    return result;
}

} // namespace Zqxxxt
