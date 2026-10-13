#include "modules/zqbs/FlightRoamDialog.h"

#include "app/AppPaths.h"
#include "app/MarkerActions.h"
#include "modules/zqbs/FlightRouteXmlStore.h"
#include "modules/zqts/SceneWidget.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QProcess>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>
#if defined(ZQXXXT_ENABLE_VIDEO)
#include <VideoSDK/VideoPlayer.h>
#endif

namespace Zqxxxt {
namespace {
QString degreesMinutesSeconds(double value)
{
    const double absolute = std::abs(value);
    const int degrees = static_cast<int>(absolute);
    const int minutes = static_cast<int>((absolute - degrees) * 60.0);
    const double seconds = ((absolute - degrees) * 60.0 - minutes) * 60.0;
    return QStringLiteral("%1%2°%3′%4″").arg(value < 0 ? QStringLiteral("−") : QString())
        .arg(degrees).arg(minutes).arg(seconds, 0, 'f', 1);
}

QPushButton *button(const QString &text, const QString &name, QWidget *parent)
{
    auto *result = new QPushButton(text, parent);
    result->setObjectName(name);
    result->setAutoDefault(false);
    return result;
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

void setColorSwatch(QPushButton *button, const QColor &color)
{
    QPixmap swatch(20, 20);
    swatch.fill(color);
    button->setIcon(QIcon(swatch));
    button->setIconSize(swatch.size());
}
}

class FlightRouteOverlay final : public QWidget
{
public:
    explicit FlightRouteOverlay(SceneWidget *scene)
        : QWidget(scene), m_scene(scene)
    {
        setObjectName(QStringLiteral("flightRouteOverlay"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setGeometry(scene->rect());
        show();
    }

    void setDocument(const FlightRouteDocument *document, int selected)
    {
        m_document = document;
        m_selected = selected;
        if (m_scene) setGeometry(m_scene->rect());
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!m_scene || !m_document) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        for (int routeIndex = 0; routeIndex < m_document->routes.size(); ++routeIndex) {
            const FlightRouteDefinition &route = m_document->routes.at(routeIndex);
            if (!route.visible || route.waypoints.isEmpty()) continue;
            QVector<QPointF> points;
            points.reserve(route.waypoints.size());
            for (const FlightRouteWaypoint &waypoint : route.waypoints) {
                QPointF position;
                if (m_scene->screenPosition(waypoint.coordinate, position))
                    points.append(position);
                else
                    points.append(QPointF(-10000, -10000));
            }
            // The route preview uses separated points, like the 3D flight path.
            // Screen spacing keeps the design overlay readable at every zoom.
            constexpr double dotSpacing = 20.0;
            const double dotRadius = routeIndex == m_selected ? 3.0 : 2.5;
            painter.setPen(Qt::NoPen);
            painter.setBrush(route.color);
            bool open = false;
            QPointF previous;
            double travelled = 0.0;
            double nextDot = dotSpacing;
            for (const QPointF &point : points) {
                if (point.x() < -1000) {
                    open = false;
                    travelled = 0.0;
                    nextDot = dotSpacing;
                    continue;
                }
                if (!open) {
                    previous = point;
                    open = true;
                    painter.drawEllipse(point, dotRadius, dotRadius);
                    continue;
                }
                const QPointF delta = point - previous;
                const double length = std::hypot(delta.x(), delta.y());
                while (length > 1e-9 && nextDot <= travelled + length) {
                    const double fraction = (nextDot - travelled) / length;
                    painter.drawEllipse(previous + delta * fraction, dotRadius, dotRadius);
                    nextDot += dotSpacing;
                }
                travelled += length;
                previous = point;
            }
            for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
                const QPointF point = points.at(pointIndex);
                if (point.x() < -1000) continue;
                painter.setBrush(QColor(8, 39, 70, 230));
                painter.setPen(QPen(route.color, routeIndex == m_selected ? 3.0 : 2.0));
                if (route.markerStyle == QStringLiteral("square")) {
                    painter.drawRect(QRectF(point.x() - 6.0, point.y() - 6.0, 12.0, 12.0));
                } else if (route.markerStyle == QStringLiteral("diamond")) {
                    painter.drawPolygon(QPolygonF{
                        QPointF(point.x(), point.y() - 7.0),
                        QPointF(point.x() + 7.0, point.y()),
                        QPointF(point.x(), point.y() + 7.0),
                        QPointF(point.x() - 7.0, point.y())});
                } else {
                    painter.drawEllipse(point, 6.0, 6.0);
                }
                painter.setPen(Qt::white);
                painter.drawText(QRectF(point.x() - 15, point.y() - 25, 30, 18),
                                 Qt::AlignCenter, QString::number(pointIndex + 1));
                const FlightRouteWaypoint &waypoint = route.waypoints.at(pointIndex);
                if (!waypoint.linkage.type.isEmpty()) {
                    painter.setPen(QPen(route.color, 1.5));
                    painter.setBrush(QColor(8, 39, 70, 235));
                    painter.drawRoundedRect(QRectF(point.x() + 8, point.y() - 11, 22, 22), 3, 3);
                    painter.setPen(Qt::white);
                    painter.drawText(QRectF(point.x() + 8, point.y() - 11, 22, 22),
                                     Qt::AlignCenter, QStringLiteral("链"));
                }
                if (!waypoint.annotation.isEmpty()) {
                    const QRectF label(point.x() + 8, point.y() - 38, 130, 24);
                    painter.setPen(QPen(route.color, 1.0));
                    painter.setBrush(QColor(8, 39, 70, 220));
                    painter.drawRoundedRect(label, 3, 3);
                    painter.setPen(Qt::white);
                    painter.drawText(label.adjusted(5, 0, -5, 0), Qt::AlignVCenter,
                                     waypoint.annotation);
                }
            }
        }
    }

private:
    QPointer<SceneWidget> m_scene;
    const FlightRouteDocument *m_document = nullptr;
    int m_selected = 0;
};

FlightRoamDialog::FlightRoamDialog(SceneWidget *scene, QWidget *parent)
    : SkinnedDialog(QStringLiteral("飞行"), parent), m_scene(scene)
{
    setObjectName(QStringLiteral("flightRoamDialog"));
    setModal(false);
    setMinimumSize(700, 480);
    resize(960, 760);
    buildUi();
    if (m_scene) {
        m_scene->installEventFilter(this);
    }
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &FlightRoamDialog::refreshStatus);
    loadInitialDocument();
    reactivate();
}

FlightRoamDialog::~FlightRoamDialog()
{
    stopSession();
    if (m_scene) m_scene->removeEventFilter(this);
    if (m_overlay) delete m_overlay;
}

void FlightRoamDialog::buildUi()
{
    auto *body = new QWidget(this);
    auto *outer = new QVBoxLayout(body);
    outer->setContentsMargins(6, 6, 6, 6);
    auto *splitter = new QSplitter(Qt::Horizontal, body);
    outer->addWidget(splitter, 1);

    auto *routePanel = new QWidget(splitter);
    auto *routeLayout = new QVBoxLayout(routePanel);
    routeLayout->setContentsMargins(0, 0, 6, 0);
    routeLayout->addWidget(new QLabel(QStringLiteral("航线列表（最多 3 条）"), routePanel));
    m_routeTable = new QTableWidget(3, 4, routePanel);
    m_routeTable->setObjectName(QStringLiteral("flightRouteTable"));
    m_routeTable->setHorizontalHeaderLabels({QStringLiteral("显示"), QStringLiteral("颜色"),
                                              QStringLiteral("名称"), QStringLiteral("航点")});
    m_routeTable->verticalHeader()->hide();
    m_routeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_routeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_routeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_routeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_routeTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_routeTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    routeLayout->addWidget(m_routeTable, 1);
    auto *designButton = button(QStringLiteral("航线设计与标注"),
                                QStringLiteral("routeDesignButton"), routePanel);
    routeLayout->addWidget(designButton);
    splitter->addWidget(routePanel);

    m_tabs = new QTabWidget(splitter);
    m_tabs->setObjectName(QStringLiteral("flightTabs"));
    auto *controlPage = new QWidget(m_tabs);
    auto *controlLayout = new QVBoxLayout(controlPage);

    auto *routeGroup = new QGroupBox(QStringLiteral("航线"), controlPage);
    auto *routeGroupLayout = new QHBoxLayout(routeGroup);
    routeGroupLayout->addWidget(new QLabel(QStringLiteral("航线选择"), routeGroup));
    m_routeCombo = new QComboBox(routeGroup);
    m_routeCombo->setObjectName(QStringLiteral("comboBox_Line"));
    for (int index = 1; index <= 3; ++index)
        m_routeCombo->addItem(QStringLiteral("航线 %1").arg(index));
    routeGroupLayout->addWidget(m_routeCombo, 1);
    controlLayout->addWidget(routeGroup);

    auto *cameraGroup = new QGroupBox(QStringLiteral("视角切换"), controlPage);
    auto *cameraLayout = new QHBoxLayout(cameraGroup);
    auto *cameraButtons = new QButtonGroup(this);
    m_cockpit = new QRadioButton(QStringLiteral("驾驶视角"), cameraGroup);
    m_cockpit->setObjectName(QStringLiteral("radioButton_FirstCam"));
    m_free = new QRadioButton(QStringLiteral("自由视角"), cameraGroup);
    m_free->setObjectName(QStringLiteral("radioButton_FreeCam"));
    m_follow = new QRadioButton(QStringLiteral("跟踪视角"), cameraGroup);
    m_follow->setObjectName(QStringLiteral("radioButton_LockCam"));
    m_cockpit->setChecked(true);
    cameraButtons->addButton(m_cockpit, 0);
    cameraButtons->addButton(m_free, 1);
    cameraButtons->addButton(m_follow, 2);
    cameraLayout->addWidget(m_cockpit);
    cameraLayout->addWidget(m_free);
    cameraLayout->addWidget(m_follow);
    controlLayout->addWidget(cameraGroup);

    auto *controlGroup = new QGroupBox(QStringLiteral("控制"), controlPage);
    auto *controls = new QGridLayout(controlGroup);
    m_start = button(QStringLiteral("开始"), QStringLiteral("pushButton_Start"), controlGroup);
    m_reset = button(QStringLiteral("复位"), QStringLiteral("pushButton_Reset"), controlGroup);
    m_speed = new QSpinBox(controlGroup);
    m_speed->setObjectName(QStringLiteral("spinBox_Speed"));
    m_speed->setRange(1, 99);
    m_routeVisible = new QCheckBox(QStringLiteral("显示航线"), controlGroup);
    m_routeVisible->setObjectName(QStringLiteral("checkBox_Airway"));
    m_routeVisible->setChecked(true);
    controls->addWidget(m_start, 0, 0);
    controls->addWidget(m_reset, 0, 1);
    controls->addWidget(new QLabel(QStringLiteral("倍速（X）"), controlGroup), 1, 0);
    controls->addWidget(m_speed, 1, 1);
    controls->addWidget(m_routeVisible, 2, 0, 1, 2);
    controlLayout->addWidget(controlGroup);

    auto *statusGroup = new QGroupBox(QStringLiteral("状态信息"), controlPage);
    auto *statusLayout = new QVBoxLayout(statusGroup);
    m_state = new QLabel(statusGroup);
    m_state->setObjectName(QStringLiteral("label_StateInfo"));
    m_state->setTextFormat(Qt::PlainText);
    m_state->setWordWrap(true);
    m_state->setMinimumHeight(100);
    m_state->setFrameShape(QFrame::StyledPanel);
    statusLayout->addWidget(m_state);
    controlLayout->addWidget(statusGroup, 1);
    m_tabs->addTab(controlPage, QStringLiteral("飞行控制"));

    auto *designPage = new QWidget(m_tabs);
    auto *designLayout = new QVBoxLayout(designPage);
    auto *tools = new QHBoxLayout;
    auto *addPointButton = button(QStringLiteral("添加航点"), QStringLiteral("addWaypointButton"), designPage);
    auto *insertPointButton = button(QStringLiteral("插入航点"), QStringLiteral("insertWaypointButton"), designPage);
    auto *deletePointButton = button(QStringLiteral("删除航点"), QStringLiteral("deleteWaypointButton"), designPage);
    auto *annotationButton = button(QStringLiteral("添加标注"), QStringLiteral("addWaypointAnnotationButton"), designPage);
    auto *linkageButton = button(QStringLiteral("添加联动"), QStringLiteral("addWaypointLinkageButton"), designPage);
    auto *clearRouteButton = button(QStringLiteral("清空航线"), QStringLiteral("clearRouteButton"), designPage);
    for (QPushButton *tool : {addPointButton, insertPointButton, deletePointButton,
                              annotationButton, linkageButton, clearRouteButton})
        tools->addWidget(tool);
    designLayout->addLayout(tools);

    m_waypointTable = new QTableWidget(designPage);
    m_waypointTable->setObjectName(QStringLiteral("waypointTable"));
    m_waypointTable->setColumnCount(7);
    m_waypointTable->setHorizontalHeaderLabels(
        {QStringLiteral("序号"), QStringLiteral("经度"), QStringLiteral("纬度"),
         QStringLiteral("高度(m)"), QStringLiteral("速度"), QStringLiteral("标注"),
         QStringLiteral("联动")});
    m_waypointTable->verticalHeader()->hide();
    m_waypointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_waypointTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_waypointTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_waypointTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    designLayout->addWidget(m_waypointTable, 1);

    auto *linkageGroup = new QGroupBox(QStringLiteral("航点联动"), designPage);
    linkageGroup->setObjectName(QStringLiteral("waypointLinkageGroup"));
    auto *linkageLayout = new QGridLayout(linkageGroup);
    m_linkageType = new QComboBox(linkageGroup);
    m_linkageType->setObjectName(QStringLiteral("linkageTypeCombo"));
    m_linkageType->addItems(flightLinkageTypes());
    m_linkageContent = new QLineEdit(linkageGroup);
    m_linkageContent->setObjectName(QStringLiteral("linkageContentEdit"));
    m_linkageParameter = new QLineEdit(linkageGroup);
    m_linkageParameter->setObjectName(QStringLiteral("linkageParameterEdit"));
    auto *saveLinkageButton = button(QStringLiteral("保存联动"), QStringLiteral("saveLinkageButton"), linkageGroup);
    auto *testLinkageButton = button(QStringLiteral("测试"), QStringLiteral("testLinkageButton"), linkageGroup);
    auto *clearLinkageButton = button(QStringLiteral("清除联动"), QStringLiteral("clearLinkageButton"), linkageGroup);
    linkageLayout->addWidget(new QLabel(QStringLiteral("联动类型"), linkageGroup), 0, 0);
    linkageLayout->addWidget(m_linkageType, 0, 1);
    linkageLayout->addWidget(new QLabel(QStringLiteral("内容"), linkageGroup), 0, 2);
    linkageLayout->addWidget(m_linkageContent, 0, 3, 1, 3);
    linkageLayout->addWidget(new QLabel(QStringLiteral("参数"), linkageGroup), 1, 0);
    linkageLayout->addWidget(m_linkageParameter, 1, 1, 1, 2);
    linkageLayout->addWidget(saveLinkageButton, 1, 3);
    linkageLayout->addWidget(testLinkageButton, 1, 4);
    linkageLayout->addWidget(clearLinkageButton, 1, 5);
    designLayout->addWidget(linkageGroup);

    auto *styleGroup = new QGroupBox(QStringLiteral("航线样式设置"), designPage);
    auto *styleLayout = new QGridLayout(styleGroup);
    m_routeName = new QLineEdit(styleGroup);
    m_routeColor = button(QStringLiteral("选择颜色"), QStringLiteral("routeColorButton"), styleGroup);
    m_lineWidth = new QDoubleSpinBox(styleGroup);
    m_lineWidth->setRange(0.5, 10.0);
    m_lineWidth->setSingleStep(0.5);
    m_markerStyle = new QComboBox(styleGroup);
    m_markerStyle->addItems({QStringLiteral("圆点"), QStringLiteral("方形"), QStringLiteral("菱形")});
    styleLayout->addWidget(new QLabel(QStringLiteral("航线名称"), styleGroup), 0, 0);
    styleLayout->addWidget(m_routeName, 0, 1, 1, 5);
    styleLayout->addWidget(new QLabel(QStringLiteral("航线颜色"), styleGroup), 1, 0);
    styleLayout->addWidget(m_routeColor, 1, 1);
    styleLayout->addWidget(new QLabel(QStringLiteral("线宽"), styleGroup), 1, 2);
    styleLayout->addWidget(m_lineWidth, 1, 3);
    styleLayout->addWidget(new QLabel(QStringLiteral("航点样式"), styleGroup), 1, 4);
    styleLayout->addWidget(m_markerStyle, 1, 5);
    designLayout->addWidget(styleGroup);
    m_tabs->addTab(designPage, QStringLiteral("航线设计"));
    splitter->addWidget(m_tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 740});

