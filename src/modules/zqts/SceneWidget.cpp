#include "modules/zqts/SceneWidget.h"
#include <QDateTime>

#include "app/AppPaths.h"
#include "app/MarkerActions.h"
#include "modules/zqbs/DeploymentSceneOverlay.h"
#include "modules/zqbs/MapAnalysis.h"
#include "modules/zqbs/PlotGeometry.h"
#include "modules/zqts/MapViewTools.h"
#include "modules/zqts/MapHomeViewpoint.h"

#include <QButtonGroup>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineF>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#if defined(ZQXXXT_ENABLE_EARTH)
#include <ShEarthSDK/EarthRuntime.h>
#endif

namespace Zqxxxt {

namespace {

QString absoluteEarthPath(const QString &configured)
{
    const QString cleaned = QDir::cleanPath(
        QDir::fromNativeSeparators(configured.trimmed()));
    const QFileInfo info(cleaned);
    if (info.isAbsolute())
        return info.absoluteFilePath();

    const QString fromData = QDir(AppPaths::dataDir()).filePath(cleaned);
    const QString fromApplication = QDir(AppPaths::applicationDir()).filePath(cleaned);
    QStringList candidates = cleaned.startsWith(QStringLiteral("Data/"),
                                                 Qt::CaseInsensitive)
        ? QStringList{fromApplication, fromData}
        : QStringList{fromData, fromApplication};
    candidates.append(QDir(QDir(AppPaths::dataDir()).filePath(QStringLiteral("earth")))
                          .filePath(QFileInfo(cleaned).fileName()));
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }
    return QFileInfo(candidates.constFirst()).absoluteFilePath();
}

QString migrateLegacyEarthName(const QString &configured)
{
    const QFileInfo info(configured);
    if (info.fileName().compare(QStringLiteral("ZqxxxtClient.earth"),
                                Qt::CaseInsensitive) != 0) {
        return configured;
    }

    const QString replacement = QDir(info.path()).filePath(
        QStringLiteral("FreeEarth_flat.earth"));
    const QString resolvedReplacement = absoluteEarthPath(replacement);
    return QFileInfo::exists(resolvedReplacement) ? replacement : configured;
}

QString configuredEarthFile()
{
    const QString environment = qEnvironmentVariable("SHDISPLAY_EARTH_FILE");
    if (!environment.trimmed().isEmpty())
        return absoluteEarthPath(environment);
    const QString systemConfig = QDir(AppPaths::dataDir()).filePath(
        QStringLiteral("Config/systemConfig.xml"));
    QFile file(systemConfig);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QDomDocument document;
        if (document.setContent(&file)) {
            const QString configured = document.documentElement()
                .firstChildElement(QStringLiteral("common"))
                .attribute(QStringLiteral("earth")).trimmed();
            if (!configured.isEmpty())
                return absoluteEarthPath(migrateLegacyEarthName(configured));
        }
    }
    return absoluteEarthPath(QStringLiteral("earth/FreeEarth_flat.earth"));
}

}

namespace {

#if defined(ZQXXXT_ENABLE_EARTH)
FreeEarth::MouseButton earthMouseButton(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton: return FreeEarth::MouseButton::Left;
    case Qt::MiddleButton: return FreeEarth::MouseButton::Middle;
    case Qt::RightButton: return FreeEarth::MouseButton::Right;
    default: return FreeEarth::MouseButton::None;
    }
}
#endif

QRectF markerScreenBounds(const Zqxxxt::SceneWidget *scene,
                          const Zqxxxt::DeploymentMarker &marker)
{
    if (scene->earthReady() && (marker.geometryType == QStringLiteral("point")
                               || marker.geometryType == QStringLiteral("label"))) {
        const QRectF rendered = scene->renderedAnnotationBounds(marker.id);
        // An annotation culled/hidden by the engine must not leave a hit area.
        return rendered.isValid() ? rendered.adjusted(-4, -4, 4, 4) : QRectF();
    }
    QPointF anchor;
    if (!scene->screenPosition(marker.position, anchor)) return {};
    if (scene->earthReady()) {
        return QRectF(anchor.x() - 18, anchor.y() - 18, 36, 36);
    }
    const QString label = marker.associationText.isEmpty()
        ? marker.name + (marker.liveState.isEmpty() ? QString()
            : QStringLiteral(" · %1").arg(marker.liveState)) : marker.associationText;
    const int width = qBound(72, QFontMetrics(scene->font()).horizontalAdvance(label) + 18, 230);
    return QRectF(anchor.x() - 10, anchor.y() - 18, width + 24, 40);
}

bool markerScreenHit(const Zqxxxt::SceneWidget *scene,
                     const Zqxxxt::DeploymentMarker &marker, const QPointF &clicked)
{
    return markerScreenBounds(scene, marker).contains(clicked);
}

}

SceneWidget::SceneWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    createToolBar();
    createMapModeButtons();
    m_toolBar->setVisible(qEnvironmentVariableIntValue("ZQXXXT_SHOW_MAP_TOOLS") == 1);
    m_modeBar->setVisible(qEnvironmentVariableIntValue("ZQXXXT_SHOW_MAP_MODE") == 1);
    m_airView = new AirViewOverlay(this);
    m_navigator = new NavigatorOverlay(this);
    m_scaleBar = new ScaleBarOverlay(this);
    m_infrared = new InfraredOverlay(this);
    m_zoomFocus = new ZoomFocusOverlay(this);
    m_zoomFocus->hide();
    m_gradientHover = new QLabel(this);
    m_gradientHover->setObjectName(QStringLiteral("measurementGradientHover"));
    m_gradientHover->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_gradientHover->setMargin(5);
    m_gradientHover->setStyleSheet(QStringLiteral(
        "QLabel { color: #07131f; background: rgba(190, 205, 214, 232); "
        "border: 1px solid #ff8000; border-radius: 2px; font-size: 13px; }"));
    m_gradientHover->hide();
    m_markerActionHint = new QLabel(this);
    m_markerActionHint->setObjectName(QStringLiteral("markerActionHoverHint"));
    m_markerActionHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_markerActionHint->setMargin(7);
    m_markerActionHint->setStyleSheet(QStringLiteral(
        "QLabel { color:#e8f8ff; background:rgba(10,44,78,238); "
        "border:1px solid #59d7ff; border-radius:3px; font-size:12px; }"));
    m_markerActionHint->hide();
    m_airView->setLocateHandler([this](double longitude, double latitude) {
        if (m_viewLocked)
            return;
        SceneViewpoint target = currentViewpoint();
        target.longitude = longitude;
        target.latitude = latitude;
        target.heading = 0.0;
        target.pitch = -89.0;
        target.range = 10000.0;
        target.valid = true;
        flyTo(target, 2.0);
    });
    m_navigator->setCommandHandler([this](NavigatorCommand command) {
        applyNavigatorCommand(command);
    });
    m_navigator->setZoomHandler([this](double rangeMeters) {
        if (m_viewLocked)
            return;
        SceneViewpoint view = currentViewpoint();
        if (!view.valid)
            return;
        view.range = rangeMeters;
        flyTo(view, 0.0);
        updateViewTools();
    });
    m_viewToolTimer = new QTimer(this);
    m_viewToolTimer->setInterval(150);
    connect(m_viewToolTimer, &QTimer::timeout, this, &SceneWidget::updateViewTools);
    m_viewToolTimer->start();
    m_infraredTimer = new QTimer(this);
    m_infraredTimer->setInterval(350);
    connect(m_infraredTimer, &QTimer::timeout,
            this, &SceneWidget::refreshInfraredPreview);
    // The focus indicator follows the rendered navigation anchor, not an
    // independent timer/scene pick that can race the scroll animation.
    setViewToolsActive(false);
    m_deploymentOverlay = new DeploymentSceneOverlay(this);
    m_deploymentOverlay->hide();
#if defined(ZQXXXT_ENABLE_EARTH)
    m_earthErrorOverlay = new QLabel(this);
    m_earthErrorOverlay->setAlignment(Qt::AlignCenter);
    m_earthErrorOverlay->setWordWrap(true);
    m_earthErrorOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_earthErrorOverlay->setStyleSheet(QStringLiteral(
        "QLabel { color:#ffcf70; background:#071727; border:1px solid #245c8f; "
        "padding:48px; font-size:17px; font-weight:600; }"));
    m_earthErrorOverlay->hide();
    auto *frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, QOverload<>::of(&SceneWidget::update));
    frameTimer->start(16);
#endif
}

SceneWidget::~SceneWidget() = default;

bool SceneWidget::loadFlightRoute(const QString &route, const QString &model, QString &error)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        std::string message;
        const bool loaded = m_earth->loadFlightRoute(route.toUtf8().toStdString(),
            model.toUtf8().toStdString(), message);
        error = QString::fromUtf8(message.c_str());
        update();
        return loaded;
    }
#endif
    error = QStringLiteral("三维地图尚未就绪，无法加载飞行漫游");
    return false;
}
bool SceneWidget::loadFlightRoute(const FlightRouteDefinition &route,
                                  const QString &model, QString &error)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        FreeEarth::FlightRouteDefinition value;
        value.name = route.name.toUtf8().toStdString();
        value.color = route.color.name(QColor::HexRgb).toStdString();
        value.lineWidth = static_cast<float>(route.lineWidth);
        value.markerStyle = route.markerStyle.toUtf8().toStdString();
        value.waypoints.reserve(static_cast<std::size_t>(route.waypoints.size()));
        for (const FlightRouteWaypoint &waypoint : route.waypoints) {
            FreeEarth::FlightWaypointDefinition point;
            point.position = {waypoint.coordinate.longitude,
                              waypoint.coordinate.latitude,
                              waypoint.coordinate.altitude};
            point.speed = waypoint.speed;
            point.annotation = waypoint.annotation.toUtf8().toStdString();
            point.linkageType = waypoint.linkage.type.toUtf8().toStdString();
            point.linkageContent = waypoint.linkage.content.toUtf8().toStdString();
            point.linkageParameter = waypoint.linkage.parameter.toUtf8().toStdString();
            value.waypoints.push_back(std::move(point));
        }
        std::string message;
        const bool loaded = m_earth->loadFlightRoute(
            value, model.toUtf8().toStdString(), message);
        error = QString::fromUtf8(message.c_str());
        update();
        return loaded;
    }
#endif
    error = QStringLiteral("三维地图尚未就绪，无法加载飞行漫游");
    return false;
}
void SceneWidget::startFlight()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) m_earth->startFlight();
#endif
}
void SceneWidget::resetFlight()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) m_earth->resetFlight();
#endif
    update();
}
void SceneWidget::closeFlight()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) m_earth->closeFlight();
#endif
    update();
}
void SceneWidget::setFlightSpeed(int value)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) m_earth->setFlightSpeed(value);
#endif
}
void SceneWidget::setFlightCamera(int value)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) m_earth->setFlightCamera(static_cast<FreeEarth::FlightCamera>(qBound(0, value, 2)));
#endif
    update();
}
void SceneWidget::setFlightRouteVisible(bool value)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) m_earth->setFlightRouteVisible(value);
#endif
    update();
}
SceneFlightStatus SceneWidget::flightStatus() const
{
    SceneFlightStatus result;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) {
        const auto state = m_earth->flightStatus();
        result.loaded = state.loaded; result.running = state.running; result.finished = state.finished;
        result.position = {state.position.longitude, state.position.latitude, state.position.altitude};
        result.elapsedSeconds = state.elapsedSeconds; result.durationSeconds = state.durationSeconds;
    }
#endif
    return result;
}
QStringList SceneWidget::takeFlightVideoEvents()
{
    QStringList result;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) for (const auto &event : m_earth->takeFlightVideoEvents())
        result.append(QString::fromUtf8(event.c_str()));
#endif
    return result;
}
QVector<FlightWaypointLinkage> SceneWidget::takeFlightLinkageEvents()
{
    QVector<FlightWaypointLinkage> result;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth) {
        for (const auto &event : m_earth->takeFlightLinkageEvents()) {
            result.append({QString::fromUtf8(event.type.c_str()),
                           QString::fromUtf8(event.content.c_str()),
                           QString::fromUtf8(event.parameter.c_str())});
        }
    }
#endif
    return result;
}

