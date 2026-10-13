#pragma once

#include <QJsonObject>

#include <QColor>
#include <QString>
#include <QVector>

namespace Zqxxxt {

struct GeoCoordinate
{
    double longitude = 0.0;
    double latitude = 0.0;
    double altitude = 0.0;
};

struct DeploymentLayer
{
    QString id;
    QString name;
    QString kind;
    QString driver;
    QString source;
    QString profile = QStringLiteral("global-geodetic");
    QString quadtreeType = QStringLiteral("google");
    QString format;
    int minLevel = 0;
    int maxLevel = -1;
    int tileSize = 0;
    QColor color = QColor(QStringLiteral("#37d6ff"));
    bool visible = true;
    double opacity = 1.0;
    double lineWidth = 2.0;
    bool runtimeLayer = false;
    bool systemLayer = false;
    QString styleName = QStringLiteral("default");
    QString styleCss;
    double minRange = 0.0;
    double maxRange = 100000000.0;
    double tileSizeFactor = 15.0;
    bool cropFeatures = false;
    bool opacityEnabled = true;
    bool cacheEnabled = false;
    bool cacheConfigured = false;
    QString cacheName;
    QString cachePath;
    bool customVectorStyle = false;
    int fontSize = 18;
    QColor fontColor = QColor(Qt::white);
    QColor fontHaloColor = QColor(56, 58, 63);
    QString pointIcon;
};

struct DeploymentMeasurement
{
    QString id;
    QString name;
    QString kind;
    QVector<GeoCoordinate> points;
    // Terrain/profile samples and generated analysis segments are persisted so
    // a measurement remains inspectable after restarting the client.
    QVector<GeoCoordinate> analysisPoints;
    QVector<GeoCoordinate> analysisSegments;
    // One state per pair in analysisSegments: 1 visible/fill, 0 blocked/cut.
    // Older XML has no states and therefore falls back to analysisColor.
    QVector<int> analysisSegmentStates;
    double value = 0.0;
    double parameter = 0.0;
    double secondaryParameter = 0.0; // flood current water level
    bool showAnalysisBorder = true;
    // The legacy flood Reset action hides the water surface until Start is
    // pressed again. Older XML has no such field and therefore keeps the
    // post-analysis surface visible, matching CFloodSurface's initial state.
    bool analysisSurfaceVisible = true;
    QString resultText;
    bool terrainBacked = false;
    bool visible = true;
    QColor lineColor = QColor(QStringLiteral("#ffff00"));
    double lineWidth = 2.0;
    double lineOpacity = 1.0;
    QColor fillColor = QColor(QStringLiteral("#e6e600"));
    double fillOpacity = 0.5;
    QColor analysisColor = QColor(QStringLiteral("#00ff00"));
    double analysisLineWidth = 2.0;
    double analysisOpacity = 1.0;
};

// Reproduces the drawing defaults used by each FreeEarthSDK measurement.
// Call this after assigning kind; persisted XML attributes may then override
// individual values without resetting a user's saved style.
inline void applyLegacyMeasurementStyleDefaults(DeploymentMeasurement &measurement)
{
    measurement.lineColor = QColor(QStringLiteral("#ffff00"));
    measurement.lineWidth = 2.0;
    measurement.lineOpacity = 1.0;
    measurement.fillColor = QColor(QStringLiteral("#e6e600"));
    measurement.fillOpacity = 0.5;
    measurement.analysisColor = QColor(QStringLiteral("#00ff00"));
    measurement.analysisLineWidth = 2.0;
    measurement.analysisOpacity = 1.0;

    if (measurement.kind == QStringLiteral("space-area")
        || measurement.kind == QStringLiteral("shadow-area")
        || measurement.kind == QStringLiteral("area")) {
        measurement.lineColor = QColor(QStringLiteral("#80e680"));
    } else if (measurement.kind == QStringLiteral("flood-analysis")
               || measurement.kind == QStringLiteral("fill-cut-analysis")) {
        measurement.lineColor = QColor(QStringLiteral("#e6e600"));
        if (measurement.kind == QStringLiteral("fill-cut-analysis"))
            measurement.analysisColor = QColor(QStringLiteral("#80e680"));
    } else if (measurement.kind == QStringLiteral("height-distance")) {
        measurement.analysisColor = QColor(QStringLiteral("#ff0000"));
        measurement.analysisLineWidth = 1.2;
    } else if (measurement.kind == QStringLiteral("line-visible")) {
        measurement.analysisLineWidth = 3.0;
    } else if (measurement.kind == QStringLiteral("contour-measure")) {
        measurement.lineOpacity = 0.8;
        measurement.fillOpacity = 0.0;
        measurement.analysisLineWidth = 1.0;
        measurement.analysisOpacity = 0.8;
    }
}

struct EnvironmentSettings
{
    int timeOfDay = 12;
    int minuteOfHour = 0;
    int secondOfMinute = 0;
    QString calendarDate; // local ISO date; empty in older configs means today
    bool syncSystemTime = false;
    bool animateTime = false;
    int timeSpeed = 1; // seconds per second, reference range 1..3600
    quint64 clockRevision = 0; // Runtime-only command serial; not persisted to XML.
    int fog = 0;
    int rain = 50;
    int snow = 50;
    int brightness = 100;
    bool fogVisible = false;
    bool rainVisible = false;
    bool snowVisible = false;
    bool sunVisible = false;
    bool atmosphereVisible = true;
    bool moonVisible = true;
    bool starsVisible = true;
    bool nebulaVisible = true;
    double ambientIntensity = 50.0; // Percent; retain all 256 reference light-slider levels.
    bool rainGlobal = true;
    double rainLongitude = 0.0;
    double rainLatitude = 0.0;
    double rainRadiusMeters = 100000.0;
    double rainHeightMeters = 20000.0;
    bool snowGlobal = true;
    double snowLongitude = 0.0;
    double snowLatitude = 0.0;
    double snowRadiusMeters = 100000.0;
    double snowHeightMeters = 20000.0;
    bool cloudVisible = false;
    double cloudAltitudeMeters = 8000.0;
    double cloudThicknessMeters = 200.0;
    int cloudDensity = 60;
    int cloudOpacity = 80;
    double cloudSpeed = 10.0;
    int cloudDirectionDegrees = 45;
    QString cloudType = QStringLiteral("cumulus");