    auto *bottom = new QHBoxLayout;
    m_fileState = new QLabel(QStringLiteral("最多 3 条航线；航点可设置联动；统一保存为 XML 文件"), body);
    bottom->addWidget(m_fileState, 1);
    auto *newButton = button(QStringLiteral("新建XML"), QStringLiteral("newFlightXmlButton"), body);
    auto *openButton = button(QStringLiteral("打开XML"), QStringLiteral("openFlightXmlButton"), body);
    auto *saveButton = button(QStringLiteral("保存XML"), QStringLiteral("saveFlightXmlButton"), body);
    auto *applyButton = button(QStringLiteral("应用"), QStringLiteral("applyFlightRouteButton"), body);
    auto *closeButton = button(QStringLiteral("关闭"), QStringLiteral("closeFlightDialogButton"), body);
    for (QPushButton *action : {newButton, openButton, saveButton, applyButton, closeButton})
        bottom->addWidget(action);
    outer->addLayout(bottom);
    contentLayout()->addWidget(body);

    connect(designButton, &QPushButton::clicked, this, [this] { m_tabs->setCurrentIndex(1); });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int tab) {
        if (tab == 1 && m_scene) {
            if (!m_overlay) m_overlay = new FlightRouteOverlay(m_scene);
            refreshOverlay();
            m_overlay->show();
        } else if (m_overlay) {
            m_overlay->hide();
        }
    });
    connect(m_routeCombo, &QComboBox::currentIndexChanged, this, &FlightRoamDialog::loadRoute);
    connect(m_routeTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        loadRoute(row);
    });
    connect(m_routeTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_loadingUi || !item || item->column() != 0 || item->row() >= m_document.routes.size()) return;
        m_document.routes[item->row()].visible = item->checkState() == Qt::Checked;
        m_dirty = true;
        refreshOverlay();
    });
    connect(m_waypointTable, &QTableWidget::itemSelectionChanged,
            this, &FlightRoamDialog::refreshLinkageEditor);
    connect(m_waypointTable, &QTableWidget::cellChanged,
            this, &FlightRoamDialog::applyPointCell);
    connect(addPointButton, &QPushButton::clicked, this, [this] { beginPointPlacement(false); });
    connect(insertPointButton, &QPushButton::clicked, this, [this] { beginPointPlacement(true); });
    connect(deletePointButton, &QPushButton::clicked, this, &FlightRoamDialog::deletePoint);
    connect(annotationButton, &QPushButton::clicked, this, &FlightRoamDialog::addAnnotation);
    connect(linkageButton, &QPushButton::clicked, this, [this] {
        m_linkageType->setFocus();
    });
    connect(clearRouteButton, &QPushButton::clicked, this, [this] {
        FlightRouteDefinition *route = currentRoute();
        if (!route || route->waypoints.isEmpty()) return;
        if (QMessageBox::question(this, QStringLiteral("清空航线"),
            QStringLiteral("确定清空“%1”的全部航点吗？").arg(route->name)) != QMessageBox::Yes) return;
        route->waypoints.clear();
        m_dirty = true;
        refreshRouteList();
        refreshWaypointTable();
        refreshOverlay();
    });
    connect(saveLinkageButton, &QPushButton::clicked, this, &FlightRoamDialog::saveLinkage);
    connect(testLinkageButton, &QPushButton::clicked, this, [this] {
        FlightWaypointLinkage linkage{m_linkageType->currentText(),
                                       m_linkageContent->text().trimmed(),
                                       m_linkageParameter->text().trimmed()};
        executeLinkage(linkage, true);
    });
    connect(clearLinkageButton, &QPushButton::clicked, this, &FlightRoamDialog::clearLinkage);
    connect(m_routeName, &QLineEdit::editingFinished, this, [this] {
        FlightRouteDefinition *route = currentRoute();
        if (!route) return;
        const QString name = m_routeName->text().trimmed();
        route->name = name.isEmpty() ? QStringLiteral("航线 %1").arg(m_selectedRoute + 1) : name;
        m_dirty = true;
        refreshRouteList();
    });
    connect(m_routeColor, &QPushButton::clicked, this, &FlightRoamDialog::chooseRouteColor);
    connect(m_lineWidth, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (m_loadingUi) return;
        if (FlightRouteDefinition *route = currentRoute()) {
            route->lineWidth = value; m_dirty = true; refreshOverlay();
        }
    });
    connect(m_markerStyle, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_loadingUi) return;
        if (FlightRouteDefinition *route = currentRoute()) {
            route->markerStyle = index == 1 ? QStringLiteral("square")
                : (index == 2 ? QStringLiteral("diamond") : QStringLiteral("circle"));
            m_dirty = true; refreshOverlay();
        }
    });
    connect(m_speed, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_scene) m_scene->setFlightSpeed(value);
    });
    connect(cameraButtons, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_scene) m_scene->setFlightCamera(id);
    });
    connect(m_routeVisible, &QCheckBox::toggled, this, [this](bool visible) {
        if (m_scene) m_scene->setFlightRouteVisible(visible);
    });
    connect(m_start, &QPushButton::clicked, this, [this] {
        if (m_scene) m_scene->startFlight();
        refreshStatus();
    });
    connect(m_reset, &QPushButton::clicked, this, [this] {
        if (m_videoDialog) m_videoDialog->close();
        if (m_scene) m_scene->resetFlight();
        refreshStatus();
    });
    connect(newButton, &QPushButton::clicked, this, &FlightRoamDialog::newDocument);
    connect(openButton, &QPushButton::clicked, this, &FlightRoamDialog::openDocument);
    connect(saveButton, &QPushButton::clicked, this, [this] { saveDocument(false); });
    connect(applyButton, &QPushButton::clicked, this, &FlightRoamDialog::applyCurrentRoute);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString FlightRoamDialog::defaultXmlPath() const
{
    return QDir(AppPaths::dataDir()).filePath(QStringLiteral("model/H6/routes/flight_routes.xml"));
}