void SceneWidget::createToolBar()
{
    m_toolBar = new QWidget(this);
    m_toolBar->setObjectName(QStringLiteral("mapToolBar"));
    m_toolBar->setStyleSheet(QStringLiteral(R"(
        #mapToolBar { background:rgba(12,34,55,220); border:1px solid #397fa9; border-radius:2px; }
        QToolButton { color:white; background:transparent; border:0; padding:2px 5px; font-size:10px; }
        QToolButton:hover { color:#43bfff; background:rgba(27,99,168,180); }
    )"));
    auto *layout = new QHBoxLayout(m_toolBar);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(1);

    struct Tool { QString name; const char *icon; };
    const Tool tools[]{
        {QStringLiteral("图层"), "layer"}, {QStringLiteral("标注"), "mark"},
        {QStringLiteral("测量"), "measure"}, {QStringLiteral("标绘"), "Plot"},
        {QStringLiteral("视角"), "viewpoint"}, {QStringLiteral("漫游"), "roamfly"},
        {QStringLiteral("环境"), "environment"}, {QStringLiteral("服务"), "service"},
        {QStringLiteral("工具"), "tool"}, {QStringLiteral("帮助"), "help"}
    };
    for (const Tool &tool : tools) {
        auto *button = new QToolButton(m_toolBar);
        button->setText(tool.name);
        button->setIcon(QIcon(QStringLiteral(":/legacy/images/toolbar/%1.png").arg(QString::fromLatin1(tool.icon))));
        button->setIconSize(QSize(28, 28));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setFixedSize(63, 52);
        layout->addWidget(button);
    }
    m_toolBar->adjustSize();
}

void SceneWidget::createMapModeButtons()
{
    m_modeBar = new QWidget(this);
    m_modeBar->setStyleSheet(QStringLiteral("background:transparent;"));
    auto *layout = new QHBoxLayout(m_modeBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);
    m_modeButtons = new QButtonGroup(this);
    m_modeButtons->setExclusive(true);
    const QStringList names{QStringLiteral("2D"), QStringLiteral("2D3D"), QStringLiteral("3D")};
    for (int i = 0; i < names.size(); ++i) {
        auto *button = new QToolButton(m_modeBar);
        button->setCheckable(true);
        button->setFixedSize(58, 42);
        button->setIconSize(QSize(58, 42));
        button->setIcon(QIcon(QStringLiteral(":/legacy/images/mapmode/%1_normalhover.png").arg(names.at(i))));
        button->setStyleSheet(QStringLiteral("QToolButton{border:0;background:transparent;} QToolButton:checked{background:#0c416f;}"));
        if (i == 2)
            button->setChecked(true);
        m_modeButtons->addButton(button, i);
        layout->addWidget(button);
    }
    connect(m_modeButtons, &QButtonGroup::idClicked, this, [this](int id) {
        m_mapMode = id;
        applyMapMode();
        update();
    });
    m_modeBar->adjustSize();
}

void SceneWidget::initializeGL()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earthInitialViewApplied && earthReady())
        m_earthRestoreViewpoint = currentViewpoint();
    m_earth.reset();
    m_earthInitializationAttempted = false;
    m_earthError.clear();
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        if (QOpenGLFunctions *functions = context->functions())
            functions->initializeOpenGLFunctions();
    }
    m_earthInitializationAttempted = true;
    initializeEarthRuntime();
#endif
}

void SceneWidget::initializeEarthRuntime()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    // Qt guarantees that its desktop OpenGL context is current in initializeGL.
    // paintGL retains a fallback path in case the widget is recreated.
    makeCurrent();
    const auto *context = QOpenGLContext::currentContext();
    if (!context || !context->isValid() || context->format().version() < qMakePair(3, 3)) {
        m_earthError = QStringLiteral("当前会话没有可用的 OpenGL 3.3 图形上下文。请在桌面会话中运行并检查显卡驱动。");
        if (m_earthErrorOverlay) {
            m_earthErrorOverlay->setText(m_earthError);
            m_earthErrorOverlay->show();
            m_earthErrorOverlay->raise();
        }
        return;
    }
    const QString configuredPath = configuredEarthFile();
    m_earthFilePath = QFileInfo(configuredPath).absoluteFilePath();
    m_earthError.clear();
    m_earth = std::make_unique<FreeEarth::EarthRuntime>();
    FreeEarth::RuntimeOptions options;
    options.earthFile = QFile::encodeName(configuredPath).toStdString();
    options.resourceDirectory = QDir(AppPaths::applicationDir()).filePath(QStringLiteral("Data")).toUtf8().toStdString();
    options.pixelWidth = qMax(1, qRound(width() * devicePixelRatioF()));
    options.pixelHeight = qMax(1, qRound(height() * devicePixelRatioF()));
    options.defaultFramebufferObject = defaultFramebufferObject();
    options.graphicsContextIsCurrent = QOpenGLContext::currentContext() != nullptr;
    if (!QFileInfo::exists(configuredPath) || !m_earth->initialize(options)) {
        const QString runtimeError = QString::fromStdString(m_earth->lastError());
        m_earthError = !QFileInfo::exists(configuredPath)
            ? QStringLiteral("地图配置文件不存在：%1").arg(m_earthFilePath)
            : (runtimeError.contains(QStringLiteral("OpenGL shader entry points"))
                ? QStringLiteral("当前图形环境无法初始化三维地图，已安全切换到错误提示页；客户端其他功能仍可使用。")
                : QStringLiteral("三维地球场景初始化失败，请检查地图地址、服务状态和运行插件。"));
        qCritical().noquote() << QStringLiteral("ShEarthSDK initialization failed: %1")
            .arg(runtimeError.isEmpty() ? m_earthError : runtimeError);
        m_earth.reset();
        if (m_earthErrorOverlay) {
            m_earthErrorOverlay->setText(
                QStringLiteral("三维地球引擎加载失败\n\n%1\n\n地图配置：%2")
                    .arg(m_earthError.isEmpty() ? QStringLiteral("未知错误") : m_earthError,
                         m_earthFilePath));
            m_earthErrorOverlay->show();
            m_earthErrorOverlay->raise();
        }
        update();
        return;
    }
    if (m_earthErrorOverlay)
        m_earthErrorOverlay->hide();
    if (!m_earth->lastError().empty())
        qWarning().noquote() << QStringLiteral("ShEarthSDK map warning: %1")
            .arg(QString::fromStdString(m_earth->lastError()));
    QString layerError;
    if (!setDeploymentLayers(m_deploymentLayers, layerError) && !layerError.isEmpty())
        qWarning().noquote() << QStringLiteral("Deployment layer synchronization failed: %1").arg(layerError);
    synchronizeEarthAnnotations();
    QTimer::singleShot(0, this, [this] {
        emit runtimeLayersChanged(runtimeLayers());
    });
#endif
}

QPointF SceneWidget::project(qreal x, qreal y, qreal z) const
{
    const qreal flatten = m_mapMode == 0 ? 0.10 : (m_mapMode == 1 ? 0.25 : 0.40);
    const qreal heightScale = m_mapMode == 0 ? 0.08 : 0.70;
    return {width() / 2.0 + (x - y) * 0.70 * m_zoom + m_pan.x(),
            height() * 0.48 + (x + y) * flatten * m_zoom - z * heightScale * m_zoom + m_pan.y()};
}

void SceneWidget::drawBuilding(QPainter &painter, qreal x, qreal y, qreal w, qreal d, qreal h, int index)
{
    const QPointF a = project(x, y), b = project(x + w, y), c = project(x + w, y + d), d0 = project(x, y + d);
    const QPointF at = project(x, y, h), bt = project(x + w, y, h), ct = project(x + w, y + d, h), dt = project(x, y + d, h);
    painter.setPen(QColor(79, 126, 145));
    painter.setBrush(index % 3 == 0 ? QColor(49, 86, 106) : QColor(41, 74, 94));
    painter.drawPolygon(QPolygonF{b, c, ct, bt});
    painter.setBrush(index % 2 == 0 ? QColor(35, 63, 82) : QColor(41, 73, 90));
    painter.drawPolygon(QPolygonF{c, d0, dt, ct});
    painter.setPen(QColor(130, 172, 177));
    painter.setBrush(index % 4 == 0 ? QColor(106, 141, 148) : QColor(82, 118, 127));
    painter.drawPolygon(QPolygonF{at, bt, ct, dt});
}

void SceneWidget::paintGL()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (!m_earthInitializationAttempted) {
        m_earthInitializationAttempted = true;
        initializeEarthRuntime();
    }
    if (m_earth && m_earth->isReady()) {
        m_earth->setDefaultFramebufferObject(defaultFramebufferObject());
        m_earth->frame();
        if (!m_earthInitialViewApplied) {
            // Bootstrap the manipulator first, then render the complete Home
            // view into this same Qt frame so its initial origin is never shown.
            m_earthInitialViewApplied = true;
            applyHomeViewpoint(0.0);
            m_earth->frame();
        } else if (m_earthRestoreViewpoint.valid) {
            // A recreated GL context must retain navigation, not repeat login Home.
            flyTo(m_earthRestoreViewpoint, 0.0);
            m_earthRestoreViewpoint.valid = false;
            m_earth->frame();
        }
        updateZoomFocus();
        if (!m_hoveredMarkerId.isEmpty())
            updateMarkerActionHover(m_markerHoverPosition);
        return;
    }
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        QOpenGLFunctions *functions = context->functions();
        functions->glViewport(0, 0,
            qMax(1, qRound(width() * devicePixelRatioF())),
            qMax(1, qRound(height() * devicePixelRatioF())));
        functions->glClearColor(0.027f, 0.09f, 0.153f, 1.0f);
        functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
                           | GL_STENCIL_BUFFER_BIT);
    }
    return;
#endif
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0, QColor(7, 23, 39));
    background.setColorAt(0.5, QColor(18, 53, 74));
    background.setColorAt(1, QColor(8, 28, 45));
    painter.fillRect(rect(), background);

    painter.setPen(QPen(QColor(45, 114, 117), 1));
    painter.setBrush(QColor(24, 62, 67));
    painter.drawPolygon(QPolygonF{project(-620, -450), project(650, -450), project(650, 470), project(-620, 470)});

    painter.setPen(QPen(QColor(37, 112, 115, 100), 1));
    for (int i = -600; i <= 600; i += 80) {
        painter.drawLine(project(i, -440), project(i, 450));
        painter.drawLine(project(-610, i * 0.7), project(640, i * 0.7));
    }

    painter.setPen(QPen(QColor(103, 123, 116), 30 * m_zoom, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(project(-620, -60, 1), project(650, -60, 1));
    painter.setPen(QPen(QColor(95, 117, 110), 24 * m_zoom, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(project(-70, -450, 1), project(-70, 470, 1));
    painter.setPen(QPen(QColor(156, 160, 153), 2));
    painter.drawLine(project(-620, -60, 2), project(650, -60, 2));
    painter.drawLine(project(-70, -450, 2), project(-70, 470, 2));

    const std::array<std::array<int, 5>, 27> blocks{{
        {{-500,-380,95,80,48}},{{-380,-370,75,105,72}},{{-270,-395,120,75,38}},{{-20,-380,100,80,66}},{{115,-370,85,120,42}},{{245,-390,135,80,75}},{{430,-350,92,100,52}},
        {{-520,-190,115,75,38}},{{-350,-185,100,85,90}},{{-185,-210,70,105,55}},{{20,-210,130,80,46}},{{190,-200,80,96,82}},{{330,-190,140,78,48}},
        {{-510,45,80,130,68}},{{-390,50,130,90,44}},{{-215,35,95,125,72}},{{10,45,82,110,55}},{{135,35,145,92,40}},{{320,40,95,125,78}},{{460,55,75,90,46}},
        {{-520,245,115,95,42}},{{-360,230,75,130,85}},{{-235,250,135,80,51}},{{20,220,105,125,72}},{{170,245,88,90,45}},{{310,225,145,110,63}},{{490,240,80,95,38}}
    }};
    for (qsizetype i = 0; i < qsizetype(blocks.size()); ++i) {
        const auto &b = blocks.at(size_t(i));
        drawBuilding(painter, b[0], b[1], b[2], b[3], b[4], int(i));
    }

    painter.setPen(QColor(155, 192, 204));
    painter.drawText(QRect(12, height() - 28, 160, 18), QStringLiteral("© 三维场景"));
}

void SceneWidget::resizeGL(int newWidth, int newHeight)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        const int pixelWidth = qMax(1, qRound(newWidth * scale));
        const int pixelHeight = qMax(1, qRound(newHeight * scale));
        m_earth->resize(pixelWidth, pixelHeight);
    }
#else
    Q_UNUSED(newWidth)
    Q_UNUSED(newHeight)
#endif
}

void SceneWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    layoutOverlays();
}

