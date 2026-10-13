#include "app/AppPaths.h"
#include "modules/zqbs/DeploymentXmlStore.h"
#include "modules/zqbs/MarkerAssociation.h"
#include "modules/zqts/MapHomeViewpoint.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDomDocument>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <iostream>

namespace {

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

QString rootTag(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QDomDocument document;
    if (!document.setContent(&file))
        return {};
    return document.documentElement().tagName();
}

QString configuredEarth(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QDomDocument document;
    if (!document.setContent(&file))
        return {};
    return document.documentElement().firstChildElement(QStringLiteral("common"))
        .attribute(QStringLiteral("earth"));
}

QString configuredFeatureLayer(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QDomDocument document;
    if (!document.setContent(&file))
        return {};
    return document.documentElement().firstChildElement(QStringLiteral("common"))
        .attribute(QStringLiteral("featureLayer"));
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Zqxxxt::AppPaths::initialize(QCoreApplication::applicationDirPath());
    QTemporaryDir directory;
    if (!require(directory.isValid(), "temporary directory unavailable"))
        return 1;

    Zqxxxt::DeploymentXmlStore store(directory.path());
    Zqxxxt::DeploymentData data;
    QString error;
    if (!require(store.load(data, error), qPrintable(error)))
        return 2;
    bool homeLoaded = false;
    const auto home = Zqxxxt::loadMapHomeViewpoint(store.systemConfigFilePath(), &homeLoaded);
    if (!require(homeLoaded && home.longitude == 107.205 && home.latitude == 27.645
                     && home.altitude == 10383909 && home.heading == -7.91417
                     && home.pitch == -89.8888 && home.range == 10383909,
                 "new system config must generate the complete reference Home pose"))
        return 40;
    {
        QFile file(store.systemConfigFilePath());
        if (!file.open(QIODevice::ReadOnly)) return 41;
        QDomDocument document;
        if (!document.setContent(&file)) return 42;
        file.close();
        auto views = document.documentElement().firstChildElement(QStringLiteral("viewPoints"));
        const auto begin = views.firstChildElement(QStringLiteral("viewPointBegin"));
        if (!require(begin.attribute(QStringLiteral("lon")) == QStringLiteral("121.414"),
                     "home reset change must not alter the independent startup pose"))
            return 43;
        auto end = views.firstChildElement(QStringLiteral("viewPointEnd"));
        end.setAttribute(QStringLiteral("lon"), 115.0);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 44;
        file.write(document.toByteArray());file.close();
        if (!require(store.load(data, error), qPrintable(error))
            || !require(Zqxxxt::loadMapHomeViewpoint(store.systemConfigFilePath()).longitude == 115.0,
                        "loading an existing config must preserve user-defined Home"))
            return 45;
    }
    if (!require(QFileInfo::exists(store.fixedFilePath()), "fixed XML not created")
        || !require(QFileInfo::exists(store.normalFilePath()), "normal XML not created")
        || !require(QFileInfo::exists(store.dynamicFilePath()), "dynamic XML not created")
        || !require(QFileInfo::exists(store.customLayerFilePath()), "legacy custom layer XML not created")
        || !require(QFileInfo::exists(store.marksFilePath()), "legacy marks XML not created")
        || !require(QFileInfo::exists(store.plotsFilePath()), "legacy plots XML not created")
        || !require(QFileInfo::exists(store.viewpointFilePath()), "legacy viewpoint XML not created")
        || !require(QFileInfo::exists(store.systemConfigFilePath()), "legacy system XML not created")
        || !require(!QFileInfo::exists(store.featureConfigFilePath()), "standalone created mandatory system feature data"))
        return 3;
    if (!require(rootTag(store.customLayerFilePath()) == QStringLiteral("layers"),
                 "customLayerConfig.xml does not use legacy layers root")
        || !require(rootTag(store.marksFilePath()) == QStringLiteral("document"),
                    "marksConfig.xml does not use legacy document root")
        || !require(rootTag(store.plotsFilePath()) == QStringLiteral("document"),
                    "plotsConfig.xml does not use legacy document root")
        || !require(rootTag(store.viewpointFilePath()) == QStringLiteral("viewpoints"),
                    "viewPointConfig.xml does not use legacy viewpoints root")
        || !require(rootTag(store.systemConfigFilePath()) == QStringLiteral("system"),
                    "systemConfig.xml does not use legacy system root")

        || !require(configuredEarth(store.systemConfigFilePath())
                        == QStringLiteral("earth/FreeEarth_flat.earth"),
                    "systemConfig.xml does not use FreeEarth_flat.earth")
        || !require(configuredFeatureLayer(store.systemConfigFilePath())
                        == QStringLiteral("Config/featureConfig.xml"),
                    "systemConfig.xml does not reference featureConfig.xml"))
        return 8;
    if (!require(data.bindingRules.isEmpty(), "standalone must not create IMS bindings")
        || !require(!data.plans.isEmpty(), "default dynamic plan missing"))
        return 4;
    if (!require(data.layers.isEmpty(), "standalone must not require shipped map datasets")) return 9;

    Zqxxxt::DeploymentMarker marker;
    marker.id = QStringLiteral("test-marker");
    marker.name = QStringLiteral("测试标记");
    marker.category = QStringLiteral("unit-annotation");
    marker.geometryType = QStringLiteral("polygon");
    marker.position = {120.12345678, 30.87654321, 12.5};
    marker.points = {
        marker.position,
        {120.22345678, 30.87654321, 12.5},
        {120.17345678, 30.97654321, 12.5}
    };
    marker.lineWidth = 3.5;
    marker.lineOpacity = 0.8;
    marker.fillColor = QColor(QStringLiteral("#aa7722"));
    marker.fillOpacity = 0.31;
    marker.description = QStringLiteral("属性窗体说明");
    marker.altitudeMode = 1;
    marker.viewpoint.longitude = 120.2;
    marker.viewpoint.latitude = 30.9;
    marker.viewpoint.altitude = 3500.0;
    marker.viewpoint.heading = 27.0;
    marker.viewpoint.pitch = -42.0;
    marker.viewpoint.range = 18000.0;
    marker.viewFlyTimeSeconds = 2.5;
    marker.viewRangeLowMeters = 100.0;
    marker.viewRangeHighMeters = 50000.0;
    marker.bindingType = QStringLiteral("device");
    marker.bindingId = QStringLiteral("device-test");
    data.markers.append(marker);
    Zqxxxt::DeploymentMarker localMarker = marker;
    localMarker.id = QStringLiteral("associated-local-marker");
    localMarker.folderId = QStringLiteral("local-folder");
    localMarker.association = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("GRID")},
        {QStringLiteral("template"), QStringLiteral("{name} / {电压} {电压单位}")},
        {QStringLiteral("fields"), QJsonArray{QJsonObject{
            {QStringLiteral("label"), QStringLiteral("电压")},
            {QStringLiteral("pointId"), QStringLiteral("voltage-1")},
            {QStringLiteral("visible"), true},
            {QStringLiteral("decimals"), 1}}}},
        {QStringLiteral("thresholdPointId"), QStringLiteral("voltage-1")},
        {QStringLiteral("comparison"), QStringLiteral("LT")},
        {QStringLiteral("threshold"), 8.0},
        {QStringLiteral("actions"), QJsonArray{QJsonObject{
            {QStringLiteral("trigger"), QStringLiteral("single")},
            {QStringLiteral("type"), QStringLiteral("video")},
            {QStringLiteral("target"), QStringLiteral("camera-1")},
            {QStringLiteral("enabled"), true}}}}};
    Zqxxxt::DeploymentLocalFolder localFolder;
    localFolder.id = QStringLiteral("local-folder");
    localFolder.name = QStringLiteral("中队本地标注");
    data.normalFolders.append(localFolder);
    data.normalMarkers.append(localMarker);

    Zqxxxt::DeploymentMarker plot = marker;
    plot.id = QStringLiteral("test-plot");
    plot.name = QStringLiteral("测试标绘");
    plot.category = QStringLiteral("作战标绘");
    plot.geometryType = QStringLiteral("plot-double-arrow");
    data.markers.append(plot);

    Zqxxxt::DeploymentMarker model = marker;
    model.id = QStringLiteral("test-model");
    model.name = QStringLiteral("测试模型");
    model.geometryType = QStringLiteral("model");
    model.icon = QStringLiteral("http://example.test/Tank.ive");
    model.pointSize = 25.0;
    model.modelSourceType = 1;
    model.modelRotationXDegrees = 12.5;
    model.modelRotationYDegrees = -25.0;
    model.modelRotationZDegrees = 37.5;
    model.modelLodLowMeters = 100.0;
    model.modelLodHighMeters = 75000.0;
    model.modelServiceUrl = model.icon;
    data.markers.append(model);

    Zqxxxt::DeploymentMarker tilt = marker;
    tilt.id = QStringLiteral("test-tilt-model");
    tilt.name = QStringLiteral("测试倾斜摄影");
    tilt.geometryType = QStringLiteral("tilt-model");
    tilt.icon = QStringLiteral("http://example.test/model.fgt");
    tilt.pointSize = 17.5;
    tilt.tiltSourceType = 2;
    tilt.tiltRotationDegrees = 37.5;
    tilt.tiltServiceUrl = tilt.icon;
    data.markers.append(tilt);

    Zqxxxt::DeploymentSceneFolder markFolder;
    markFolder.id = QStringLiteral("folder-mark");
    markFolder.name = marker.category;
    markFolder.description = QStringLiteral("标记文件夹");
    data.sceneFolders.append(markFolder);
    Zqxxxt::DeploymentSceneFolder plotFolder;
    plotFolder.id = QStringLiteral("folder-plot");
    plotFolder.name = plot.category;
    plotFolder.description = QStringLiteral("标绘文件夹");
    plotFolder.plot = true;
    data.sceneFolders.append(plotFolder);

    Zqxxxt::DeploymentLayer layer;
    layer.id = QStringLiteral("test-vector-layer");
    layer.name = QStringLiteral("测试矢量层");
    layer.kind = QStringLiteral("vector");
    layer.driver = QStringLiteral("ogr");
    layer.source = QStringLiteral("D:/map/test.shp");
    layer.color = QColor(QStringLiteral("#12abcd"));
    layer.opacity = 0.7;
    layer.lineWidth = 4.0;
    layer.minRange = 1250;
    layer.maxRange = 50000000;
    layer.customVectorStyle = true;
    layer.fontSize = 24;
    layer.fontColor = QColor(QStringLiteral("#eeddcc"));
    layer.fontHaloColor = QColor(QStringLiteral("#223344"));
    layer.pointIcon = QStringLiteral("D:/map/icon.png");
    layer.cacheConfigured = true;
    layer.cacheEnabled = true;
    layer.cacheName = QStringLiteral("测试缓存");
    layer.cachePath = QStringLiteral("D:/map/cache");
    data.layers.append(layer);

    Zqxxxt::DeploymentLayer elevation;
    elevation.id = QStringLiteral("test-flatgis-elevation");
    elevation.name = QStringLiteral("测试高程层");
    elevation.kind = QStringLiteral("elevation");
    elevation.driver = QStringLiteral("flatgis");
    elevation.source = QStringLiteral("D:/map/dem_global_0-10_flat");
    elevation.profile = QStringLiteral("global-geodetic");
    elevation.quadtreeType = QStringLiteral("google");
    elevation.format = QStringLiteral("osgb");
    elevation.minLevel = 0;
    elevation.maxLevel = 10;
    elevation.tileSize = 15;
    elevation.visible = true;
    data.layers.append(elevation);

    Zqxxxt::DeploymentMeasurement measurement;
    measurement.id = QStringLiteral("test-distance");
    measurement.name = QStringLiteral("测试距离");
    measurement.kind = QStringLiteral("distance");
    measurement.value = 123.456;
    measurement.points = {{120.0, 30.0, 0.0}, {120.001, 30.001, 0.0}};
    measurement.lineColor = QColor(QStringLiteral("#123456"));
    measurement.lineWidth = 3.5;
    measurement.lineOpacity = 0.62;
    measurement.fillColor = QColor(QStringLiteral("#654321"));
    measurement.fillOpacity = 0.27;
    measurement.analysisColor = QColor(QStringLiteral("#abcdef"));
    measurement.analysisLineWidth = 1.7;
    measurement.analysisOpacity = 0.73;
    measurement.secondaryParameter = 42.5;
    measurement.showAnalysisBorder = false;
    measurement.analysisSurfaceVisible = false;
    measurement.analysisSegments = {{120.0, 30.0, 10.0},
                                    {120.001, 30.001, 20.0}};
    measurement.analysisSegmentStates = {0};
    data.measurements.append(measurement);
    data.environment.rainVisible = true;
    data.environment.rain = 63;
    data.environment.rainColor = QColor(QStringLiteral("#778899"));
    data.environment.rainLineWidth = 2.5;
    data.environment.rainOpacity = 0.42;
    data.environment.snowVisible = true;
    data.environment.snowParticleSize = 3.5;
    data.environment.snowOpacity = 0.67;
    data.environment.fogVisible = true;
    data.environment.fogColor = QColor(QStringLiteral("#aabbcc"));
    data.environment.fogOpacity = 0.38;
    data.environment.cloudColor = QColor(QStringLiteral("#ccddee"));
    data.environment.sunColor = QColor(QStringLiteral("#fedcba"));
    data.environment.sunSize = 18.0;
    data.environment.moonOpacity = 0.58;
    data.environment.starSize = 1.5;
    data.viewpoints.first().flyTimeSeconds = 4.5;
    if (!require(store.saveAll(data, error), qPrintable(error)))
        return 5;

    Zqxxxt::DeploymentData reloaded;
    if (!require(store.load(reloaded, error), qPrintable(error)))
        return 6;
    const auto local = std::find_if(reloaded.normalMarkers.cbegin(),
        reloaded.normalMarkers.cend(), [](const Zqxxxt::DeploymentMarker &value) {
            return value.id == QStringLiteral("associated-local-marker");
        });
    if (!require(local != reloaded.normalMarkers.cend()
            && local->association == localMarker.association,
            "local marker association XML round-trip failed")) return 62;
    const QJsonObject sampledPoint{
        {QStringLiteral("enabled"), true},
        {QStringLiteral("latestValueJson"), QStringLiteral("7.2")},
        {QStringLiteral("sampledAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("staleAfterSeconds"), 60},
        {QStringLiteral("unit"), QStringLiteral("kV")}};
    const QHash<QString, QJsonObject> sampled{{QStringLiteral("voltage-1"), sampledPoint}};
    if (!require(Zqxxxt::MarkerAssociation::pointIds(local->association).size() == 1
            && Zqxxxt::MarkerAssociation::label(local->name, local->association, sampled)
                == QStringLiteral("测试标记 / 7.2 kV")
            && Zqxxxt::MarkerAssociation::alert(local->association, sampled)
            && Zqxxxt::MarkerAssociation::label(local->name, local->association, {})
                == QStringLiteral("测试标记 / -- "),
            "associated label, threshold or missing-value handling failed")) return 63;
    const QJsonObject alarmPoint{
        {QStringLiteral("enabled"), true},
        {QStringLiteral("latestValueJson"), QStringLiteral("1")},
        {QStringLiteral("sampledAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("staleAfterSeconds"), 60}};
    const QHash<QString, QJsonObject> alarmPoints{{QStringLiteral("alarm-1"), alarmPoint}};
    QJsonObject alarmAssociation{
        {QStringLiteral("type"), QStringLiteral("ALARM")},
        {QStringLiteral("alarmRules"), QJsonArray{QJsonObject{
            {QStringLiteral("pointId"), QStringLiteral("alarm-1")},
            {QStringLiteral("value"), QStringLiteral("1")},
            {QStringLiteral("action"), QStringLiteral("COLOR")},
            {QStringLiteral("color"), QStringLiteral("#ff9900")}}}}};
    if (!require(Zqxxxt::MarkerAssociation::pointIds(alarmAssociation)
            == QStringList{QStringLiteral("alarm-1")}
            && Zqxxxt::MarkerAssociation::alarmRule(alarmAssociation, alarmPoints)
                .value(QStringLiteral("action")).toString() == QStringLiteral("COLOR")
            && !Zqxxxt::MarkerAssociation::alert(alarmAssociation, alarmPoints)
            && Zqxxxt::MarkerAssociation::label(QStringLiteral("测试标记"),
                alarmAssociation, alarmPoints) == QStringLiteral("测试标记"),
            "alarm color rule did not match its data point value")) return 64;
    QJsonArray alarmRules = alarmAssociation.value(QStringLiteral("alarmRules")).toArray();
    alarmRules.append(QJsonObject{{QStringLiteral("pointId"), QStringLiteral("alarm-1")},
        {QStringLiteral("value"), QStringLiteral("1")},
        {QStringLiteral("action"), QStringLiteral("FLASH")}});
    alarmAssociation.insert(QStringLiteral("alarmRules"), alarmRules);
    if (!require(Zqxxxt::MarkerAssociation::alert(alarmAssociation, alarmPoints),
            "later matching flash action did not take precedence")) return 65;
    alarmRules.append(QJsonObject{{QStringLiteral("pointId"), QStringLiteral("alarm-1")},
        {QStringLiteral("value"), QStringLiteral("1")},
        {QStringLiteral("action"), QStringLiteral("RESET")}});
    alarmAssociation.insert(QStringLiteral("alarmRules"), alarmRules);
    if (!require(Zqxxxt::MarkerAssociation::alarmRule(alarmAssociation, alarmPoints)
            .value(QStringLiteral("action")).toString() == QStringLiteral("RESET")
            && !Zqxxxt::MarkerAssociation::alert(alarmAssociation, alarmPoints),
            "restore normal action did not override earlier matching rules")) return 66;
    const bool markerRoundTrip = std::any_of(reloaded.markers.cbegin(), reloaded.markers.cend(),
        [](const Zqxxxt::DeploymentMarker &value) {
            return value.id == QStringLiteral("test-marker")
                && value.bindingType == QStringLiteral("device")
                && value.bindingId == QStringLiteral("device-test")
                && value.geometryType == QStringLiteral("polygon")
                && value.points.size() == 3
                && qFuzzyCompare(value.lineWidth, 3.5)
                && qFuzzyCompare(value.lineOpacity, 0.8)
                && value.fillColor == QColor(QStringLiteral("#aa7722"))
                && qFuzzyCompare(value.fillOpacity, 0.31)
                && value.description == QStringLiteral("属性窗体说明")
                && value.altitudeMode == 1
                && qFuzzyCompare(value.viewpoint.longitude, 120.2)
                && qFuzzyCompare(value.viewpoint.pitch, -42.0)
                && qFuzzyCompare(value.viewpoint.range, 18000.0)
                && qFuzzyCompare(value.viewFlyTimeSeconds, 2.5)
                && qFuzzyCompare(value.viewRangeLowMeters, 100.0)
                && qFuzzyCompare(value.viewRangeHighMeters, 50000.0);
        });
    const bool layerRoundTrip = std::any_of(reloaded.layers.cbegin(), reloaded.layers.cend(),
        [](const Zqxxxt::DeploymentLayer &value) {
            return value.id == QStringLiteral("test-vector-layer")
                && value.driver == QStringLiteral("ogr")
                && value.color == QColor(QStringLiteral("#12abcd"))
                && qFuzzyCompare(value.lineWidth, 4.0)
                && value.minRange == 1250 && value.maxRange == 50000000
                && value.customVectorStyle && value.fontSize == 24
                && value.fontColor == QColor(QStringLiteral("#eeddcc"))
                && value.fontHaloColor == QColor(QStringLiteral("#223344"))
                && value.pointIcon == QStringLiteral("D:/map/icon.png")
                && value.cacheConfigured && value.cacheEnabled
                && value.cacheName == QStringLiteral("测试缓存")
                && value.cachePath == QStringLiteral("D:/map/cache");
        });
    const bool elevationRoundTrip = std::any_of(
        reloaded.layers.cbegin(), reloaded.layers.cend(),
        [](const Zqxxxt::DeploymentLayer &value) {
            return value.id == QStringLiteral("test-flatgis-elevation")
                && value.kind == QStringLiteral("elevation")
                && value.driver == QStringLiteral("flatgis")
                && value.source == QStringLiteral("D:/map/dem_global_0-10_flat")
                && value.profile == QStringLiteral("global-geodetic")
                && value.quadtreeType == QStringLiteral("google")
                && value.format == QStringLiteral("osgb")
                && value.minLevel == 0 && value.maxLevel == 10
                && value.tileSize == 15 && value.visible;
        });
    const bool measurementRoundTrip = std::any_of(
        reloaded.measurements.cbegin(), reloaded.measurements.cend(),
        [](const Zqxxxt::DeploymentMeasurement &value) {
            return value.id == QStringLiteral("test-distance")
                && value.points.size() == 2
                && value.lineColor == QColor(QStringLiteral("#123456"))
                && qFuzzyCompare(value.lineWidth, 3.5)
                && qFuzzyCompare(value.lineOpacity, 0.62)
                && value.fillColor == QColor(QStringLiteral("#654321"))
                && qFuzzyCompare(value.fillOpacity, 0.27)
                && value.analysisColor == QColor(QStringLiteral("#abcdef"))
                && qFuzzyCompare(value.analysisLineWidth, 1.7)
                && qFuzzyCompare(value.analysisOpacity, 0.73)
                && qFuzzyCompare(value.secondaryParameter, 42.5)
                && !value.showAnalysisBorder
                && !value.analysisSurfaceVisible
                && value.analysisSegments.size() == 2
                && value.analysisSegmentStates == QVector<int>{0};
        });
    const bool plotRoundTrip = std::any_of(reloaded.markers.cbegin(), reloaded.markers.cend(),
        [](const Zqxxxt::DeploymentMarker &value) {
            return value.id == QStringLiteral("test-plot")
                && value.geometryType == QStringLiteral("plot-double-arrow");
        });
    const bool modelRoundTrip = std::any_of(
        reloaded.markers.cbegin(), reloaded.markers.cend(),
        [](const Zqxxxt::DeploymentMarker &value) {
            return value.id == QStringLiteral("test-model")
                && value.geometryType == QStringLiteral("model")
                && value.modelSourceType == 1
                && qFuzzyCompare(value.modelRotationXDegrees, 12.5)
                && qFuzzyCompare(value.modelRotationYDegrees, -25.0)
                && qFuzzyCompare(value.modelRotationZDegrees, 37.5)
                && qFuzzyCompare(value.modelLodLowMeters, 100.0)
                && qFuzzyCompare(value.modelLodHighMeters, 75000.0)
                && value.modelServiceUrl
                    == QStringLiteral("http://example.test/Tank.ive")
                && value.icon == value.modelServiceUrl
                && qFuzzyCompare(value.pointSize, 25.0);
        });
    const bool tiltRoundTrip = std::any_of(
        reloaded.markers.cbegin(), reloaded.markers.cend(),
        [](const Zqxxxt::DeploymentMarker &value) {
            return value.id == QStringLiteral("test-tilt-model")
                && value.geometryType == QStringLiteral("tilt-model")
                && value.tiltSourceType == 2
                && qFuzzyCompare(value.tiltRotationDegrees, 37.5)
                && value.tiltServiceUrl
                    == QStringLiteral("http://example.test/model.fgt")
                && value.icon == value.tiltServiceUrl
                && qFuzzyCompare(value.pointSize, 17.5);
        });
    const bool foldersRoundTrip = reloaded.sceneFolders.size() >= 2
        && std::any_of(reloaded.sceneFolders.cbegin(), reloaded.sceneFolders.cend(),
            [](const Zqxxxt::DeploymentSceneFolder &folder) {
                return folder.id == QStringLiteral("folder-plot") && folder.plot;
            });
    const bool viewpointRoundTrip = !reloaded.viewpoints.isEmpty()
        && qFuzzyCompare(reloaded.viewpoints.first().flyTimeSeconds, 4.5);
    const Zqxxxt::EnvironmentSettings &environment = reloaded.environment;
    const bool environmentRoundTrip = environment.rainVisible
        && environment.rain == 63
        && environment.rainColor == QColor(QStringLiteral("#778899"))
        && qFuzzyCompare(environment.rainLineWidth, 2.5)
        && qFuzzyCompare(environment.rainOpacity, 0.42)
        && environment.snowVisible
        && qFuzzyCompare(environment.snowParticleSize, 3.5)
        && qFuzzyCompare(environment.snowOpacity, 0.67)
        && environment.fogVisible
        && environment.fogColor == QColor(QStringLiteral("#aabbcc"))
        && qFuzzyCompare(environment.fogOpacity, 0.38)
        && environment.cloudColor == QColor(QStringLiteral("#ccddee"))
        && environment.sunColor == QColor(QStringLiteral("#fedcba"))
        && qFuzzyCompare(environment.sunSize, 18.0)
        && qFuzzyCompare(environment.moonOpacity, 0.58)
        && qFuzzyCompare(environment.starSize, 1.5);
    if (!require(markerRoundTrip, "normal annotation XML round-trip failed")
        || !require(layerRoundTrip, "fixed layer XML round-trip failed")
        || !require(elevationRoundTrip,
                    "FlatGIS elevation metadata XML round-trip failed")
        || !require(measurementRoundTrip, "fixed XML round-trip failed")
        || !require(plotRoundTrip, "legacy plot XML round-trip failed")
        || !require(modelRoundTrip, "model XML round-trip failed")
        || !require(tiltRoundTrip, "tilt-model XML round-trip failed")
        || !require(foldersRoundTrip, "legacy scene folder XML round-trip failed")
        || !require(viewpointRoundTrip, "legacy viewpoint XML round-trip failed")
        || !require(environmentRoundTrip, "environment style XML round-trip failed"))
        return 7;

    // Old measurement XML has no animation-visibility attribute. Loading it
    // must preserve the pre-existing post-analysis presentation (visible),
    // while new XML can persist Reset/hidden state explicitly.
    {
        QFile file(store.fixedFilePath());
        if (!file.open(QIODevice::ReadOnly))
            return 46;
        QDomDocument document;
        if (!document.setContent(&file))
            return 47;
        file.close();
        const QDomNodeList measurements =
            document.elementsByTagName(QStringLiteral("measurement"));
        bool removed = false;
        for (int index = 0; index < measurements.size(); ++index) {
            QDomElement element = measurements.at(index).toElement();
            if (element.attribute(QStringLiteral("id"))
                == QStringLiteral("test-distance")) {
                element.removeAttribute(QStringLiteral("analysisSurfaceVisible"));
                removed = true;
                break;
            }
        }
        if (!removed || !file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return 48;
        file.write(document.toByteArray());
        file.close();
        Zqxxxt::DeploymentData compatible;
        if (!require(store.load(compatible, error), qPrintable(error)))
            return 49;
        const auto found = std::find_if(compatible.measurements.cbegin(),
            compatible.measurements.cend(), [](const auto &value) {
                return value.id == QStringLiteral("test-distance");
            });
        if (!require(found != compatible.measurements.cend()
                     && found->analysisSurfaceVisible,
                     "legacy XML did not default analysis surface to visible"))
            return 50;
    }
    return 0;
}