void FlightRoamDialog::loadInitialDocument()
{
    m_filePath = defaultXmlPath();
    QString error;
    if (!FlightRouteXmlStore::load(m_filePath, m_document, error)) {
        m_document = newFlightRouteDocument();
        m_fileState->setText(error);
    } else {
        m_fileState->setText(QFileInfo(m_filePath).fileName());
    }
    refreshRouteList();
    loadRoute(0);
}

void FlightRoamDialog::reactivate()
{
    if (m_overlay && m_tabs->currentIndex() == 1) m_overlay->show();
    // Opening the editor must not mutate the live OSG scene. The operator
    // explicitly applies a route after inspecting or editing its waypoints.
    m_start->setEnabled(false);
    m_reset->setEnabled(false);
    m_state->setText(m_scene ? QStringLiteral("请选择航线，点击“应用”后开始飞行")
                             : QStringLiteral("三维地图尚未就绪"));
    m_timer->start();
}

void FlightRoamDialog::done(int result)
{
    stopSession();
    SkinnedDialog::done(result);
}

void FlightRoamDialog::stopSession()
{
    stopPointPlacement();
    if (m_timer) m_timer->stop();
    if (m_videoDialog) m_videoDialog->close();
    if (m_overlay) m_overlay->hide();
    if (m_scene) m_scene->closeFlight();
}