void SceneWidget::layoutOverlays()
{
    if (m_toolBar)
        m_toolBar->move(qMax(6, (width() - m_toolBar->width()) / 2), 8);
    if (m_modeBar)
        m_modeBar->move(14, qMax(8, height() - m_modeBar->height() - 12));
    if (m_scaleBar)
        m_scaleBar->move(10, qMax(8, height() - m_scaleBar->height() - 10));
    if (m_airView)
        m_airView->move(qMax(8, width() - m_airView->width() - 10),
                        qMax(8, height() - m_airView->height() - 10));
    if (m_navigator)
        m_navigator->move(qMax(8, width() - m_navigator->width() - 10), 10);
    if (m_infrared) {
        const int infraredWidth = qMax(1, width() / 4);
        const int infraredHeight = qMax(1, height() / 4);
        m_infrared->resize(infraredWidth, infraredHeight);
        m_infrared->move(0, qMax(0, height() - infraredHeight));
    }
    if (m_deploymentOverlay) {
        m_deploymentOverlay->setGeometry(rect());
        if (m_deploymentOverlay->isVisible())
            m_deploymentOverlay->raise();
    }
    raiseViewTools();
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earthErrorOverlay) {
        m_earthErrorOverlay->setGeometry(rect());
        if (m_earthErrorOverlay->isVisible())
            m_earthErrorOverlay->raise();
    }
#endif
}

void SceneWidget::mousePressEvent(QMouseEvent *event)
{
    clearMarkerActionHover();
    m_lastMouse = event->position().toPoint();
    m_markerPressPosition = event->position();
    m_markerPressButton = event->button();
    if (event->button() == Qt::LeftButton && m_deploymentOverlay
        && m_deploymentOverlay->beginControlPointDrag(event->position())) {
        event->accept();
        return;
    }
    if (m_viewLocked) {
        event->accept();
        return;
    }
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        m_earth->mousePress(
            float(event->position().x() * scale), float(event->position().y() * scale),
            earthMouseButton(event->button()));
        event->accept();
        return;
    }
#endif
    QOpenGLWidget::mousePressEvent(event);
}

void SceneWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QString clickedMarkerId;
    if (m_deploymentOverlay && m_deploymentOverlay->isVisible()
        && m_deploymentOverlay->navigationInteractionActive() && !m_markerPointEditing
        && !m_suppressMarkerRelease && event->button() == m_markerPressButton
        && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
        && (event->position() - m_markerPressPosition).manhattanLength() <= 6) {
        for (auto marker = m_deploymentMarkers.crbegin(); marker != m_deploymentMarkers.crend(); ++marker) {
            if (!marker->visible
                || (marker->associationHideFlash && !marker->associationFlashOn)
                || marker->association.value(QStringLiteral("actions")).toArray().isEmpty()) continue;
            if (markerScreenHit(this, *marker, event->position())) {
                clickedMarkerId = marker->id;
                break;
            }
        }
    }
    m_markerPressButton = Qt::NoButton;
    if (m_suppressMarkerRelease) m_suppressMarkerRelease = false;
    if (event->button() == Qt::LeftButton && m_deploymentOverlay
        && m_deploymentOverlay->endControlPointDrag()) {
        event->accept();
        return;
    }
    if (m_viewLocked) {
        event->accept();
        return;
    }
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        m_earth->mouseRelease(
            float(event->position().x() * scale), float(event->position().y() * scale),
            earthMouseButton(event->button()));
        emitView();
        if (!clickedMarkerId.isEmpty()) {
            const QPoint global = mapToGlobal(event->position().toPoint());
            if (event->button() == Qt::RightButton) {
                emit deploymentMarkerTriggered(clickedMarkerId, QStringLiteral("context"), global);
            } else {
                const int generation = ++m_markerClickGeneration;
                QTimer::singleShot(QApplication::doubleClickInterval(), this,
                    [this, clickedMarkerId, global, generation] {
                        if (generation == m_markerClickGeneration)
                            emit deploymentMarkerTriggered(clickedMarkerId, QStringLiteral("single"), global);
                    });
            }
        }
        event->accept();
        return;
    }
#endif
    QOpenGLWidget::mouseReleaseEvent(event);
    if (!clickedMarkerId.isEmpty()) {
        const QPoint global = mapToGlobal(event->position().toPoint());
        if (event->button() == Qt::RightButton) {
            emit deploymentMarkerTriggered(clickedMarkerId, QStringLiteral("context"), global);
        } else {
            const int generation = ++m_markerClickGeneration;
            QTimer::singleShot(QApplication::doubleClickInterval(), this,
                [this, clickedMarkerId, global, generation] {
                    if (generation == m_markerClickGeneration)
                        emit deploymentMarkerTriggered(clickedMarkerId, QStringLiteral("single"), global);
                });
        }
    }
}

void SceneWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::NoButton) updateMarkerActionHover(event->position());
    else clearMarkerActionHover();
    if (event->buttons().testFlag(Qt::LeftButton) && m_deploymentOverlay
        && m_deploymentOverlay->updateControlPointDrag(event->position())) {
        if (m_gradientHover)
            m_gradientHover->hide();
        event->accept();
        return;
    }
    if (m_viewLocked && event->buttons() != Qt::NoButton) {
        if (m_gradientHover)
            m_gradientHover->hide();
        event->accept();
        return;
    }
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        m_earth->mouseMove(
            float(event->position().x() * scale), float(event->position().y() * scale));
        if (event->buttons() == Qt::NoButton)
            updateGradientHover(event->position());
        else if (m_gradientHover)
            m_gradientHover->hide();
        emitView();
        event->accept();
        return;
    }
#endif
    if (event->buttons().testFlag(Qt::LeftButton)) {
        const QPoint current = event->position().toPoint();
        const QPoint delta = current - m_lastMouse;
        m_pan += delta;
        m_longitude += delta.x() * 0.00001;
        m_latitude -= delta.y() * 0.00001;
        m_lastMouse = current;
        update();
        emitView();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void SceneWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_suppressMarkerRelease = true;
    ++m_markerClickGeneration;
    if (m_deploymentOverlay && m_deploymentOverlay->isVisible()
        && m_deploymentOverlay->navigationInteractionActive() && !m_markerPointEditing) {
        for (auto marker = m_deploymentMarkers.crbegin(); marker != m_deploymentMarkers.crend(); ++marker) {
            if (!marker->visible
                || (marker->associationHideFlash && !marker->associationFlashOn)
                || marker->association.value(QStringLiteral("actions")).toArray().isEmpty()) continue;
            if (markerScreenHit(this, *marker, event->position())) {
                // A double click cancels the pending single-click external action.
                event->accept();
                return;
            }
        }
    }
    ++m_markerClickGeneration;
    if (m_viewLocked) {
        event->accept();
        return;
    }
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        m_earth->mouseDoubleClick(float(event->position().x() * scale),
            float(event->position().y() * scale), earthMouseButton(event->button()));
        event->accept();
        return;
    }
#endif
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void SceneWidget::leaveEvent(QEvent *event)
{
    clearMarkerActionHover();
    QOpenGLWidget::leaveEvent(event);
}

void SceneWidget::clearMarkerActionHover()
{
    if (m_hoveredMarkerId.isEmpty()) return;
    m_hoveredMarkerId.clear();
    m_hoveredMarkerBounds = {};
    unsetCursor();
    if (m_markerActionHint) m_markerActionHint->hide();
    if (m_deploymentOverlay) m_deploymentOverlay->refresh();
}

void SceneWidget::updateMarkerActionHover(const QPointF &position)
{
    m_markerHoverPosition = position;
    if (!m_deploymentOverlay || !m_deploymentOverlay->isVisible()
        || !m_deploymentOverlay->navigationInteractionActive() || m_markerPointEditing) {
        clearMarkerActionHover();
        return;
    }
    for (auto marker = m_deploymentMarkers.crbegin(); marker != m_deploymentMarkers.crend(); ++marker) {
        if (!marker->visible
            || (marker->associationHideFlash && !marker->associationFlashOn)) continue;
        const QRectF bounds = markerScreenBounds(this, *marker);
        if (!bounds.contains(position)) continue;
        QStringList hints;
        bool single = false;
        bool context = false;
        for (const QJsonValue &value : marker->association.value(QStringLiteral("actions")).toArray()) {
            const QJsonObject action = value.toObject();
            if (!action.value(QStringLiteral("enabled")).toBool(true)) continue;
            const QString trigger = action.value(QStringLiteral("trigger")).toString();
            if (trigger == QStringLiteral("single") || trigger == QStringLiteral("double")) single = true;
            if (trigger == QStringLiteral("context")) context = true;
        }
        if (!single && !context) continue;
        if (single) hints.append(QStringLiteral("单击执行联动"));
        if (context) hints.append(QStringLiteral("右键查看操作"));
        const bool changed = m_hoveredMarkerId != marker->id || m_hoveredMarkerBounds != bounds;
        m_hoveredMarkerId = marker->id;
        m_hoveredMarkerBounds = bounds;
        setCursor(Qt::PointingHandCursor);
        m_markerActionHint->setText(hints.join(QStringLiteral(" · ")));
        m_markerActionHint->adjustSize();
        m_markerActionHint->move(qBound(0, int(position.x()) + 16,
                qMax(0, width() - m_markerActionHint->width())),
            qBound(0, int(position.y()) + 16,
                qMax(0, height() - m_markerActionHint->height())));
        m_markerActionHint->show();
        m_markerActionHint->raise();
        if (changed) m_deploymentOverlay->refresh();
        return;
    }
    clearMarkerActionHover();
}

void SceneWidget::wheelEvent(QWheelEvent *event)
{
    if (m_viewLocked) {
        event->accept();
        return;
    }
    const int wheelDelta = event->angleDelta().y() != 0
        ? event->angleDelta().y() : event->pixelDelta().y();
    if (wheelDelta == 0) {
        event->accept(); // Ignore touchpad phase/end and horizontal-only events.
        return;
    }
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        m_earth->mouseWheel(float(event->position().x() * scale),
                            float(event->position().y() * scale),
                            wheelDelta > 0);
        emitView();
        event->accept();
        return;
    }
#endif
    m_zoom = qBound<qreal>(0.55, m_zoom * (wheelDelta > 0 ? 1.1 : 0.9), 2.2);
    update();
    emitView();
    event->accept();
}

void SceneWidget::resetView()
{
    applyHomeViewpoint(MapHomeViewpoint::flightSeconds);
}

void SceneWidget::applyHomeViewpoint(double seconds)
{
    // Like the reference Home action, restore the complete configured end
    // viewpoint, not the startup view or a pitch derived from the map mode.
    bool loaded = false;
    const MapHomeViewpoint home = loadMapHomeViewpoint(
        QDir(AppPaths::dataDir()).filePath(QStringLiteral("Config/systemConfig.xml")), &loaded);
    if (!loaded)
        qWarning() << "Map home config unavailable/invalid; using reference home viewpoint";
    SceneViewpoint viewpoint;
    viewpoint.longitude = home.longitude;
    viewpoint.latitude = home.latitude;
    viewpoint.altitude = home.altitude;
    viewpoint.heading = home.heading;
    viewpoint.pitch = home.pitch;
    viewpoint.range = home.range;
    viewpoint.valid = true;
    m_pan = {};
    m_zoom = 1.0;
    flyTo(viewpoint, seconds);
}

void SceneWidget::emitView()
{
    if (m_deploymentOverlay && m_deploymentOverlay->isVisible())
        m_deploymentOverlay->refresh();
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const FreeEarth::Viewpoint viewpoint = m_earth->viewpoint();
        if (viewpoint.valid) {
            m_longitude = viewpoint.longitude;
            m_latitude = viewpoint.latitude;
            emit viewChanged(m_longitude, m_latitude,
                             viewpoint.elevationMeters, viewpoint.rangeMeters);
            return;
        }
    }
