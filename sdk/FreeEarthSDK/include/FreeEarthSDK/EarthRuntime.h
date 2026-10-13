#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace FreeEarth {

enum class MouseButton
{
    None = 0,
    Left = 1,
    Middle = 2,
    Right = 3
};

struct Viewpoint
{
    double longitude = 0.0;
    double latitude = 0.0;
    double elevationMeters = 0.0;
    double headingDegrees = 0.0;
    double pitchDegrees = -35.0;
    double rangeMeters = 0.0;
    bool valid = false;
};

struct RuntimeOptions
{
    std::string earthFile;
    // Optional application resource root, independent of the selected map/project.
    std::string resourceDirectory;
    int pixelWidth = 1;
    int pixelHeight = 1;
    unsigned int defaultFramebufferObject = 0;
    bool graphicsContextIsCurrent = false;
};

struct LayerInfo
{
    std::string name;
    std::string type;
    bool visible = true;
    float opacity = 1.0f;
    bool applicationManaged = false;
};

// Runtime-owned map layer. Keeping this DTO free of Qt/OSG types makes the
// public API suitable for a separately built FreeEarthSDK later.
struct MapLayerDefinition
{
    std::string id;
    std::string name;
    std::string kind;   // imagery, elevation, vector
    std::string driver; // tms, gdal, flatgis, ogr
    std::string source;
    // Raster tiling metadata. maxLevel is the highest source-data LOD; -1
    // lets the driver discover it from metadata/immediate level directories.
    std::string profile = "global-geodetic";
    // Legacy FlatGIS uses "google" for north-origin rows (no Y flip) and
    // "tms" for south-origin rows (flip from osgEarth TileKey Y).
    std::string quadtreeType = "google";
    std::string format;
    int minLevel = 0;
    int maxLevel = -1;
    int tileSize = 0;
    std::string color = "#37d6ff";
    bool visible = true;
    float opacity = 1.0f;
    float lineWidth = 2.0f;
    std::string styleName = "default";
    std::string styleCss;
    float minRange = 0.0f;
    float maxRange = 100000000.0f;
    float tileSizeFactor = 15.0f;
    bool cropFeatures = false;
    bool cacheEnabled = false;
    bool cacheConfigured = false;
    std::string cachePath, cacheName;
    bool customVectorStyle = false;
    int fontSize = 18;
    std::string fontColor = "#ffffff";
    std::string fontHaloColor = "#383a3f";
    std::string pointIcon;
};

struct GeographicPoint
{
    double longitude = 0.0;
    double latitude = 0.0;
    double altitude = 0.0;
};

enum class GeometryAltitudeTechnique
{
    Automatic = -1,
    Map = 0,
    Scene = 1,
    Gpu = 2,
    Drape = 3
};

struct MapLabelItemDefinition
{
    std::string text;
    std::string backgroundImage;
    std::string textColor = "#ffffff";
    std::string backgroundColor = "#4d00ff";
    float width = 100.0f;
    float height = 30.0f;
    float textSize = 12.0f;
    float backgroundOpacity = 0.5f;
    int alignment = 1;
};

struct MapAnnotationDefinition
{
    std::string id;
    std::string name;
    // Original application kind (model, billboard, particle, plot-* ...).
    // `type` below is the expanded render primitive.
    std::string kind = "point";
    std::string type = "point";
    std::string text;
    std::string resourcePath;
    std::string color = "#37d6ff";
    std::string fillColor = "#37d6ff";
    std::vector<GeographicPoint> points;
    bool visible = true;
    float lineWidth = 2.0f;
    float lineOpacity = 1.0f;
    float fillOpacity = 0.22f;
    float pointSize = 10.0f;
    float textSize = 16.0f;
    float rotationDegrees = 0.0f;
    int tiltSourceType = 0;
    int modelSourceType = 0;
    float modelRotationXDegrees = 0.0f;
    float modelRotationYDegrees = 0.0f;
    float modelRotationZDegrees = 0.0f;
    double modelLodLowMeters = 0.0;
    double modelLodHighMeters = 100000000.0;
    float labelWidth = 130.0f;
    float labelHeight = 30.0f;
    std::string labelBackgroundImage;
    std::string labelTextColor = "#ffff00";
    std::string labelBackgroundColor = "#ffffff";
    float labelBackgroundOpacity = 1.0f;
    int labelAlignment = 1;
    std::vector<MapLabelItemDefinition> labelItems;
    // Pairwise coloured-line metadata.  `segmentStates` contains one value
    // per point pair (1 visible/green, 0 blocked/red). Viewshed rays are
    // contiguous groups; any remaining pairs form the circular completion
    // grid that appears after the last spoke, as in RadialLineOfSightNode.
    std::vector<int> segmentStates;
    std::vector<unsigned int> raySegmentCounts;
    unsigned int sweepStartRay = 0;
    int altitudeMode = 0; // 0 clamp, 1 relative, 2 absolute
    // osgEarth 2.7 compiled pixel-width measurement strokes as native GL
    // lines. osgEarth 3 uses GPU-expanded quads by default, which has
    // different smoothing/DPI behaviour. Enable this only for definitions
    // that reproduce a legacy measurement primitive.
    bool useLegacyGlLines = false;
    GeometryAltitudeTechnique altitudeTechnique =
        GeometryAltitudeTechnique::Automatic;
    // Legacy Feature geometry compiler settings: -1 keeps osgEarth defaults;
    // interpolation uses 0=great-circle, 1=rhumb-line.
    int tessellationSegments = -1;
    int geographicInterpolation = -1;
    int renderBin = -1;
    bool depthWrite = true;
    // Used only by the legacy flood-water surface definition.
    double minimumHeight = 0.0;
    double maximumHeight = 0.0;
    bool borderVisible = true;
};

