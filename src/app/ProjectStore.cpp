#include "ProjectStore.h"
#include "AppPaths.h"
#include "MarkerActions.h"
#include "modules/zqbs/DeploymentXmlStore.h"
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QUuid>

namespace Zqxxxt::ProjectStore {
namespace {
bool write(const QString &path, const QByteArray &bytes, QString &error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = QStringLiteral("无法创建项目目录"); return false; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        error = QStringLiteral("项目文件写入失败：%1").arg(QFileInfo(path).fileName()); return false;
    }
    return true;
}
QJsonObject readManifest(const QString &root)
{
    QFile file(QDir(root).filePath(manifestName()));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool saveManifest(const QString &root, const QJsonObject &value, QString &error)
{ return write(QDir(root).filePath(manifestName()), QJsonDocument(value).toJson(), error); }
bool managedPath(const QString &path)
{
    if (QDir::isAbsolutePath(path) || path.contains('\\') || path.contains(':')) return false;
    const auto parts = path.split('/');
    if (parts.contains("..") || parts.contains(".") || parts.contains(QString())) return false;
    for (const auto &part : parts)
        if (part.compare("cache_flat", Qt::CaseInsensitive) == 0 || part.compare("cache", Qt::CaseInsensitive) == 0) return false;
    return path.startsWith("Data/assets/");
}
bool safeFile(const QString &root, const QString &relative)
{
    QString cursor = QDir(root).absolutePath();
    if (QFileInfo(cursor).isSymLink()) return false;
    for (const auto &part : relative.split('/')) {
        cursor = QDir(cursor).filePath(part);
        if (QFileInfo(cursor).isSymLink()) return false;
    }
    return QFileInfo(cursor).isFile();
}
QStringList baseFiles()
{
    QStringList files;
    for (const auto &file : configFiles()) files << "Data/Config/" + file;
    files << "Data/earth/FreeEarth_flat.earth" << "Data/model/H6/routes/flight_routes.xml";
    files << "Data/earth/examples/HttpTms.earth" << "Data/earth/examples/HttpXyz.earth";
    return files;
}
}
QString manifestName() { return QStringLiteral("shdisplay-project.json"); }
QStringList configFiles()
{
    return {"fixed-spatial.xml", "normal-annotations.xml", "squad-annotations.xml", "dynamic-plans.xml",
        "customLayerConfig.xml", "marksConfig.xml", "plotsConfig.xml", "viewPointConfig.xml", "systemConfig.xml", "featureConfig.xml"};
}
bool isProject(const QString &root, QString &error)
{
    const auto manifest = readManifest(root);
    if (manifest.value("format").toString() != "ShDisplayViewer" || manifest.value("schemaVersion").toInt() != 1) {
        error = QStringLiteral("请选择包含有效 shdisplay-project.json 的项目目录。"); return false;
    }
    for (const auto &entry : manifest.value("assets").toArray()) {
        if (!entry.isString() || !managedPath(entry.toString())) {
            error = QStringLiteral("项目资源清单包含不安全的路径。"); return false;
        }
    }
    for (const auto &relative : baseFiles()) {
        const auto path = QDir(root).filePath(relative);
        if (QFileInfo::exists(path) && !safeFile(root, relative)) {
            error = QStringLiteral("项目配置不能通过符号链接指向外部文件。"); return false;
        }
    }
    if (QFileInfo(root).isSymLink() || QFileInfo(QDir(root).filePath("Data")).isSymLink()
        || QFileInfo(QDir(root).filePath("Data/Config")).isSymLink()) {
        error = QStringLiteral("项目目录不能通过符号链接指向外部目录。"); return false;
    }
    return true;
}
bool create(const QString &root, QString &error, bool demo)
{
    if (QFileInfo::exists(root) && !QDir(root).isEmpty()) {
        error = QStringLiteral("新建项目需要空目录，已有内容不会覆盖。"); return false;
    }
    if (!saveManifest(root, {{"format", "ShDisplayViewer"}, {"schemaVersion", 1},
        {"name", QFileInfo(root).fileName()}, {"assets", QJsonArray{}}}, error)) return false;
    if (!write(QDir(root).filePath("Data/earth/FreeEarth_flat.earth"),
        "<?xml version=\"1.0\"?><map name=\"ShDisplayViewer offline scene\" type=\"geocentric\" version=\"2\"><options><lighting>false</lighting></options></map>\n", error)) return false;
    DeploymentXmlStore store(QDir(root).filePath("Data/Config"));
    DeploymentData data;
    if (!store.load(data, error)) return false;
    data.plans.clear();
    if (demo) {
        DeploymentPlan plan;
        plan.id = "demo-plan"; plan.name = QStringLiteral("示例：集结、部署、处置");
        plan.description = QStringLiteral("纯虚构场景；离线可编辑和播放，不包含真实单位或设备。");
        const QStringList names{QStringLiteral("集结"), QStringLiteral("展开部署"), QStringLiteral("处置完成")};
        for (int i = 0; i < 3; ++i) {
            DeploymentPlanStep step;
            step.id = QStringLiteral("demo-step-%1").arg(i); step.name = names[i];
            step.description = QStringLiteral("%1：依次展示两个标记，可暂停、拖动时间轴或编辑视角。").arg(names[i]);
            step.sequentialDisplay = true; step.markerDurationSeconds = 4;
            step.viewpoint.longitude = 0; step.viewpoint.latitude = 0; step.viewpoint.range = 8000;
            step.viewpoint.pitch = -70;
            for (int j = 0; j < 2; ++j) {
                DeploymentMarker marker;
                marker.id = QStringLiteral("demo-marker-%1-%2").arg(i).arg(j);
                marker.name = QStringLiteral("%1·点位%2").arg(names[i]).arg(j + 1);
                marker.geometryType = "point";
                marker.position = {(j - .5) * .012, (i - 1) * .01, 0};
                marker.points = {marker.position};
                marker.color = QColor(i == 0 ? "#36c8ff" : i == 1 ? "#ffb940" : "#45dc91");
                marker.viewpoint = step.viewpoint;
                marker.viewpoint.longitude = marker.position.longitude;
                marker.viewpoint.latitude = marker.position.latitude;
                marker.viewFlyTimeSeconds = 1;
                step.markers.append(marker);
            }
            plan.steps.append(step);
        }
        data.plans.append(plan);
    }
    return store.saveAll(data, error);
}
bool clone(const QString &source, const QString &destination, QString &error)
{
    if (!isProject(source, error)) return false;
    if (QFileInfo(destination).isSymLink() || (QFileInfo::exists(destination) && !QDir(destination).isEmpty())) {
        error = QStringLiteral("导出/另存目标必须为空目录。"); return false;
    }
    auto files = baseFiles();
    const auto manifest = readManifest(source);
    for (const auto &value : manifest.value("assets").toArray()) files << value.toString();
    // Exact file allowlist: never walk a map, model, cache, or arbitrary project tree.
    for (const auto &relative : files) {
        const auto from = QDir(source).filePath(relative);
        if (!QFileInfo::exists(from) && !relative.startsWith("Data/assets/")) continue;
        if (!safeFile(source, relative)) { error = QStringLiteral("资源缺失或使用了符号链接：%1").arg(relative); return false; }
        const auto to = QDir(destination).filePath(relative);
        if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !QFile::copy(from, to)) {
            error = QStringLiteral("复制项目文件失败：%1").arg(relative); return false;
        }
    }
    return saveManifest(destination, manifest, error);
}
bool importLegacy(const QString &source, const QString &destination, QString &error)
{
    if (!QFileInfo(QDir(source).filePath("dynamic-plans.xml")).isFile()) {
        error = QStringLiteral("所选目录没有 dynamic-plans.xml。"); return false;
    }
    if (!create(destination, error, false)) return false;
    for (const auto &name : configFiles()) {
        if (name == "systemConfig.xml" || name == "featureConfig.xml") continue;
        const auto from = QDir(source).filePath(name);
        if (!QFileInfo::exists(from)) continue;
        if (!safeFile(source, name)) { error = QStringLiteral("导入源包含符号链接。"); return false; }
        QFile file(from);
        if (!file.open(QIODevice::ReadOnly) || !write(QDir(destination).filePath("Data/Config/" + name), file.readAll(), error)) return false;
    }
    DeploymentXmlStore store(QDir(destination).filePath("Data/Config")); DeploymentData data;
    return store.load(data, error);
}
bool importAsset(const QString &root, const QString &source, QString &relative, QString &error)
{
    if (!isProject(root, error)) return false;
    if (!QFileInfo(source).isFile() || QFileInfo(source).isSymLink()) {
        error = QStringLiteral("请选择一个普通资源文件。"); return false;
    }
    relative = QStringLiteral("assets/%1/%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), QFileInfo(source).fileName());
    const auto destination = QDir(root).filePath("Data/" + relative);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(source, destination)) {
        error = QStringLiteral("资源复制失败。"); return false;
    }
    auto manifest = readManifest(root); auto assets = manifest.value("assets").toArray();
    assets.append("Data/" + relative); manifest.insert("assets", assets);
    return saveManifest(root, manifest, error);
}
bool setEarth(const QString &root, const QString &source, QString &error)
{
    if (!isProject(root, error)) return false;
    const QString config = QDir(root).filePath("Data/Config/systemConfig.xml");
    QFile file(config); QDomDocument document;
    if (!file.open(QIODevice::ReadOnly) || !document.setContent(&file)) { error = QStringLiteral("地图配置无法读取"); return false; }
    file.close();
    // Keep external .earth references in place; do not recursively copy referenced map datasets.
    document.documentElement().firstChildElement("common").setAttribute("earth", QFileInfo(source).absoluteFilePath());
    return write(config, document.toByteArray(2), error);
}
QStringList validateResources(const DeploymentData &data)
{
    QStringList report; int network = 0; int external = 0;
    const auto check = [&](const QString &path, const QString &label) {
        if (path.isEmpty() || path.startsWith(':') || path.startsWith("earth:")) return;
        const QUrl url(path);
        if (url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "rtsp" || url.scheme() == "rtsps") { ++network; return; }
        if (QFileInfo(path).isAbsolute()) ++external;
        if (!QFileInfo::exists(AppPaths::assetPath(path))) report << QStringLiteral("缺失资源：%1").arg(label);
    };
    const auto marker = [&](const DeploymentMarker &item) {
        check(item.icon, item.name);
        if (item.geometryType == "label") {
            check(item.labelBackgroundImage, item.name + QStringLiteral("（标题背景）"));
            for (const auto &row : item.labelItems) check(row.backgroundImage, item.name + QStringLiteral("（标牌背景）"));
        }
        if (item.modelSourceType == 1) check(item.modelServiceUrl, item.name + QStringLiteral("（模型服务）"));
        if (item.tiltSourceType == 2) check(item.tiltServiceUrl, item.name + QStringLiteral("（倾斜服务）"));
        for (const auto &value : item.association.value("actions").toArray()) {
            const auto action = MarkerActions::normalize(value.toObject()); QString error;
            if (action.isEmpty()) report << QStringLiteral("不支持的旧动作已禁用：%1").arg(item.name);
            else if (!MarkerActions::validate(action, &error)) report << item.name + "：" + error;
            else if (action.value("type").toString() == "program") {
                const auto path = MarkerActions::target(action);
                if (path.isEmpty()) report << QStringLiteral("当前平台没有程序配置：%1").arg(item.name);
                else check(path, item.name + QStringLiteral("（程序）"));
            } else ++network;
        }
    };
    for (const auto &item : data.markers) marker(item);
    for (const auto &item : data.normalMarkers) marker(item);
    for (const auto &plan : data.plans) for (const auto &step : plan.steps) for (const auto &item : step.markers) marker(item);
    for (const auto &layer : data.layers) check(layer.source, layer.name);
    report.prepend(QStringLiteral("资源检查：%1 项提示；%2 个网络引用；%3 个外部绝对路径。").arg(report.size()).arg(network).arg(external));
    report << QStringLiteral("网络连通性、RTSP 解码与外部地图内部引用需现场验证。导出只包含配置和通过“导入单个资源”登记的文件；地图数据集、外部程序、缓存不会打包。");
    return report;
}
}
