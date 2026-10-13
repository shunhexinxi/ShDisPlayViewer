#include "app/AppPaths.h"
#include "app/MarkerActions.h"
#include "app/MarkerActionEditor.h"
#include "app/ProjectStore.h"
#include "modules/zqbs/DeploymentXmlStore.h"
#include "modules/zqbs/FlightRouteXmlStore.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QFileInfo>
#include <QTableWidget>
#include <QTemporaryDir>
#include <iostream>
using namespace Zqxxxt;
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ShDisplayViewerTests");
    QStandardPaths::setTestModeEnabled(true);
    int failures = 0;
    auto check = [&](bool ok, const char *text) { if (!ok) { ++failures; std::cerr << text << '\n'; } };
    check(MarkerActions::normalize({{"type", "control"}, {"target", "device"}}).isEmpty(), "IMS control survived");
    auto video = MarkerActions::normalize({{"type", "video"}, {"target", "__custom_rtsp__"}, {"parameter", "rtsp://example.invalid/live"}, {"trigger", "double"}});
    check(MarkerActions::validate(video) && video.value("trigger") == "single", "legacy custom RTSP migration");
    check(!MarkerActions::validate({{"type", "video"}, {"target", "file:///movie.mp4"}}), "local video accepted");
    check(!MarkerActions::validate({{"type", "web"}, {"target", "file:///bin/sh"}}), "non-web scheme accepted");
    QJsonObject program{{"type", "program"}, {"platformTargets", QJsonObject{{"windows", "C:/Tools/demo.exe"}, {"linux", "/opt/demo"}}}};
    check(MarkerActions::target(program, "windows") == "C:/Tools/demo.exe" && MarkerActions::target(program, "linux") == "/opt/demo", "platform paths mixed");
    MarkerActionEditor editor({{"actions", QJsonArray{video, program, QJsonObject{{"type", "duty"}}}}});
    check(editor.findChild<QTableWidget *>("markerActionTable")->rowCount() == 2, "action editor did not filter IMS");
    check(editor.value().value("actions").toArray().size() == 2, "action roundtrip failed");
    auto *actionTable = editor.findChild<QTableWidget *>("markerActionTable");
    check(actionTable->columnCount() == 6
        && actionTable->horizontalHeaderItem(2)->text() == QStringLiteral("地址 / 程序路径"),
        "URL and program path are not unified");
    const auto preserved = editor.value().value("actions").toArray()[1].toObject();
    check(MarkerActions::target(preserved, "windows") == "C:/Tools/demo.exe"
        && MarkerActions::target(preserved, "linux") == "/opt/demo", "editing lost the other legacy platform path");
    auto *trigger = static_cast<QComboBox *>(actionTable->cellWidget(0, 4));
    auto *recovery = static_cast<QComboBox *>(actionTable->cellWidget(0, 5));
    trigger->setCurrentIndex(trigger->findData("auto"));
    for (int seconds : {0, 10, 30, 60}) {
        recovery->setCurrentIndex(recovery->findData(seconds));
        const auto saved = editor.value().value("actions").toArray()[0].toObject();
        MarkerActionEditor reopened({{"actions", QJsonArray{saved}}});
        const auto restored = reopened.value().value("actions").toArray()[0].toObject();
        check(restored.value("trigger") == "auto" && restored.value("recoverySeconds").toInt() == seconds,
            "automatic trigger or recovery option did not round-trip");
    }
    auto *actionType = static_cast<QComboBox *>(actionTable->cellWidget(0, 1));
    actionType->setCurrentIndex(actionType->findData("web"));
    actionTable->item(0, 2)->setText("https://example.invalid/fire");
    check(!recovery->isEnabled() && recovery->currentData().toInt() == 0,
        "system browser still offers timed closing");
    check(MarkerActions::normalize({{"type", "web"}, {"recoverySeconds", 10}}).value("recoverySeconds").toInt() == 0,
        "web recovery was not normalized to no action");
    check(MarkerActions::normalize({{"type", "video"}, {"recoverySeconds", 7}}).value("recoverySeconds").toInt() == 0,
        "unsupported recovery delay survived");
    check(MarkerActions::target({{"type", "program"}, {"target", "tools/demo"},
        {"platformTargets", QJsonObject{{"windows", "old.exe"}}}}, "windows") == "tools/demo",
        "explicit unified target does not override legacy paths");
    QTemporaryDir temporary; QString error;
    const QString portable = temporary.filePath("portable"), loose = temporary.filePath("unbundled");
    check(ProjectStore::clone(QStringLiteral(SHDISPLAY_DEFAULT_PROJECT_DIR), portable, error), "release template clone failed");
    check(QFileInfo::exists(portable + "/Data/earth/examples/HttpTms.earth") && QFileInfo::exists(portable + "/Data/earth/examples/HttpXyz.earth"), "map service templates lost during project export");
    check(AppPaths::defaultProjectDir(portable) == portable, "portable project was not selected");
    check(AppPaths::defaultProjectDir(loose) == loose + "/Projects/default", "unbundled project escaped application directory");
    check(AppPaths::initialize(portable, {}, &error), "portable storage initialization failed");
    check(AppPaths::dataDir() == portable + "/Data", "portable data directory mismatch");
    check(AppPaths::earthCacheDir() == portable + "/cache/earth"
        && AppPaths::temporaryDir() == portable + "/cache/temporary"
        && AppPaths::configDir() == portable + "/settings"
        && AppPaths::logDir() == portable + "/log", "generated storage escaped application directory");
    const QString previousWorkingDirectory = QDir::currentPath();
    QDir::setCurrent(temporary.path());
    qputenv("OSGEARTH_CACHE_PATH", QFile::encodeName(temporary.filePath("legacy-user-cache")));
    check(AppPaths::initialize(portable, {}, &error)
        && qEnvironmentVariable("OSGEARTH_CACHE_PATH") == QDir::toNativeSeparators(AppPaths::earthCacheDir())
        && qEnvironmentVariable("XDG_CACHE_HOME") == AppPaths::cacheDir(),
        "working directory or legacy cache environment changed portable storage");
    QDir::setCurrent(previousWorkingDirectory);
    const QString moved = temporary.filePath("moved-installation");
    check(AppPaths::initialize(moved, {}, &error)
        && AppPaths::earthCacheDir() == moved + "/cache/earth", "relocated installation reused the old cache path");
    QFile blocked(temporary.filePath("blocked-storage"));
    check(blocked.open(QIODevice::WriteOnly), "cannot create blocked storage fixture");
    blocked.close();
    check(!AppPaths::initialize(blocked.fileName(), {}, &error) && !error.isEmpty()
        && AppPaths::applicationDir() == moved, "unwritable installation silently fell back or changed active paths");
    AppPaths::initialize(portable, temporary.filePath("explicit"));
    check(AppPaths::dataDir() == temporary.filePath("explicit/Data"), "explicit project did not override portable default");
    DeploymentXmlStore bundled(portable + "/Data/Config"); DeploymentData bundledData;
    check(bundled.load(bundledData, error) && bundledData.plans.size() == 1 && bundledData.plans[0].steps.size() == 3, "release configuration failed to load");
    FlightRouteDocument routes;
    check(FlightRouteXmlStore::load(portable + "/Data/model/H6/routes/flight_routes.xml", routes, error) && routes.routes.size() == 3, "bundled flight configuration failed to load");
    const auto featureFile = portable + "/Data/Config/featureConfig.xml";
    auto writeFeature = [&](const QByteArray &xml) {
        QFile file(featureFile); return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(xml) == xml.size();
    };
    check(writeFeature("<layers><layer name=\"test\"><features><url>example.shp</url></features><styles><style>stroke: #ffffff;</style></styles></layer></layers>"), "cannot write one-layer configuration");
    check(bundled.load(bundledData, error) && bundledData.layers.size() == 1, "standalone vectors still require ten layers");
    check(writeFeature("<layers><layer name=\"incomplete\"/></layers>"), "cannot write invalid layer");
    check(!bundled.load(bundledData, error), "incomplete vector configuration accepted");
    const QString root = temporary.filePath("source"), copy = temporary.filePath("presentation");
    AppPaths::initialize({}, root);
    check(ProjectStore::create(root, error), error.toUtf8().constData());
    check(ProjectStore::isProject(root, error), "generated project invalid");
    DeploymentXmlStore store(root + "/Data/Config"); DeploymentData original;
    check(store.load(original, error) && original.plans.size() == 1 && original.plans[0].steps.size() == 3, "standalone demo failed");
    original.plans[0].steps[0].markers[0].association = {{"actions", QJsonArray{video, program}}};
    check(store.saveAll(original, error), "save actions failed");
    QDir().mkpath(root + "/Data/earth/cache_flat");
    QFile cache(root + "/Data/earth/cache_flat/do-not-export"); cache.open(QIODevice::WriteOnly); cache.write("sentinel"); cache.close();
    QFile asset(temporary.filePath("icon.txt")); asset.open(QIODevice::WriteOnly); asset.write("demo"); asset.close();
    QString relative;
    check(ProjectStore::importAsset(root, asset.fileName(), relative, error), "asset registration failed");
    check(ProjectStore::clone(root, copy, error), error.toUtf8().constData());
    check(!QFileInfo::exists(copy + "/Data/earth/cache_flat") && QFileInfo::exists(copy + "/Data/" + relative), "clone copied cache or lost managed asset");
    DeploymentXmlStore presentation(copy + "/Data/Config"); DeploymentData changed;
    check(presentation.load(changed, error), "copy load failed");
    check(changed.plans[0].steps[0].markers[0].association.value("actions").toArray().size() == 2, "dynamic marker actions lost");
    changed.plans[0].name = "temporary change"; presentation.saveAll(changed, error);
    store.load(original, error);
    check(original.plans[0].name != changed.plans[0].name, "presentation overwrote source");
    check(!ProjectStore::clone(root, copy, error), "clone overwrote existing files");
    const QString manifest = root + "/" + ProjectStore::manifestName();
    QFile file(manifest); file.open(QIODevice::ReadOnly); auto object = QJsonDocument::fromJson(file.readAll()).object(); file.close();
    object.insert("assets", QJsonArray{"Data/assets/../../secret"});
    file.open(QIODevice::WriteOnly | QIODevice::Truncate); file.write(QJsonDocument(object).toJson()); file.close();
    check(!ProjectStore::isProject(root, error), "path traversal accepted");
    std::cout << "Standalone actions, project roundtrip, presentation isolation, asset allowlist: " << failures << " failures\n";
    return failures ? 1 : 0;
}