struct AnnotationLineStyleStatus
{
    std::string id;
    std::string color;
    float width = 0.0f;
    float opacity = 0.0f;
    bool pixelUnits = false;
    bool legacyGlLines = false;
    int altitudeMode = 0;
    GeometryAltitudeTechnique altitudeTechnique =
        GeometryAltitudeTechnique::Automatic;
    int tessellationSegments = -1;
    int geographicInterpolation = -1;
    int renderBin = -1;
    bool depthWrite = true;
};

struct WaterSurfaceRuntimeStatus
{
    std::string id;
    std::string resourceDirectory;
    bool resourcesReady = false;
    bool programInstalled = false;
    bool clippedToPolygon = false;
    bool borderVisible = false;
    std::size_t polygonVertexCount = 0;
    float waveLength = 0.0f;
    float waveSpeed = 0.0f;
    double waveTimeSeconds = 0.0;
    double height = 0.0;
    double minimumHeight = 0.0;
    double maximumHeight = 0.0;
    // The old surface shader had no bound colorMap/tint. Its explicit colours
    // belong only to the vertical border and wall.
    std::string tint;
    float opacity = 0.0f;
    std::string borderColor;
    float borderWidth = 0.0f;
    float borderOpacity = 0.0f;
    std::string wallColor;
    float wallOpacity = 0.0f;
};

struct AnnotationRuntimeStatus
{
    std::size_t requestedDefinitionCount = 0;
    std::size_t renderedNodeCount = 0;
    std::size_t measurementInfoNodeCount = 0;
    std::size_t measurementPlainTextNodeCount = 0;
    std::size_t modernLabelNodeCount = 0;
    std::size_t viewshedNodeCount = 0;
    std::size_t viewshedPerimeterNodeCount = 0;
    std::size_t activeViewshedAnimationCount = 0;
    std::size_t completedViewshedAnimationCount = 0;
    std::size_t revealedViewshedRayCount = 0;
    std::size_t totalViewshedRayCount = 0;
    std::size_t visibleColoredSegmentCount = 0;
    std::size_t blockedColoredSegmentCount = 0;
    std::size_t activeWaterAnimationCount = 0;
    unsigned long long terrainSampleRequestCount = 0;
    std::vector<AnnotationLineStyleStatus> lineStyles;
    std::vector<WaterSurfaceRuntimeStatus> waterSurfaces;
    std::vector<std::string> displayedTexts;
};

enum class FlightCamera { Cockpit = 0, Free = 1, Follow = 2 };

struct FlightWaypointDefinition
{
    GeographicPoint position;
    double speed = 15.0;
    std::string annotation;
    std::string linkageType;
    std::string linkageContent;
    std::string linkageParameter;
};

struct FlightRouteDefinition
{
    std::string name;
    std::string color = "#19e7ed";
    float lineWidth = 2.0f;
    std::string markerStyle = "circle";
    std::vector<FlightWaypointDefinition> waypoints;
};

struct FlightLinkageEvent
{
    std::string type;
    std::string content;
    std::string parameter;
};

struct FlightStatus
{
    bool loaded = false;
    bool running = false;
    bool finished = false;
    GeographicPoint position;
    double elapsedSeconds = 0.0;
    double durationSeconds = 0.0;
    int speed = 1;
};