FlightRouteDefinition *FlightRoamDialog::currentRoute()
{
    return m_selectedRoute >= 0 && m_selectedRoute < m_document.routes.size()
        ? &m_document.routes[m_selectedRoute] : nullptr;
}

FlightRouteWaypoint *FlightRoamDialog::currentWaypoint()
{
    FlightRouteDefinition *route = currentRoute();
    const int row = m_waypointTable ? m_waypointTable->currentRow() : -1;
    return route && row >= 0 && row < route->waypoints.size()
        ? &route->waypoints[row] : nullptr;
}

void FlightRoamDialog::loadRoute(int index)
{
    if (index < 0 || index >= m_document.routes.size()) return;
    m_selectedRoute = index;
    m_loadingUi = true;
    m_routeCombo->setCurrentIndex(index);
    m_routeTable->selectRow(index);
    const FlightRouteDefinition &route = m_document.routes.at(index);
    m_routeName->setText(route.name);
    m_lineWidth->setValue(route.lineWidth);
    m_markerStyle->setCurrentIndex(route.markerStyle == QStringLiteral("square") ? 1
        : (route.markerStyle == QStringLiteral("diamond") ? 2 : 0));
    setColorSwatch(m_routeColor, route.color);
    m_loadingUi = false;
    refreshWaypointTable();
    refreshOverlay();
}