#endif
    emit viewChanged(m_longitude, m_latitude, 0.0, 1260.0 / m_zoom);
}

void SceneWidget::applyMapMode()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        FreeEarth::Viewpoint viewpoint = m_earth->viewpoint();
        if (!viewpoint.valid) {
            viewpoint.longitude = m_longitude;
            viewpoint.latitude = m_latitude;
            viewpoint.rangeMeters = 1500000.0;
            viewpoint.valid = true;
        }
        const double pitch = m_mapMode == 0 ? -90.0 : (m_mapMode == 1 ? -62.0 : -35.0);
        viewpoint.pitchDegrees = pitch;
        m_earth->setViewpoint(viewpoint, 0.35);
    }
#endif
}

SceneViewpoint SceneWidget::currentViewpoint() const
{
    SceneViewpoint result;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const FreeEarth::Viewpoint value = m_earth->viewpoint();
        result.longitude = value.longitude;
        result.latitude = value.latitude;
        result.altitude = value.elevationMeters;
        result.heading = value.headingDegrees;
        result.pitch = value.pitchDegrees;
        result.range = value.rangeMeters;
        result.valid = value.valid;
        return result;
    }
#endif
    result.longitude = m_longitude;
    result.latitude = m_latitude;
    result.range = 1260.0 / m_zoom;
    result.valid = true;
    return result;
}

QStringList SceneWidget::runtimeLayers() const
{
    QStringList result;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const std::vector<FreeEarth::LayerInfo> layers = m_earth->layers();
        for (const FreeEarth::LayerInfo &layer : layers) {
            if (!layer.applicationManaged)
                result.append(QString::fromStdString(layer.name));
        }
    }
#endif
    return result;
}

bool SceneWidget::earthReady() const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    return m_earth && m_earth->isReady();
#else
    return false;
#endif
}

bool SceneWidget::hasElevationData() const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    return m_earth && m_earth->isReady() && m_earth->hasElevationData();
#else
    return false;
#endif
}

bool SceneWidget::hasElevationLayer() const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    return m_earth && m_earth->isReady() && m_earth->hasElevationLayer();
#else
    return false;
#endif
}

bool SceneWidget::setRuntimeLayerVisible(const QString &name, bool visible)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const bool changed = m_earth->setLayerVisible(name.toStdString(), visible);
        if (changed)
            update();
        return changed;
    }
#else
    Q_UNUSED(name)
    Q_UNUSED(visible)
#endif
    return false;
}

bool SceneWidget::setRuntimeLayerOpacity(const QString &name, double opacity)
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const bool changed = m_earth->setLayerOpacity(
            name.toStdString(), float(qBound(0.0, opacity, 1.0)));
        if (changed)
            update();
        return changed;
    }
#else
    Q_UNUSED(name)
    Q_UNUSED(opacity)
#endif
    return false;
}

bool SceneWidget::setDeploymentLayers(const QVector<DeploymentLayer> &layers,
                                      QString &errorMessage)
{
    m_deploymentLayers = layers;
    errorMessage.clear();
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        std::vector<FreeEarth::MapLayerDefinition> definitions;
        definitions.reserve(static_cast<size_t>(layers.size()));
        for (const DeploymentLayer &layer : layers) {
            if (layer.runtimeLayer || layer.source.startsWith(QStringLiteral("earth:"))
                || layer.source.startsWith(QStringLiteral("local:"))) {
                continue;
            }
            FreeEarth::MapLayerDefinition value;
            value.id = layer.id.toUtf8().toStdString();
            value.name = layer.name.toUtf8().toStdString();
            value.kind = layer.kind.toUtf8().toStdString();
            value.driver = layer.driver.toUtf8().toStdString();
            const QString runtimeSource = layer.source.contains(QStringLiteral("://"))
                ? layer.source : QDir::toNativeSeparators(layer.source);
            value.source = runtimeSource.toUtf8().toStdString();
            value.profile = layer.profile.toUtf8().toStdString();
            value.quadtreeType = layer.quadtreeType.toUtf8().toStdString();
            value.format = layer.format.toUtf8().toStdString();
            value.minLevel = layer.minLevel;
            value.maxLevel = layer.maxLevel;
            value.tileSize = layer.tileSize;
            value.color = layer.color.name(QColor::HexRgb).toStdString();
            value.visible = layer.visible;
            value.opacity = layer.opacityEnabled ? static_cast<float>(qBound(0.0, layer.opacity, 1.0)) : 1.0f;
            value.lineWidth = static_cast<float>(qMax(0.1, layer.lineWidth));
            value.cacheEnabled = layer.cacheEnabled;
            value.cacheConfigured = layer.cacheConfigured;
            // Legacy per-layer absolute cache paths must not escape this installation.
            value.cachePath = QFile::encodeName(AppPaths::earthCacheDir()).toStdString();
            value.cacheName = layer.cacheName.toUtf8().toStdString();
            value.customVectorStyle = layer.customVectorStyle;
            value.fontSize = layer.fontSize;
            value.fontColor = layer.fontColor.name(QColor::HexRgb).toStdString();
            value.fontHaloColor = layer.fontHaloColor.name(QColor::HexRgb).toStdString();
            value.pointIcon = QDir::fromNativeSeparators(layer.pointIcon).toUtf8().toStdString();
            value.styleName = layer.styleName.toUtf8().toStdString();
            value.styleCss = layer.styleCss.toUtf8().toStdString();
            value.minRange = static_cast<float>(qMax(0.0, layer.minRange));
            value.maxRange = static_cast<float>(qMax(layer.minRange, layer.maxRange));
            value.tileSizeFactor = static_cast<float>(qMax(1.0, layer.tileSizeFactor));
            value.cropFeatures = layer.cropFeatures;
            definitions.push_back(std::move(value));
        }
        std::string error;
        const bool result = m_earth->synchronizeMapLayers(definitions, error);
        errorMessage = QString::fromStdString(error);
        update();
        return result;
    }
#endif
    return true;
}

bool SceneWidget::coordinateAtScreen(const QPointF &position,
                                     GeoCoordinate &coordinate) const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        const qreal scale = devicePixelRatioF();
        return m_earth->screenToGeographic(
            float(position.x() * scale), float(position.y() * scale),
            coordinate.longitude, coordinate.latitude, coordinate.altitude);
    }
#endif
    const SceneViewpoint view = currentViewpoint();
    const double spanLatitude = qMax(0.0001, view.range / 111320.0 * 1.5);
    const double cosLatitude = qMax(0.1, std::cos(view.latitude * 3.14159265358979323846 / 180.0));
    coordinate.longitude = view.longitude
        + (position.x() / qMax(1, width()) - 0.5) * spanLatitude / cosLatitude;
    coordinate.latitude = view.latitude
        - (position.y() / qMax(1, height()) - 0.5) * spanLatitude;
    coordinate.altitude = 0.0;
    return true;
}

QRectF SceneWidget::renderedAnnotationBounds(const QString &id) const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        float left = 0.0f, top = 0.0f, width = 0.0f, height = 0.0f;
        if (m_earth->annotationScreenBounds(id.toUtf8().toStdString(),
                                           left, top, width, height)) {
            const qreal ratio = devicePixelRatioF();
            return QRectF(left / ratio, top / ratio, width / ratio, height / ratio);
        }
    }
#else
    Q_UNUSED(id);
#endif
    return {};
}

bool SceneWidget::screenPosition(const GeoCoordinate &coordinate,
                                 QPointF &position) const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        float x = 0.0f;
        float y = 0.0f;
        if (!m_earth->geographicToScreen(coordinate.longitude,
                                         coordinate.latitude,
                                         coordinate.altitude, x, y)) {
            return false;
        }
        const qreal scale = devicePixelRatioF();
        position = QPointF(x / scale, y / scale);
        return rect().adjusted(-40, -40, 40, 40).contains(position.toPoint());
    }
#endif
    const SceneViewpoint view = currentViewpoint();
    const double spanLatitude = qMax(0.0001, view.range / 111320.0 * 1.5);
    const double cosLatitude = qMax(0.1, std::cos(view.latitude * 3.14159265358979323846 / 180.0));
    position.setX((0.5 + (coordinate.longitude - view.longitude)
                         * cosLatitude / spanLatitude) * width());
    position.setY((0.5 - (coordinate.latitude - view.latitude)
                         / spanLatitude) * height());
    return rect().adjusted(-40, -40, 40, 40).contains(position.toPoint());
}

ExpandedAnnotationGeometry SceneWidget::expandedAnnotationGeometry(
    const QString &kind,
    const QVector<GeoCoordinate> &controlPoints) const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (isPlotAnnotation(kind) && m_earth && m_earth->isReady()) {
        std::vector<FreeEarth::GeographicPoint> controls;
        controls.reserve(static_cast<std::size_t>(controlPoints.size()));
        for (const GeoCoordinate &point : controlPoints) {
            controls.push_back({point.longitude, point.latitude,
                                point.altitude});
        }
        std::vector<FreeEarth::GeographicPoint> calculated;
        std::string error;
        if (m_earth->calculatePlotGeometry(kind.toUtf8().toStdString(),
                                           controls, calculated, error)) {
            ExpandedAnnotationGeometry result;
            result.geometryType = annotationIsArea(kind)
                ? QStringLiteral("polygon") : QStringLiteral("line");
            result.points.reserve(static_cast<qsizetype>(calculated.size()));
            for (const FreeEarth::GeographicPoint &point : calculated) {
                result.points.append({point.longitude, point.latitude,
                                      point.altitude});
            }
            return result;
        }
    }
#endif
    return expandAnnotationGeometry(kind, controlPoints);
}

bool SceneWidget::sampleTerrainElevations(QVector<GeoCoordinate> &points,
                                          QString &errorMessage) const
{
    ++m_terrainSampleRequestCount;
    errorMessage.clear();
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        std::vector<FreeEarth::GeographicPoint> values;
        values.reserve(static_cast<size_t>(points.size()));
        for (const GeoCoordinate &point : points)
            values.push_back({point.longitude, point.latitude, point.altitude});
        std::string error;
        if (!m_earth->sampleTerrainElevations(values, error)) {
            errorMessage = QString::fromStdString(error);
            return false;
        }
        for (qsizetype index = 0; index < points.size(); ++index)
            points[index].altitude = values.at(static_cast<size_t>(index)).altitude;
        return true;
    }
#endif
    errorMessage = QStringLiteral("三维地图运行时未就绪，无法读取高程数据");
    return false;
}

bool SceneWidget::saveSceneSnapshot(const QString &filePath,
                                    QString &errorMessage)
{
    errorMessage.clear();
    if (filePath.isEmpty()) {
        errorMessage = QStringLiteral("截图文件路径为空");
        return false;
    }
    const QPixmap snapshot = grab();
    if (snapshot.isNull() || !snapshot.save(filePath)) {
        errorMessage = QStringLiteral("无法保存地图截图：%1").arg(filePath);
        return false;
    }
    return true;
}

void SceneWidget::setViewToolsActive(bool active)
{
    m_viewToolsActive = active;
    if (m_airView)
        m_airView->setVisible(active && m_airViewRequested);
    if (m_navigator)
        m_navigator->setVisible(active && m_navigatorRequested);
    if (m_scaleBar)
        m_scaleBar->setVisible(active && m_scaleBarRequested);
    if (m_infrared)
        m_infrared->setVisible(active && m_infraredRequested);
    if (m_infraredTimer) {
        if (active && m_infraredRequested) {
            refreshInfraredPreview();
            m_infraredTimer->start();
        } else {
            m_infraredTimer->stop();
        }
    }
    layoutOverlays();
    updateViewTools();
}

void SceneWidget::setAirViewVisible(bool visible)
{
    m_airViewRequested = visible;
    if (m_airView)
        m_airView->setVisible(m_viewToolsActive && visible);
    layoutOverlays();
    updateViewTools();
}

void SceneWidget::setNavigatorVisible(bool visible)
{
    m_navigatorRequested = visible;
    if (m_navigator)
        m_navigator->setVisible(m_viewToolsActive && visible);
    layoutOverlays();
    updateViewTools();
}

