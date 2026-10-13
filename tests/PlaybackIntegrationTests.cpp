#include "app/AppPaths.h"
#include "app/ProjectStore.h"
#include "app/MarkerActionEditor.h"
#include "modules/zqbs/ZqbsPage.h"
#include "modules/zqbs/DeploymentPropertyDialog.h"
#include "modules/zqts/SceneWidget.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QUrl>
#include <algorithm>
#include <iostream>
#include <cmath>

using namespace Zqxxxt;
class ActionUrlRecorder : public QObject {
    Q_OBJECT
public:
    QList<QUrl> opened;
public slots:
    void open(const QUrl &url) { opened.append(url); }
};
static void advance(int milliseconds)
{ QEventLoop loop; QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec(); }
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ShDisplayPlaybackTests");
    QStandardPaths::setTestModeEnabled(true);
    int failures = 0;
    auto check = [&](bool ok, const char *message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
    QTemporaryDir root; QString error;
    AppPaths::initialize({}, root.path());
    if (!ProjectStore::create(root.path(), error)) { std::cerr << error.toStdString(); return 1; }
    DeploymentXmlStore store(root.filePath("Data/Config")); DeploymentData data;
    if (!store.load(data, error)) return 1;
    for (auto &step : data.plans[0].steps) {
        step.markerDurationSeconds = 1;
        for (auto &marker : step.markers) marker.viewFlyTimeSeconds = .05;
    }
    ActionUrlRecorder urls;
    QDesktopServices::setUrlHandler("https", &urls, "open");
    auto webAction = [](const QString &suffix, const QString &trigger, bool enabled = true) {
        return QJsonObject{{"type", "web"}, {"trigger", trigger}, {"enabled", enabled},
            {"target", QStringLiteral("https://example.invalid/") + suffix}};
    };
    data.plans[0].steps[0].markers[0].association = {{"actions", QJsonArray{
        webAction("first-auto", "auto"), webAction("first-click", "single"),
        webAction("disabled", "auto", false)}}};
    data.plans[0].steps[1].markers[0].association = {{"actions", QJsonArray{webAction("second-auto", "auto")}}};
    DeploymentLayer currentLayer;
    currentLayer.id = QStringLiteral("earth-current");
    currentLayer.name = QStringLiteral("Current Map Layer");
    currentLayer.source = QStringLiteral("earth:Current Map Layer");
    currentLayer.runtimeLayer = true;
    currentLayer.visible = false;
    currentLayer.opacity = 0.42;
    DeploymentLayer removedLayer = currentLayer;
    removedLayer.id = QStringLiteral("earth-removed");
    removedLayer.name = QStringLiteral("Removed Map Layer");
    removedLayer.source = QStringLiteral("earth:Removed Map Layer");
    data.layers.append(currentLayer);
    data.layers.append(removedLayer);
    if (!store.saveAll(data, error)) return 1;
    QWidget host;
    auto *layout = new QVBoxLayout(&host);
    auto *scene = new SceneWidget(&host);
    ZqbsPage page(scene);
    layout->addWidget(page.leftPanel()); layout->addWidget(page.rightPanel());
    layout->addWidget(page.topPanel()); layout->addWidget(page.bottomPanel()); layout->addWidget(scene);
    page.setActive(true);
    const QStringList actualEarthLayers{QStringLiteral("Current Map Layer")};
    check(QMetaObject::invokeMethod(scene, "runtimeLayersChanged", Qt::DirectConnection,
        Q_ARG(QStringList, actualEarthLayers)), "could not notify page of loaded map layers");
    const auto &reconciledLayers = page.data().layers;
    const auto currentIt = std::find_if(reconciledLayers.cbegin(), reconciledLayers.cend(),
        [](const DeploymentLayer &layer) { return layer.id == QStringLiteral("earth-current"); });
    check(currentIt != reconciledLayers.cend() && !currentIt->visible
        && std::abs(currentIt->opacity - 0.42) < 0.000001,
        "existing map layer display settings were lost");
    check(std::none_of(reconciledLayers.cbegin(), reconciledLayers.cend(),
        [](const DeploymentLayer &layer) { return layer.id == QStringLiteral("earth-removed"); }),
        "removed earth layer persisted in page data");
    auto *sceneTree = page.leftPanel()->findChild<QTreeWidget *>("deploymentSceneTree");
    check(sceneTree && sceneTree->findItems(QStringLiteral("Removed Map Layer"),
        Qt::MatchExactly | Qt::MatchRecursive, 0).isEmpty(),
        "removed earth layer persisted in scene tree");
    DeploymentData reloadedLayers;
    check(store.load(reloadedLayers, error), "could not reload reconciled map layers");
    check(std::none_of(reloadedLayers.layers.cbegin(), reloadedLayers.layers.cend(),
        [](const DeploymentLayer &layer) { return layer.id == QStringLiteral("earth-removed"); }),
        "removed earth layer persisted in fixed-spatial.xml");
    // Playback tests enter the rehearsal module independently of the startup default.
    for (auto *button : page.topPanel()->findChildren<QPushButton *>())
        if (button->property("deploymentDomain").toBool()
            && button->property("domainIndex").toInt() == 2) button->click();
    auto *play = host.findChild<QPushButton *>("dynamicPlayPause");
    auto *stop = host.findChild<QPushButton *>("dynamicStop");
    check(play && stop, "playback controls absent from standalone window");
    if (!play || !stop) return 1;
    play->click();
    check(urls.opened.isEmpty(), "automatic action ran before marker arrival");
    advance(750);
    check(urls.opened.size() == 1 && urls.opened[0].path() == "/first-auto",
        "automatic reveal did not execute exactly the enabled auto action");
    check(page.activeDynamicMarkers().size() == 1, "first dynamic marker did not reveal after arrival");
    play->click(); const auto pausedCount = page.activeDynamicMarkers().size(); advance(1100);
    check(page.activeDynamicMarkers().size() == pausedCount, "paused page kept advancing");
    check(urls.opened.size() == 1, "pause or blink retriggered the auto action");
    play->click(); advance(1900);
    check(page.activeDynamicMarkers().size() >= 3, "resume did not preserve elapsed time/cumulative prior steps");
    check(urls.opened.size() == 2 && urls.opened[1].path() == "/second-auto",
        "resume did not deliver the next step action once");
    const auto clickMarkerId = data.plans[0].steps[0].markers[0].id;
    check(QMetaObject::invokeMethod(scene, "deploymentMarkerTriggered", Qt::DirectConnection,
        Q_ARG(QString, clickMarkerId), Q_ARG(QString, QStringLiteral("single")), Q_ARG(QPoint, QPoint())),
        "could not simulate map marker click");
    check(urls.opened.size() == 3 && urls.opened.last().path() == "/first-click",
        "mouse click executed automatic actions or ignored the mouse action");
    stop->click();
    check(page.activeDynamicMarkers().isEmpty(), "stop did not hide dynamic markers");
    auto *preview = host.findChild<QPushButton *>("dynamicTimelineStep_0");
    if (preview) preview->click();
    advance(100);
    check(urls.opened.size() == 3, "preview triggered automatic actions");
    play->click(); advance(750);
    check(urls.opened.size() == 4 && urls.opened.last().path() == "/first-auto",
        "replay did not reset automatic action delivery");
    stop->click();
    QDesktopServices::unsetUrlHandler("https");

    DeploymentMarker marker = data.plans[0].steps[0].markers[0];
    marker.association = {{"actions", QJsonArray{QJsonObject{{"type", "video"}, {"target", "rtsp://example.invalid/live"}}}}};
    bool accepted = false;
    DeploymentMarker savedMarker;
    DeploymentViewpoint current = marker.viewpoint;
    int captureCalls = 0;
    bool viewAvailable = true;
    QDialog *dialog = showDeploymentMarkerProperties(marker,
        [&]() -> std::optional<DeploymentViewpoint> {
            ++captureCalls;
            return viewAvailable ? std::make_optional(current) : std::nullopt;
        }, {}, &host,
        [&](bool ok, const DeploymentMarker &value) {
            savedMarker = value;
            accepted = ok && value.association.value("actions").toArray().size() == 1
                && value.bindingId.isEmpty();
        });
    DeploymentMarker localMarker = marker;
    localMarker.folderId = QStringLiteral("local-test-folder");
    QDialog *localDialog = showDeploymentMarkerProperties(localMarker,
        [] { return std::optional<DeploymentViewpoint>{}; }, {}, &host,
        [](bool, const DeploymentMarker &) {});
    check(dialog->size() == localDialog->size(), "dynamic and local marker property sizes differ");
    if (const int shot = app.arguments().indexOf("--ui-screenshot"); shot >= 0
        && shot + 1 < app.arguments().size()) {
        dialog->findChild<QTabWidget *>("markerPropertyTabs")->setCurrentIndex(2);
        dialog->grab().save(app.arguments()[shot + 1]);
    }
    localDialog->reject();
    advance(20);
    check(captureCalls == 0, "opening properties captured the map prematurely");
    auto *capture = dialog->findChild<QPushButton *>("captureMarkerViewpoint");
    auto *status = dialog->findChild<QLabel *>("markerViewpointCaptureStatus");
    auto field = [&](const char *name) { return dialog->findChild<QDoubleSpinBox *>(name)->value(); };
    const auto fieldsMatch = [&] {
        return std::abs(field("viewLongitude") - current.longitude) < 0.000001
            && std::abs(field("viewLatitude") - current.latitude) < 0.000001
            && std::abs(field("viewHeight") - current.altitude) < 0.001
            && std::abs(field("viewHeading") - current.heading) < 0.001
            && std::abs(field("viewPitch") - current.pitch) < 0.001
            && std::abs(field("viewRange") - current.range) < 0.01;
    };
    current.longitude = 121.481234; current.latitude = 31.231234;
    current.altitude = 234.125; current.heading = 67.125;
    current.pitch = -43.625; current.range = 8765.25;
    capture->click();
    check(captureCalls == 1 && fieldsMatch(), "capture did not read all six current camera fields");
    // Move the map again while the SAME modeless property window stays open.
    current.longitude = 118.781234; current.latitude = 32.041234;
    current.altitude = 567.25; current.heading = -123.5;
    current.pitch = -62.25; current.range = 4321.75;
    capture->click();
    check(captureCalls == 2 && fieldsMatch(), "second capture reused a stale camera snapshot");
    viewAvailable = false;
    capture->click();
    check(fieldsMatch() && !status->isHidden(), "unavailable camera overwrote the existing viewpoint");
    viewAvailable = true;
    capture->click();
    check(status->isHidden(), "capture error did not clear after camera recovery");
    auto *actions = dialog->findChild<QTableWidget *>("markerActionTable");
    check(actions && actions->rowCount() == 1, "dynamic marker with no folder has no action editor");
    const QString imageFile = root.filePath("chosen-marker.png");
    QFile image(imageFile);
    check(image.open(QIODevice::WriteOnly), "could not create marker picker fixture");
    image.close();
    auto *browse = dialog->findChild<QPushButton *>("markerResourceBrowse");
    auto *imagePath = dialog->findChild<QLineEdit *>("markerImagePath");
    check(browse && imagePath, "point marker has no image file picker");
    if (browse && imagePath) {
        bool pickerOpened = false;
        QTimer::singleShot(0, dialog, [&] {
            auto *picker = dialog->findChild<QFileDialog *>("markerResourceFileDialog");
            pickerOpened = picker && picker->isVisible()
                && picker->testOption(QFileDialog::DontUseNativeDialog);
            if (picker) {
                picker->selectFile(imageFile);
                static_cast<QDialog *>(picker)->accept();
            }
        });
        browse->click();
        check(pickerOpened && QDir::cleanPath(QDir::fromNativeSeparators(imagePath->text()))
                  == QDir::cleanPath(QDir::fromNativeSeparators(imageFile)),
              "marker image browse did not open a working Qt file picker");
    }
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    advance(20);
    check(accepted, "dynamic marker action was not saved by the property editor");
    data.plans[0].steps[0].markers[0] = savedMarker;
    check(store.saveAll(data, error), "captured viewpoint save failed");
    DeploymentData loaded;
    check(store.load(loaded, error), "captured viewpoint reload failed");
    const auto &persisted = loaded.plans[0].steps[0].markers[0].viewpoint;
    check(std::abs(persisted.longitude - current.longitude) < 0.000001
        && std::abs(persisted.latitude - current.latitude) < 0.000001
        && std::abs(persisted.altitude - current.altitude) < 0.001
        && std::abs(persisted.heading - current.heading) < 0.001
        && std::abs(persisted.pitch - current.pitch) < 0.001
        && std::abs(persisted.range - current.range) < 0.01,
        "latest captured camera was not persisted in dynamic plans XML");
    page.setActive(false);
    std::cout << "Playback, actions and live marker viewpoint capture: " << failures << " failures\n";
    return failures ? 1 : 0;
}
#include "PlaybackIntegrationTests.moc"