void FlightRoamDialog::refreshRouteList()
{
    m_loadingUi = true;
    for (int row = 0; row < 3; ++row) {
        const FlightRouteDefinition &route = m_document.routes.at(row);
        auto *visible = readOnlyItem(QString());
        visible->setFlags(visible->flags() | Qt::ItemIsUserCheckable);
        visible->setCheckState(route.visible ? Qt::Checked : Qt::Unchecked);
        m_routeTable->setItem(row, 0, visible);
        auto *color = readOnlyItem(QString());
        color->setBackground(route.color);
        m_routeTable->setItem(row, 1, color);
        m_routeTable->setItem(row, 2, readOnlyItem(route.name));
        m_routeTable->setItem(row, 3, readOnlyItem(QString::number(route.waypoints.size())));
        m_routeCombo->setItemText(row, route.name);
    }
    m_loadingUi = false;
}

void FlightRoamDialog::refreshWaypointTable(int selectedRow)
{
    const FlightRouteDefinition *route = currentRoute();
    m_loadingUi = true;
    m_waypointTable->setRowCount(route ? route->waypoints.size() : 0);
    if (route) {
        for (int row = 0; row < route->waypoints.size(); ++row) {
            const FlightRouteWaypoint &point = route->waypoints.at(row);
            m_waypointTable->setItem(row, 0, readOnlyItem(QString::number(row + 1)));
            m_waypointTable->setItem(row, 1, new QTableWidgetItem(QString::number(point.coordinate.longitude, 'f', 8)));
            m_waypointTable->setItem(row, 2, new QTableWidgetItem(QString::number(point.coordinate.latitude, 'f', 8)));
            m_waypointTable->setItem(row, 3, new QTableWidgetItem(QString::number(point.coordinate.altitude, 'f', 2)));
            m_waypointTable->setItem(row, 4, new QTableWidgetItem(QString::number(point.speed, 'f', 1)));
            m_waypointTable->setItem(row, 5, new QTableWidgetItem(point.annotation));
            m_waypointTable->setItem(row, 6, readOnlyItem(point.linkage.type.isEmpty()
                ? QStringLiteral("未设置") : point.linkage.type));
        }
    }
    m_loadingUi = false;
    if (route && !route->waypoints.isEmpty()) {
        const int row = selectedRow >= 0 ? qMin(selectedRow, route->waypoints.size() - 1) : 0;
        m_waypointTable->selectRow(row);
    } else {
        refreshLinkageEditor();
    }
}