void SceneWidget::setScaleBarVisible(bool visible)
{
    m_scaleBarRequested = visible;
    if (m_scaleBar)
        m_scaleBar->setVisible(m_viewToolsActive && visible);
    layoutOverlays();
    updateViewTools();
}

void SceneWidget::setInfraredVisible(bool visible)
{
    m_infraredRequested = visible;
    if (m_infrared)
        m_infrared->setVisible(m_viewToolsActive && visible);
    if (m_infraredTimer) {
        if (m_viewToolsActive && visible) {
            refreshInfraredPreview();
            m_infraredTimer->start();
        } else {
            m_infraredTimer->stop();
        }
    }
    layoutOverlays();
}

void SceneWidget::setViewLocked(bool locked)
{
    if (m_viewLocked == locked)
        return;
    m_viewLocked = locked;
    if (locked) {
        // Re-applying the current viewpoint cancels an active inertial throw
        // in EarthManipulator, so locking takes effect immediately.
        const SceneViewpoint view = currentViewpoint();
        if (view.valid)
            flyTo(view, 0.0);
    }
}

bool SceneWidget::airViewVisible() const noexcept
{
    return m_airViewRequested;
}

bool SceneWidget::navigatorVisible() const noexcept
{
    return m_navigatorRequested;
}

bool SceneWidget::scaleBarVisible() const noexcept
{
    return m_scaleBarRequested;
}

bool SceneWidget::infraredVisible() const noexcept
{
    return m_infraredRequested;
}

bool SceneWidget::viewLocked() const noexcept
{
    return m_viewLocked;
}

void SceneWidget::updateViewTools()
{

    if (!m_viewToolsActive)
        return;

    const SceneViewpoint view = currentViewpoint();
    if (view.valid) {
        if (m_airViewRequested && m_airView)
            m_airView->setViewpoint(view.longitude, view.latitude,
                                    view.heading, view.range);
        if (m_navigatorRequested && m_navigator)
        {
            m_navigator->setHeading(view.heading);
            m_navigator->setRangeMeters(view.range);
        }
    }

    if (!m_scaleBarRequested || !m_scaleBar)
        return;

    const qreal barX = m_scaleBar->x() + ScaleBarOverlay::sampleStartX();
    const qreal barY = m_scaleBar->y() + 35;
    GeoCoordinate start;
    GeoCoordinate end;
    double distance = 0.0;
    if (coordinateAtScreen(QPointF(barX, barY), start)
        && coordinateAtScreen(
            QPointF(barX + ScaleBarOverlay::segmentWidth(), barY), end)) {
        distance = geographicDistanceMeters(start.longitude, start.latitude,
                                            end.longitude, end.latitude);
    }
    if ((!std::isfinite(distance) || distance <= 0.0) && view.valid) {
        const double verticalFieldOfView = 30.0 * 3.14159265358979323846 / 180.0;
        const double metersPerPixel = 2.0 * qMax(1.0, view.range)
            * std::tan(verticalFieldOfView / 2.0) / qMax(1, height());
        distance = metersPerPixel * ScaleBarOverlay::segmentWidth();
    }
    m_scaleBar->setSegmentDistance(distance);
}

void SceneWidget::refreshInfraredPreview()
{
    if (!m_viewToolsActive || !m_infraredRequested || !m_infrared
        || width() <= 0 || height() <= 0) {
        return;
    }
    const QImage frame = grabFramebuffer();
    if (frame.isNull())
        return;
    const QSize target = m_infrared->size();
    const QImage scaled = frame.scaled(target.expandedTo(QSize(1, 1)),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    m_infrared->setFrame(makeInfraredImage(scaled, 0.6));
}

void SceneWidget::applyNavigatorCommand(NavigatorCommand command)
{
    if (m_viewLocked)
        return;
    SceneViewpoint view = currentViewpoint();
    if (!view.valid)
        return;

    const double latitudeStep = qBound(0.00001,
        qMax(1.0, view.range) / 111320.0 * 0.08, 15.0);
    const double cosineLatitude = qMax(0.1,
        std::cos(view.latitude * 3.14159265358979323846 / 180.0));
    const double longitudeStep = latitudeStep / cosineLatitude;
    switch (command) {
    case NavigatorCommand::North:
        view.heading = 0.0;
        break;
    case NavigatorCommand::PanNorth:
        view.latitude = qMin(89.9, view.latitude + latitudeStep);
        break;
    case NavigatorCommand::PanSouth:
        view.latitude = qMax(-89.9, view.latitude - latitudeStep);
        break;
    case NavigatorCommand::PanWest:
        view.longitude -= longitudeStep;
        break;
    case NavigatorCommand::PanEast:
        view.longitude += longitudeStep;
        break;
    case NavigatorCommand::RotateLeft:
        view.heading -= 5.0;
        break;
    case NavigatorCommand::RotateRight:
        view.heading += 5.0;
        break;
    case NavigatorCommand::PitchUp:
        view.pitch = qMin(-5.0, view.pitch + 3.0);
        break;
    case NavigatorCommand::PitchDown:
        view.pitch = qMax(-90.0, view.pitch - 3.0);
        break;
    case NavigatorCommand::ZoomIn:
        view.range = qMax(10.0, view.range * 0.78);
        break;
    case NavigatorCommand::ZoomOut:
        view.range = qMin(100000000.0, view.range * 1.28);
        break;
    }
    while (view.longitude > 180.0)
        view.longitude -= 360.0;
    while (view.longitude < -180.0)
        view.longitude += 360.0;
    while (view.heading >= 360.0)
        view.heading -= 360.0;
    while (view.heading < 0.0)
        view.heading += 360.0;
    view.valid = true;
    flyTo(view, 0.0);
    updateViewTools();
}

void SceneWidget::raiseViewTools()
{
    if (m_infrared && m_infrared->isVisible())
        m_infrared->raise();
    if (m_airView && m_airView->isVisible())
        m_airView->raise();
    if (m_scaleBar && m_scaleBar->isVisible())
        m_scaleBar->raise();
    if (m_navigator && m_navigator->isVisible())
        m_navigator->raise();
    if (m_zoomFocus && m_zoomFocus->isVisible())
        m_zoomFocus->raise();
    if (m_gradientHover && m_gradientHover->isVisible())
        m_gradientHover->raise();
}

void SceneWidget::updateZoomFocus()
{
    if (!m_zoomFocus)
        return;
#if defined(ZQXXXT_ENABLE_EARTH)
    float x = 0.0f, y = 0.0f;
    if (m_earth && m_earth->zoomFocusScreenPosition(x, y)) {
        const qreal scale = devicePixelRatioF();
        m_zoomFocus->setFocusPosition(QPointF(x / scale, y / scale));
        if (!m_zoomFocus->isVisible()) {
            m_zoomFocus->show();
            m_zoomFocus->raise();
        }
        return;
    }
#endif
    m_zoomFocus->hide();
}

void SceneWidget::updateGradientHover(const QPointF &position)
{
    if (!m_gradientHover)
        return;
    GeoCoordinate cursor;
    if (!coordinateAtScreen(position, cursor)) {
        m_gradientHover->hide();
        return;
    }
    constexpr int GridSize = 50;
    constexpr double Pi = 3.14159265358979323846;
    for (auto iterator = m_deploymentMeasurements.crbegin();
         iterator != m_deploymentMeasurements.crend(); ++iterator) {
        const DeploymentMeasurement &measurement = *iterator;
        if (!measurement.visible
            || measurement.kind != QStringLiteral("gradient-analysis")
            || measurement.points.size() < 2
            || measurement.analysisPoints.size() != GridSize * GridSize) {
            continue;
        }
        const GeoCoordinate &first = measurement.points.constFirst();
        const GeoCoordinate &last = measurement.points.constLast();
        const double longitudeSpan = last.longitude - first.longitude;
        const double latitudeSpan = last.latitude - first.latitude;
        if (std::abs(longitudeSpan) < 1e-12 || std::abs(latitudeSpan) < 1e-12)
            continue;
        const double x = (cursor.longitude - first.longitude) / longitudeSpan;
        const double y = (cursor.latitude - first.latitude) / latitudeSpan;
        if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0)
            continue;
        const int column = std::clamp(qRound(x * (GridSize - 1)), 0, GridSize - 1);
        const int row = std::clamp(qRound(y * (GridSize - 1)), 0, GridSize - 1);
        const auto pointAt = [&](int sampleRow, int sampleColumn) -> const GeoCoordinate & {
            return measurement.analysisPoints.at(sampleRow * GridSize + sampleColumn);
        };
        const GeoCoordinate &sample = pointAt(row, column);
        const GeoCoordinate &left = pointAt(row, std::max(0, column - 1));
        const GeoCoordinate &right = pointAt(row, std::min(GridSize - 1, column + 1));
        const GeoCoordinate &down = pointAt(std::max(0, row - 1), column);
        const GeoCoordinate &up = pointAt(std::min(GridSize - 1, row + 1), column);
        double eastGradient = (right.altitude - left.altitude)
            / std::max(0.01, horizontalDistance(left, right));
        double northGradient = (up.altitude - down.altitude)
            / std::max(0.01, horizontalDistance(down, up));
        if (right.longitude < left.longitude)
            eastGradient = -eastGradient;
        if (up.latitude < down.latitude)
            northGradient = -northGradient;
        const double gradient = std::hypot(eastGradient, northGradient);
        const double slopeDegrees = std::atan(gradient) * 180.0 / Pi;
        double aspectDegrees = 0.0;
        if (gradient > 1e-12) {
            aspectDegrees = std::atan2(-eastGradient, -northGradient) * 180.0 / Pi;
            if (aspectDegrees < 0.0)
                aspectDegrees += 360.0;
        }
        m_gradientHover->setText(QStringLiteral(
            "经度: %1度\n纬度: %2度\n高度: %3米\n坡度: %4度\n坡向: %5度")
            .arg(cursor.longitude, 0, 'f', 6)
            .arg(cursor.latitude, 0, 'f', 6)
            .arg(sample.altitude, 0, 'f', 4)
            .arg(slopeDegrees, 0, 'f', 5)
            .arg(aspectDegrees, 0, 'f', 4));
        m_gradientHover->adjustSize();
        int leftPosition = qRound(position.x()) + 12;
        int topPosition = qRound(position.y()) + 12;
        if (leftPosition + m_gradientHover->width() > width() - 4)
            leftPosition = qMax(4, qRound(position.x()) - m_gradientHover->width() - 12);
        if (topPosition + m_gradientHover->height() > height() - 4)
            topPosition = qMax(4, qRound(position.y()) - m_gradientHover->height() - 12);
        m_gradientHover->move(leftPosition, topPosition);
        m_gradientHover->show();
        m_gradientHover->raise();
        return;
    }
    m_gradientHover->hide();
}

void SceneWidget::setDeploymentMarkers(const QVector<DeploymentMarker> &markers)
{
    m_deploymentMarkers = markers;
    for (auto &marker : m_deploymentMarkers) {
        QJsonArray actions;
        for (const auto &value : marker.association.value("actions").toArray()) {
            const auto action = MarkerActions::normalize(value.toObject());
            if (!action.isEmpty() && action.value("enabled").toBool(true)) actions.append(action);
        }
        marker.association = {{"type", "NONE"}, {"actions", actions}};
    }
    synchronizeEarthAnnotations();
    if (!m_hoveredMarkerId.isEmpty()) {
        // Rebuilt annotations acquire their bounds in the next draw. Keep the
        // hover until then so live text updates do not lose a stationary cursor.
        if (earthReady()) update();
        else updateMarkerActionHover(mapFromGlobal(QCursor::pos()));
    }
    if (m_deploymentOverlay)
        m_deploymentOverlay->refresh();
}

void SceneWidget::setAssociationFlashPhase(bool on)
{
    for (DeploymentMarker &marker : m_deploymentMarkers) {
        marker.associationFlashOn = on;
#if defined(ZQXXXT_ENABLE_EARTH)
        if (marker.associationHideFlash && m_earth && m_earth->isReady())
            m_earth->setAnnotationVisible(marker.id.toUtf8().toStdString(), marker.visible && on);
#endif
    }
    if (m_deploymentOverlay) m_deploymentOverlay->refresh();
}