struct WeatherOptions
{
    bool visible = false, global = true;
    float intensity = 0.5f;
    double longitude = 0.0, latitude = 0.0, radiusMeters = 100000.0, heightMeters = 20000.0;
};
struct EnvironmentOptions
{
    bool sun = false, atmosphere = true, moon = true, stars = true, nebula = true;
    float ambient = 0.5f;
    WeatherOptions rain, snow;
    bool fog = false;
    float fogIntensity = 0.0f;
    long long utcSeconds = 0; // explicit UTC, Qt converts from the displayed local time
    bool syncSystemTime = false, animateTime = false;
    double timeSpeed = 1.0; // simulated seconds / real second (reference: 1..3600)
    unsigned long long clockRevision = 0; // Explicit seek/reset; ordinary refreshes keep time running.
};
struct EnvironmentStatus
{
    bool skyReady = false, nebulaReady = false, rainActive = false, snowActive = false;
    double cameraAltitudeMeters = 0.0, fogDensity = 0.0;
    long long utcSeconds = 0;
    unsigned int rainParticles = 0, snowParticles = 0;
    std::string warning;
};

class EarthRuntime final
{
public:
    EarthRuntime();
    ~EarthRuntime();

    EarthRuntime(const EarthRuntime &) = delete;
    EarthRuntime &operator=(const EarthRuntime &) = delete;
    EarthRuntime(EarthRuntime &&) noexcept;
    EarthRuntime &operator=(EarthRuntime &&) noexcept;

    bool initialize(const RuntimeOptions &options);
    void shutdown();

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] const std::string &lastError() const noexcept;

    void setEnvironment(const EnvironmentOptions &settings);
    [[nodiscard]] EnvironmentStatus environmentStatus() const;
    void setDefaultFramebufferObject(unsigned int framebufferObject);
    void frame();
    void resize(int pixelWidth, int pixelHeight);

    void mousePress(float x, float y, MouseButton button);
    void mouseDoubleClick(float x, float y, MouseButton button);
    void mouseRelease(float x, float y, MouseButton button);
    void mouseMove(float x, float y);
    void mouseWheel(float x, float y, bool scrollUp);
    // Physical window pixels; sourced from the active zoom's terrain anchor.
    bool zoomFocusScreenPosition(float &x, float &y) const;

    void setViewpoint(const Viewpoint &viewpoint, double transitionSeconds = 0.0);
    [[nodiscard]] Viewpoint viewpoint() const;

    bool loadFlightRoute(const std::string &routeFile,
                         const std::string &modelFile, std::string &error);
    bool loadFlightRoute(const FlightRouteDefinition &route,
                         const std::string &modelFile, std::string &error);
    void startFlight();
    void resetFlight();
    void closeFlight();
    void setFlightSpeed(int multiplier);
    void setFlightCamera(FlightCamera camera);
    void setFlightRouteVisible(bool visible);
    [[nodiscard]] FlightStatus flightStatus() const;
    std::vector<std::string> takeFlightVideoEvents();
    std::vector<FlightLinkageEvent> takeFlightLinkageEvents();

    [[nodiscard]] std::vector<LayerInfo> layers() const;
    bool setLayerVisible(const std::string &name, bool visible);
    bool setLayerOpacity(const std::string &name, float opacity);
    bool synchronizeMapLayers(const std::vector<MapLayerDefinition> &layers,
                              std::string &errorMessage);
    bool synchronizeAnnotations(
        const std::vector<MapAnnotationDefinition> &annotations,
        std::string &errorMessage);
    // Toggle an already synchronized annotation without rebuilding the scene.
    bool setAnnotationVisible(const std::string &id, bool visible);
    // Bounds of a screen annotation actually drawn in the last frame. Physical
    // pixels, top-left origin; includes icon/text placement and terrain height.
    bool annotationScreenBounds(const std::string &id, float &left, float &top,
                                float &width, float &height) const;
    [[nodiscard]] AnnotationRuntimeStatus annotationRuntimeStatus() const;
    // Expands original control points with the same FePlots algorithms used
    // by the legacy reference client. This keeps the live drawing preview and
    // the committed 3D node geometrically identical.
    bool calculatePlotGeometry(
        const std::string &kind,
        const std::vector<GeographicPoint> &controlPoints,
        std::vector<GeographicPoint> &geometry,
        std::string &errorMessage) const;
    bool screenToGeographic(float x, float y, double &longitude,
                            double &latitude, double &altitude) const;
    bool geographicToScreen(double longitude, double latitude, double altitude,
                            float &x, float &y) const;
    [[nodiscard]] bool hasElevationLayer() const;
    [[nodiscard]] bool hasElevationData() const;
    bool sampleTerrainElevations(std::vector<GeographicPoint> &points,
                                 std::string &errorMessage);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace FreeEarth
