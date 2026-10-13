#pragma once
#include <QDateTime>

#include "modules/zqbs/DeploymentTypes.h"
#include "modules/zqbs/FlightRouteTypes.h"

#include <QOpenGLWidget>
#include <QPoint>
#include <QRectF>
#include <QStringList>

#include <functional>
#include <memory>

class QButtonGroup;
class QEvent;
class QLabel;
class QTimer;
class QToolButton;

#if defined(ZQXXXT_ENABLE_EARTH)
namespace FreeEarth {
class EarthRuntime;
}
#endif

namespace Zqxxxt {

class DeploymentSceneOverlay;
class AirViewOverlay;
class InfraredOverlay;
class NavigatorOverlay;
class ScaleBarOverlay;
class ZoomFocusOverlay;
enum class NavigatorCommand;
struct ExpandedAnnotationGeometry;

struct SceneViewpoint
{
    double longitude = 121.414;
    double latitude = 31.1828;
    double altitude = 0.0;
    double heading = 0.0;
    double pitch = -35.0;
    double range = 1500000.0;
    bool valid = false;
};

struct SceneFlightStatus
{
    bool loaded = false;
    bool running = false;
    bool finished = false;
    GeoCoordinate position;
    double elapsedSeconds = 0.0;
    double durationSeconds = 0.0;
};

class SceneWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit SceneWidget(QWidget *parent = nullptr);
    ~SceneWidget() override;

    [[nodiscard]] SceneViewpoint currentViewpoint() const;
    [[nodiscard]] QStringList runtimeLayers() const;
    [[nodiscard]] bool earthReady() const;
    [[nodiscard]] bool hasElevationLayer() const;
    [[nodiscard]] bool hasElevationData() const;
    bool setRuntimeLayerVisible(const QString &name, bool visible);
    bool setRuntimeLayerOpacity(const QString &name, double opacity);
    bool setDeploymentLayers(const QVector<DeploymentLayer> &layers,
                             QString &errorMessage);
    bool coordinateAtScreen(const QPointF &position, GeoCoordinate &coordinate) const;
    bool screenPosition(const GeoCoordinate &coordinate, QPointF &position) const;
    [[nodiscard]] QRectF renderedAnnotationBounds(const QString &id) const;
    bool sampleTerrainElevations(QVector<GeoCoordinate> &points,
                                 QString &errorMessage) const;
    [[nodiscard]] quint64 terrainSampleRequestCount() const noexcept
    {
        return m_terrainSampleRequestCount;
    }
    bool saveSceneSnapshot(const QString &filePath, QString &errorMessage);

    void setViewToolsActive(bool active);
    void setAirViewVisible(bool visible);
    void setNavigatorVisible(bool visible);
    void setScaleBarVisible(bool visible);
    void setInfraredVisible(bool visible);
    void setViewLocked(bool locked);
    [[nodiscard]] bool airViewVisible() const noexcept;
    [[nodiscard]] bool navigatorVisible() const noexcept;
    [[nodiscard]] bool scaleBarVisible() const noexcept;
    [[nodiscard]] bool infraredVisible() const noexcept;
    [[nodiscard]] bool viewLocked() const noexcept;