void SceneWidget::beginMarkerPointEditing(
    const DeploymentMarker &marker,
    std::function<void(const QVector<GeoCoordinate> &)> pointsChanged)
{
    m_markerEditPreview = marker;
    if (m_markerEditPreview.points.isEmpty())
        m_markerEditPreview.points.append(m_markerEditPreview.position);
    m_markerEditPreview.position = m_markerEditPreview.points.constFirst();
    m_markerEditPointsChanged = std::move(pointsChanged);
    m_markerPointEditing = true;
    synchronizeEarthAnnotations();
    if (m_deploymentOverlay)
        m_deploymentOverlay->refresh();
}

void SceneWidget::endMarkerPointEditing()
{
    if (!m_markerPointEditing)
        return;
    m_markerPointEditing = false;
    m_markerEditPointsChanged = {};
    synchronizeEarthAnnotations();
    if (m_deploymentOverlay) {
        (void)m_deploymentOverlay->endControlPointDrag();
        m_deploymentOverlay->refresh();
    }
}

void SceneWidget::synchronizeEarthAnnotations()
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (!m_earth || !m_earth->isReady())
        return;
    QVector<DeploymentMarker> markers = m_deploymentMarkers;
    if (m_markerPointEditing) {
        auto existing = std::find_if(markers.begin(), markers.end(), [this](const DeploymentMarker &value) {
            return value.id == m_markerEditPreview.id;
        });
        if (existing == markers.end())
            markers.append(m_markerEditPreview);
        else
            *existing = m_markerEditPreview;
    }
    std::vector<FreeEarth::MapAnnotationDefinition> definitions;
    definitions.reserve(static_cast<size_t>(markers.size()));
    const SceneViewpoint synchronizationView = currentViewpoint();
    for (const DeploymentMarker &marker : markers) {
        FreeEarth::MapAnnotationDefinition value;
        value.id = marker.id.toUtf8().toStdString();
        value.kind = marker.geometryType.toUtf8().toStdString();
        QString label = marker.associationText.isEmpty()
            ? (marker.geometryType == QStringLiteral("label") && !marker.text.isEmpty()
                ? marker.text : marker.name) : marker.associationText;
        if (marker.live && !marker.liveState.isEmpty())
            label += QStringLiteral(" · %1").arg(marker.liveState);
        value.name = marker.name.toUtf8().toStdString();
        value.text = label.toUtf8().toStdString();
        const QVector<GeoCoordinate> controls = marker.points.isEmpty()
            ? QVector<GeoCoordinate>{marker.position} : marker.points;
        const ExpandedAnnotationGeometry expanded = expandAnnotationGeometry(
            marker.geometryType, controls);
        value.type = expanded.geometryType.toUtf8().toStdString();
        QString resourcePath = marker.icon.trimmed();
        if (!resourcePath.isEmpty() && QFileInfo(resourcePath).isRelative())
            resourcePath = AppPaths::assetPath(resourcePath);
        value.resourcePath = QDir::toNativeSeparators(resourcePath)
            .toUtf8().toStdString();
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
        value.color = color.name(QColor::HexRgb).toStdString();
        const QColor fillColor = marker.associationColor.isValid()
            ? marker.associationColor : marker.fillColor.isValid() ? marker.fillColor : color;
        value.fillColor = fillColor.name(QColor::HexRgb).toStdString();
        value.visible = marker.visible;
        value.lineWidth = static_cast<float>(marker.lineWidth);
        value.lineOpacity = static_cast<float>(marker.lineOpacity);
        value.fillOpacity = static_cast<float>(marker.fillOpacity);
        double effectivePointSize = marker.pointSize;
        // Image/particle resources use map-sized values. Model and tilt
        // scales are stored directly (10 == 1.0) and are migrated by ZqbsPage.
        if (std::abs(effectivePointSize - 20.0) < 0.001) {
            const double referenceRange = marker.viewpoint.range > 0.0
                ? marker.viewpoint.range : synchronizationView.range;
            const double markSize = std::max(
                10.0, std::abs(referenceRange) / 10.0);
            if (marker.geometryType == QStringLiteral("particle"))
                effectivePointSize = markSize;
            else if (marker.geometryType == QStringLiteral("billboard")
                     || marker.geometryType == QStringLiteral("overlay"))
                effectivePointSize = markSize / 2.0;
        }
        value.pointSize = static_cast<float>(effectivePointSize);
        value.textSize = static_cast<float>(marker.textSize);
        value.rotationDegrees = static_cast<float>(
            marker.geometryType == QStringLiteral("tilt-model")
                ? marker.tiltRotationDegrees : 0.0);
        value.tiltSourceType = marker.tiltSourceType;
        value.modelSourceType = marker.modelSourceType;
        value.modelRotationXDegrees = static_cast<float>(
            marker.geometryType == QStringLiteral("model")
                ? marker.modelRotationXDegrees : 0.0);
        value.modelRotationYDegrees = static_cast<float>(
            marker.geometryType == QStringLiteral("model")
                ? marker.modelRotationYDegrees : 0.0);
        value.modelRotationZDegrees = static_cast<float>(
            marker.geometryType == QStringLiteral("model")
                ? marker.modelRotationZDegrees : 0.0);
        value.modelLodLowMeters = marker.modelLodLowMeters;
        value.modelLodHighMeters = marker.modelLodHighMeters;
        value.labelWidth = static_cast<float>(marker.labelWidth);
        value.labelHeight = static_cast<float>(marker.labelHeight);
        QString labelBackground = marker.labelBackgroundImage.trimmed();
        if (!labelBackground.isEmpty() && QFileInfo(labelBackground).isRelative())
            labelBackground = AppPaths::assetPath(labelBackground);
        value.labelBackgroundImage = QDir::toNativeSeparators(labelBackground)
            .toUtf8().toStdString();
        value.labelTextColor = marker.labelTextColor.name(QColor::HexRgb)
            .toStdString();
        value.labelBackgroundColor = (marker.associationColor.isValid()
            ? marker.associationColor : marker.labelBackgroundColor).name(QColor::HexRgb)
            .toStdString();
        value.labelBackgroundOpacity = static_cast<float>(
            marker.labelBackgroundColor.alphaF());
        value.labelAlignment = marker.labelAlignment;
        for (const DeploymentLabelItem &item : marker.labelItems) {
            if (item.text.trimmed().isEmpty()
                || isInternalLabelPlaceholder(item.text)) {
                continue;
            }
            FreeEarth::MapLabelItemDefinition row;
            row.text = item.text.toUtf8().toStdString();
            QString background = item.backgroundImage.trimmed();
            if (!background.isEmpty() && QFileInfo(background).isRelative())
                background = AppPaths::assetPath(background);
            row.backgroundImage = QDir::toNativeSeparators(background)
                .toUtf8().toStdString();
            row.textColor = item.textColor.name(QColor::HexRgb).toStdString();
            row.backgroundColor = item.backgroundColor.name(QColor::HexRgb)
                .toStdString();
            row.width = static_cast<float>(item.width);
            row.height = static_cast<float>(item.height);
            row.textSize = static_cast<float>(item.textSize);
            row.backgroundOpacity = static_cast<float>(item.backgroundColor.alphaF());
            row.alignment = item.alignment;
            value.labelItems.push_back(std::move(row));
        }
        value.altitudeMode = marker.altitudeMode;
        // Reference FePlots and FeExtNode objects both require the original
        // control points. Passing the preview polygon here was the reason
        // different plot/mark types collapsed to generic rectangles.
        for (const GeoCoordinate &point : controls) {
            value.points.push_back({point.longitude, point.latitude,
                                    point.altitude});
        }
        definitions.push_back(std::move(value));
    }
    const auto appendPoints = [](FreeEarth::MapAnnotationDefinition &definition,
                                 const QVector<GeoCoordinate> &points) {
        for (const GeoCoordinate &point : points)
            definition.points.push_back({point.longitude, point.latitude, point.altitude});
    };
    const auto rectanglePoints = [](const QVector<GeoCoordinate> &points) {
        QVector<GeoCoordinate> result;
        if (points.size() < 2)
            return result;
        const GeoCoordinate &a = points.constFirst();
        const GeoCoordinate &b = points.constLast();
        result = {{a.longitude, a.latitude, a.altitude},
                  {b.longitude, a.latitude, a.altitude},
                  {b.longitude, b.latitude, b.altitude},
                  {a.longitude, b.latitude, b.altitude}};
        return result;
    };
    for (const DeploymentMeasurement &measurement : m_deploymentMeasurements) {
        if (!measurement.visible || measurement.points.size() < 2)
            continue;
        const QString prefix = QStringLiteral("measurement:%1").arg(measurement.id);
        QVector<GeoCoordinate> geometry = measurement.points;
        const bool rectangular = measurement.kind == QStringLiteral("gradient-analysis")
            || measurement.kind == QStringLiteral("contour-measure");
        if (rectangular)
            geometry = rectanglePoints(measurement.points);
        if (measurement.kind == QStringLiteral("contour-measure")) {
            for (GeoCoordinate &point : geometry)
                point.altitude = 0.0;
        }

        FreeEarth::MapAnnotationDefinition base;
        base.id = (prefix + QStringLiteral(":geometry")).toUtf8().toStdString();
        base.kind = "measurement";
        base.name.clear();
        base.text.clear();
        base.type = (measurementIsArea(measurement.kind) || rectangular)
            ? "polygon" : "line";
        base.color = measurement.lineColor.name(QColor::HexRgb).toStdString();
        base.fillColor = measurement.fillColor.name(QColor::HexRgb).toStdString();
        base.lineWidth = static_cast<float>(measurement.lineWidth);
        base.lineOpacity = static_cast<float>(measurement.lineOpacity);
        base.fillOpacity = static_cast<float>(measurement.fillOpacity);
        // The legacy measurements use pixel rasterized lines. osgEarth's
        // modern expanded stroke doubles the requested width on this build.
        base.useLegacyGlLines = true;
        if (measurement.kind == QStringLiteral("surface-distance")) {
            base.useLegacyGlLines = true;
            base.altitudeTechnique = FreeEarth::GeometryAltitudeTechnique::Gpu;
            base.tessellationSegments = 150;
            base.geographicInterpolation = 1;
        } else if (measurement.kind == QStringLiteral("space-area")
                   || measurement.kind == QStringLiteral("flood-analysis")
                   || measurement.kind == QStringLiteral("fill-cut-analysis")) {
            base.useLegacyGlLines = true;
            base.altitudeTechnique = FreeEarth::GeometryAltitudeTechnique::Drape;
            base.geographicInterpolation = 1;
            if (measurement.kind != QStringLiteral("space-area"))
                base.tessellationSegments = 150;
        } else if (measurement.kind == QStringLiteral("contour-measure")) {
            base.useLegacyGlLines = true;
            base.altitudeTechnique = FreeEarth::GeometryAltitudeTechnique::Gpu;
            base.tessellationSegments = 100;
            base.geographicInterpolation = 1;
            base.renderBin = 100;
        }
        if (measurement.kind == QStringLiteral("gradient-analysis")
            || measurement.kind == QStringLiteral("contour-measure"))
            base.fillOpacity = 0.0f;
        if (measurement.kind == QStringLiteral("line-visible")
            || measurement.kind == QStringLiteral("radial-line-visible")) {
            base.lineOpacity = 0.0f;
            base.fillOpacity = 0.0f;
        }
        base.altitudeMode = measurement.kind == QStringLiteral("surface-distance")
            || measurement.kind == QStringLiteral("profile-analysis")
            || measurement.kind == QStringLiteral("space-area")
            || measurement.kind == QStringLiteral("gradient-analysis")
            || measurement.kind == QStringLiteral("flood-analysis")
            || measurement.kind == QStringLiteral("fill-cut-analysis")
            || measurement.kind == QStringLiteral("contour-measure") ? 0 : 2;
        const int baseAltitudeMode = base.altitudeMode;
        appendPoints(base, geometry);
        if (measurement.kind != QStringLiteral("line-visible")
            && measurement.kind != QStringLiteral("radial-line-visible")) {
            definitions.push_back(std::move(base));
        }

        if (measurement.kind == QStringLiteral("gradient-analysis")
            && measurement.analysisPoints.size() == 50 * 50) {
            FreeEarth::MapAnnotationDefinition gradient;
            gradient.id = (prefix + QStringLiteral(":gradient-grid"))
                .toUtf8().toStdString();
            gradient.name.clear();
            gradient.kind = "measurement-gradient";
            gradient.type = "gradient-grid";
            gradient.fillOpacity = 0.50f;
            gradient.altitudeMode = 2;
            appendPoints(gradient, measurement.analysisPoints);
            definitions.push_back(std::move(gradient));
        }

        // Flood and fill/cut retain the terrain-clamped selection boundary.
        // Flood adds CFloodSurface's clipped animated water; fill/cut uses the
        // old result's static blue reference plane (it never used water).
        if (measurement.kind == QStringLiteral("flood-analysis")
            || measurement.kind == QStringLiteral("fill-cut-analysis")) {
            QVector<GeoCoordinate> planePoints = geometry;
            const double planeHeight = measurement.kind == QStringLiteral("flood-analysis")
                ? measurement.secondaryParameter : measurement.parameter;
            for (GeoCoordinate &point : planePoints)
                point.altitude = planeHeight;
            if (measurement.kind == QStringLiteral("flood-analysis")) {
                if (measurement.analysisSurfaceVisible
                    && !measurement.analysisPoints.isEmpty()) {
                    double minimum = std::numeric_limits<double>::max();
                    double maximum = std::numeric_limits<double>::lowest();
                    for (const GeoCoordinate &sample : measurement.analysisPoints) {
                        minimum = std::min(minimum, sample.altitude);
                        maximum = std::max(maximum, sample.altitude);
                    }
                    FreeEarth::MapAnnotationDefinition water;
                    water.id = (prefix + QStringLiteral(":water-surface"))
                        .toUtf8().toStdString();
                    water.kind = "measurement-water-surface";
                    water.type = "polygon";
                    water.resourcePath = "texture/river";
                    // CFloodSurface's exact vertical border constant. Its wall
                    // colour is installed by the runtime; the old wave shader
                    // has no explicit surface tint/colorMap asset.
                    water.color = "#2dbb9f";
                    water.lineWidth = 2.0f;
                    water.lineOpacity = 1.0f;
                    water.fillOpacity = 1.0f;
                    water.altitudeMode = 2;
                    water.minimumHeight = minimum;
                    water.maximumHeight = maximum;
                    water.borderVisible = measurement.showAnalysisBorder;
                    appendPoints(water, planePoints);
                    definitions.push_back(std::move(water));
                }
            } else {
                FreeEarth::MapAnnotationDefinition plane;
                plane.id = (prefix + QStringLiteral(":analysis-plane"))
                    .toUtf8().toStdString();
                plane.kind = "measurement-analysis";
                plane.type = "polygon";
                plane.color = "#80e680";
                plane.fillColor = "#0000e6";
                plane.lineWidth = static_cast<float>(measurement.analysisLineWidth);
                plane.lineOpacity = static_cast<float>(measurement.analysisOpacity);
                plane.fillOpacity = 0.50f;
                plane.altitudeMode = 2;
                plane.useLegacyGlLines = true;
                plane.tessellationSegments = 150;
                plane.geographicInterpolation = 1;
                appendPoints(plane, planePoints);
                definitions.push_back(std::move(plane));
                for (qsizetype index = 0; index < geometry.size(); ++index) {
                    FreeEarth::MapAnnotationDefinition side;
                    side.id = (prefix + QStringLiteral(":fill-side:%1").arg(index))
                        .toUtf8().toStdString();
                    side.kind = "measurement-analysis";
                    side.type = "line";
                    side.color = "#80e680";
                    side.lineWidth = static_cast<float>(measurement.analysisLineWidth);
                    side.lineOpacity = static_cast<float>(measurement.analysisOpacity);
                    side.altitudeMode = 2;
                    side.useLegacyGlLines = true;
                    side.tessellationSegments = 150;
                    side.geographicInterpolation = 1;
                    appendPoints(side, {geometry.at(index), planePoints.at(index)});
                    definitions.push_back(std::move(side));
                }
            }
        }

        QVector<GeoCoordinate> analysisSegments = measurement.analysisSegments;
        if (measurement.kind == QStringLiteral("height-distance")
            && analysisSegments.isEmpty() && measurement.points.size() >= 2) {
            const GeoCoordinate &first = measurement.points.constFirst();
            const GeoCoordinate &last = measurement.points.constLast();
            GeoCoordinate corner = last.altitude <= first.altitude ? last : first;
            corner.altitude = std::max(first.altitude, last.altitude);
            analysisSegments = {first, corner, corner, last};
        }
        if (measurement.kind == QStringLiteral("line-visible")
            && analysisSegments.size() >= 2) {
            FreeEarth::MapAnnotationDefinition sightLines;
            sightLines.id = (prefix + QStringLiteral(":sight-lines"))
                .toUtf8().toStdString();
            sightLines.kind = "measurement-line-of-sight";
            sightLines.type = "colored-lines";
            sightLines.lineWidth = static_cast<float>(measurement.analysisLineWidth);
            sightLines.lineOpacity = static_cast<float>(measurement.analysisOpacity);
            sightLines.altitudeMode = 2;
            appendPoints(sightLines, analysisSegments);
            for (int state : measurement.analysisSegmentStates)
                sightLines.segmentStates.push_back(state);
            definitions.push_back(std::move(sightLines));
        }

        if (measurement.kind == QStringLiteral("radial-line-visible")) {
            const ViewshedAnalysis viewshed = analyzeViewshedTerrain(
                measurement.analysisPoints, ViewshedRayCount,
                ViewshedDetectCount);
            QVector<GeoCoordinate> radialSegments = viewshed.valid
                ? viewshed.radialSegments : analysisSegments;
            QVector<int> radialStates = viewshed.valid
                ? viewshed.radialSegmentStates
                : measurement.analysisSegmentStates;
            QVector<int> raySegmentCounts = viewshed.valid
                ? viewshed.raySegmentCounts : QVector<int>{};
            QVector<GeoCoordinate> perimeter;

            if (viewshed.valid) {
                perimeter.reserve(ViewshedRayCount + 1);
                for (int ray = 0; ray < ViewshedRayCount; ++ray) {
                    perimeter.append(measurement.analysisPoints.at(
                        ray * (ViewshedDetectCount + 1)
                        + ViewshedDetectCount));
                }
            } else if (!radialSegments.isEmpty()) {
                // Older compatible XML stores only one or two segments per
                // ray. Recover grouping from the observer start coordinate;
                // never invent a zero-height perimeter when saved terrain is
                // unavailable.
                const GeoCoordinate observer = radialSegments.constFirst();
                int currentCount = 0;
                for (qsizetype index = 0; index + 1 < radialSegments.size();
                     index += 2) {
                    const GeoCoordinate &from = radialSegments.at(index);
                    const bool startsRay = std::abs(from.longitude - observer.longitude) < 1e-8
                        && std::abs(from.latitude - observer.latitude) < 1e-8;
                    if (startsRay && currentCount > 0) {
                        raySegmentCounts.append(currentCount);
                        perimeter.append(radialSegments.at(index - 1));
                        currentCount = 0;
                    }
                    ++currentCount;
                }
                if (currentCount > 0) {
                    raySegmentCounts.append(currentCount);
                    perimeter.append(radialSegments.constLast());
                }
            }

            if (perimeter.size() >= 3) {
                perimeter.append(perimeter.constFirst());
                FreeEarth::MapAnnotationDefinition outline;
                outline.id = (prefix + QStringLiteral(":viewshed-perimeter"))
                    .toUtf8().toStdString();
                outline.kind = "measurement-viewshed-perimeter";
                outline.type = "line";
                outline.color = "#ffff00";
                outline.lineWidth = 3.0f;
                outline.lineOpacity = 1.0f;
                // RadialLineOfSightNode's yellow ring is terrain clamped.
                outline.altitudeMode = 0;
                outline.useLegacyGlLines = true;
                outline.altitudeTechnique =
                    FreeEarth::GeometryAltitudeTechnique::Scene;
                outline.tessellationSegments = 0;
                outline.geographicInterpolation = 0;
                outline.depthWrite = false;
                appendPoints(outline, perimeter);
                definitions.push_back(std::move(outline));
            }

            if (!radialSegments.isEmpty() && !raySegmentCounts.isEmpty()) {
                FreeEarth::MapAnnotationDefinition sweep;
                sweep.id = (prefix + QStringLiteral(":viewshed-sweep"))
                    .toUtf8().toStdString();
                sweep.kind = "measurement-viewshed";
                sweep.type = "viewshed";
                sweep.lineWidth = 2.0f;
                sweep.lineOpacity = static_cast<float>(measurement.analysisOpacity);
                sweep.altitudeMode = 2;
                appendPoints(sweep, radialSegments);
                for (int state : radialStates)
                    sweep.segmentStates.push_back(state);
                for (int count : raySegmentCounts)
                    sweep.raySegmentCounts.push_back(
                        static_cast<unsigned int>(std::max(0, count)));
                if (viewshed.valid) {
                    appendPoints(sweep, viewshed.circularSegments);
                    for (int state : viewshed.circularSegmentStates)
                        sweep.segmentStates.push_back(state);
                    sweep.sweepStartRay = static_cast<unsigned int>(
                        viewshedSweepStartRay(measurement.analysisPoints,
                            measurement.points.constLast(), ViewshedRayCount,
                            ViewshedDetectCount));
                } else {
                    int nearest = 0;
                    double nearestDistance = std::numeric_limits<double>::max();
                    for (qsizetype index = 0; index + 1 < perimeter.size(); ++index) {
                        const double value = horizontalDistance(
                            perimeter.at(index), measurement.points.constLast());
                        if (value < nearestDistance) {
                            nearestDistance = value;
                            nearest = int(index);
                        }
                    }
                    sweep.sweepStartRay = static_cast<unsigned int>(nearest);
                }
                definitions.push_back(std::move(sweep));
            }
        }

        if (measurement.kind != QStringLiteral("line-visible")
            && measurement.kind != QStringLiteral("radial-line-visible")) {
        for (qsizetype index = 1; index < analysisSegments.size(); index += 2) {
            FreeEarth::MapAnnotationDefinition segment;
            segment.id = (prefix + QStringLiteral(":analysis:%1").arg(index / 2))
                .toUtf8().toStdString();
            segment.kind = "measurement-analysis";
            segment.type = "line";
            segment.name.clear();
            segment.text.clear();
            const int stateIndex = static_cast<int>(index / 2);
            const int state = stateIndex < measurement.analysisSegmentStates.size()
                ? measurement.analysisSegmentStates.at(stateIndex) : 1;
            const QColor segmentColor = state == 0
                ? QColor(QStringLiteral("#ff0000")) : measurement.analysisColor;
            segment.color = segmentColor.name(QColor::HexRgb).toStdString();
            segment.lineWidth = static_cast<float>(measurement.analysisLineWidth);
            segment.lineOpacity = static_cast<float>(measurement.analysisOpacity);
            segment.altitudeMode = measurement.kind == QStringLiteral("contour-measure")
                ? 0 : 2;
            segment.useLegacyGlLines = true;
            if (measurement.kind == QStringLiteral("contour-measure")) {
                segment.useLegacyGlLines = true;
                segment.altitudeTechnique =
                    FreeEarth::GeometryAltitudeTechnique::Gpu;
                segment.renderBin = 100;
            }
            appendPoints(segment, {analysisSegments.at(index - 1),
                                   analysisSegments.at(index)});
            definitions.push_back(std::move(segment));
        }
        }
        if (measurement.kind == QStringLiteral("contour-measure")) {
            QSet<qint64> labelledLevels;
            for (qsizetype index = 1; index < analysisSegments.size(); index += 2) {
                const GeoCoordinate &anchor = analysisSegments.at(index - 1);
                const qint64 level = qRound64(anchor.altitude);
                if (labelledLevels.contains(level))
                    continue;
                labelledLevels.insert(level);
                FreeEarth::MapAnnotationDefinition label;
                label.id = (prefix + QStringLiteral(":contour-label:%1").arg(level))
                    .toUtf8().toStdString();
                label.kind = "measurement-contour-label";
                label.type = "label";
                label.text = QString::number(level).toUtf8().toStdString();
                label.color = "#ffffff";
                label.labelTextColor = "#ffffff";
                label.textSize = 17.0f;
                label.altitudeMode = 0;
                appendPoints(label, {anchor});
                definitions.push_back(std::move(label));
            }
        }
        if (measurement.kind == QStringLiteral("line-visible")
            && !measurement.points.isEmpty()) {
            const auto samePoint = [](const GeoCoordinate &a, const GeoCoordinate &b) {
                return std::abs(a.longitude - b.longitude) < 1e-8
                    && std::abs(a.latitude - b.latitude) < 1e-8;
            };
            const auto appendInfo = [&](qsizetype index, const GeoCoordinate &point,
                                        const QString &text) {
                FreeEarth::MapAnnotationDefinition label;
                label.id = (prefix + QStringLiteral(":sight-info:%1").arg(index))
                    .toUtf8().toStdString();
                label.kind = "measurement-info";
                label.type = "label";
                label.text = text.toUtf8().toStdString();
                label.color = "#ffffff";
                label.labelTextColor = "#ffffff";
                label.textSize = 15.0f;
                label.altitudeMode = 2;
                appendPoints(label, {point});
                definitions.push_back(std::move(label));
            };
            const GeoCoordinate observer = analysisSegments.isEmpty()
                ? GeoCoordinate{measurement.points.constFirst().longitude,
                                measurement.points.constFirst().latitude,
                                measurement.points.constFirst().altitude
                                    + LineOfSightObserverHeightMeters}
                : analysisSegments.constFirst();
            appendInfo(0, observer, QStringLiteral(
                "角色: 观察点\n经度: %1度\n纬度: %2度\n高度: %3米")
                .arg(observer.longitude, 0, 'f', 3)
                .arg(observer.latitude, 0, 'f', 3)
                .arg(observer.altitude, 0, 'f', 2));
            qsizetype segmentIndex = 0;
            for (qsizetype targetIndex = 1; targetIndex < measurement.points.size();
                 ++targetIndex) {
                const GeoCoordinate &targetControl = measurement.points.at(targetIndex);
                GeoCoordinate target = targetControl;
                double visibleDistance = 0.0;
                double invisibleDistance = 0.0;
                while (segmentIndex + 1 < analysisSegments.size()) {
                    const int stateIndex = static_cast<int>(segmentIndex / 2);
                    const GeoCoordinate &from = analysisSegments.at(segmentIndex);
                    const GeoCoordinate &to = analysisSegments.at(segmentIndex + 1);
                    const double length = polylineDistance({from, to}, true);
                    const int state = stateIndex < measurement.analysisSegmentStates.size()
                        ? measurement.analysisSegmentStates.at(stateIndex) : 1;
                    if (state == 0) invisibleDistance += length;
                    else visibleDistance += length;
                    segmentIndex += 2;
                    target = to;
                    if (samePoint(to, targetControl))
                        break;
                }
                appendInfo(targetIndex, target, QStringLiteral(
                    "角色: 被观察点\n经度: %1度\n纬度: %2度\n高度: %3米\n可视距离: %4米\n不可视距离: %5米")
                    .arg(target.longitude, 0, 'f', 3)
                    .arg(target.latitude, 0, 'f', 3)
                    .arg(target.altitude, 0, 'f', 2)
                    .arg(visibleDistance, 0, 'f', 2)
                    .arg(invisibleDistance, 0, 'f', 2));
            }
        }
        if (measurement.kind == QStringLiteral("radial-line-visible")
            && measurement.points.size() >= 2) {
            GeoCoordinate anchor = measurement.points.constFirst();
            anchor.altitude += 3.0;
            const double radius = horizontalDistance(
                measurement.points.constFirst(), measurement.points.constLast());
            FreeEarth::MapAnnotationDefinition label;
            label.id = (prefix + QStringLiteral(":viewshed-info")).toUtf8().toStdString();
            label.kind = "measurement-info";
            label.type = "label";
            label.text = QStringLiteral(
                "视高: %1米\n半径: %2米\n经度: %3度\n纬度: %4度")
                .arg(anchor.altitude, 0, 'f', 3)
                .arg(radius, 0, 'f', 3)
                .arg(anchor.longitude, 0, 'f', 6)
                .arg(anchor.latitude, 0, 'f', 6).toUtf8().toStdString();
            label.color = "#ffffff";
            label.labelTextColor = "#ffffff";
            label.textSize = 15.0f;
            label.altitudeMode = 2;
            appendPoints(label, {anchor});
            definitions.push_back(std::move(label));
            FreeEarth::MapAnnotationDefinition heightMarker;
            heightMarker.id = (prefix + QStringLiteral(":viewshed-height-marker"))
                .toUtf8().toStdString();
            heightMarker.kind = "measurement-viewshed-control";
            heightMarker.type = "point";
            heightMarker.color = "#ff00ff";
            heightMarker.pointSize = 7.0f;
            heightMarker.altitudeMode = 2;
            appendPoints(heightMarker, {anchor});
            definitions.push_back(std::move(heightMarker));
        }

        const bool valueLabel = measurement.kind == QStringLiteral("space-distance")
            || measurement.kind == QStringLiteral("surface-distance")
            || measurement.kind == QStringLiteral("height-distance")
            || measurement.kind == QStringLiteral("space-area")
            || measurement.kind == QStringLiteral("shadow-area");
        if (valueLabel) {
            GeoCoordinate anchor = measurement.points.constLast();
            if (measurement.kind == QStringLiteral("height-distance")) {
                const GeoCoordinate &first = measurement.points.constFirst();
                anchor = anchor.altitude <= first.altitude ? anchor : first;
                anchor.altitude = std::max(first.altitude,
                    measurement.points.constLast().altitude);
            }
            FreeEarth::MapAnnotationDefinition label;
            label.id = (prefix + QStringLiteral(":label")).toUtf8().toStdString();
            label.kind = "measurement-info";
            label.type = "label";
            label.name.clear();
            label.text = measurementMapLabel(measurement.kind, measurement.value)
                .toUtf8().toStdString();
            label.color = "#ffff00";
            label.labelTextColor = "#ffff00";
            label.textSize = 17.0f;
            label.altitudeMode = baseAltitudeMode;
            appendPoints(label, {anchor});
            definitions.push_back(std::move(label));
        }
    }
    std::string error;
    if (!m_earth->synchronizeAnnotations(definitions, error) && !error.empty())
        qWarning().noquote() << QStringLiteral("Map annotation synchronization failed: %1")
            .arg(QString::fromStdString(error));
    for (const DeploymentMarker &marker : m_deploymentMarkers) {
        if (marker.association.isEmpty()) continue;
        m_earth->setAnnotationVisible(marker.id.toUtf8().toStdString(),
            marker.visible && (!marker.associationHideFlash || marker.associationFlashOn));
    }
    update();
