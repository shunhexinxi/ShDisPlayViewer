#include "app/AppPaths.h"
#include "modules/zqbs/DeploymentXmlStore.h"
#include "modules/zqbs/NormalAnnotationsTree.h"
#include "modules/zqbs/MarkerAssociation.h"
#include "ui/AppTheme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTreeWidgetItemIterator>
#include <iostream>

using namespace Zqxxxt;

namespace {
int failures = 0;
void check(bool result, const char *message)
{
    if (!result) { ++failures; std::cerr << message << '\n'; }
}
QByteArray bytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
QTreeWidgetItem *find(QTreeWidget &tree, const QString &type, const QString &id = {})
{
    for (QTreeWidgetItemIterator it(&tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toString() == type
            && (*it)->data(0, Qt::UserRole + 1).toString() == id) return *it;
    return nullptr;
}
bool visible(const QVector<DeploymentMarker> &markers, const QString &id)
{
    const auto it = std::find_if(markers.cbegin(), markers.cend(),
        [&id](const auto &marker) { return marker.id == id; });
    return it != markers.cend() && it->visible;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    AppPaths::initialize(QCoreApplication::applicationDirPath());
    AppTheme::apply(app);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary directory missing");
    DeploymentXmlStore store(directory.path());
    DeploymentData data;
    QString error;
    if (!store.load(data, error)) { std::cerr << qPrintable(error); return 1; }
    check(data.normalMarkers.isEmpty() && data.normalFolders.isEmpty(), "fresh squad data is not empty");
    check(store.normalFilePath().endsWith("/squad-annotations.xml"), "wrong independent storage file");
    DeploymentMarker fixed;
    fixed.id = "fixed-marker"; fixed.name = QStringLiteral("固定标记不可出现在中队树中");
    fixed.category = QStringLiteral("固定标记文件夹");
    fixed.position = {119.1, 36.2, 15.0}; fixed.points = {fixed.position};
    data.markers = {fixed};
    check(store.saveFixedAnnotations(data, error), "save fixed fixture failed");
    // Simulate upgrade: the old mirror contains fixed marks but the new file does not exist yet.
    check(QFile::remove(store.normalFilePath()), "remove temporary new file failed");
    const QStringList protectedPaths{store.fixedFilePath(), store.fixedAnnotationsFilePath(),
        store.marksFilePath(), store.plotsFilePath(), store.dynamicFilePath(),
        store.customLayerFilePath(), store.viewpointFilePath()};
    QMap<QString, QByteArray> originals;
    for (const auto &path : protectedPaths) {
        originals.insert(path, bytes(path));
        check(!originals.value(path).isEmpty(), "baseline fixed file is empty");
    }
    check(store.load(data, error), "upgrade load failed");
    check(data.markers.size() == 1 && data.normalMarkers.isEmpty() && data.normalFolders.isEmpty(),
        "upgrade duplicated fixed annotations into normal data");
    check(data.bindingRules.isEmpty(), "standalone unexpectedly has default IMS rules");
    data.normalFolders = {
        {"squad-a", {}, QStringLiteral("中队设施"), QStringLiteral("本地自定义文件夹"), true},
        {"squad-b", "squad-a", QStringLiteral("巡逻路线"), {}, true},
        {"squad-empty", {}, QStringLiteral("待补充标注"), {}, false}};
    DeploymentMarker point = fixed;
    point.id = "squad-point"; point.folderId = "squad-a";
    point.name = QStringLiteral("一号岗楼"); point.category = data.normalFolders[0].name;
    point.bindingType = "sentryPost"; point.bindingId = "post-1";
    point.viewpoint.longitude = 120.01; point.viewpoint.latitude = 36.05;
    point.viewpoint.heading = 75.0; point.viewpoint.pitch = -32.0; point.viewpoint.range = 2310.0;
    point.viewFlyTimeSeconds = 1.5;
    DeploymentMarker plot = point;
    plot.id = "squad-plot"; plot.folderId = "squad-b";
    plot.name = QStringLiteral("日常巡逻路线"); plot.category = data.normalFolders[1].name;
    plot.geometryType = "plot-bezier-arrow";
    plot.bindingType.clear(); plot.bindingId.clear();
    applyLegacyAnnotationStyleDefaults(plot);
    plot.lineWidth = 3.25; plot.lineOpacity = 0.7; plot.fillOpacity = 0.45;
    plot.points = {{119.1, 36.2, 15.0}, {119.2, 36.3, 20.0}, {119.3, 36.4, 30.0}};
    data.normalMarkers = {point, plot};
    data.imsLiveVisible = false;
    check(store.saveNormal(data, error), "save squad fixture failed");
    DeploymentData loaded;
    check(store.load(loaded, error), "reload squad fixture failed");
    check(loaded.normalFolders.size() == 3 && loaded.normalMarkers.size() == 2,
        "normal folders/markers did not round trip");
    if (loaded.normalFolders.size() != 3 || loaded.normalMarkers.size() != 2) return 1;
    check(!loaded.imsLiveVisible && !loaded.normalFolders[2].visible,
        "folder visibility did not persist");
    check(loaded.normalMarkers[1].points.size() == 3
        && loaded.normalMarkers[1].lineWidth == 3.25
        && loaded.normalMarkers[1].lineOpacity == 0.7
        && loaded.normalMarkers[1].fillOpacity == 0.45
        && loaded.normalMarkers[1].color == plot.color
        && loaded.normalMarkers[1].fillColor == plot.fillColor,
        "plot control points or styles lost in independent XML");
    check(loaded.normalMarkers[0].viewpoint.heading == 75.0
        && loaded.normalMarkers[0].viewpoint.pitch == -32.0
        && loaded.normalMarkers[0].viewpoint.range == 2310.0
        && loaded.normalMarkers[0].viewFlyTimeSeconds == 1.5
        && loaded.normalMarkers[0].bindingId == "post-1", "full viewpoint or binding lost");

    DeploymentMarker live = point;
    live.id = "live-post"; live.folderId.clear(); live.live = true;
    live.position = {120.5, 36.5, 25.0}; live.liveState = QStringLiteral("在线");
    DeploymentMarker camera = live;
    camera.id = "live-camera"; camera.name = QStringLiteral("值班室摄像机");
    camera.bindingType = "camera"; camera.bindingId = "camera-1";
    const QVector<DeploymentMarker> liveMarkers{live, camera};
    auto scene = composeDeploymentMarkers(loaded, liveMarkers, {});
    check(visible(scene, fixed.id) && visible(scene, point.id) && !visible(scene, camera.id),
        "IMS root incorrectly hides local/fixed data or fails to hide IMS");
    loaded.normalFolders[0].visible = false;
    loaded.imsLiveVisible = true;
    scene = composeDeploymentMarkers(loaded, liveMarkers, {});
    check(!visible(scene, point.id) && !visible(scene, plot.id) && visible(scene, live.id),
        "parent folder mask failed or hidden binding consumed live object");
    check(loaded.normalMarkers[0].visible && loaded.normalMarkers[1].visible
        && loaded.normalFolders[1].visible, "hiding parent overwrote child checkbox states");
    loaded.normalFolders[0].visible = true;
    loaded.normalMarkers[1].visible = false;
    loaded.normalLocalVisible = false;
    scene = composeDeploymentMarkers(loaded, liveMarkers, {});
    check(!visible(scene, point.id) && visible(scene, fixed.id), "local root mask affected fixed data");
    loaded.normalLocalVisible = true;
    scene = composeDeploymentMarkers(loaded, liveMarkers, {});
    check(visible(scene, point.id) && !visible(scene, plot.id), "restoring root lost child state");
    loaded.normalLocalVisible = false;
    loaded.normalFolders[0].visible = false;
    check(revealNormalMarkerAncestors(loaded, "squad-b")
        && loaded.normalLocalVisible && loaded.normalFolders[0].visible
        && loaded.normalFolders[1].visible,
        "new marker did not reveal its hidden root and folder lineage");
    scene = composeDeploymentMarkers(loaded, liveMarkers, {});
    check(visible(scene, point.id) && !visible(scene, plot.id),
        "revealing a new marker changed individual marker visibility");
    loaded.imsLiveVisible = false;
    scene = composeDeploymentMarkers(loaded, {camera}, {});
    check(!visible(scene, camera.id), "IMS refresh overrides hidden root");
    loaded.imsLiveVisible = true;

    QTreeWidget tree;
    tree.setHeaderHidden(true); tree.setColumnCount(1);
    const auto icon = [](const QString &) { return QStringLiteral(":/deployment/icons/point.png"); };
    buildNormalAnnotationsTree(&tree, loaded, {}, icon);
    check(tree.topLevelItemCount() == 1 && find(tree, "normal-local-root"),
        "normal tree must contain only the local annotations root");
    check(find(tree, "normal-local-root")
        && find(tree, "normal-local-root")->text(0) == QStringLiteral("本地标注"),
        "local annotations root label is wrong");
    check(find(tree, "normal-folder", "squad-empty") != nullptr, "empty custom folder missing");
    check(find(tree, "normal-marker", point.id) != nullptr && find(tree, "normal-marker", plot.id) != nullptr,
        "normal marker/plot leaves missing");
    check(find(tree, "normal-marker", fixed.id) == nullptr && find(tree, "marker", fixed.id) == nullptr,
        "fixed marker leaked into normal tree");
    auto *plotItem = find(tree, "normal-marker", plot.id);
    check(plotItem && plotItem->parent()->data(0, Qt::UserRole + 1) == "squad-b"
        && plotItem->text(0) == plot.name, "plot folder or displayed name is wrong");
    auto *empty = find(tree, "normal-folder", "squad-empty");
    check(empty && !empty->icon(0).isNull(), "folder icon resource missing");
    check(plotItem && !plotItem->icon(0).isNull(), "marker icon resource missing");
    check(empty && (empty->flags() & Qt::ItemIsUserCheckable) && empty->checkState(0) == Qt::Unchecked,
        "folder check control missing");
    check(!find(tree, "normal-ims-root") && !find(tree, "live") && !find(tree, "binding-rule"),
        "IMS live data branch must not appear in the normal tree");
    tree.clear();
    buildNormalAnnotationsTree(&tree, loaded, QStringLiteral("日常巡逻"), icon);
    check(find(tree, "normal-folder", "squad-a") && find(tree, "normal-folder", "squad-b")
        && find(tree, "normal-marker", plot.id) && !find(tree, "normal-marker", point.id),
        "filtered tree lost hierarchy or displayed unrelated leaves");
    loaded.normalFolders[0].name = QStringLiteral("中队设施（已改名）");
    loaded.normalMarkers[0].name = QStringLiteral("一号岗楼（属性已编辑）");
    check(store.saveNormal(loaded, error) && store.load(loaded, error), "edit/save/reload failed");
    check(loaded.normalMarkers[0].folderId == "squad-a", "rename broke stable folder membership");
    auto deleted = loaded;
    removeNormalFolder(deleted, "squad-a");
    check(deleted.normalFolders.size() == 1 && deleted.normalMarkers.isEmpty()
        && deleted.markers.size() == 1 && deleted.bindingRules.isEmpty(),
        "recursive folder delete lost unrelated data or left children behind");
    check(store.saveNormal(deleted, error) && store.load(deleted, error)
        && deleted.normalMarkers.isEmpty() && deleted.normalFolders.size() == 1, "delete not persisted");
    for (auto it = originals.cbegin(); it != originals.cend(); ++it)
        check(bytes(it.key()) == it.value(), "normal operations modified a fixed/dynamic config file");

    auto invalid = loaded;
    invalid.normalFolders[0].parentId = "squad-b";
    check(store.saveNormal(invalid, error), "invalid temporary fixture write failed");
    DeploymentData rejected;
    check(!store.load(rejected, error), "cyclic folder configuration must fail safely");

    DeploymentMarker nodeMarker;
    nodeMarker.id = "label-nodes"; nodeMarker.folderId = "label-folder";
    nodeMarker.geometryType = "label"; nodeMarker.name = QStringLiteral("东侧电网");
    nodeMarker.text = QStringLiteral("{name}实时监测");
    nodeMarker.points = {{120.0, 30.0, 0.0}};
    nodeMarker.position = nodeMarker.points.first();
    DeploymentLabelItem voltage;
    voltage.text = QStringLiteral("电压：{电压} {电压单位}，再次读取 {电压}");
    voltage.width = 288; voltage.height = 36;
    voltage.textColor = QColor("#35ddbb"); voltage.alignment = 2;
    DeploymentLabelItem currentRow;
    currentRow.text = QStringLiteral("电流：{电流} {电流单位}");
    currentRow.width = 178; currentRow.height = 28;
    nodeMarker.labelItems = {voltage, currentRow};
    nodeMarker.association = QJsonObject{
        {"type", "LABEL"}, {"imsBound", true},
        {"fields", QJsonArray{
            QJsonObject{{"label", QStringLiteral("电压")}, {"pointId", "voltage"},
                {"decimals", 1}, {"visible", true}},
            QJsonObject{{"label", QStringLiteral("电流")}, {"pointId", "current"},
                {"unit", "mA"}, {"decimals", 2}, {"visible", true}}}}};
    const QJsonObject sampledPoint{{"enabled", true}, {"unit", "kV"},
        {"latestValueJson", "8.65"}, {"staleAfterSeconds", 60},
        {"sampledAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    QHash<QString, QJsonObject> readings{{"voltage", sampledPoint}, {"current", sampledPoint}};
    auto displayed = nodeMarker;
    MarkerAssociation::applyLabelNodes(displayed, readings);
    check(displayed.associationText == QStringLiteral("东侧电网实时监测")
        && displayed.labelItems[0].text == QStringLiteral("电压：8.7 kV，再次读取 8.7")
        && displayed.labelItems[1].text == QStringLiteral("电流：8.65 mA"),
        "parent and independent child placeholders/units/decimals not resolved");
    check(displayed.labelItems[0].width == 288 && displayed.labelItems[0].height == 36
        && displayed.labelItems[0].textColor == voltage.textColor
        && displayed.labelItems[0].alignment == 2
        && displayed.labelItems[1].width == 178
        && nodeMarker.labelItems[0].text == voltage.text,
        "node display overwrote saved templates or individual styles");
    readings["current"].insert("latestValueJson", "12.34");
    auto refreshed = nodeMarker;
    MarkerAssociation::applyLabelNodes(refreshed, readings);
    check(refreshed.associationText == displayed.associationText
        && refreshed.labelItems[0].text == displayed.labelItems[0].text
        && refreshed.labelItems[1].text == QStringLiteral("电流：12.34 mA"),
        "child-only data change was lost");
    readings["voltage"].insert("sampledAt",
        QDateTime::currentDateTimeUtc().addSecs(-120).toString(Qt::ISODate));
    readings.remove("current");
    const auto expired = MarkerAssociation::labelNodes(nodeMarker, readings);
    check(expired[1] == QStringLiteral("电压：-- kV，再次读取 --")
        && expired[2] == QStringLiteral("电流：-- mA"), "stale/missing child values not cleared");
    nodeMarker.association.insert("template", QStringLiteral("总览 {电压}"));
    check(MarkerAssociation::labelNodes(nodeMarker, readings).first() == QStringLiteral("总览 --")
        && MarkerAssociation::labelNodes(nodeMarker, readings)[1] == expired[1],
        "parent override changed child templates");
    readings["voltage"] = sampledPoint;
    readings["voltage"].insert("latestValueJson", QStringLiteral("\"{name}\""));
    check(MarkerAssociation::nodeText(QStringLiteral("{电压} {name} {未映射}"),
        nodeMarker.name, nodeMarker.association, readings)
        == QStringLiteral("{name} 东侧电网 {未映射}"),
        "point contents recursively interpreted as templates or unknown tokens erased");
    auto unboundNode = nodeMarker;
    unboundNode.association.insert("imsBound", false);
    check(MarkerAssociation::labelNodes(unboundNode, readings).first() == nodeMarker.text,
        "unbound label unexpectedly interpreted templates");

    // Round trip the ORIGINAL templates, never the resolved live scene copy.
    DeploymentXmlStore labelStore(directory.filePath(QStringLiteral("label-roundtrip")));
    DeploymentData labelData;
    check(labelStore.load(labelData, error), "label test store initialization failed");
    labelData.normalFolders = {{"label-folder", {}, QStringLiteral("标牌"), {}, true}};
    labelData.normalMarkers = {nodeMarker};
    check(labelStore.saveNormal(labelData, error) && labelStore.load(labelData, error),
        "label template XML round trip failed");
    check(labelData.normalMarkers.size() == 1
        && labelData.normalMarkers[0].text == nodeMarker.text
        && labelData.normalMarkers[0].labelItems.size() == 2
        && labelData.normalMarkers[0].labelItems[0].text == voltage.text
        && labelData.normalMarkers[0].labelItems[0].width == voltage.width
        && labelData.normalMarkers[0].labelItems[0].textColor == voltage.textColor
        && labelData.normalMarkers[0].association == nodeMarker.association,
        "saved label node contents, styles or mapping changed after reload");

    if (argc > 1 && failures == 0) {
        tree.clear();
        buildNormalAnnotationsTree(&tree, loaded, {}, icon);
        tree.setStyleSheet(AppTheme::deploymentPanelStyleSheet());
        tree.setWindowTitle(QStringLiteral("常态化数据树 · 回归测试样例"));
        tree.resize(420, 480); tree.expandAll(); tree.show();
        app.processEvents();
        check(tree.grab().save(QString::fromLocal8Bit(argv[1])), "tree screenshot failed");
    }
    if (!failures) std::cout << "Normal annotations: storage isolation, CRUD, tree, filters, visibility and IMS refresh passed.\n";
    return failures ? 1 : 0;
}