    void setDeploymentMarkers(const QVector<DeploymentMarker> &markers);
    void setAssociationFlashPhase(bool on);
    void setDeploymentMeasurements(const QVector<DeploymentMeasurement> &measurements);
    void setEnvironmentSettings(const EnvironmentSettings &settings);
    QDateTime environmentTime() const;
    void setDeploymentOverlayVisible(bool visible);
    void beginMarkerPlacement();
    void beginAnnotationPlacement(const QString &kind);
    void beginMeasurement(const QString &kind);
    void beginBlackboard(const QString &tool, const QColor &color);
    void setBlackboardTool(const QString &tool);
    void setBlackboardColor(const QColor &color);
    void clearBlackboard();
    void closeBlackboard();
    void cancelDeploymentInteraction();
    void beginMarkerPointEditing(
        const DeploymentMarker &marker,
        std::function<void(const QVector<GeoCoordinate> &)> pointsChanged);
    void endMarkerPointEditing();
    void flyTo(const SceneViewpoint &viewpoint, double seconds = 0.8);
    bool loadFlightRoute(const QString &routeFile, const QString &modelFile, QString &error);
    bool loadFlightRoute(const FlightRouteDefinition &route, const QString &modelFile,
                         QString &error);
    void startFlight();
    void resetFlight();
    void closeFlight();
    void setFlightSpeed(int multiplier);
    void setFlightCamera(int camera); // 0 驾驶，1 自由，2 跟踪
    void setFlightRouteVisible(bool visible);
    [[nodiscard]] SceneFlightStatus flightStatus() const;
    QStringList takeFlightVideoEvents();
    QVector<FlightWaypointLinkage> takeFlightLinkageEvents();

public slots:
    void resetView();

signals:
    void viewChanged(double longitude, double latitude, double height, double cameraHeight);
    void interactionInstructionChanged(const QString &text);
    void runtimeLayersChanged(const QStringList &layers);
    void mapPointSelected(const Zqxxxt::GeoCoordinate &coordinate);
    void annotationCompleted(const QString &kind,
                             const QVector<Zqxxxt::GeoCoordinate> &points);
    void measurementCompleted(const QString &kind,
                              const QVector<Zqxxxt::GeoCoordinate> &points,
                              double value);
    void deploymentMarkerTriggered(const QString &markerId,
                                   const QString &trigger,
                                   const QPoint &globalPosition);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    friend class DeploymentSceneOverlay;
    void createToolBar();
    void createMapModeButtons();
    void layoutOverlays();
    QPointF project(qreal x, qreal y, qreal z = 0) const;
    void drawBuilding(QPainter &painter, qreal x, qreal y, qreal w, qreal d, qreal h, int index);
    void emitView();
    void applyMapMode();
    void applyHomeViewpoint(double seconds);
    void initializeEarthRuntime();
    void synchronizeEarthAnnotations();
    void updateViewTools();
    void refreshInfraredPreview();
    void applyNavigatorCommand(NavigatorCommand command);
    void raiseViewTools();
    void updateZoomFocus();
    void updateGradientHover(const QPointF &position);
    void updateMarkerActionHover(const QPointF &position);
    void clearMarkerActionHover();
    [[nodiscard]] ExpandedAnnotationGeometry expandedAnnotationGeometry(
        const QString &kind,
        const QVector<GeoCoordinate> &controlPoints) const;

    QWidget *m_toolBar = nullptr;
    QWidget *m_modeBar = nullptr;
    AirViewOverlay *m_airView = nullptr;
    NavigatorOverlay *m_navigator = nullptr;
    ScaleBarOverlay *m_scaleBar = nullptr;
    InfraredOverlay *m_infrared = nullptr;
    ZoomFocusOverlay *m_zoomFocus = nullptr;
    QLabel *m_gradientHover = nullptr;
    QLabel *m_markerActionHint = nullptr;
    DeploymentSceneOverlay *m_deploymentOverlay = nullptr;
    QTimer *m_viewToolTimer = nullptr;
    QTimer *m_infraredTimer = nullptr;
    QButtonGroup *m_modeButtons = nullptr;
    QPoint m_lastMouse;
    QPointF m_markerPressPosition;
    Qt::MouseButton m_markerPressButton = Qt::NoButton;
    int m_markerClickGeneration = 0;
    bool m_suppressMarkerRelease = false;
    QString m_hoveredMarkerId;
    QRectF m_hoveredMarkerBounds;
    QPointF m_markerHoverPosition;
    QPointF m_pan;
    qreal m_zoom = 1.0;
    int m_mapMode = 2;
    double m_longitude = 121.41400;
    double m_latitude = 31.18280;
    QString m_earthFilePath;
    QString m_earthError;
    mutable quint64 m_terrainSampleRequestCount = 0;
    QVector<DeploymentMarker> m_deploymentMarkers;
    DeploymentMarker m_markerEditPreview;
    std::function<void(const QVector<GeoCoordinate> &)> m_markerEditPointsChanged;
    bool m_markerPointEditing = false;
    bool m_viewToolsActive = false;
    bool m_airViewRequested = false;
    bool m_navigatorRequested = false;
    bool m_scaleBarRequested = true;
    bool m_infraredRequested = false;
    bool m_viewLocked = false;
    QVector<DeploymentLayer> m_deploymentLayers;
    QVector<DeploymentMeasurement> m_deploymentMeasurements;
    EnvironmentSettings m_environment;
#if defined(ZQXXXT_ENABLE_EARTH)
    std::unique_ptr<FreeEarth::EarthRuntime> m_earth;
    QLabel *m_earthErrorOverlay = nullptr;
    bool m_earthInitializationAttempted = false;
    bool m_earthInitialViewApplied = false;
    SceneViewpoint m_earthRestoreViewpoint;
#endif
};

}