#endif
}

void SceneWidget::setDeploymentMeasurements(
    const QVector<DeploymentMeasurement> &measurements)
{
    m_deploymentMeasurements = measurements;
    if (m_gradientHover)
        m_gradientHover->hide();
    synchronizeEarthAnnotations();
    if (m_deploymentOverlay)
        m_deploymentOverlay->refresh();
}

void SceneWidget::setEnvironmentSettings(const EnvironmentSettings &settings)
{
    m_environment = settings;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        FreeEarth::EnvironmentOptions env;
        env.sun = settings.sunVisible; env.atmosphere = settings.atmosphereVisible;
        env.moon = settings.moonVisible; env.stars = settings.starsVisible; env.nebula = settings.nebulaVisible;
        env.ambient = settings.ambientIntensity / 100.0f;
        env.fog = settings.fogVisible; env.fogIntensity = settings.fog / 100.0f;
        env.rain = {settings.rainVisible, settings.rainGlobal, settings.rain / 100.0f,
            settings.rainLongitude, settings.rainLatitude, settings.rainRadiusMeters, settings.rainHeightMeters};
        env.snow = {settings.snowVisible, settings.snowGlobal, settings.snow / 100.0f,
            settings.snowLongitude, settings.snowLatitude, settings.snowRadiusMeters, settings.snowHeightMeters};
        QDate date = QDate::fromString(settings.calendarDate, Qt::ISODate);
        if (!date.isValid()) date = QDate::currentDate();
        env.utcSeconds = QDateTime(date, QTime(settings.timeOfDay, settings.minuteOfHour, settings.secondOfMinute)).toSecsSinceEpoch();
        env.syncSystemTime = settings.syncSystemTime; env.animateTime = settings.animateTime;
        env.timeSpeed = settings.timeSpeed; env.clockRevision = settings.clockRevision;
        m_earth->setEnvironment(env);
    }