    // Retained for old configurations and the non-3D fallback only.
    // Native 3D uses the reference sky, precipitation and fog parameters.
    QColor atmosphereColor = QColor(QStringLiteral("#3c91d2"));
    double atmosphereOpacity = 0.14;
    QColor fogColor = QColor(QStringLiteral("#a8b3cf"));
    double fogOpacity = 1.0;
    QColor rainColor = QColor(QStringLiteral("#8c8c8c"));
    double rainLineWidth = 1.0;
    double rainOpacity = 0.5;
    QColor snowColor = QColor(QStringLiteral("#cccccc"));
    double snowParticleSize = 2.0;
    double snowOpacity = 0.5;
    QColor cloudColor = QColor(QStringLiteral("#e1ebf2"));
    QColor sunColor = QColor(QStringLiteral("#ffe073"));
    double sunSize = 15.0;
    double sunOpacity = 0.82;
    QColor moonColor = QColor(QStringLiteral("#dcebff"));
    double moonSize = 12.0;
    double moonOpacity = 0.75;
    QColor starColor = QColor(QStringLiteral("#ffffff"));
    double starSize = 1.0;
    double starOpacity = 1.0;
};

struct DeploymentViewpoint
{
    QString id;
    QString name;
    double longitude = 121.414;
    double latitude = 31.1828;
    double altitude = 0.0;
    double heading = 0.0;
    double pitch = -35.0;
    double range = 1500000.0;
    double flyTimeSeconds = 3.0;
};

// The legacy scene list stores marker and plot folders as first-class XML
// nodes.  Keep them explicitly so empty folders, visibility and properties
// survive a restart instead of being inferred only from marker categories.
struct DeploymentSceneFolder
{
    QString id;
    QString name;
    QString description;
    bool plot = false;
    bool visible = true;
    DeploymentViewpoint viewpoint;
};

// Squadron annotations are a separate data domain. Folder identity must not
// depend on a display name (renaming must not move or lose annotations).
struct DeploymentLocalFolder
{
    QString id;
    QString parentId;
    QString name;
    QString description;
    bool visible = true;
};

// Screen-space label rows used by the reference CExLabelNode editor.  The
// title lives on DeploymentMarker; these are the rows displayed underneath
// it and edited in the label tree.
struct DeploymentLabelItem
{
    QString text;
    double width = 100.0;
    double height = 30.0;
    double textSize = 12.0;
    QColor textColor = QColor(QStringLiteral("#ffffff"));
    QColor backgroundColor = QColor(77, 0, 255, 128);
    QString backgroundImage = QStringLiteral("texture/mark/label_item.png");
    int alignment = 1;
};

struct DeploymentMarker
{
    QString id;
    QString folderId; // Used only by squad-annotations.xml, not fixed layers.
    QString name;
    QString category;
    QString geometryType = QStringLiteral("point");
    QString icon;
    // MarkVisitProcessor::E_MARK_INITIALIZE defaults from the reference
    // application. Existing XML values still win when a saved marker is read.
    QColor color = QColor(QStringLiteral("#00ff00"));
    GeoCoordinate position;
    QVector<GeoCoordinate> points;
    QString text;
    QString description;
    double lineWidth = 1.0;
    double lineOpacity = 1.0;
    QColor fillColor = QColor(QStringLiteral("#ffff00"));
    double fillOpacity = 0.5;
    double pointSize = 20.0;
    double textSize = 15.0;
    // Reference LabelStyleWidget title defaults.
    double labelWidth = 130.0;
    double labelHeight = 30.0;
    QColor labelTextColor = QColor(QStringLiteral("#ffff00"));
    QColor labelBackgroundColor = QColor(QStringLiteral("#ffffff"));
    QString labelBackgroundImage = QStringLiteral("texture/mark/label_title.png");
    int labelAlignment = 1;
    QVector<DeploymentLabelItem> labelItems;
    // CExLodModelNodeOption fields used by the reference ModelStyleWidget.
    // The active resource remains in `icon`; these values preserve how it is
    // loaded and rendered when the property dialog is reopened.
    int modelSourceType = 0; // 0 = local file, 1 = service URL
    double modelRotationXDegrees = 0.0;
    double modelRotationYDegrees = 0.0;
    double modelRotationZDegrees = 0.0;
    double modelLodLowMeters = 0.0;
    double modelLodHighMeters = 100000000.0;
    QString modelServiceUrl;
    // CExTiltModelNodeOption fields used by the reference
    // TiltModelStyleWidget: configuration file, source data directory, or
    // service URL; plus the model's Z-axis rotation.
    int tiltSourceType = 0;
    double tiltRotationDegrees = 0.0;
    QString tiltServiceUrl;
    // Matches the legacy property dialog: clamp to terrain, relative to
    // terrain, or absolute altitude.
    int altitudeMode = 0;
    DeploymentViewpoint viewpoint;
    double viewFlyTimeSeconds = 1.0;
    double viewRangeLowMeters = 0.0;
    double viewRangeHighMeters = 100000000.0;
    bool visible = true;
    QString bindingType;
    QString bindingId;
    // Saved only for local annotation associations; runtime values stay in IMS.
    QJsonObject association;
    QString associationText;
    bool associationAlert = false;
    bool associationHideFlash = false;
    QColor associationColor;
    bool associationFlashOn = true;
    QString liveState;
    bool live = false;
};

// Defaults used by the original MarkVisitProcessor/PlotVisitProcessor.
// Marker and plotting nodes deliberately have different line/fill defaults.
inline void applyLegacyAnnotationStyleDefaults(DeploymentMarker &marker)
{
    marker.lineOpacity = 1.0;
    marker.pointSize = 20.0;
    marker.textSize = 15.0;
    if (marker.geometryType.startsWith(QStringLiteral("plot-"))) {
        marker.color = QColor(QStringLiteral("#ffff00"));
        marker.lineWidth = 2.0;
        marker.fillColor = QColor(QStringLiteral("#ff0000"));
        marker.fillOpacity = 0.8;
    } else {
        marker.color = QColor(QStringLiteral("#00ff00"));
        marker.lineWidth = 1.0;
        marker.fillColor = QColor(QStringLiteral("#ffff00"));
        marker.fillOpacity = 0.5;
    }
}

struct DeploymentBindingRule
{
    QString entityType;
    QString label;
    QColor color;
    bool visible = true;
};

struct DeploymentPlanStep
{
    QString id;
    QString name;
    QString description;
    int durationSeconds = 8;
    // Sequential slots include the marker's saved flight, 0.5s blink and hold.
    // Keep simultaneous duration separate so switching modes is lossless.
    int markerDurationSeconds = 4;
    DeploymentViewpoint viewpoint;
    QVector<DeploymentMarker> markers;
    // Older XML has no display mode and continues to show the whole step at once.
    bool sequentialDisplay = false;
};

struct DeploymentPlan
{
    QString id;
    QString name;
    QString description;
    bool loop = false;
    QVector<DeploymentPlanStep> steps;
};

struct DeploymentData
{
    QVector<DeploymentLayer> layers;
    QVector<DeploymentMeasurement> measurements;
    EnvironmentSettings environment;
    QVector<DeploymentViewpoint> viewpoints;
    QVector<DeploymentSceneFolder> sceneFolders;
    QVector<DeploymentMarker> markers;
    QVector<DeploymentBindingRule> bindingRules;
    QVector<DeploymentLocalFolder> normalFolders;
    QVector<DeploymentMarker> normalMarkers;
    bool normalLocalVisible = true;
    bool imsLiveVisible = true;
    QVector<DeploymentPlan> plans;
};

} // namespace Zqxxxt