void FlightRoamDialog::refreshLinkageEditor()
{
    const FlightRouteWaypoint *point = currentWaypoint();
    const bool enabled = point != nullptr;
    m_linkageType->setEnabled(enabled);
    m_linkageContent->setEnabled(enabled);
    m_linkageParameter->setEnabled(enabled);
    if (!point) {
        m_linkageContent->clear();
        m_linkageParameter->clear();
        return;
    }
    const int index = flightLinkageTypes().indexOf(point->linkage.type);
    m_linkageType->setCurrentIndex(index >= 0 ? index : 0);
    m_linkageContent->setText(point->linkage.content);
    m_linkageParameter->setText(point->linkage.parameter);
}

void FlightRoamDialog::refreshOverlay()
{
    if (m_overlay) {
        m_overlay->setDocument(&m_document, m_selectedRoute);
        m_overlay->raise();
    }
}

bool FlightRoamDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scene && m_placingPoints
        && (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease)) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::RightButton && event->type() == QEvent::MouseButtonRelease) {
            stopPointPlacement();
            return true;
        }
        if (mouse->button() == Qt::LeftButton) {
            if (event->type() == QEvent::MouseButtonRelease) {
                GeoCoordinate coordinate;
                if (m_scene->coordinateAtScreen(mouse->position(), coordinate))
                    addPoint(coordinate);
                else
                    m_fileState->setText(QStringLiteral("当前位置无法取得地理坐标"));
            }
            return true;
        }
    }
    return SkinnedDialog::eventFilter(watched, event);
}