#endif
    update();
    if (m_deploymentOverlay)
        m_deploymentOverlay->refresh();
}

QDateTime SceneWidget::environmentTime() const
{
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady())
        return QDateTime::fromSecsSinceEpoch(m_earth->environmentStatus().utcSeconds);
#endif
    QDate date = QDate::fromString(m_environment.calendarDate, Qt::ISODate);
    return QDateTime(date.isValid() ? date : QDate::currentDate(),
        QTime(m_environment.timeOfDay, m_environment.minuteOfHour, m_environment.secondOfMinute));
}

void SceneWidget::setDeploymentOverlayVisible(bool visible)
{
    if (!m_deploymentOverlay)
        return;
    m_deploymentOverlay->setVisible(visible);
    if (visible) {
        m_deploymentOverlay->setGeometry(rect());
        m_deploymentOverlay->raise();
        m_deploymentOverlay->refresh();
        raiseViewTools();
    } else {
        m_deploymentOverlay->cancelInteraction();
    }
}

void SceneWidget::beginMarkerPlacement()
{
    if (m_gradientHover)
        m_gradientHover->hide();
    if (m_deploymentOverlay)
        m_deploymentOverlay->beginMarkerPlacement();
}

void SceneWidget::beginAnnotationPlacement(const QString &kind)
{
    if (m_gradientHover)
        m_gradientHover->hide();
    if (m_deploymentOverlay)
        m_deploymentOverlay->beginAnnotationPlacement(kind);
}

void SceneWidget::beginMeasurement(const QString &kind)
{
    if (m_gradientHover)
        m_gradientHover->hide();
    if (m_deploymentOverlay)
        m_deploymentOverlay->beginMeasurement(kind);
}

void SceneWidget::beginBlackboard(const QString &tool, const QColor &color)
{
    if (m_deploymentOverlay)
        m_deploymentOverlay->beginBlackboard(tool, color);
}

void SceneWidget::setBlackboardTool(const QString &tool)
{
    if (m_deploymentOverlay)
        m_deploymentOverlay->setBlackboardTool(tool);
}

void SceneWidget::setBlackboardColor(const QColor &color)
{
    if (m_deploymentOverlay)
        m_deploymentOverlay->setBlackboardColor(color);
}

void SceneWidget::clearBlackboard()
{
    if (m_deploymentOverlay)
        m_deploymentOverlay->clearBlackboard();
}

void SceneWidget::closeBlackboard()
{
    if (m_deploymentOverlay)
        m_deploymentOverlay->closeBlackboard();
}

void SceneWidget::cancelDeploymentInteraction()
{
    if (m_deploymentOverlay)
        m_deploymentOverlay->cancelInteraction();
}

void SceneWidget::flyTo(const SceneViewpoint &viewpoint, double seconds)
{
    if (!viewpoint.valid)
        return;
    m_longitude = viewpoint.longitude;
    m_latitude = viewpoint.latitude;
#if defined(ZQXXXT_ENABLE_EARTH)
    if (m_earth && m_earth->isReady()) {
        FreeEarth::Viewpoint value;
        value.longitude = viewpoint.longitude;
        value.latitude = viewpoint.latitude;
        value.elevationMeters = viewpoint.altitude;
        value.headingDegrees = viewpoint.heading;
        value.pitchDegrees = viewpoint.pitch;
        value.rangeMeters = viewpoint.range;
        value.valid = true;
        m_earth->setViewpoint(value, seconds);
    }
#else
    Q_UNUSED(seconds)
#endif
    update();
    if (m_deploymentOverlay)
        m_deploymentOverlay->refresh();
    emitView();
}

}