void FlightRoamDialog::beginPointPlacement(bool insert)
{
    if (!m_scene) {
        m_fileState->setText(QStringLiteral("三维地图尚未就绪"));
        return;
    }
    m_insertBefore = insert ? m_waypointTable->currentRow() : -1;
    if (insert && m_insertBefore < 0) {
        m_fileState->setText(QStringLiteral("请先选择要插入位置的航点"));
        return;
    }
    m_placingPoints = true;
    m_fileState->setText(insert
        ? QStringLiteral("在地图单击插入航点；右键取消")
        : QStringLiteral("在地图连续单击添加航点；右键完成"));
}

void FlightRoamDialog::stopPointPlacement()
{
    if (!m_placingPoints) return;
    m_placingPoints = false;
    m_insertBefore = -1;
    m_fileState->setText(m_dirty ? QStringLiteral("航线已修改，尚未保存")
                                 : QFileInfo(m_filePath).fileName());
}

void FlightRoamDialog::addPoint(const GeoCoordinate &coordinate)
{
    FlightRouteDefinition *route = currentRoute();
    if (!route) return;
    FlightRouteWaypoint waypoint;
    waypoint.coordinate = coordinate;
    waypoint.speed = 15.0;
    int selected = route->waypoints.size();
    if (m_insertBefore >= 0) {
        selected = qMin(m_insertBefore, route->waypoints.size());
        route->waypoints.insert(selected, waypoint);
        m_insertBefore = -1;
        m_placingPoints = false;
    } else {
        route->waypoints.append(waypoint);
    }
    m_dirty = true;
    refreshRouteList();
    refreshWaypointTable(selected);
    refreshOverlay();
}

void FlightRoamDialog::deletePoint()
{
    FlightRouteDefinition *route = currentRoute();
    const int row = m_waypointTable->currentRow();
    if (!route || row < 0 || row >= route->waypoints.size()) return;
    route->waypoints.removeAt(row);
    m_dirty = true;
    refreshRouteList();
    refreshWaypointTable(qMax(0, row - 1));
    refreshOverlay();
}

void FlightRoamDialog::addAnnotation()
{
    FlightRouteWaypoint *point = currentWaypoint();
    if (!point) return;
    bool accepted = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("航点标注"),
        QStringLiteral("标注文字"), QLineEdit::Normal, point->annotation, &accepted).trimmed();
    if (!accepted) return;
    point->annotation = text;
    m_dirty = true;
    refreshWaypointTable(m_waypointTable->currentRow());
    refreshOverlay();
}

void FlightRoamDialog::saveLinkage()
{
    FlightRouteWaypoint *point = currentWaypoint();
    if (!point) return;
    const QString content = m_linkageContent->text().trimmed();
    if (content.isEmpty()) {
        m_fileState->setText(QStringLiteral("联动内容不能为空"));
        return;
    }
    point->linkage = {m_linkageType->currentText(), content,
                      m_linkageParameter->text().trimmed()};
    m_dirty = true;
    const int row = m_waypointTable->currentRow();
    refreshWaypointTable(row);
    refreshOverlay();
    m_fileState->setText(QStringLiteral("航点联动已保存到当前编辑内容"));
}

void FlightRoamDialog::clearLinkage()
{
    FlightRouteWaypoint *point = currentWaypoint();
    if (!point) return;
    point->linkage = {};
    m_dirty = true;
    const int row = m_waypointTable->currentRow();
    refreshWaypointTable(row);
    refreshOverlay();
}

void FlightRoamDialog::applyPointCell(int row, int column)
{
    if (m_loadingUi || column == 0 || column == 6) return;
    FlightRouteDefinition *route = currentRoute();
    if (!route || row < 0 || row >= route->waypoints.size()) return;
    QTableWidgetItem *item = m_waypointTable->item(row, column);
    if (!item) return;
    FlightRouteWaypoint &point = route->waypoints[row];
    bool ok = false;
    const double value = item->text().toDouble(&ok);
    if (column == 1 && ok && std::abs(value) <= 180.0) point.coordinate.longitude = value;
    else if (column == 2 && ok && std::abs(value) <= 90.0) point.coordinate.latitude = value;
    else if (column == 3 && ok && std::isfinite(value)) point.coordinate.altitude = value;
    else if (column == 4 && ok && value >= 1.0 && value <= 999.0) point.speed = value;
    else if (column == 5) point.annotation = item->text().trimmed();
    else {
        m_fileState->setText(QStringLiteral("航点输入值无效"));
        refreshWaypointTable(row);
        return;
    }
    m_dirty = true;
    refreshOverlay();
}

void FlightRoamDialog::chooseRouteColor()
{
    FlightRouteDefinition *route = currentRoute();
    if (!route) return;
    const QColor color = QColorDialog::getColor(route->color, this, QStringLiteral("航线颜色"));
    if (!color.isValid()) return;
    route->color = color;
    setColorSwatch(m_routeColor, color);
    m_dirty = true;
    refreshRouteList();
    refreshOverlay();
}

void FlightRoamDialog::newDocument()
{
    stopPointPlacement();
    m_document = newFlightRouteDocument();
    m_filePath.clear();
    m_dirty = true;
    refreshRouteList();
    loadRoute(0);
    m_tabs->setCurrentIndex(1);
    m_fileState->setText(QStringLiteral("新航线文档，尚未保存"));
}

void FlightRoamDialog::openDocument()
{
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("打开航线 XML"),
        QFileInfo(m_filePath.isEmpty() ? defaultXmlPath() : m_filePath).absolutePath(),
        QStringLiteral("航线 XML (*.xml)"));
    if (filePath.isEmpty()) return;
    FlightRouteDocument document;
    QString error;
    if (!FlightRouteXmlStore::load(filePath, document, error)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), error);
        return;
    }
    m_document = document;
    m_filePath = filePath;
    m_dirty = false;
    refreshRouteList();
    loadRoute(0);
    m_fileState->setText(QFileInfo(filePath).fileName());
}

bool FlightRoamDialog::saveDocument(bool saveAs)
{
    QString filePath = m_filePath;
    if (saveAs || filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(this, QStringLiteral("保存航线 XML"),
            filePath.isEmpty() ? defaultXmlPath() : filePath,
            QStringLiteral("航线 XML (*.xml)"));
        if (filePath.isEmpty()) return false;
        if (!filePath.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive))
            filePath += QStringLiteral(".xml");
    }
    QString error;
    if (!FlightRouteXmlStore::save(filePath, m_document, error)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return false;
    }
    m_filePath = filePath;
    m_dirty = false;
    m_fileState->setText(QStringLiteral("已保存：%1").arg(QFileInfo(filePath).fileName()));
    return true;
}

void FlightRoamDialog::applyCurrentRoute()
{
    FlightRouteDefinition *route = currentRoute();
    if (!route || !m_scene) {
        m_start->setEnabled(false);
        m_reset->setEnabled(false);
        if (!m_scene) m_state->setText(QStringLiteral("三维地图尚未就绪"));
        return;
    }
    if (route->waypoints.size() < 2) {
        m_scene->closeFlight();
        m_start->setEnabled(false);
        m_reset->setEnabled(false);
        m_state->setText(QStringLiteral("当前航线至少需要两个航点"));
        return;
    }
    if (m_videoDialog) m_videoDialog->close();
    const QString model = AppPaths::assetPath(QStringLiteral("model/wrj.ive"));
    QString error;
    const bool loaded = m_scene->loadFlightRoute(*route, model, error);
    m_start->setEnabled(loaded);
    m_reset->setEnabled(loaded);
    if (!loaded) {
        m_state->setText(error);
        return;
    }
    m_scene->setFlightSpeed(m_speed->value());
    m_scene->setFlightRouteVisible(m_routeVisible->isChecked());
    m_scene->setFlightCamera(m_cockpit->isChecked() ? 0 : (m_free->isChecked() ? 1 : 2));
    refreshStatus();
}

void FlightRoamDialog::refreshStatus()
{
    // The editor preview is only needed on the design tab. Projecting all
    // points and raising a QWidget every 100 ms stresses the live map scene.
    if (m_overlay && m_tabs->currentIndex() == 1
        && (++m_overlayRefreshTick % 5) == 0) {
        m_overlay->update();
    }
    if (!m_scene) return;
    const SceneFlightStatus state = m_scene->flightStatus();
    if (state.loaded) {
        m_state->setText(QStringLiteral("经度： %1\n纬度： %2\n高度： %3 m")
            .arg(degreesMinutesSeconds(state.position.longitude),
                 degreesMinutesSeconds(state.position.latitude))
            .arg(std::trunc(state.position.altitude), 0, 'f', 0));
    }
    for (const FlightWaypointLinkage &event : m_scene->takeFlightLinkageEvents())
        executeLinkage(event, false);
}

void FlightRoamDialog::executeLinkage(const FlightWaypointLinkage &linkage, bool test)
{
    if (!test) return; // Automatic fly-through never opens programs or remote streams.
    QString type;
    if (linkage.type == QStringLiteral("自定义视频") || linkage.type == QStringLiteral("视频联动")) type = "video";
    else if (linkage.type == QStringLiteral("打开网页")) type = "web";
    else if (linkage.type == QStringLiteral("打开程序")) type = "program";
    if (type.isEmpty()) return;
    MarkerActions::execute({{"type", type}, {"target", linkage.content}, {"parameter", linkage.parameter}}, this);
}

void FlightRoamDialog::openVideo(const QString &content, const QString &)
{
    MarkerActions::execute({{"type", "video"}, {"target", content}}, this);
}

}
