#include "modules/zqbs/ZqbsPage.h"

#include "app/AppPaths.h"
#include "app/MarkerActions.h"
#include "modules/zqbs/MapAnalysis.h"
#include "modules/zqbs/PlotGeometry.h"
#include "modules/zqbs/DeploymentPropertyDialog.h"
#include "modules/zqbs/FlightRoamDialog.h"
#include "modules/zqbs/LayerAddDialog.h"
#include "modules/zqbs/NormalAnnotationsTree.h"
#include "modules/zqbs/DynamicPlanEditor.h"
#include "modules/zqbs/DynamicStepPlayback.h"
#include "modules/zqbs/DynamicTimelineButton.h"
#include "modules/zqbs/EnvironmentSettingsDialog.h"
#include "modules/zqts/SceneWidget.h"
#include "ui/AppTheme.h"
#include "ui/SkinnedDialog.h"

#include <QButtonGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QCursor>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QPointer>
#include <QRadioButton>
#include <QScrollBar>
#include <QScreen>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QToolButton>
#include <QTextBrowser>
#include <QTimer>
#include <QTimeZone>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QUrlQuery>
#include <QUrl>
#include <QSharedPointer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <tuple>
#include <utility>

namespace {

Zqxxxt::DeploymentViewpointProvider markerViewpointProvider(Zqxxxt::SceneWidget *scene)
{
    return [scene = QPointer<Zqxxxt::SceneWidget>(scene)]()
        -> std::optional<Zqxxxt::DeploymentViewpoint> {
        if (!scene)
            return std::nullopt;
        const auto current = scene->currentViewpoint();
        if (!current.valid)
            return std::nullopt;
        Zqxxxt::DeploymentViewpoint view;
        view.longitude = current.longitude;
        view.latitude = current.latitude;
        view.altitude = current.altitude;
        view.heading = current.heading;
        view.pitch = current.pitch;
        view.range = current.range;
        return view;
    };
}

constexpr int ItemTypeRole = Qt::UserRole;
constexpr int ItemIdRole = Qt::UserRole + 1;
constexpr int ItemAuxRole = Qt::UserRole + 2;

QString newId(const QString &prefix)
{
    return prefix + QLatin1Char('-')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString domainName(int domain)
{
    switch (domain) {
    case 0: return QStringLiteral("基础场景 · 空间部署");
    case 1: return QStringLiteral("部署标注 · 部署分组标注与实时点位");
    case 2: return QStringLiteral("动态演练 · 演练方案演练");
    default: return {};
    }
}

QString entityTypeName(const QString &type)
{
    if (type == QStringLiteral("organization")) return QStringLiteral("组织");
    if (type == QStringLiteral("sentryPost")) return QStringLiteral("哨位");
    if (type == QStringLiteral("device")) return QStringLiteral("设备");
    if (type == QStringLiteral("camera")) return QStringLiteral("摄像机");
    if (type == QStringLiteral("alarm")) return QStringLiteral("报警");
    return type;
}

QString formattedCoordinate(const Zqxxxt::GeoCoordinate &point)
{
    return QStringLiteral("%1, %2, %3 m")
        .arg(point.longitude, 0, 'f', 7)
        .arg(point.latitude, 0, 'f', 7)
        .arg(point.altitude, 0, 'f', 1);
}

QString measurementText(const Zqxxxt::DeploymentMeasurement &measurement)
{
    return measurement.resultText.isEmpty()
        ? Zqxxxt::formatMeasurementValue(measurement.kind, measurement.value)
        : measurement.resultText.section(QLatin1Char('\n'), 0, 0);
}

QString panelStyle()
{
    return Zqxxxt::AppTheme::deploymentPanelStyleSheet();
}

QIcon whiteChevronIcon(bool pointsRight)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::white, 2.2, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    if (pointsRight) {
        path.moveTo(5.0, 3.0);
        path.lineTo(11.0, 8.0);
        path.lineTo(5.0, 13.0);
    } else {
        path.moveTo(11.0, 3.0);
        path.lineTo(5.0, 8.0);
        path.lineTo(11.0, 13.0);
    }
    painter.drawPath(path);
    return QIcon(pixmap);
}

QWidget *createDeploymentDialogBody(Zqxxxt::SkinnedDialog &dialog)
{
    auto *body = new QWidget(&dialog);
    body->setObjectName(QStringLiteral("deploymentDialogBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(panelStyle());
    dialog.contentLayout()->addWidget(body);
    return body;
}

QTreeWidgetItem *makeItem(QTreeWidgetItem *parent, const QString &text,
                          const QString &type, const QString &id,
                          const QString &aux = {})
{
    auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem;
    item->setText(0, text);
    item->setData(0, ItemTypeRole, type);
    item->setData(0, ItemIdRole, id);
    item->setData(0, ItemAuxRole, aux);
    return item;
}

QString treeItemStateKey(const QTreeWidgetItem *item)
{
    QStringList path;
    for (const QTreeWidgetItem *current = item; current; current = current->parent()) {
        const QString type = current->data(0, ItemTypeRole).toString();
        if (type.startsWith(QStringLiteral("normal-"))
            || type == QStringLiteral("live") || type == QStringLiteral("binding-rule")) {
            path.prepend(type + QChar(0x1f) + current->data(0, ItemIdRole).toString());
            continue;
        }
        path.prepend(QStringLiteral("%1\x1f%2\x1f%3\x1f%4")
            .arg(current->data(0, ItemTypeRole).toString(),
                 current->data(0, ItemIdRole).toString(),
                 current->data(0, ItemAuxRole).toString(), current->text(0)));
    }
    return path.join(QChar(0x1e));
}

QSet<QString> expandedTreeItemKeys(const QTreeWidget *tree)
{
    QSet<QString> keys;
    if (!tree)
        return keys;
    for (QTreeWidgetItemIterator iterator(const_cast<QTreeWidget *>(tree));
         *iterator; ++iterator) {
        if ((*iterator)->isExpanded())
            keys.insert(treeItemStateKey(*iterator));
    }
    return keys;
}

class ProfileChart final : public QWidget
{
public:
    explicit ProfileChart(QVector<Zqxxxt::GeoCoordinate> points,
                          QWidget *parent = nullptr)
        : QWidget(parent), m_points(std::move(points))
    {
        setMinimumSize(620, 260);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(QStringLiteral("#0b3767")));
        const QRectF chart = QRectF(rect()).adjusted(58, 24, -24, -42);
        painter.setPen(QColor(QStringLiteral("#6ca9d7")));
        painter.drawRect(chart);
        if (m_points.size() < 2)
            return;
        double minimum = m_points.constFirst().altitude;
        double maximum = minimum;
        for (const auto &point : m_points) {
            minimum = std::min(minimum, point.altitude);
            maximum = std::max(maximum, point.altitude);
        }
        if (maximum - minimum < 0.1)
            maximum = minimum + 0.1;
        QPainterPath path;
        for (qsizetype index = 0; index < m_points.size(); ++index) {
            const double x = chart.left() + chart.width() * index / (m_points.size() - 1);
            const double y = chart.bottom() - chart.height()
                * (m_points.at(index).altitude - minimum) / (maximum - minimum);
            if (index == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        QPainterPath fill = path;
        fill.lineTo(chart.bottomRight());
        fill.lineTo(chart.bottomLeft());
        fill.closeSubpath();
        painter.fillPath(fill, QColor(48, 190, 230, 55));
        painter.setPen(QPen(QColor(QStringLiteral("#55e8ff")), 2));
        painter.drawPath(path);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(4, chart.top() - 8, 50, 20), Qt::AlignRight,
                         QString::number(maximum, 'f', 1));
        painter.drawText(QRectF(4, chart.bottom() - 10, 50, 20), Qt::AlignRight,
                         QString::number(minimum, 'f', 1));
        painter.drawText(QRectF(chart.left(), chart.bottom() + 8,
                                chart.width(), 24), Qt::AlignCenter,
                         QStringLiteral("剖面采样序列（%1 点）").arg(m_points.size()));
    }

private:
    QVector<Zqxxxt::GeoCoordinate> m_points;
};

class DeploymentToolPalette final : public QFrame
{
public:
    DeploymentToolPalette(const QString &title, const QString &titleIcon,
                          int columns, QWidget *owner)
        : QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint),
          m_columns(qMax(1, columns))
    {
        setObjectName(QStringLiteral("deploymentToolPalette"));
        setAttribute(Qt::WA_DeleteOnClose);
        setStyleSheet(panelStyle());
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(1, 1, 1, 1);
        root->setSpacing(0);
        auto *heading = new QWidget(this);
        heading->setObjectName(QStringLiteral("deploymentPaletteHeading"));
        auto *headingLayout = new QHBoxLayout(heading);
        headingLayout->setContentsMargins(8, 4, 5, 4);
        auto *icon = new QLabel(heading);
        icon->setFixedSize(24, 24);
        icon->setPixmap(QIcon(titleIcon).pixmap(22, 22));
        auto *label = new QLabel(title, heading);
        label->setObjectName(QStringLiteral("deploymentPaletteTitle"));
        auto *close = new QToolButton(heading);
        close->setObjectName(QStringLiteral("deploymentPaletteClose"));
        close->setText(QStringLiteral("×"));
        headingLayout->addWidget(icon);
        headingLayout->addWidget(label);
        headingLayout->addStretch();
        headingLayout->addWidget(close);
        root->addWidget(heading);
        auto *body = new QWidget(this);
        body->setObjectName(QStringLiteral("deploymentPaletteBody"));
        m_grid = new QGridLayout(body);
        m_grid->setContentsMargins(0, 0, 0, 0);
        m_grid->setHorizontalSpacing(0);
        m_grid->setVerticalSpacing(0);
        root->addWidget(body);
        connect(close, &QToolButton::clicked, this, &QWidget::close);
        if (owner) setProperty("ownerName", owner->objectName());
    }

    void addTool(const QString &text, const QString &iconPath,
                 std::function<void()> operation)
    {
        auto *button = new QToolButton(this);
        button->setObjectName(QStringLiteral("deploymentPaletteTool"));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setText(text);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(34, 34));
        button->setFixedSize(118, 70);
        const int index = m_count++;
        m_grid->addWidget(button, index / m_columns, index % m_columns);
        connect(button, &QToolButton::clicked, this,
                [this, operation = std::move(operation)] {
            close();
            operation();
        });
    }

    void addToggleTool(const QString &text, const QString &iconPath,
                       bool checked, std::function<void(bool)> operation)
    {
        auto *button = new QToolButton(this);
        button->setObjectName(QStringLiteral("deploymentPaletteTool"));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setText(text);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(34, 34));
        button->setFixedSize(118, 70);
        button->setCheckable(true);
        button->setChecked(checked);
        button->setStyleSheet(QStringLiteral(
            "#deploymentPaletteTool:checked { background:#0a5488; "
            "border:1px solid #52d8ff; color:white; }"));
        const int index = m_count++;
        m_grid->addWidget(button, index / m_columns, index % m_columns);
        connect(button, &QToolButton::clicked, this,
                [this, button, operation = std::move(operation)] {
            const bool value = button->isChecked();
            close();
            operation(value);
        });
    }

    void showBeside(QWidget *anchor)
    {
        adjustSize();
        QPoint position = QCursor::pos();
        if (anchor) {
            const QPoint right = anchor->mapToGlobal(QPoint(anchor->width() + 8, 0));
            const QPoint left = anchor->mapToGlobal(QPoint(-width() - 8, 0));
            position = right;
            if (QScreen *screen = QGuiApplication::screenAt(right)) {
                if (right.x() + width() > screen->availableGeometry().right()) position = left;
            }
        }
        if (QScreen *screen = QGuiApplication::screenAt(position)) {
            const QRect area = screen->availableGeometry();
            position.setX(qBound(area.left(), position.x(), area.right() - width() + 1));
            position.setY(qBound(area.top(), position.y(), area.bottom() - height() + 1));
        }
        move(position);
        show();
        raise();
    }

private:
    QGridLayout *m_grid = nullptr;
    int m_columns = 2;
    int m_count = 0;
};

QString deploymentIcon(const QString &fileName)
{
    return QStringLiteral(":/deployment/icons/") + fileName;
}

QString annotationIcon(const QString &kind)
{
    static const QHash<QString, QString> icons{
        {QStringLiteral("point"), QStringLiteral("point.png")},
        {QStringLiteral("line"), QStringLiteral("line.png")},
        {QStringLiteral("polygon"), QStringLiteral("polygon.png")},
        {QStringLiteral("model"), QStringLiteral("model.png")},
        {QStringLiteral("billboard"), QStringLiteral("billboard.png")},
        {QStringLiteral("overlay"), QStringLiteral("cover.png")},
        {QStringLiteral("arc"), QStringLiteral("arc.png")},
        {QStringLiteral("ellipse"), QStringLiteral("ellipse.png")},
        {QStringLiteral("rectangle"), QStringLiteral("rectangle.png")},
        {QStringLiteral("sector"), QStringLiteral("sector.png")},
        {QStringLiteral("circle"), QStringLiteral("circle.png")},
        {QStringLiteral("text"), QStringLiteral("text.png")},
        {QStringLiteral("tilt-model"), QStringLiteral("oblique_model.png")},
        {QStringLiteral("particle"), QStringLiteral("particle.png")},
        {QStringLiteral("label"), QStringLiteral("label.png")},
        {QStringLiteral("plot-straight-arrow"), QStringLiteral("StraightArrow.png")},
        {QStringLiteral("plot-straight-multi-arrow"), QStringLiteral("StraightMoreArrow.png")},
        {QStringLiteral("plot-dovetail-diagonal-arrow"), QStringLiteral("DovetailDiagonalArrow.png")},
        {QStringLiteral("plot-dovetail-diagonal-multi-arrow"), QStringLiteral("DovetailDiagonalMoreArrow.png")},
        {QStringLiteral("plot-dovetail-straight-arrow"), QStringLiteral("DoveTailStraightArrow.png")},
        {QStringLiteral("plot-dovetail-straight-multi-arrow"), QStringLiteral("DoveTailStraightMoreArrow.png")},
        {QStringLiteral("plot-diagonal-arrow"), QStringLiteral("DiagonalArrow.png")},
        {QStringLiteral("plot-diagonal-multi-arrow"), QStringLiteral("DiagonalMoreArrow.png")},
        {QStringLiteral("plot-double-arrow"), QStringLiteral("DoubleArrow.png")},
        {QStringLiteral("plot-closed-curve"), QStringLiteral("CloseCurve.png")},
        {QStringLiteral("plot-rounded-rect"), QStringLiteral("RoundedRect.png")},
        {QStringLiteral("plot-straight-line-arrow"), QStringLiteral("StraightLineArrow.png")},
        {QStringLiteral("plot-bezier-arrow"), QStringLiteral("BezierCurveArrow.png")},
        {QStringLiteral("plot-polyline-arrow"), QStringLiteral("PolyLineArrow.png")},
        {QStringLiteral("plot-parallel-search"), QStringLiteral("ParallelSearch.png")},
        {QStringLiteral("plot-sector-search"), QStringLiteral("SectorSearch.png")},
        {QStringLiteral("plot-cardinal-arrow"), QStringLiteral("CardinalCurveArrow.png")}
    };
    return deploymentIcon(icons.value(kind, Zqxxxt::isPlotAnnotation(kind)
        ? QStringLiteral("StraightArrow.png") : QStringLiteral("point.png")));
}

QString defaultAnnotationResource(const QString &kind)
{
    static const QHash<QString, QString> resources{
        {QStringLiteral("point"), QStringLiteral("markers/images/point_32.png")},
        // The reference code names Tank.ive, but that file is not present in
        // the delivered sample Data/model directory. Use the packaged L15
        // model so a newly created marker always has a real resource.
        {QStringLiteral("model"), QStringLiteral("model/wrj.ive")},
        {QStringLiteral("billboard"), QStringLiteral("texture/mark/gt.png")},
        {QStringLiteral("overlay"), QStringLiteral("markers/images/label_title.png")},
        {QStringLiteral("particle"), QStringLiteral("particle/particle/x6/x6.xml")},
        {QStringLiteral("label"), QString()}
    };
    return resources.value(kind);
}

QTableWidget *coordinateTable(const QVector<Zqxxxt::GeoCoordinate> &points,
                              QWidget *parent)
{
    auto *table = new QTableWidget(points.size(), 4, parent);
    table->setObjectName(QStringLiteral("measurementCoordinateTable"));
    table->setHorizontalHeaderLabels({QStringLiteral("ID"),
        QStringLiteral("经度（°）"), QStringLiteral("纬度（°）"),
        QStringLiteral("高度（m）")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (qsizetype row = 0; row < points.size(); ++row) {
        const Zqxxxt::GeoCoordinate &point = points.at(row);
        const QString values[]{QString::number(row + 1),
            QString::number(point.longitude, 'g', 8),
            QString::number(point.latitude, 'g', 8),
            QString::number(point.altitude, 'g', 8)};
        for (int column = 0; column < 4; ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, column, item);
        }
    }
    return table;
}

bool rebuildContourSegments(Zqxxxt::DeploymentMeasurement &measurement,
                            QString &error)
{
    const int gridSize = qRound(std::sqrt(double(measurement.analysisPoints.size())));
    if (gridSize < 2 || gridSize * gridSize != measurement.analysisPoints.size()) {
        error = QStringLiteral("等高线采样网格不完整，请重新执行等高线分析。");
        return false;
    }
    const Zqxxxt::GeoCoordinate &first = measurement.points.constFirst();
    const Zqxxxt::GeoCoordinate &last = measurement.points.constLast();
    const QVector<Zqxxxt::GeoCoordinate> polygon{
        {first.longitude, first.latitude, 0.0},
        {last.longitude, first.latitude, 0.0},
        {last.longitude, last.latitude, 0.0},
        {first.longitude, last.latitude, 0.0}};
    double minimum = std::numeric_limits<double>::max();
    double maximum = std::numeric_limits<double>::lowest();
    for (const Zqxxxt::GeoCoordinate &sample : measurement.analysisPoints) {
        if (!Zqxxxt::pointInPolygon(sample, polygon))
            continue;
        minimum = std::min(minimum, sample.altitude);
        maximum = std::max(maximum, sample.altitude);
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        error = QStringLiteral("等高线范围内没有有效高程。");
        return false;
    }
    const double interval = std::clamp(measurement.parameter, 100.0, 1000.0);
    const double firstLevel = std::ceil(minimum / interval) * interval;
    const auto interpolate = [](const Zqxxxt::GeoCoordinate &a,
                                const Zqxxxt::GeoCoordinate &b, double level) {
        const double denominator = b.altitude - a.altitude;
        const double t = std::abs(denominator) < 1e-9
            ? 0.5 : qBound(0.0, (level - a.altitude) / denominator, 1.0);
        return Zqxxxt::GeoCoordinate{
            a.longitude + (b.longitude - a.longitude) * t,
            a.latitude + (b.latitude - a.latitude) * t, level};
    };
    measurement.analysisSegments.clear();
    measurement.analysisSegmentStates.clear();
    int levelCount = 0;
    for (double level = firstLevel; level <= maximum; level += interval) {
        ++levelCount;
        for (int row = 0; row + 1 < gridSize; ++row) {
            for (int column = 0; column + 1 < gridSize; ++column) {
                const Zqxxxt::GeoCoordinate &a = measurement.analysisPoints.at(row * gridSize + column);
                const Zqxxxt::GeoCoordinate &b = measurement.analysisPoints.at(row * gridSize + column + 1);
                const Zqxxxt::GeoCoordinate &c = measurement.analysisPoints.at((row + 1) * gridSize + column + 1);
                const Zqxxxt::GeoCoordinate &d = measurement.analysisPoints.at((row + 1) * gridSize + column);
                const Zqxxxt::GeoCoordinate center{(a.longitude + c.longitude) * 0.5,
                                                   (a.latitude + c.latitude) * 0.5, 0.0};
                if (!Zqxxxt::pointInPolygon(center, polygon))
                    continue;
                QVector<Zqxxxt::GeoCoordinate> crossings;
                const auto edge = [&crossings, &interpolate, level](
                    const Zqxxxt::GeoCoordinate &left,
                    const Zqxxxt::GeoCoordinate &right) {
                    if ((left.altitude <= level && right.altitude > level)
                        || (right.altitude <= level && left.altitude > level))
                        crossings.append(interpolate(left, right, level));
                };
                edge(a, b); edge(b, c); edge(c, d); edge(d, a);
                if (crossings.size() >= 2) {
                    measurement.analysisSegments += {crossings.at(0), crossings.at(1)};
                    measurement.analysisSegmentStates.append(1);
                    if (crossings.size() == 4) {
                        measurement.analysisSegments += {crossings.at(2), crossings.at(3)};
                        measurement.analysisSegmentStates.append(1);
                    }
                }
            }
        }
    }
    measurement.value = measurement.analysisSegments.size() / 2;
    measurement.resultText = QStringLiteral("生成等高线段 %1\n等高距 %2 m\n高程范围 %3–%4 m\n等高层级 %5")
        .arg(measurement.analysisSegments.size() / 2)
        .arg(interval, 0, 'f', 1).arg(minimum, 0, 'f', 2)
        .arg(maximum, 0, 'f', 2).arg(levelCount);
    return true;
}

} // namespace

namespace Zqxxxt {

ZqbsPage::ZqbsPage(SceneWidget *scene, QObject *parent)
    : QObject(parent), m_scene(scene), m_store(), m_planTimer(new QTimer(this))
{
    Q_ASSERT(m_scene);
    createPanels();

    m_planTimer->setSingleShot(true);
    m_planTimer->setTimerType(Qt::PreciseTimer);
    connect(m_planTimer, &QTimer::timeout, this, &ZqbsPage::advancePlanPlayback);
    connect(m_scene, &SceneWidget::mapPointSelected,
            this, &ZqbsPage::receiveMapPoint);
    connect(m_scene, &SceneWidget::annotationCompleted,
            this, &ZqbsPage::receiveAnnotation);
    connect(m_scene, &SceneWidget::measurementCompleted,
            this, &ZqbsPage::receiveMeasurement);
    connect(m_scene, &SceneWidget::deploymentMarkerTriggered,
            this, &ZqbsPage::handleMarkerTrigger);
    connect(m_scene, &SceneWidget::runtimeLayersChanged, this,
            [this](const QStringList &names) {
        QSet<QString> currentNames;
        for (const QString &name : names) currentNames.insert(name);
        const auto oldSize = m_data.layers.size();
        m_data.layers.erase(std::remove_if(m_data.layers.begin(), m_data.layers.end(),
            [&currentNames](const DeploymentLayer &layer) {
                return layer.runtimeLayer && layer.source.startsWith(QStringLiteral("earth:"))
                    && !currentNames.contains(layer.source.mid(6));
            }), m_data.layers.end());
        bool changed = m_data.layers.size() != oldSize;
        for (const QString &name : names) {
            const bool exists = std::any_of(m_data.layers.cbegin(), m_data.layers.cend(),
                [&name](const DeploymentLayer &layer) {
                    return layer.runtimeLayer && layer.source == QStringLiteral("earth:") + name;
                });
            if (!exists) {
                DeploymentLayer layer;
                layer.id = newId(QStringLiteral("runtime-layer"));
                layer.name = name;
                layer.kind = QStringLiteral("runtime");
                layer.driver = QStringLiteral("earth");
                layer.source = QStringLiteral("earth:") + name;
                layer.runtimeLayer = true;
                m_data.layers.append(layer);
                changed = true;
            }
        }
        if (changed) {
            saveFixed();
            rebuildTree();
        }
        updateScene();
    });

    QString errorMessage;
    if (!m_store.load(m_data, errorMessage))
        updateStatus(errorMessage, true);
    else {
        bool migratedModelScale = false;
        for (DeploymentMarker &marker : m_data.markers) {
            if (marker.geometryType != QStringLiteral("model")
                && marker.geometryType != QStringLiteral("tilt-model")) {
                continue;
            }
            const double range = std::abs(marker.viewpoint.range);
            const double previousAutomaticSize = std::max(10.0, range / 10.0);
            const double tolerance = std::max(1.0, previousAutomaticSize * 0.05);
            if (range > 0.0 && marker.pointSize > 10.0
                && std::abs(marker.pointSize - previousAutomaticSize)
                    <= tolerance) {
                marker.pointSize = 10.0;
                migratedModelScale = true;
            }
        }
        if (migratedModelScale) {
            QString saveError;
            if (!m_store.saveFixedAnnotations(m_data, saveError))
                updateStatus(saveError, true);
        }
        updateStatus(QStringLiteral("本地部署数据已加载：%1").arg(m_store.directory()));
    }

    rebuildTree();
    rebuildTimeline();
    updateScene();
}

QWidget *ZqbsPage::leftPanel() const { return m_leftPanel; }
QWidget *ZqbsPage::rightPanel() const { return m_rightPanel; }
QWidget *ZqbsPage::topPanel() const { return m_topPanel; }
QWidget *ZqbsPage::bottomPanel() const { return m_bottomPanel; }

void ZqbsPage::createPanels()
{
    m_leftPanel = createLeftPanel();
    m_rightPanel = createRightPanel();
    m_topPanel = createTopPanel();
    m_bottomPanel = createBottomPanel();
    createSceneOverlays();
    updateDomainChrome();
}

QWidget *ZqbsPage::createLeftPanel()
{
    auto *panel = new QWidget;
    panel->setObjectName(QStringLiteral("deploymentLeftPanel"));
    panel->setFixedWidth(260);
    panel->setStyleSheet(panelStyle());
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 9);
    layout->setSpacing(7);
    auto *heading = new QWidget(panel);
    heading->setObjectName(QStringLiteral("deploymentSectionHeading"));
    auto *headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(12, 8, 12, 8);
    headingLayout->setSpacing(1);
    m_domainTitle = new QLabel(panel);
    m_domainTitle->setObjectName(QStringLiteral("deploymentSectionTitle"));
    m_domainSubtitle = new QLabel(panel);
    m_domainSubtitle->setObjectName(QStringLiteral("deploymentSectionSubtitle"));
    headingLayout->addWidget(m_domainTitle);
    headingLayout->addWidget(m_domainSubtitle);
    layout->addWidget(heading);
    m_filter = new QLineEdit(panel);
    m_filter->setPlaceholderText(QStringLiteral("筛选当前数据…"));
    m_filter->setContentsMargins(8, 0, 8, 0);
    layout->addWidget(m_filter);
    m_tree = new QTreeWidget(panel);
    m_tree->setHeaderHidden(true);
    m_tree->setObjectName(QStringLiteral("deploymentSceneTree"));
    m_tree->setToolTip(QStringLiteral("右键可添加、删除或编辑属性；空白处可新建预案/文件夹"));
    m_tree->setAlternatingRowColors(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    auto *treeContainer = new QWidget(panel);
    auto *treeLayout = new QVBoxLayout(treeContainer);
    treeLayout->setContentsMargins(8, 0, 8, 0);
    treeLayout->addWidget(m_tree);
    layout->addWidget(treeContainer, 1);
    connect(m_filter, &QLineEdit::textChanged, this, [this] { rebuildTree(); });
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &ZqbsPage::treeItemChanged);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ZqbsPage::treeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &ZqbsPage::treeItemActivated);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ZqbsPage::showSceneTreeContextMenu);
    return panel;
}

QWidget *ZqbsPage::createRightPanel()
{
    auto *panel = new QWidget;
    panel->setObjectName(QStringLiteral("deploymentRightPanel"));
    panel->setFixedWidth(260);
    panel->setStyleSheet(panelStyle());
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    m_inspectorTitle = new QLabel(QStringLiteral("属性与数据绑定"), panel);
    m_inspectorTitle->setObjectName(QStringLiteral("deploymentInspectorTitle"));
    layout->addWidget(m_inspectorTitle);
    m_inspector = new QTextBrowser(panel);
    m_inspector->setOpenExternalLinks(false);
    layout->addWidget(m_inspector, 1);
    auto *hint = new QLabel(QStringLiteral(
        "双击标记/视角可定位。方案与标记关联动作保存在当前项目。"), panel);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#b8d9f5;font-size:11px;"));
    layout->addWidget(hint);
    return panel;
}

QPushButton *ZqbsPage::createDomainButton(const QString &text, Domain domain)
{
    auto *button = createToolButton(text);
    button->setProperty("deploymentDomain", true);
    button->setProperty("domainIndex", static_cast<int>(domain));
    button->setCheckable(true);
    button->setMinimumWidth(210);
    m_domainButtons->addButton(button, static_cast<int>(domain));
    return button;
}

QPushButton *ZqbsPage::createToolButton(const QString &text)
{
    auto *button = new QPushButton(text);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QWidget *ZqbsPage::createTopPanel()
{
    auto *panel = new QWidget;
    panel->setObjectName(QStringLiteral("deploymentTopPanel"));
    panel->setFixedHeight(66);
    panel->setStyleSheet(panelStyle());
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(2);
    m_domainButtons = new QButtonGroup(this);
    m_domainButtons->setExclusive(true);
    auto *fixed = createDomainButton(QStringLiteral("01  基础场景\n空间部署 · 固定图层"), Domain::Fixed);
    auto *normal = createDomainButton(QStringLiteral("02  部署标注\n本地标记 · 单击关联动作"), Domain::Normal);
    auto *dynamic = createDomainButton(QStringLiteral("03  动态演练\n演练方案 · 时间轴演练"), Domain::Dynamic);
    m_domainButtons->button(static_cast<int>(m_domain))->setChecked(true);
    layout->addWidget(fixed);
    layout->addWidget(normal);
    layout->addWidget(dynamic);
    layout->addStretch();
    connect(m_domainButtons, &QButtonGroup::idClicked, this,
            [this](int id) { setDomain(static_cast<Domain>(id)); });
    return panel;
}

QWidget *ZqbsPage::createBottomPanel()
{
    auto *panel = new QWidget;
    panel->setObjectName(QStringLiteral("deploymentBottomPanel"));
    panel->setMinimumHeight(104);
    panel->setMaximumHeight(154);
    panel->setStyleSheet(panelStyle());
    auto *root = new QVBoxLayout(panel);
    root->setContentsMargins(8, 6, 8, 4);
    root->setSpacing(4);

    m_summaryPanel = new QWidget(panel);
    m_summaryPanel->setObjectName(QStringLiteral("deploymentSummaryPanel"));
    auto *summaryLayout = new QHBoxLayout(m_summaryPanel);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(6);
    for (int index = 0; index < 4; ++index) {
        auto *card = new QFrame(m_summaryPanel);
        card->setObjectName(QStringLiteral("deploymentSummaryCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 7, 12, 7);
        cardLayout->setSpacing(1);
        auto *label = new QLabel(card);
        label->setObjectName(QStringLiteral("deploymentSummaryLabel"));
        auto *value = new QLabel(QStringLiteral("0"), card);
        value->setObjectName(QStringLiteral("deploymentSummaryValue"));
        cardLayout->addWidget(label);
        cardLayout->addWidget(value);
        summaryLayout->addWidget(card, 1);
        m_summaryLabels.append(label);
        m_summaryValues.append(value);
    }
    root->addWidget(m_summaryPanel, 1);

    m_timelinePanel = new QWidget(panel);
    m_timelinePanel->setObjectName(QStringLiteral("deploymentTimelinePanel"));
    auto *timelineRoot = new QVBoxLayout(m_timelinePanel);
    timelineRoot->setContentsMargins(8, 5, 8, 5);
    timelineRoot->setSpacing(4);
    auto *timelineControls = new QHBoxLayout;
    auto *timelineTitle = new QLabel(QStringLiteral("预案推演时间轴"), m_timelinePanel);
    timelineTitle->setObjectName(QStringLiteral("deploymentTimelineTitle"));
    auto *previous = createToolButton(QStringLiteral("上一步"));
    m_playButton = createToolButton(QStringLiteral("播放"));
    auto *next = createToolButton(QStringLiteral("下一步"));
    auto *stop = createToolButton(QStringLiteral("停止"));
    previous->setObjectName(QStringLiteral("dynamicPreviousStep"));
    m_playButton->setObjectName(QStringLiteral("dynamicPlayPause"));
    next->setObjectName(QStringLiteral("dynamicNextStep"));
    stop->setObjectName(QStringLiteral("dynamicStop"));
    m_stepCounter = new QLabel(QStringLiteral("0 / 0"), m_timelinePanel);
    m_stepCounter->setObjectName(QStringLiteral("deploymentStepCounter"));
    m_stepDescription = new QLabel(QStringLiteral("请选择预案和步骤"), m_timelinePanel);
    m_stepDescription->setObjectName(QStringLiteral("deploymentStepDescription"));
    timelineControls->addWidget(timelineTitle);
    timelineControls->addSpacing(8);
    timelineControls->addWidget(previous);
    timelineControls->addWidget(m_playButton);
    timelineControls->addWidget(next);
    timelineControls->addWidget(stop);
    timelineControls->addStretch();
    timelineControls->addWidget(m_stepCounter);
    timelineControls->addWidget(m_stepDescription);
    timelineRoot->addLayout(timelineControls);
    m_timelineStepsPanel = new QWidget(m_timelinePanel);
    m_timelineStepsPanel->setObjectName(QStringLiteral("deploymentTimelineSteps"));
    m_timelineStepsLayout = new QHBoxLayout(m_timelineStepsPanel);
    m_timelineStepsLayout->setContentsMargins(6, 4, 6, 4);
    m_timelineStepsLayout->setSpacing(4);
    timelineRoot->addWidget(m_timelineStepsPanel);
    m_timeline = new QSlider(Qt::Horizontal, m_timelinePanel);
    m_timeline->hide();
    root->addWidget(m_timelinePanel, 1);

    m_status = new QLabel(panel);
    m_status->setObjectName(QStringLiteral("deploymentStatus"));
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    root->addWidget(m_status);
    connect(previous, &QPushButton::clicked, this, &ZqbsPage::previousPlanStep);
    connect(next, &QPushButton::clicked, this, &ZqbsPage::nextPlanStep);
    connect(stop, &QPushButton::clicked, this, &ZqbsPage::stopPlan);
    connect(m_playButton, &QPushButton::clicked, this, [this] {
        if (m_planState == PlanState::Playing) pausePlan(); else playPlan();
    });
    connect(m_timeline, &QSlider::valueChanged, this, [this](int value) {
        if (!m_timeline->isSliderDown())
            return;
        showPlanStep(value, false);
    });
    return panel;
}

void ZqbsPage::createSceneOverlays()
{
    m_scene->installEventFilter(this);
    m_sceneTools = new QWidget(m_scene);
    m_sceneTools->setObjectName(QStringLiteral("deploymentSceneTools"));
    auto *barLayout = new QHBoxLayout(m_sceneTools);
    barLayout->setContentsMargins(4, 4, 4, 4);
    barLayout->setSpacing(3);
    m_sceneToolToggle = createToolButton(QString());
    m_sceneToolToggle->setProperty("sceneToolToggle", true);
    m_sceneToolToggle->setFixedSize(36, 34);
    m_sceneToolToggle->setIconSize(QSize(15, 15));
    barLayout->addWidget(m_sceneToolToggle);
    m_sceneToolContent = new QWidget(m_sceneTools);
    m_sceneToolContent->setObjectName(QStringLiteral("deploymentSceneToolContent"));
    auto *layout = new QHBoxLayout(m_sceneToolContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    const QStringList names{QStringLiteral("测量"), QStringLiteral("环境"),
        QStringLiteral("视角"), QStringLiteral("工具"), QStringLiteral("飞行")};
    QVector<QPushButton *> buttons;
    for (const QString &name : names) {
        auto *button = createToolButton(name);
        button->setProperty("sceneTool", true);
        button->setFixedSize(72, 34);
        buttons.append(button);
        layout->addWidget(button);
    }
    m_fullScreenButton = createToolButton(QStringLiteral("地图全屏"));
    m_fullScreenButton->setObjectName(QStringLiteral("deploymentFullScreenButton"));
    m_fullScreenButton->setProperty("sceneTool", true);
    m_fullScreenButton->setFixedSize(112, 34);
    m_fullScreenButton->setCheckable(true);
    layout->addWidget(m_fullScreenButton);
    barLayout->addWidget(m_sceneToolContent);
    connect(buttons.at(0), &QPushButton::clicked, this, &ZqbsPage::showMeasurementMenu);
    connect(buttons.at(1), &QPushButton::clicked, this, &ZqbsPage::showEnvironmentMenu);
    connect(buttons.at(2), &QPushButton::clicked, this, &ZqbsPage::showViewpointMenu);
    connect(buttons.at(3), &QPushButton::clicked, this, &ZqbsPage::showToolMenu);
    buttons.at(4)->setObjectName(QStringLiteral("deploymentFlightButton"));
    connect(buttons.at(4), &QPushButton::clicked, this, &ZqbsPage::showFlightDialog);
    connect(m_fullScreenButton, &QPushButton::clicked,
            this, &ZqbsPage::mapFullScreenRequested);
    const auto updateToggleIcon = [this] {
        if (!m_sceneToolToggle || !m_sceneTools)
            return;
        m_sceneToolToggle->setText(QString());
        m_sceneToolToggle->setIcon(whiteChevronIcon(!m_sceneToolsExpanded));
        m_sceneToolToggle->setToolTip(m_sceneToolsExpanded
            ? QStringLiteral("收起地图工具栏")
            : QStringLiteral("展开地图工具栏"));
    };
    updateToggleIcon();
    connect(m_sceneToolToggle, &QPushButton::clicked, this,
            [this, updateToggleIcon] {
        m_sceneToolsExpanded = !m_sceneToolsExpanded;
        if (m_sceneToolContent)
            m_sceneToolContent->setVisible(m_sceneToolsExpanded);
        updateToggleIcon();
        positionSceneOverlays();
    });
    m_sceneTools->setStyleSheet(panelStyle() + QStringLiteral(R"(
        #deploymentSceneTools {
            background:rgba(7,38,73,224);
            border:1px solid #2f92d0;
            border-radius:3px;
        }
        QPushButton[sceneToolToggle="true"] {
            color:#d9f3ff;
            background:#0b4a82;
            border:1px solid #3aa6df;
            min-width:36px;
            max-width:36px;
            min-height:34px;
            max-height:34px;
            padding:0;
            font-weight:600;
        }
        QPushButton[sceneToolToggle="true"]:hover {
            background:#12619f;
        }
    )"));
    m_sceneTools->hide();
}

void ZqbsPage::setMapFullScreen(bool fullScreen)
{
    if (!m_fullScreenButton || m_mapFullScreen == fullScreen)
        return;
    if (fullScreen)
        m_sceneToolsExpandedBeforeFullScreen = m_sceneToolsExpanded;
    m_mapFullScreen = fullScreen;
    if (m_sceneToolToggle && m_sceneToolsExpanded !=
        (fullScreen ? true : m_sceneToolsExpandedBeforeFullScreen))
        m_sceneToolToggle->click();
    const QSignalBlocker blocker(m_fullScreenButton);
    m_fullScreenButton->setChecked(fullScreen);
    m_fullScreenButton->setText(fullScreen
        ? QStringLiteral("退出地图全屏") : QStringLiteral("地图全屏"));
    m_fullScreenButton->setToolTip(fullScreen
        ? QStringLiteral("退出地图全屏") : QStringLiteral("地图全屏"));
}

bool ZqbsPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scene && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        positionSceneOverlays();
    return QObject::eventFilter(watched, event);
}

void ZqbsPage::positionSceneOverlays()
{
    if (!m_scene || !m_sceneTools) return;
    m_sceneTools->adjustSize();
    // Keep the toggle fixed in the upper-left. Since the horizontal content
    // follows it in the layout, the bar always expands towards the right.
    m_sceneTools->move(12, 12);
    m_sceneTools->raise();
}

void ZqbsPage::setActive(bool active)
{
    m_active = active;
    m_scene->setDeploymentOverlayVisible(active);
    m_scene->setViewToolsActive(active);
    if (m_sceneTools) m_sceneTools->setVisible(active);
    if (active) {
        positionSceneOverlays();
        activate();
    } else {
        pausePlan();
        if (m_markerPropertyDialog)
            m_markerPropertyDialog->reject();
        if (m_viewpointLocateDialog)
            m_viewpointLocateDialog->reject();
        if (m_flightDialog)
            m_flightDialog->reject();
        if (m_environmentDialog)
            m_environmentDialog->reject();
        if (m_blackboardPanel)
            m_blackboardPanel->close();
        m_scene->cancelDeploymentInteraction();
    }
}

void ZqbsPage::activate()
{
    updateDomainChrome();
    rebuildTree();
    rebuildTimeline();
    updateScene();
}

void ZqbsPage::setDomain(Domain domain)
{
    m_domain = domain;
    if (auto *button = m_domainButtons->button(static_cast<int>(domain)))
        button->setChecked(true);
    updateDomainChrome();
    rebuildTree();
    rebuildTimeline();
    updateBottomSummary();
}

void ZqbsPage::updateDomainChrome()
{
    QString title;
    QString subtitle;
    QString inspector;
    if (m_domain == Domain::Fixed) {
        title = QStringLiteral("场景列表");
        subtitle = QStringLiteral("系统 · 自定义 · 标记 · 标绘 · 视点");
        inspector = QStringLiteral("场景对象属性");
    } else if (m_domain == Domain::Normal) {
        title = QStringLiteral("部署标注");
        subtitle = QStringLiteral("本地标记 · 鼠标单击关联动作");
        inspector = QStringLiteral("标注属性与实时数据绑定");
    } else {
        title = QStringLiteral("动态演练");
        subtitle = QStringLiteral("右键新建、删除、属性 · 分步骤推演");
        inspector = QStringLiteral("预案步骤与推演视角");
    }
    if (m_domainTitle) m_domainTitle->setText(title);
    if (m_domainSubtitle) m_domainSubtitle->setText(subtitle);
    if (m_inspectorTitle) m_inspectorTitle->setText(inspector);
    if (m_filter) m_filter->setPlaceholderText(QStringLiteral("筛选%1…").arg(title));
}

void ZqbsPage::updateBottomSummary()
{
    if (m_summaryLabels.size() < 4 || m_summaryValues.size() < 4) return;
    QStringList labels;
    QStringList values;
    if (m_domain == Domain::Fixed) {
        const int visible = std::count_if(m_data.layers.cbegin(), m_data.layers.cend(),
            [](const DeploymentLayer &layer) { return layer.visible; });
        const int plots = std::count_if(m_data.markers.cbegin(), m_data.markers.cend(),
            [](const DeploymentMarker &marker) { return isPlotAnnotation(marker.geometryType); });
        labels = {QStringLiteral("场景图层"), QStringLiteral("当前可见"),
                  QStringLiteral("标记 / 标绘"), QStringLiteral("视点图层")};
        values = {QString::number(m_data.layers.size()), QString::number(visible),
                  QStringLiteral("%1 / %2").arg(m_data.markers.size() - plots).arg(plots),
                  QString::number(m_data.viewpoints.size())};
    } else if (m_domain == Domain::Normal) {
        int actions = 0;
        for (const auto &marker : m_data.normalMarkers) actions += marker.association.value("actions").toArray().size();
        labels = {QStringLiteral("部署标注"), QStringLiteral("分组"), QStringLiteral("关联动作"), QStringLiteral("存储")};
        values = {QString::number(m_data.normalMarkers.size()), QString::number(m_data.normalFolders.size()),
                  QString::number(actions), QStringLiteral("本地项目")};
    } else {
        int steps = 0;
        int markers = 0;
        for (const DeploymentPlan &plan : m_data.plans) {
            steps += plan.steps.size();
            for (const DeploymentPlanStep &step : plan.steps) markers += step.markers.size();
        }
        labels = {QStringLiteral("演练方案"), QStringLiteral("演练步骤"),
                  QStringLiteral("动态标记"), QStringLiteral("当前步骤")};
        values = {QString::number(m_data.plans.size()), QString::number(steps),
                  QString::number(markers), m_currentStep < 0 ? QStringLiteral("—")
                                                              : QString::number(m_currentStep + 1)};
    }
    for (int index = 0; index < 4; ++index) {
        m_summaryLabels.at(index)->setText(labels.at(index));
        m_summaryValues.at(index)->setText(values.at(index));
    }
}

void ZqbsPage::rebuildTree()
{
    if (!m_tree)
        return;
    const QSet<QString> expandedKeys = expandedTreeItemKeys(m_tree);
    const QString currentKey = m_tree->currentItem()
        ? treeItemStateKey(m_tree->currentItem()) : QString();
    m_treeRebuilding = true;
    QSignalBlocker blocker(m_tree);
    m_tree->clear();
    updateDomainChrome();
    const QString filter = m_filter ? m_filter->text().trimmed() : QString();
    const auto matches = [&filter](const QString &text) {
        return filter.isEmpty() || text.contains(filter, Qt::CaseInsensitive);
    };
    const auto checkable = [](QTreeWidgetItem *item, bool checked) {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    };

    if (m_domain == Domain::Fixed) {
        auto *systemRoot = makeItem(nullptr, QStringLiteral("系统图层"),
                                    QStringLiteral("scene-system-root"), {});
        systemRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("layer_effect.png"))));
        m_tree->addTopLevelItem(systemRoot);
        auto *earthRoot = makeItem(systemRoot, QStringLiteral("地图配置图层"),
                                   QStringLiteral("scene-system-folder"), {});
        earthRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("folder.png"))));
        const auto featureCount = std::count_if(m_data.layers.cbegin(), m_data.layers.cend(),
            [](const DeploymentLayer &layer) { return layer.systemLayer; });
        auto *featureRoot = makeItem(systemRoot, QStringLiteral("系统矢量图层（%1）").arg(featureCount),
                                     QStringLiteral("scene-system-feature-folder"), {});
        featureRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("load_vector.png"))));
        int earthVisible = 0;
        int featureVisible = 0;
        for (const DeploymentLayer &layer : m_data.layers) {
            if ((!layer.runtimeLayer && !layer.systemLayer) || !matches(layer.name))
                continue;
            QTreeWidgetItem *parent = layer.systemLayer ? featureRoot : earthRoot;
            auto *item = makeItem(parent, layer.name, QStringLiteral("layer"),
                                  layer.id, layer.systemLayer
                                      ? QStringLiteral("system-feature")
                                      : QStringLiteral("system"));
            item->setIcon(0, QIcon(deploymentIcon(layer.systemLayer
                ? QStringLiteral("load_vector.png")
                : QStringLiteral("load_image.png"))));
            checkable(item, layer.visible);
            if (layer.systemLayer)
                featureVisible += layer.visible ? 1 : 0;
            else
                earthVisible += layer.visible ? 1 : 0;
        }
        checkable(earthRoot, earthRoot->childCount() > 0
            && earthVisible == earthRoot->childCount());
        checkable(featureRoot, featureRoot->childCount() > 0
            && featureVisible == featureRoot->childCount());
        const int systemCount = earthRoot->childCount() + featureRoot->childCount();
        checkable(systemRoot, systemCount > 0
            && earthVisible + featureVisible == systemCount);

        auto *customRoot = makeItem(nullptr, QStringLiteral("自定义图层"),
                                    QStringLiteral("scene-custom-root"), {});
        customRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("load_vector.png"))));
        m_tree->addTopLevelItem(customRoot);
        auto *imageryRoot = makeItem(customRoot, QStringLiteral("影像图层"),
                                     QStringLiteral("scene-custom-kind"), {}, QStringLiteral("imagery"));
        auto *elevationRoot = makeItem(customRoot, QStringLiteral("高程图层"),
                                       QStringLiteral("scene-custom-kind"), {}, QStringLiteral("elevation"));
        auto *vectorRoot = makeItem(customRoot, QStringLiteral("矢量图层"),
                                    QStringLiteral("scene-custom-kind"), {}, QStringLiteral("vector"));
        imageryRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("load_image.png"))));
        elevationRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("load_altitude.png"))));
        vectorRoot->setIcon(0, QIcon(deploymentIcon(QStringLiteral("load_vector.png"))));
        QHash<QString, QTreeWidgetItem *> customKinds{{QStringLiteral("imagery"), imageryRoot},
            {QStringLiteral("elevation"), elevationRoot}, {QStringLiteral("vector"), vectorRoot}};
        QHash<QString, int> customVisible;
        for (const DeploymentLayer &layer : m_data.layers) {
            if (layer.runtimeLayer || layer.systemLayer || !matches(layer.name))
                continue;
            QTreeWidgetItem *parent = customKinds.value(layer.kind, vectorRoot);
            auto *item = makeItem(parent, layer.name, QStringLiteral("layer"),
                                  layer.id, QStringLiteral("custom"));
            item->setIcon(0, parent->icon(0));
            checkable(item, layer.visible);
            customVisible[layer.kind] += layer.visible ? 1 : 0;
        }
        int customCount = 0;
        int customVisibleCount = 0;
        for (auto it = customKinds.cbegin(); it != customKinds.cend(); ++it) {
            checkable(it.value(), it.value()->childCount() > 0
                && customVisible.value(it.key()) == it.value()->childCount());
            customCount += it.value()->childCount();
            customVisibleCount += customVisible.value(it.key());
        }
        checkable(customRoot, customCount > 0 && customVisibleCount == customCount);

        const auto buildMarkerBranch = [this, &matches, &checkable](bool plot) {
            const QString rootType = plot ? QStringLiteral("scene-plot-root")
                                          : QStringLiteral("scene-mark-root");
            auto *root = makeItem(nullptr, plot ? QStringLiteral("标绘图层")
                                                 : QStringLiteral("标记图层"), rootType, {});
            root->setIcon(0, QIcon(plot ? QStringLiteral(":/legacy/images/toolbar/Plot.png")
                                        : QStringLiteral(":/legacy/images/toolbar/mark.png")));
            m_tree->addTopLevelItem(root);
            QHash<QString, QTreeWidgetItem *> folderItems;
            QHash<QString, bool> emptyFolderVisibility;
            for (const DeploymentSceneFolder &folder : m_data.sceneFolders) {
                if (folder.plot != plot || !matches(folder.name))
                    continue;
                auto *item = makeItem(root, folder.name,
                    plot ? QStringLiteral("scene-plot-folder")
                         : QStringLiteral("scene-mark-folder"),
                    folder.id, folder.name);
                item->setIcon(0, QIcon(deploymentIcon(QStringLiteral("folder.png"))));
                checkable(item, folder.visible);
                folderItems.insert(folder.name, item);
                emptyFolderVisibility.insert(folder.name, folder.visible);
            }
            int shown = 0;
            int visible = 0;
            for (const DeploymentMarker &marker : m_data.markers) {
                if (isPlotAnnotation(marker.geometryType) != plot || !matches(marker.name))
                    continue;
                QString category = marker.category.trimmed();
                if (category.isEmpty())
                    category = plot ? QStringLiteral("战术标绘") : QStringLiteral("地图标记");
                QTreeWidgetItem *folder = folderItems.value(category);
                if (!folder) {
                    folder = makeItem(root, category,
                        plot ? QStringLiteral("scene-plot-folder")
                             : QStringLiteral("scene-mark-folder"), {}, category);
                    folder->setIcon(0, QIcon(deploymentIcon(QStringLiteral("folder.png"))));
                    checkable(folder, true);
                    folderItems.insert(category, folder);
                }
                auto *item = makeItem(folder,
                    marker.name,
                    QStringLiteral("marker"), marker.id,
                    plot ? QStringLiteral("plot") : QStringLiteral("mark"));
                item->setIcon(0, QIcon(annotationIcon(marker.geometryType)));
                checkable(item, marker.visible);
                ++shown;
                visible += marker.visible ? 1 : 0;
            }
            for (auto it = folderItems.cbegin(); it != folderItems.cend(); ++it) {
                int childVisible = 0;
                for (int index = 0; index < it.value()->childCount(); ++index)
                    childVisible += it.value()->child(index)->checkState(0) == Qt::Checked ? 1 : 0;
                checkable(it.value(), it.value()->childCount() == 0
                    ? emptyFolderVisibility.value(it.key(), true)
                    : childVisible == it.value()->childCount());
            }
            checkable(root, shown > 0 && visible == shown);
        };
        buildMarkerBranch(false);
        buildMarkerBranch(true);

        auto *viewsRoot = makeItem(nullptr, QStringLiteral("视点图层"),
                                   QStringLiteral("scene-viewpoint-root"), {});
        viewsRoot->setIcon(0, QIcon(QStringLiteral(":/legacy/images/toolbar/viewpoint.png")));
        m_tree->addTopLevelItem(viewsRoot);
        for (const DeploymentViewpoint &view : m_data.viewpoints) {
            if (!matches(view.name))
                continue;
            auto *item = makeItem(viewsRoot, view.name, QStringLiteral("viewpoint"), view.id);
            item->setIcon(0, QIcon(deploymentIcon(QStringLiteral("viewpoint_locate.png"))));
        }
    } else if (m_domain == Domain::Normal) {
        buildNormalAnnotationsTree(m_tree, m_data, filter, annotationIcon);
    } else {
        for (const DeploymentPlan &plan : m_data.plans) {
            if (!matches(plan.name) && filter.size() > 0) {
                const bool stepMatches = std::any_of(plan.steps.cbegin(), plan.steps.cend(),
                    [&matches](const DeploymentPlanStep &step) {
                        return matches(step.name) || std::any_of(step.markers.cbegin(), step.markers.cend(),
                            [&matches](const DeploymentMarker &marker) { return matches(marker.name); });
                    });
                if (!stepMatches) continue;
            }
            auto *planItem = makeItem(nullptr,
                QStringLiteral("预案 · %1").arg(plan.name),
                QStringLiteral("plan"), plan.id);
            planItem->setIcon(0, QIcon(deploymentIcon(QStringLiteral("folder.png"))));
            planItem->setToolTip(0, QStringLiteral("右键：添加演练步骤、属性、删除"));
            m_tree->addTopLevelItem(planItem);
            for (qsizetype index = 0; index < plan.steps.size(); ++index) {
                const DeploymentPlanStep &step = plan.steps.at(index);
                auto *item = makeItem(planItem,
                    QStringLiteral("%1. %2（%3 秒）")
                        .arg(index + 1).arg(step.name).arg(dynamicStepDurationMs(step) / 1000),
                    QStringLiteral("step"), step.id, plan.id);
                item->setIcon(0, QIcon(deploymentIcon(QStringLiteral("folder.png"))));
                item->setToolTip(0, QStringLiteral("右键：添加动态标记、属性、删除"));
                for (const DeploymentMarker &marker : step.markers) {
                    auto *leaf = makeItem(item, marker.name,
                             QStringLiteral("dynamic-marker"), marker.id,
                             plan.id + QLatin1Char('|') + step.id);
                    leaf->setIcon(0, QIcon(annotationIcon(marker.geometryType)));
                    leaf->setToolTip(0, QStringLiteral("右键：上移、添加动态标记、属性、删除"));
                }
            }
        }
    }
    // Rebuilding after an attribute edit must not expand every system layer.
    // Restore only the branches the operator had explicitly opened.
    for (QTreeWidgetItemIterator iterator(m_tree); *iterator; ++iterator) {
        QTreeWidgetItem *item = *iterator;
        item->setExpanded(expandedKeys.contains(treeItemStateKey(item)));
        if (!currentKey.isEmpty() && treeItemStateKey(item) == currentKey)
            m_tree->setCurrentItem(item);
    }
    m_treeRebuilding = false;
    updateBottomSummary();
}

void ZqbsPage::rebuildTimeline()
{
    const bool dynamic = m_domain == Domain::Dynamic;
    m_timelinePanel->setVisible(dynamic);
    m_summaryPanel->setVisible(!dynamic);
    if (!dynamic)
        return;
    // Do not destroy/recreate the widget holding the mouse grab during a drag.
    if (m_timelineScrubbing) { updateTimelineProgress(); return; }
    DeploymentPlan *plan = selectedPlan();
    const int count = plan ? plan->steps.size() : 0;
    const QSignalBlocker blocker(m_timeline);
    m_timeline->setRange(0, qMax(0, count - 1));
    m_timeline->setEnabled(count > 0);
    m_stepCounter->setText(count > 0
        ? QStringLiteral("%1 / %2").arg(m_currentStep >= 0 && m_currentStep < count ? m_currentStep + 1 : 0).arg(count)
        : QStringLiteral("0 / 0"));
    if (count == 0) {
        m_stepDescription->setText(QStringLiteral("请选择预案并新增步骤"));
        m_playButton->setEnabled(false);
    } else {
        m_playButton->setEnabled(true);
        if (m_currentStep >= 0 && m_currentStep < count) {
            m_timeline->setValue(m_currentStep);
            const auto &step = plan->steps.at(m_currentStep);
            m_stepDescription->setText((step.description.isEmpty() ? step.name : step.description)
                + (step.sequentialDisplay ? QStringLiteral(" · 顺序显示") : QStringLiteral(" · 同时显示")));
        } else {
            m_stepDescription->setText(QStringLiteral("尚未播放，演练标记已隐藏"));
        }
    }
    while (QLayoutItem *item = m_timelineStepsLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->setObjectName(QString());
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_stepButtons.clear();
    if (!plan || count == 0) {
        auto *empty = new QLabel(QStringLiteral("选择左侧预案后新增步骤，推演节点将在此按时间顺序展开"),
                                 m_timelineStepsPanel);
        empty->setObjectName(QStringLiteral("deploymentTimelineEmpty"));
        m_timelineStepsLayout->addWidget(empty);
        m_timelineStepsLayout->addStretch();
    } else {
        for (int index = 0; index < count; ++index) {
            const DeploymentPlanStep &step = plan->steps.at(index);
            auto *button = new DynamicTimelineButton(QStringLiteral("%1\n%2")
                .arg(index + 1, 2, 10, QLatin1Char('0')).arg(step.name), m_timelineStepsPanel);
            button->setObjectName(QStringLiteral("dynamicTimelineStep_%1").arg(index));
            button->setToolTip(QStringLiteral("%1 · %2 秒\n点击或左右拖动定位时间，可跨阶段拖动")
                .arg(step.name).arg(dynamicStepDurationMs(step) / 1000));
            button->seekRequested = [guard = QPointer<ZqbsPage>(this)](
                const QPoint &position, DynamicTimelineButton::SeekGesture gesture) {
                if (guard) guard->scrubTimeline(position, static_cast<int>(gesture));
            };
            connect(button, &QPushButton::clicked, this,
                    [this, index] { showPlanStep(index, false); });
            m_timelineStepsLayout->addWidget(button, 1);
            m_stepButtons.append(button);
        }
    }
    updateTimelineProgress();
    updateBottomSummary();
}

void ZqbsPage::updateTimelineProgress()
{
    if (m_domain != Domain::Dynamic) return;
    const auto *plan = findDynamicPlan(m_data, m_selectedPlanId);
    if (!plan) return;
    const bool timed = m_planState == PlanState::Playing || m_planState == PlanState::Paused
        || m_planState == PlanState::Preview;
    const bool completed = m_planState == PlanState::Completed && m_playbackPlanId == plan->id;
    if (completed) {
        m_stepCounter->setText(QStringLiteral("%1 / %1").arg(plan->steps.size()));
        m_stepDescription->setText(QStringLiteral("演练已完成，全部演练标记已保留"));
    } else if (m_currentStep >= 0 && m_currentStep < plan->steps.size()) {
        const auto &step = plan->steps[m_currentStep];
        m_stepCounter->setText(QStringLiteral("%1 / %2").arg(m_currentStep + 1).arg(plan->steps.size()));
        m_stepDescription->setText((step.description.isEmpty() ? step.name : step.description)
            + (step.sequentialDisplay ? QStringLiteral(" · 顺序显示") : QStringLiteral(" · 同时显示")));
    }
    const qint64 elapsed = planElapsedMs();
    for (int index = 0; index < m_stepButtons.size() && index < plan->steps.size(); ++index) {
        const auto &step = plan->steps[index];
        const qint64 duration = dynamicStepDurationMs(step);
        const bool current = index == m_currentStep;
        const qint64 shownTime = completed || (timed && index < m_currentStep)
            ? duration : (timed && current ? qMin(elapsed, duration) : 0);
        static_cast<DynamicTimelineButton *>(m_stepButtons[index])->setPlaybackProgress(
            completed || (timed && index < m_currentStep) ? 1.0 : dynamicStepProgress(step, shownTime),
            shownTime, duration, current);
    }
}

void ZqbsPage::updateInspector(QTreeWidgetItem *item)
{
    if (!item) {
        m_inspector->setHtml(QStringLiteral("<h3>方案演练</h3><p>从左侧选择数据查看属性。</p>"));
        return;
    }
    const QString type = item->data(0, ItemTypeRole).toString();
    const QString id = item->data(0, ItemIdRole).toString();
    QString html;
    if (type == QStringLiteral("layer")) {
        const auto found = std::find_if(m_data.layers.cbegin(), m_data.layers.cend(),
            [&id](const DeploymentLayer &value) { return value.id == id; });
        if (found != m_data.layers.cend()) {
            const QString identity = found->systemLayer ? QStringLiteral("系统矢量图层")
                : (found->runtimeLayer ? QStringLiteral("地图配置图层")
                                       : QStringLiteral("自定义图层"));
            QString detail;
            if (found->systemLayer) {
                detail = QStringLiteral("<p><b>显示范围：</b>%1 ～ %2 m</p>"
                                        "<p><b>样式：</b>%3</p>")
                    .arg(found->minRange, 0, 'f', 0)
                    .arg(found->maxRange, 0, 'f', 0)
                    .arg(found->styleName.toHtmlEscaped());
            }
            html = QStringLiteral("<h3>%1</h3><p><b>身份：</b>%2</p><p><b>类别：</b>%3</p>"
                                  "<p><b>驱动：</b>%4</p><p><b>来源：</b>%5</p>"
                                  "<p><b>透明度：</b>%6%</p>%7")
                .arg(found->name.toHtmlEscaped(), identity.toHtmlEscaped(),
                     found->kind.toHtmlEscaped(), found->driver.toHtmlEscaped(),
                     found->source.toHtmlEscaped())
                .arg(qRound(found->opacity * 100.0))
                .arg(detail);
        }
    } else if (type == QStringLiteral("measurement")) {
        const auto found = std::find_if(m_data.measurements.cbegin(), m_data.measurements.cend(),
            [&id](const DeploymentMeasurement &value) { return value.id == id; });
        if (found != m_data.measurements.cend()) {
            html = QStringLiteral("<h3>%1</h3><p><b>类型：</b>%2</p><p><b>结果：</b>%3</p><p><b>控制点：</b>%4</p>"
                                  "<p><b>地形采样：</b>%5</p><p>%6</p>")
                .arg(found->name.toHtmlEscaped(),
                     measurementDisplayName(found->kind),
                     measurementText(*found)).arg(found->points.size())
                .arg(found->terrainBacked ? QStringLiteral("是") : QStringLiteral("否"),
                     found->resultText.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
        }
    } else if (type == QStringLiteral("viewpoint")) {
        const auto found = std::find_if(m_data.viewpoints.cbegin(), m_data.viewpoints.cend(),
            [&id](const DeploymentViewpoint &value) { return value.id == id; });
        if (found != m_data.viewpoints.cend()) {
            html = QStringLiteral("<h3>%1</h3><p><b>中心：</b>%2, %3</p><p><b>航向/俯仰：</b>%4° / %5°</p><p><b>距离：</b>%6 m</p>")
                .arg(found->name.toHtmlEscaped()).arg(found->longitude, 0, 'f', 7)
                .arg(found->latitude, 0, 'f', 7).arg(found->heading, 0, 'f', 1)
                .arg(found->pitch, 0, 'f', 1).arg(found->range, 0, 'f', 1);
        }
    } else if (type == QStringLiteral("normal-folder")) {
        if (const auto *folder = normalFolder(m_data, id))
            html = QStringLiteral("<h3>%1</h3><p>%2</p><p>独立部署分组标注文件夹，可右键添加标记、标绘和子文件夹。</p>")
                .arg(folder->name.toHtmlEscaped(), folder->description.toHtmlEscaped());
    } else if (type == QStringLiteral("marker") || type == QStringLiteral("normal-marker") || type == QStringLiteral("live")) {
        const QVector<DeploymentMarker> &source = type == QStringLiteral("live")
            ? m_liveMarkers : (type == QStringLiteral("normal-marker") ? m_data.normalMarkers : m_data.markers);
        const auto found = std::find_if(source.cbegin(), source.cend(),
            [&id](const DeploymentMarker &value) { return value.id == id; });
        if (found != source.cend()) {
            html = QStringLiteral("<h3>%1</h3><p><b>类别：</b>%2</p><p><b>几何：</b>%3</p><p><b>坐标：</b>%4</p>"
                                  "<p><b>鼠标单击动作：</b>%5</p>")
                .arg(found->name.toHtmlEscaped(), found->category.toHtmlEscaped(),
                     found->geometryType.toHtmlEscaped(),
                     formattedCoordinate(found->position).toHtmlEscaped(),
                     QString::number(found->association.value("actions").toArray().size()));
        }
    } else if (type == QStringLiteral("binding-rule")) {
        const DeploymentBindingRule *rule = bindingRule(id);
        if (rule) {
            html = QStringLiteral("<h3>%1</h3><p><b>实体类型：</b>%2</p><p><b>显示：</b>%3</p>"
                                  "<p><b>颜色：</b><span style='color:%4'>%4</span></p>")
                .arg(rule->label.toHtmlEscaped(), rule->entityType.toHtmlEscaped(),
                     rule->visible ? QStringLiteral("是") : QStringLiteral("否"),
                     rule->color.name(QColor::HexRgb));
        }
    } else if (type == QStringLiteral("plan") || type == QStringLiteral("step")
               || type == QStringLiteral("dynamic-marker")) {
        const QString aux = item->data(0, ItemAuxRole).toString();
        const QString planId = type == QStringLiteral("plan") ? id
            : (type == QStringLiteral("dynamic-marker") ? aux.section(QLatin1Char('|'), 0, 0) : aux);
        const auto plan = std::find_if(m_data.plans.cbegin(), m_data.plans.cend(),
            [&planId](const DeploymentPlan &value) { return value.id == planId; });
        if (plan != m_data.plans.cend()) {
            html = QStringLiteral("<h3>%1</h3><p>%2</p><p><b>步骤数：</b>%3</p><p><b>循环：</b>%4</p>")
                .arg(plan->name.toHtmlEscaped(), plan->description.toHtmlEscaped())
                .arg(plan->steps.size()).arg(plan->loop ? QStringLiteral("是") : QStringLiteral("否"));
            if (type == QStringLiteral("step")) {
                const auto step = std::find_if(plan->steps.cbegin(), plan->steps.cend(),
                    [&id](const DeploymentPlanStep &value) { return value.id == id; });
                if (step != plan->steps.cend()) {
                    html += QStringLiteral("<hr><h4>%1</h4><p>%2</p><p><b>持续：</b>%3 秒</p><p><b>动态标记：</b>%4</p>")
                        .arg(step->name.toHtmlEscaped(), step->description.toHtmlEscaped())
                        .arg(dynamicStepDurationMs(*step) / 1000).arg(step->markers.size());
                    if (step->sequentialDisplay)
                        html += QStringLiteral("<p><b>每标记时长：</b>%1 秒（含飞行、0.5 秒闪烁及保持）</p>")
                            .arg(step->markerDurationSeconds);
                }
            } else if (type == QStringLiteral("dynamic-marker")) {
                const QString stepId = aux.section(QLatin1Char('|'), 1, 1);
                const auto step = std::find_if(plan->steps.cbegin(), plan->steps.cend(),
                    [&stepId](const DeploymentPlanStep &value) { return value.id == stepId; });
                if (step != plan->steps.cend()) {
                    const auto marker = std::find_if(step->markers.cbegin(), step->markers.cend(),
                        [&id](const DeploymentMarker &value) { return value.id == id; });
                    if (marker != step->markers.cend()) {
                        html += QStringLiteral("<hr><h4>%1</h4><p><b>坐标：</b>%2</p>")
                            .arg(marker->name.toHtmlEscaped(),
                                 formattedCoordinate(marker->position).toHtmlEscaped());
                    }
                }
            }
        }
    }
    if (html.isEmpty())
        html = QStringLiteral("<p>请选择具体数据项。</p>");
    m_inspector->setHtml(html);
}

void ZqbsPage::updateAssociatedMarkers(bool)
{
    m_scene->setDeploymentMarkers(composeDeploymentMarkers(m_data, {}, m_dynamicMarkers));
}

void ZqbsPage::handleMarkerTrigger(const QString &markerId, const QString &trigger,
                                   const QPoint &)
{
    if (!m_active || trigger != QStringLiteral("single")) return;
    const auto visible = composeDeploymentMarkers(m_data, {}, m_dynamicMarkers);
    const auto marker = std::find_if(visible.cbegin(), visible.cend(),
        [&markerId](const DeploymentMarker &value) { return value.visible && value.id == markerId; });
    if (marker == visible.cend()) return;
    for (const auto &value : marker->association.value(QStringLiteral("actions")).toArray()) {
        const auto action = MarkerActions::normalize(value.toObject());
        if (!action.isEmpty() && action.value(QStringLiteral("enabled")).toBool(true)
            && action.value(QStringLiteral("trigger")).toString() == QStringLiteral("single"))
            executeMarkerAction(action);
    }
}

void ZqbsPage::executeMarkerAction(const QJsonObject &action, bool automatic)
{
    if (!automatic && action.value(QStringLiteral("type")).toString() == QStringLiteral("video")) pausePlan();
    QString error;
    if (!MarkerActions::execute(action, m_scene, automatic, &error) && !error.isEmpty())
        updateStatus(QStringLiteral("关联动作失败：%1").arg(error), true);
}

void ZqbsPage::executeAutomaticMarkerActions(const DeploymentPlanStep &step,
                                            qint64 elapsedMs, bool consumeOnly)
{
    if (!consumeOnly && (!m_active || m_planState != PlanState::Playing)) return;
    // Reveal count is stable during blink, unlike the map's visible flag.
    const auto count = dynamicStepVisibleCount(step, elapsedMs);
    for (qsizetype index = 0; index < count; ++index) {
        const auto &marker = step.markers[index];
        if (!marker.visible) continue;
        const auto actions = marker.association.value(QStringLiteral("actions")).toArray();
        for (qsizetype actionIndex = 0; actionIndex < actions.size(); ++actionIndex) {
            const auto action = MarkerActions::normalize(actions[actionIndex].toObject());
            if (action.isEmpty() || !action.value(QStringLiteral("enabled")).toBool(true)
                || action.value(QStringLiteral("trigger")).toString() != QStringLiteral("auto")) continue;
            const QString key = QStringLiteral("%1/%2").arg(index).arg(actionIndex);
            if (m_executedAutoActions.contains(key)) continue;
            // Consume before execution to prevent timer reentry/retries on failure.
            m_executedAutoActions.insert(key);
            if (!consumeOnly) executeMarkerAction(action, true);
        }
    }
}

bool ZqbsPage::approveAutomaticPrograms(const DeploymentPlan &plan)
{
    QJsonArray programs;
    QStringList names;
    for (const auto &step : plan.steps) for (const auto &marker : step.markers) {
        if (!marker.visible) continue;
        for (const auto &value : marker.association.value(QStringLiteral("actions")).toArray()) {
            const auto action = MarkerActions::normalize(value.toObject());
            if (action.value(QStringLiteral("type")).toString() != QStringLiteral("program")
                || action.value(QStringLiteral("trigger")).toString() != QStringLiteral("auto")
                || !action.value(QStringLiteral("enabled")).toBool(true)) continue;
            programs.append(action);
            if (names.size() < 6) names.append(MarkerActions::target(action));
        }
    }
    if (programs.isEmpty()) return true;
    const auto signature = QCryptographicHash::hash(plan.id.toUtf8()
        + QJsonDocument(programs).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
    if (signature == m_approvedAutoPrograms) return true;
    if (QMessageBox::question(m_leftPanel, QStringLiteral("演练自动打开程序"),
        QStringLiteral("此预案包含 %1 个自动程序动作：\n%2\n\n允许本次演练自动运行这些程序？请仅播放可信预案。")
            .arg(programs.size()).arg(names.join(QLatin1Char('\n'))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return false;
    m_approvedAutoPrograms = signature;
    return true;
}

void ZqbsPage::updateScene()
{
    updateAssociatedMarkers(true);
    m_scene->setDeploymentMeasurements(m_data.measurements);
    m_scene->setEnvironmentSettings(m_environmentDialog
        ? m_environmentDialog->settings() : m_data.environment);
    QString layerError;
    const bool layersReady = m_scene->setDeploymentLayers(m_data.layers, layerError);
    if (!layersReady && !layerError.isEmpty() && layerError != m_lastLayerError) {
        m_lastLayerError = layerError;
        updateStatus(QStringLiteral("图层加载失败：%1").arg(layerError), true);
    } else if (layersReady) {
        m_lastLayerError.clear();
    }
    for (const DeploymentLayer &layer : m_data.layers) {
        if (layer.runtimeLayer && layer.source.startsWith(QStringLiteral("earth:")))
        {
            m_scene->setRuntimeLayerVisible(layer.source.mid(6), layer.visible);
            m_scene->setRuntimeLayerOpacity(layer.source.mid(6), layer.opacity);
        }
    }
}

void ZqbsPage::updateStatus(const QString &message, bool error)
{
    if (m_status) {
        m_status->setText(message);
        m_status->setStyleSheet(error
            ? QStringLiteral("color:#ffd080;background:transparent;")
            : QStringLiteral("color:#bce7ff;background:transparent;"));
        m_status->setToolTip(message);
    }
    emit statusChanged(message);
}

void ZqbsPage::saveFixed()
{
    QString error;
    if (!m_store.saveFixed(m_data, error)) updateStatus(error, true);
    else updateStatus(QStringLiteral("基础场景已保存"));
}

void ZqbsPage::saveFixedAnnotations()
{
    QString error;
    if (!m_store.saveFixedAnnotations(m_data, error)) updateStatus(error, true);
    else updateStatus(QStringLiteral("固定化标记标绘已保存"));
}

void ZqbsPage::saveNormal()
{
    QString error;
    if (!m_store.saveNormal(m_data, error)) updateStatus(error, true);
    else updateStatus(QStringLiteral("部署标注已保存到当前项目"));
}

void ZqbsPage::reloadXml()
{
    DeploymentData loaded;
    QString error;
    if (!m_store.load(loaded, error)) {
        updateStatus(error, true);
        return;
    }
    stopPlan();
    m_data = std::move(loaded);
    m_selectedPlanId.clear();
    m_selectedStepId.clear();
    m_currentStep = -1;
    m_dynamicMarkers.clear();
    rebuildTree();
    rebuildTimeline();
    updateScene();
    updateStatus(QStringLiteral("Data/Config 场景配置已重新载入"));
}

void ZqbsPage::showSceneTreeContextMenu(const QPoint &position)
{
    if (!m_tree) return;
    if (m_domain == Domain::Dynamic) {
        QTreeWidgetItem *target = m_tree->itemAt(position);
        if (target) m_tree->setCurrentItem(target);
        // Never retain a tree pointer across QMenu::exec: IMS refresh may replace it.
        const QString type = target ? target->data(0, ItemTypeRole).toString() : QString();
        const QString id = target ? target->data(0, ItemIdRole).toString() : QString();
        const QString aux = target ? target->data(0, ItemAuxRole).toString() : QString();
        bool canMoveUp = false;
        if (type == QStringLiteral("dynamic-marker")) {
            const auto *step = findDynamicStep(m_data, aux.section(QLatin1Char('|'), 0, 0),
                                               aux.section(QLatin1Char('|'), 1, 1));
            canMoveUp = step && canMoveDynamicMarkerUp(*step, id);
        }
        pausePlan();
        QMenu menu(m_tree);
        populateDynamicPlanMenu(menu, type, [this, type, id, aux](DynamicAction action, const QString &kind) {
            if (m_domain != Domain::Dynamic) return;
            const QString planId = type == QStringLiteral("plan") ? id
                : (type == QStringLiteral("step") ? aux : aux.section(QLatin1Char('|'), 0, 0));
            const QString stepId = type == QStringLiteral("step") ? id : aux.section(QLatin1Char('|'), 1, 1);
            if (action == DynamicAction::MoveUp) {
                if (type == QStringLiteral("dynamic-marker"))
                    moveSelectedDynamicMarkerUp(planId, stepId, id);
                return;
            }
            if (!prepareDynamicEdit()) return;
            if (action == DynamicAction::AddPlan) { addPlan(); return; }
            const bool exists = type == QStringLiteral("plan") ? findDynamicPlan(m_data, planId) != nullptr
                : type == QStringLiteral("step") ? findDynamicStep(m_data, planId, stepId) != nullptr
                : findDynamicMarker(m_data, planId, stepId, id) != nullptr;
            if (!exists) { updateStatus(QStringLiteral("对象已不存在，请重新选择"), true); return; }
            m_selectedPlanId = planId; m_selectedStepId = stepId;
            if (action == DynamicAction::AddStep) { editDynamicStep(planId); return; }
            if (action == DynamicAction::AddMarker) { addDynamicMarker(kind); return; }
            if (action == DynamicAction::Remove) { removeDynamicItem(type, id, aux); return; }
            selectDynamicTreeItem(type, id, aux);
            editSelected();
        }, canMoveUp);
        menu.exec(m_tree->viewport()->mapToGlobal(position));
        return;
    }
    QTreeWidgetItem *item = m_tree->itemAt(position);
    if (!item) {
        if (m_domain == Domain::Normal) {
            QMenu menu(m_tree);
            menu.addAction(QIcon(deploymentIcon(QStringLiteral("folder.png"))),
                QStringLiteral("添加文件夹"), this, [this] { editNormalFolder(); });
            menu.exec(m_tree->viewport()->mapToGlobal(position));
        }
        return;
    }
    m_tree->setCurrentItem(item);
    const QString type = item->data(0, ItemTypeRole).toString();
    const QString id = item->data(0, ItemIdRole).toString();
    const QString aux = item->data(0, ItemAuxRole).toString();
    QMenu menu(m_tree);
    const auto action = [&menu](const QString &text, const QString &icon,
                                std::function<void()> operation) {
        QAction *value = menu.addAction(icon.isEmpty() ? QIcon() : QIcon(icon), text);
        QObject::connect(value, &QAction::triggered, &menu,
                         [operation = std::move(operation)] { operation(); });
    };
    // IMS refreshes can rebuild the tree while a context menu is open.
    // Resolve by stable identity instead of retaining a deleted item pointer.
    const auto contextItem = [this, type, id]() -> QTreeWidgetItem * {
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(0, ItemTypeRole) == type && (*it)->data(0, ItemIdRole) == id)
                return *it;
        return nullptr;
    };
    const auto addMarkerMenu = [this, &menu](bool plot, const QString &category) {
        const bool normal = m_domain == Domain::Normal;
        QMenu *add = menu.addMenu(QIcon(plot
            ? QStringLiteral(":/legacy/images/toolbar/Plot.png")
            : QStringLiteral(":/legacy/images/toolbar/mark.png")),
            plot ? QStringLiteral("添加标绘") : QStringLiteral("添加标记"));
        QAction *folderEntry = add->addAction(
            QIcon(deploymentIcon(QStringLiteral("folder.png"))),
            QStringLiteral("文件夹"));
        connect(folderEntry, &QAction::triggered, add,
                [this, plot, normal, category] {
                    if (normal) editNormalFolder({}, category); else addSceneFolder(plot);
                });
        addAnnotationTypeActions(*add, plot, [this, category, normal](const QString &kind) {
            addMarker(kind, normal ? QString() : category, !normal,
                normal ? category : QString());
        });
    };

    if (type == QStringLiteral("normal-local-root")) {
        action(QStringLiteral("添加文件夹"), deploymentIcon(QStringLiteral("folder.png")),
               [this] { editNormalFolder(); });
    } else if (type == QStringLiteral("normal-folder")) {
        action(QStringLiteral("添加子文件夹"), deploymentIcon(QStringLiteral("folder.png")),
               [this, id] { editNormalFolder({}, id); });
        addMarkerMenu(false, id);
        addMarkerMenu(true, id);
        action(QStringLiteral("删除"), deploymentIcon(QStringLiteral("delete_normal.png")),
               [this] { removeSelected(); });
        action(QStringLiteral("属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this, id] { editNormalFolder(id); });
    } else if (type == QStringLiteral("normal-marker")) {
        addMarkerMenu(false, aux);
        addMarkerMenu(true, aux);
        action(QStringLiteral("定位"), deploymentIcon(QStringLiteral("viewpoint_locate.png")),
               [this, contextItem] { if (auto *current = contextItem()) treeItemActivated(current, 0); });
        action(QStringLiteral("删除"), deploymentIcon(QStringLiteral("delete_normal.png")),
               [this] { removeSelected(); });
        action(QStringLiteral("属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this] { editSelected(); });
    } else if (type == QStringLiteral("live") || type == QStringLiteral("binding-rule")) {
        if (type == QStringLiteral("live"))
            action(QStringLiteral("定位"), deploymentIcon(QStringLiteral("viewpoint_locate.png")),
                   [this, contextItem] { if (auto *current = contextItem()) treeItemActivated(current, 0); });
        action(QStringLiteral("查看属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this, contextItem] {
                   auto *current = contextItem();
                   if (!current) return;
                   updateInspector(current);
                   QMessageBox::information(m_leftPanel, QStringLiteral("IMS 数据属性"), m_inspector->toPlainText());
               });
    } else if (type == QStringLiteral("layer")) {
        if (aux == QStringLiteral("custom"))
            action(QStringLiteral("删除"), deploymentIcon(QStringLiteral("delete_normal.png")),
                   [this] { removeSelected(); });
        action(QStringLiteral("属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this] { editSelected(); });
    } else if (type == QStringLiteral("scene-custom-root")) {
        action(QStringLiteral("清空"), deploymentIcon(QStringLiteral("clear_mark.png")),
               [this] { clearSceneBranch(QStringLiteral("scene-custom-root")); });
    } else if (type == QStringLiteral("scene-custom-kind")) {
        const QString label = aux == QStringLiteral("imagery") ? QStringLiteral("添加影像")
            : (aux == QStringLiteral("elevation") ? QStringLiteral("添加高程")
                                                   : QStringLiteral("添加矢量"));
        const QString icon = aux == QStringLiteral("imagery") ? QStringLiteral("load_image.png")
            : (aux == QStringLiteral("elevation") ? QStringLiteral("load_altitude.png")
                                                   : QStringLiteral("load_vector.png"));
        action(label, deploymentIcon(icon), [this, aux] { addLayer(aux); });
        action(QStringLiteral("清空"), deploymentIcon(QStringLiteral("clear_mark.png")),
               [this, aux] { clearSceneBranch(QStringLiteral("scene-custom-kind"), aux); });
    } else if (type == QStringLiteral("scene-mark-root")
               || type == QStringLiteral("scene-plot-root")) {
        const bool plot = type == QStringLiteral("scene-plot-root");
        action(QStringLiteral("添加文件夹"), deploymentIcon(QStringLiteral("folder.png")),
               [this, plot] { addSceneFolder(plot); });
        action(QStringLiteral("清空"), deploymentIcon(QStringLiteral("clear_mark.png")),
               [this, type] { clearSceneBranch(type); });
    } else if (type == QStringLiteral("scene-mark-folder")
               || type == QStringLiteral("scene-plot-folder")) {
        const bool plot = type == QStringLiteral("scene-plot-folder");
        addMarkerMenu(plot, aux);
        action(QStringLiteral("清空"), deploymentIcon(QStringLiteral("clear_mark.png")),
               [this, type, aux] { clearSceneBranch(type + QStringLiteral("-contents"), aux); });
        action(QStringLiteral("删除"), deploymentIcon(QStringLiteral("delete_normal.png")),
               [this, type, aux] { clearSceneBranch(type, aux); });
        action(QStringLiteral("属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this, id] { editSceneFolder(id); });
    } else if (type == QStringLiteral("marker")) {
        const bool plot = aux == QStringLiteral("plot");
        const QString category = item->parent() ? item->parent()->data(0, ItemAuxRole).toString()
                                                : QString();
        addMarkerMenu(plot, category);
        action(QStringLiteral("删除"), deploymentIcon(QStringLiteral("delete_normal.png")),
               [this] { removeSelected(); });
        action(QStringLiteral("属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this] { editSelected(); });
    } else if (type == QStringLiteral("scene-viewpoint-root")) {
        action(QStringLiteral("添加视点"), deploymentIcon(QStringLiteral("viewpoint_obtain.png")),
               [this] { saveViewpoint(); });
        action(QStringLiteral("清空视点图层"), deploymentIcon(QStringLiteral("clear_mark.png")),
               [this] { clearSceneBranch(QStringLiteral("scene-viewpoint-root")); });
    } else if (type == QStringLiteral("viewpoint")) {
        action(QStringLiteral("定位"), deploymentIcon(QStringLiteral("viewpoint_locate.png")),
               [this, contextItem] { if (auto *current = contextItem()) treeItemActivated(current, 0); });
        action(QStringLiteral("属性"), deploymentIcon(QStringLiteral("setttings.png")),
               [this, id] { editViewpoint(id); });
        action(QStringLiteral("删除视点"), deploymentIcon(QStringLiteral("delete_normal.png")),
               [this] { removeSelected(); });
    }
    if (!menu.actions().isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(position));
}

void ZqbsPage::editNormalFolder(const QString &folderId, const QString &parentId)
{
    const auto *existing = normalFolder(m_data, folderId);
    if ((!folderId.isEmpty() && !existing)
        || (!parentId.isEmpty() && !normalFolder(m_data, parentId))) return;
    DeploymentLocalFolder edited = existing ? *existing : DeploymentLocalFolder{};
    if (!existing) {
        edited.id = newId(QStringLiteral("squad-folder"));
        edited.parentId = parentId;
        edited.name = QStringLiteral("新建文件夹");
    }
    const bool creating = folderId.isEmpty();
    SkinnedDialog dialog(creating ? QStringLiteral("新增部署分组标注文件夹")
                                  : QStringLiteral("部署分组标注文件夹属性"), m_leftPanel);
    dialog.setObjectName(QStringLiteral("normalFolderDialog"));
    auto *layout = new QVBoxLayout(createDeploymentDialogBody(dialog));
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(edited.name, &dialog);
    name->setObjectName(QStringLiteral("normalFolderName"));
    name->setMinimumWidth(name->fontMetrics().horizontalAdvance(QString(24, QLatin1Char('0'))));
    auto *description = new QLineEdit(edited.description, &dialog);
    auto *visible = new QCheckBox(QStringLiteral("显示文件夹内容"), &dialog);
    visible->setChecked(edited.visible);
    const auto *parent = normalFolder(m_data, edited.parentId);
    form->addRow(QStringLiteral("上级文件夹"), new QLabel(parent ? parent->name : QStringLiteral("部署分组本地标注"), &dialog));
    form->addRow(QStringLiteral("名称"), name);
    form->addRow(QStringLiteral("说明"), description);
    form->addRow(QString(), visible);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString text = name->text().trimmed();
        const bool duplicate = std::any_of(m_data.normalFolders.cbegin(), m_data.normalFolders.cend(),
            [&](const DeploymentLocalFolder &folder) {
                return folder.id != edited.id && folder.parentId == edited.parentId
                    && folder.name.compare(text, Qt::CaseInsensitive) == 0;
            });
        if (text.isEmpty() || duplicate) {
            QMessageBox::warning(&dialog, QStringLiteral("名称无效"),
                text.isEmpty() ? QStringLiteral("请输入文件夹名称。") : QStringLiteral("同一父级下已存在该名称。"));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.adjustSize();
    if (dialog.exec() != QDialog::Accepted) return;
    edited.name = name->text().trimmed();
    edited.description = description->text().trimmed();
    edited.visible = visible->isChecked();
    if (creating) m_data.normalFolders.append(edited);
    else for (auto &folder : m_data.normalFolders) if (folder.id == edited.id) folder = edited;
    for (auto &marker : m_data.normalMarkers)
        if (marker.folderId == edited.id) marker.category = edited.name;
    saveNormal();
    rebuildTree();
    updateScene();
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if ((*it)->data(0, ItemTypeRole) == QStringLiteral("normal-folder")
            && (*it)->data(0, ItemIdRole) == edited.id) {
            m_tree->setCurrentItem(*it);
            for (auto *parentItem = (*it)->parent(); parentItem; parentItem = parentItem->parent())
                parentItem->setExpanded(true);
            break;
        }
    }
}

void ZqbsPage::addSceneFolder(bool plot)
{
    SkinnedDialog dialog(plot ? QStringLiteral("新增标绘文件夹")
                              : QStringLiteral("新增标记文件夹"), m_leftPanel);
    auto *layout = new QVBoxLayout(createDeploymentDialogBody(dialog));
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(plot ? QStringLiteral("战术标绘")
                                    : QStringLiteral("地图标记"), &dialog);
    auto *description = new QLineEdit(&dialog);
    form->addRow(QStringLiteral("文件夹名称"), name);
    form->addRow(QStringLiteral("说明"), description);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return;
    const QString folderName = name->text().trimmed();
    const bool duplicate = std::any_of(m_data.sceneFolders.cbegin(), m_data.sceneFolders.cend(),
        [plot, &folderName](const DeploymentSceneFolder &folder) {
            return folder.plot == plot && folder.name == folderName;
        });
    if (duplicate) {
        QMessageBox::warning(m_leftPanel, QStringLiteral("名称重复"),
                             QStringLiteral("同一图层中已存在该文件夹。"));
        return;
    }
    DeploymentSceneFolder folder;
    folder.id = newId(QStringLiteral("folder"));
    folder.name = folderName;
    folder.description = description->text().trimmed();
    folder.plot = plot;
    const SceneViewpoint current = m_scene->currentViewpoint();
    folder.viewpoint.longitude = current.longitude;
    folder.viewpoint.latitude = current.latitude;
    folder.viewpoint.altitude = current.altitude;
    folder.viewpoint.heading = current.heading;
    folder.viewpoint.pitch = current.pitch;
    folder.viewpoint.range = current.range;
    m_data.sceneFolders.append(folder);
    saveFixedAnnotations();
    rebuildTree();
}

void ZqbsPage::editSceneFolder(const QString &folderId)
{
    auto found = std::find_if(m_data.sceneFolders.begin(), m_data.sceneFolders.end(),
        [&folderId](const DeploymentSceneFolder &folder) { return folder.id == folderId; });
    if (found == m_data.sceneFolders.end()) {
        updateStatus(QStringLiteral("该文件夹来自标记分类，请先用“添加文件夹”建立可编辑文件夹"), true);
        return;
    }
    SkinnedDialog dialog(QStringLiteral("文件夹属性"), m_leftPanel);
    auto *layout = new QVBoxLayout(createDeploymentDialogBody(dialog));
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(found->name, &dialog);
    auto *description = new QLineEdit(found->description, &dialog);
    auto *visible = new QCheckBox(QStringLiteral("显示文件夹内容"), &dialog);
    visible->setChecked(found->visible);
    form->addRow(QStringLiteral("名称"), name);
    form->addRow(QStringLiteral("说明"), description);
    form->addRow(QString(), visible);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return;
    const QString previousName = found->name;
    found->name = name->text().trimmed();
    found->description = description->text().trimmed();
    found->visible = visible->isChecked();
    for (DeploymentMarker &marker : m_data.markers) {
        if (isPlotAnnotation(marker.geometryType) == found->plot
            && marker.category == previousName) {
            marker.category = found->name;
            marker.visible = found->visible;
        }
    }
    saveFixedAnnotations();
    rebuildTree();
    updateScene();
}

void ZqbsPage::clearSceneBranch(const QString &branchType, const QString &branchId)
{
    QString prompt;
    if (branchType == QStringLiteral("scene-custom-root")) prompt = QStringLiteral("清空全部自定义图层？");
    else if (branchType == QStringLiteral("scene-custom-kind")) prompt = QStringLiteral("清空该类自定义图层？");
    else if (branchType == QStringLiteral("scene-mark-root")) prompt = QStringLiteral("清空全部标记文件夹和标记？");
    else if (branchType == QStringLiteral("scene-plot-root")) prompt = QStringLiteral("清空全部标绘文件夹和标绘？");
    else if (branchType.endsWith(QStringLiteral("-contents"))) prompt = QStringLiteral("清空该文件夹中的全部内容？");
    else if (branchType == QStringLiteral("scene-viewpoint-root")) prompt = QStringLiteral("清空全部视点？");
    else prompt = QStringLiteral("删除该文件夹及其中全部内容？");
    if (QMessageBox::question(m_leftPanel, QStringLiteral("确认操作"), prompt)
        != QMessageBox::Yes)
        return;
    bool fixedChanged = false;
    bool normalChanged = false;
    if (branchType == QStringLiteral("scene-custom-root")) {
        m_data.layers.erase(std::remove_if(m_data.layers.begin(), m_data.layers.end(),
            [](const DeploymentLayer &layer) {
                return !layer.runtimeLayer && !layer.systemLayer;
            }), m_data.layers.end());
        fixedChanged = true;
    } else if (branchType == QStringLiteral("scene-custom-kind")) {
        m_data.layers.erase(std::remove_if(m_data.layers.begin(), m_data.layers.end(),
            [&branchId](const DeploymentLayer &layer) {
                return !layer.runtimeLayer && !layer.systemLayer
                    && layer.kind == branchId;
            }), m_data.layers.end());
        fixedChanged = true;
    } else if (branchType == QStringLiteral("scene-viewpoint-root")) {
        m_data.viewpoints.clear();
        fixedChanged = true;
    } else {
        const bool plot = branchType.startsWith(QStringLiteral("scene-plot"));
        const bool root = branchType.endsWith(QStringLiteral("-root"));
        m_data.markers.erase(std::remove_if(m_data.markers.begin(), m_data.markers.end(),
            [plot, root, &branchId](const DeploymentMarker &marker) {
                return isPlotAnnotation(marker.geometryType) == plot
                    && (root || marker.category == branchId);
            }), m_data.markers.end());
        if (!branchType.endsWith(QStringLiteral("-contents"))) {
            m_data.sceneFolders.erase(std::remove_if(m_data.sceneFolders.begin(), m_data.sceneFolders.end(),
                [plot, root, &branchId](const DeploymentSceneFolder &folder) {
                    return folder.plot == plot && (root || folder.name == branchId);
                }), m_data.sceneFolders.end());
        }
        normalChanged = true;
    }
    if (fixedChanged) saveFixed();
    if (normalChanged) saveFixedAnnotations();
    rebuildTree();
    updateScene();
}

void ZqbsPage::showLayerMenu()
{
    auto *palette = new DeploymentToolPalette(QStringLiteral("图层管理"),
        QStringLiteral(":/legacy/images/toolbar/layer.png"), 2, m_leftPanel);
    palette->addTool(QStringLiteral("影像图层"), deploymentIcon(QStringLiteral("load_image.png")),
                     [this] { addLayer(QStringLiteral("imagery")); });
    palette->addTool(QStringLiteral("高程图层"), deploymentIcon(QStringLiteral("load_altitude.png")),
                     [this] { addLayer(QStringLiteral("elevation")); });
    palette->addTool(QStringLiteral("矢量图层"), deploymentIcon(QStringLiteral("load_vector.png")),
                     [this] { addLayer(QStringLiteral("vector")); });
    palette->addTool(QStringLiteral("图层列表"), deploymentIcon(QStringLiteral("layer_effect.png")),
                     [this] { setDomain(Domain::Fixed); });
    palette->addTool(QStringLiteral("编辑选中"), deploymentIcon(QStringLiteral("setttings.png")),
                     [this] { editSelected(); });
    palette->addTool(QStringLiteral("删除选中"), deploymentIcon(QStringLiteral("delete_normal.png")),
                     [this] { removeSelected(); });
    palette->showBeside(qobject_cast<QWidget *>(sender()));
}

void ZqbsPage::addLayer(const QString &presetKind)
{
    LayerAddDialog dialog(presetKind.isEmpty() ? QStringLiteral("vector") : presetKind,
                          AppPaths::dataDir(), m_leftPanel);
    if (dialog.exec() != QDialog::Accepted) return;
    DeploymentLayer layer = dialog.layer();
    layer.id = newId(QStringLiteral("layer"));
    QVector<DeploymentLayer> candidate = m_data.layers;
    candidate.append(layer);
    QString error;
    if (!m_scene->setDeploymentLayers(candidate, error)) {
        const QString loadError = error;
        QString restoreError;
        m_scene->setDeploymentLayers(m_data.layers, restoreError);
        QMessageBox::warning(m_leftPanel, QStringLiteral("图层加载失败"),
                             QStringLiteral("数据源未能加载，请检查路径、服务或格式。\n%1").arg(loadError));
        return;
    }
    m_data.layers = std::move(candidate);
    saveFixed();
    setDomain(Domain::Fixed);
    rebuildTree();
    updateScene();
}

void ZqbsPage::showAnnotationMenu()
{
    if (m_domain == Domain::Dynamic) {
        auto *palette = new DeploymentToolPalette(QStringLiteral("动态标记"),
            QStringLiteral(":/legacy/images/toolbar/mark.png"), 2, m_leftPanel);
        palette->addTool(QStringLiteral("新增动态点"), deploymentIcon(QStringLiteral("point.png")),
                         [this] { addDynamicMarker(); });
        palette->addTool(QStringLiteral("新增预案"), deploymentIcon(QStringLiteral("save.png")),
                         [this] { addPlan(); });
        palette->addTool(QStringLiteral("新增步骤"), deploymentIcon(QStringLiteral("viewpoint_obtain.png")),
                         [this] { addStep(); });
        palette->addTool(QStringLiteral("编辑选中"), deploymentIcon(QStringLiteral("setttings.png")),
                         [this] { editSelected(); });
        palette->addTool(QStringLiteral("删除选中"), deploymentIcon(QStringLiteral("delete_normal.png")),
                         [this] { removeSelected(); });
        palette->showBeside(qobject_cast<QWidget *>(sender()));
        return;
    }
    auto *palette = new DeploymentToolPalette(QStringLiteral("标注与标绘"),
        QStringLiteral(":/legacy/images/toolbar/mark.png"), 2, m_leftPanel);
    struct Entry { const char *label; const char *kind; const char *icon; };
    const Entry entries[] = {
        {"点标注", "point", "point.png"}, {"文字标注", "text", "text.png"},
        {"线标注", "line", "line.png"}, {"面标注", "polygon", "polygon.png"},
        {"矩形", "rectangle", "rectangle.png"}, {"圆形", "circle", "circle.png"},
        {"椭圆", "ellipse", "ellipse.png"}, {"扇形", "sector", "sector.png"},
        {"直箭头", "plot-straight-arrow", "StraightArrow.png"},
        {"双箭头", "plot-double-arrow", "DoubleArrow.png"},
        {"平行搜索", "plot-parallel-search", "ParallelSearch.png"},
        {"扇形搜索", "plot-sector-search", "SectorSearch.png"}
    };
    for (const Entry &entry : entries) {
        const QString kind = QString::fromLatin1(entry.kind);
        palette->addTool(QString::fromUtf8(entry.label), deploymentIcon(QString::fromLatin1(entry.icon)),
                         [this, kind] { addMarker(kind); });
    }
    palette->addTool(QStringLiteral("编辑选中"), deploymentIcon(QStringLiteral("setttings.png")),
                     [this] { editSelected(); });
    palette->addTool(QStringLiteral("删除选中"), deploymentIcon(QStringLiteral("delete_normal.png")),
                     [this] { removeSelected(); });
    palette->addTool(QStringLiteral("清空标注"), deploymentIcon(QStringLiteral("clear_mark.png")),
        [this, normal = m_domain == Domain::Normal] {
        if (m_markerPropertyDialog) {
            m_markerPropertyDialog->raise();
            updateStatus(QStringLiteral("请先完成当前标注的属性编辑"), true);
            return;
        }
        auto &markers = normal ? m_data.normalMarkers : m_data.markers;
        if (markers.isEmpty()) return;
        if (QMessageBox::question(m_leftPanel, QStringLiteral("清空本地标注"),
                QStringLiteral("确认删除%1的全部 %2 个标注？另一模块不会删除，文件夹保留。")
                    .arg(normal ? QStringLiteral("部署分组本地标注") : QStringLiteral("基础场景"))
                    .arg(markers.size())) == QMessageBox::Yes) {
            markers.clear();
            if (normal) saveNormal(); else saveFixedAnnotations();
            rebuildTree();
            updateScene();
        }
    });
    palette->showBeside(qobject_cast<QWidget *>(sender()));
}

void ZqbsPage::addMarker(const QString &presetType, const QString &category,
                         bool keepFixedDomain, const QString &normalFolderId)
{
    if (m_markerPropertyDialog) {
        m_markerPropertyDialog->raise();
        updateStatus(QStringLiteral("请先完成当前标注的属性编辑"), true);
        return;
    }
    const bool normal = !keepFixedDomain && m_domain == Domain::Normal;
    QString targetFolderId = normalFolderId;
    if (normal && targetFolderId.isEmpty() && m_tree->currentItem()) {
        auto *item = m_tree->currentItem();
        const QString type = item->data(0, ItemTypeRole).toString();
        if (type == QStringLiteral("normal-folder")) targetFolderId = item->data(0, ItemIdRole).toString();
        else if (type == QStringLiteral("normal-marker")) targetFolderId = item->data(0, ItemAuxRole).toString();
    }
    const auto *folder = normal ? normalFolder(m_data, targetFolderId) : nullptr;
    if (normal && !folder) {
        updateStatus(QStringLiteral("请先在“部署分组本地标注”下右键添加或选择一个文件夹"), true);
        return;
    }
    m_pendingMarkerType = presetType.isEmpty() ? QStringLiteral("point") : presetType;
    m_pendingMarkerName = QStringLiteral("%1 %2").arg(annotationDisplayName(m_pendingMarkerType))
        .arg((normal ? m_data.normalMarkers : m_data.markers).size() + 1);
    m_pendingMarkerCategory = normal ? folder->name
        : (!category.trimmed().isEmpty() ? category.trimmed()
            : (isPlotAnnotation(m_pendingMarkerType) ? QStringLiteral("战术标绘") : QStringLiteral("地图标记")));
    m_pendingNormalFolderId = normal ? targetFolderId : QString();
    m_returnToFixedAfterPlacement = !normal;
    m_pendingPlacement = normal ? PendingPlacement::NormalMarker : PendingPlacement::FixedMarker;
    m_scene->beginAnnotationPlacement(m_pendingMarkerType);
    const QString operation = annotationAutoFinishPoints(m_pendingMarkerType) == 1
        ? QStringLiteral("单击地图确定位置，随后设置属性")
        : QStringLiteral("左键添加控制点，右键结束并设置属性");
    updateStatus(QStringLiteral("正在绘制“%1”：%2")
                     .arg(annotationDisplayName(m_pendingMarkerType), operation));
}

void ZqbsPage::showMeasurementMenu()
{
    auto *palette = new DeploymentToolPalette(QStringLiteral("测量"),
        QStringLiteral(":/legacy/images/toolbar/measure.png"), 2, m_leftPanel);
    struct Entry { const char *label; const char *kind; const char *icon; };
    const Entry entries[] = {
        {"空间距离", "space-distance", "space_distance.png"},
        {"地表距离", "surface-distance", "surface_distance.png"},
        {"高差分析", "height-distance", "height_measure.png"},
        {"空间面积", "space-area", "area_measure.png"},
        {"投影面积", "shadow-area", "project_area.png"},
        {"通视分析", "line-visible", "line_visible_measure.png"},
        {"视域分析", "radial-line-visible", "view_measure.png"},
        {"剖面分析", "profile-analysis", "profile_measure.png"},
        {"坡度分析", "gradient-analysis", "gradient_measure.png"},
        {"淹没分析", "flood-analysis", "flood_measure.png"},
        {"填挖方", "fill-cut-analysis", "cutfill_measure.png"},
        {"等高线", "contour-measure", "contour_measure.png"}
    };
    for (const Entry &entry : entries) {
        const QString kind = QString::fromLatin1(entry.kind);
        palette->addTool(QString::fromUtf8(entry.label), deploymentIcon(QString::fromLatin1(entry.icon)),
                         [this, kind] { addMeasurement(kind); });
    }
    palette->addTool(QStringLiteral("清除"), deploymentIcon(QStringLiteral("clear_mark.png")), [this] {
        if (m_data.measurements.isEmpty())
            return;
        if (QMessageBox::question(m_leftPanel, QStringLiteral("清除测量成果"),
                QStringLiteral("确认清除全部 %1 项测量成果？")
                    .arg(m_data.measurements.size())) == QMessageBox::Yes) {
            m_data.measurements.clear();
            saveFixed();
            rebuildTree();
            updateScene();
        }
    });
    palette->showBeside(qobject_cast<QWidget *>(sender()));
}

void ZqbsPage::addMeasurement(const QString &kind)
{
    if (kind.isEmpty())
        return;
    if (measurementNeedsTerrain(kind) && !m_scene->hasElevationLayer()) {
        QMessageBox::warning(m_leftPanel, QStringLiteral("缺少高程数据"),
            QStringLiteral("“%1”需要 DEM/高程图层。当前地图只有影像数据，"
                           "请先在“图层”中添加并启用高程图层后再执行。")
                .arg(measurementDisplayName(kind)));
        updateStatus(QStringLiteral("%1未启动：缺少高程图层")
                         .arg(measurementDisplayName(kind)), true);
        return;
    }
    // Reference parameters live in the result window and are initialized by
    // the measurement result itself; there is no modal prompt before drawing.
    m_pendingMeasurementParameter = measurementDefaultParameter(kind);
    setDomain(Domain::Fixed);
    m_scene->beginMeasurement(kind);
    updateStatus(QStringLiteral("%1：单击添加控制点，移动鼠标实时预览，右键完成")
                      .arg(measurementDisplayName(kind)));
}

bool ZqbsPage::prepareDynamicEdit()
{
    if (m_markerPropertyDialog) {
        m_markerPropertyDialog->raise(); m_markerPropertyDialog->activateWindow();
        updateStatus(QStringLiteral("请先完成当前标记的属性编辑"), true);
        return false;
    }
    pausePlan();
    m_scene->cancelDeploymentInteraction();
    m_pendingPlacement = PendingPlacement::None;
    m_pendingDynamicPlanId.clear(); m_pendingDynamicStepId.clear();
    return true;
}

bool ZqbsPage::commitDynamicPlans(const QVector<DeploymentPlan> &plans)
{
    DeploymentData candidate = m_data;
    candidate.plans = plans;
    QString error;
    if (!m_store.saveDynamic(candidate, error)) {
        updateStatus(error, true);
        QMessageBox::warning(m_leftPanel, QStringLiteral("动态数据保存失败"), error);
        return false;
    }
    m_data.plans = plans;
    updateStatus(QStringLiteral("动态预案数据已保存"));
    return true;
}

void ZqbsPage::refreshDynamicData()
{
    DeploymentPlan *plan = selectedPlan();
    DeploymentPlanStep *step = plan ? selectedStep() : nullptr;
    // CRUD is an explicit editing preview, never an asynchronous playback refresh.
    resetPlanPlayback(step ? PlanState::Preview : PlanState::Stopped);
    m_dynamicMarkers = step ? step->markers : QVector<DeploymentMarker>{};
    if (!plan) m_selectedPlanId.clear();
    if (!step) { m_selectedStepId.clear(); m_currentStep = -1; }
    else {
        for (int index = 0; index < plan->steps.size(); ++index)
            if (plan->steps[index].id == step->id) { m_currentStep = index; break; }
    }
    rebuildTree(); rebuildTimeline(); updateScene();
    updateInspector(m_tree->currentItem());
}

void ZqbsPage::selectDynamicTreeItem(const QString &type, const QString &id, const QString &aux)
{
    const QSignalBlocker blocker(m_tree);
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        auto *item = *it;
        if (item->data(0, ItemTypeRole) != type || item->data(0, ItemIdRole) != id
            || (!aux.isEmpty() && item->data(0, ItemAuxRole) != aux)) continue;
        m_tree->setCurrentItem(item);
        for (auto *parent = item->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
        m_tree->scrollToItem(item);
        updateInspector(item);
        return;
    }
}

void ZqbsPage::moveSelectedDynamicMarkerUp(const QString &planId, const QString &stepId,
                                           const QString &markerId)
{
    if (!prepareDynamicEdit()) return;
    DeploymentData candidate = m_data;
    if (!moveDynamicMarkerUp(candidate, planId, stepId, markerId)) {
        updateStatus(QStringLiteral("标记不存在或已是本步骤的第一个标记"), true);
        return;
    }
    if (!commitDynamicPlans(candidate.plans)) return;
    m_selectedPlanId = planId;
    m_selectedStepId = stepId;
    m_filter->clear();
    refreshDynamicData();
    selectDynamicTreeItem(QStringLiteral("dynamic-marker"), markerId,
                          planId + QLatin1Char('|') + stepId);
    updateStatus(QStringLiteral("标记已在当前步骤中上移一位，演练顺序已保存"));
}

void ZqbsPage::addPlan()
{
    editDynamicPlan();
}

void ZqbsPage::editDynamicPlan(QString planId)
{
    if (!prepareDynamicEdit()) return;
    const bool creating = planId.isEmpty();
    DeploymentPlan edited;
    if (creating) {
        edited.id = newId(QStringLiteral("plan"));
        edited.description = QStringLiteral("本地时间轴演练方案");
    } else {
        auto *plan = findDynamicPlan(m_data, planId);
        if (!plan) return;
        edited = *plan;
    }
    if (!editDynamicPlanProperties(edited, creating, m_leftPanel)) return;
    DeploymentData candidate = m_data;
    if (creating) candidate.plans.append(edited);
    else {
        auto *plan = findDynamicPlan(candidate, planId);
        if (!plan) return;
        // Only metadata is editable here; preserve any existing steps and marks.
        plan->name = edited.name; plan->description = edited.description; plan->loop = edited.loop;
    }
    if (!commitDynamicPlans(candidate.plans)) return;
    m_selectedPlanId = edited.id;
    if (creating) m_selectedStepId.clear();
    m_filter->clear();
    setDomain(Domain::Dynamic);
    refreshDynamicData();
    selectDynamicTreeItem(QStringLiteral("plan"), edited.id);
}

void ZqbsPage::addStep()
{
    if (!prepareDynamicEdit()) return;
    auto *plan = selectedPlan();
    if (!plan) { addPlan(); plan = selectedPlan(); }
    if (plan) editDynamicStep(plan->id);
}

void ZqbsPage::editDynamicStep(QString planId, QString stepId)
{
    if (!prepareDynamicEdit() || !findDynamicPlan(m_data, planId)) return;
    const bool creating = stepId.isEmpty();
    const auto currentView = [this] {
        const auto current = m_scene->currentViewpoint();
        DeploymentViewpoint value;
        value.longitude = current.longitude; value.latitude = current.latitude;
        value.altitude = current.altitude; value.heading = current.heading;
        value.pitch = current.pitch; value.range = current.range;
        return value;
    };
    DeploymentPlanStep edited;
    if (creating) {
        edited.id = newId(QStringLiteral("step"));
        edited.viewpoint = currentView();
        edited.viewpoint.id = newId(QStringLiteral("step-view"));
        edited.viewpoint.flyTimeSeconds = 1.0;
    } else {
        auto *step = findDynamicStep(m_data, planId, stepId);
        if (!step) return;
        edited = *step;
    }
    if (!editDynamicStepProperties(edited, creating, m_leftPanel, currentView)) return;
    edited.viewpoint.name = edited.name + QStringLiteral("视角");
    DeploymentData candidate = m_data;
    auto *plan = findDynamicPlan(candidate, planId);
    if (!plan) return;
    if (creating) plan->steps.append(edited);
    else {
        auto *step = findDynamicStep(candidate, planId, stepId);
        if (!step) return;
        edited.markers = step->markers;
        *step = edited;
    }
    if (!commitDynamicPlans(candidate.plans)) return;
    m_selectedPlanId = planId; m_selectedStepId = edited.id;
    m_filter->clear();
    refreshDynamicData();
    selectDynamicTreeItem(QStringLiteral("step"), edited.id, planId);
}

void ZqbsPage::removeDynamicItem(const QString &type, const QString &id, const QString &aux)
{
    if (!prepareDynamicEdit()) return;
    const QString planId = type == QStringLiteral("plan") ? id
        : (type == QStringLiteral("step") ? aux : aux.section(QLatin1Char('|'), 0, 0));
    const QString stepId = type == QStringLiteral("step") ? id : aux.section(QLatin1Char('|'), 1, 1);
    QString description;
    if (type == QStringLiteral("plan")) {
        auto *plan = findDynamicPlan(m_data, planId);
        if (!plan) return;
        int markers = 0; for (const auto &step : plan->steps) markers += step.markers.size();
        description = QStringLiteral("确认删除预案“%1”及其 %2 个步骤、%3 个动态标记？")
            .arg(plan->name).arg(plan->steps.size()).arg(markers);
    } else if (type == QStringLiteral("step")) {
        auto *step = findDynamicStep(m_data, planId, stepId);
        if (!step) return;
        description = QStringLiteral("确认删除步骤“%1”及其 %2 个动态标记？").arg(step->name).arg(step->markers.size());
    } else {
        auto *marker = findDynamicMarker(m_data, planId, stepId, id);
        if (!marker) return;
        description = QStringLiteral("确认删除动态标记“%1”？").arg(marker->name);
    }
    if (QMessageBox::question(m_leftPanel, QStringLiteral("确认删除"), description
        + QStringLiteral("\n只修改当前动态预案；固定化、部署标注不受影响。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    // Re-resolve after the modal confirmation instead of using a stale item/iterator.
    DeploymentData candidate = m_data;
    if (!removeDynamicData(candidate, type, id, aux)) {
        updateStatus(QStringLiteral("对象已不存在，未删除其他数据"), true);
        return;
    }
    if (!commitDynamicPlans(candidate.plans)) return;
    refreshDynamicData();
    if (type == QStringLiteral("dynamic-marker"))
        selectDynamicTreeItem(QStringLiteral("step"), stepId, planId);
    else if (type == QStringLiteral("step"))
        selectDynamicTreeItem(QStringLiteral("plan"), planId);
}

void ZqbsPage::addDynamicMarker(const QString &kind)
{
    if (!prepareDynamicEdit()) return;
    const auto *step = findDynamicStep(m_data, m_selectedPlanId, m_selectedStepId);
    if (!step) {
        updateStatus(QStringLiteral("请先选择或新增一个预案步骤"), true);
        return;
    }
    m_pendingDynamicPlanId = m_selectedPlanId;
    m_pendingDynamicStepId = m_selectedStepId;
    m_pendingMarkerType = kind.isEmpty() ? QStringLiteral("point") : kind;
    m_pendingMarkerName = QStringLiteral("%1 %2")
        .arg(annotationDisplayName(m_pendingMarkerType)).arg(step->markers.size() + 1);
    m_pendingMarkerCategory = QStringLiteral("plan");
    m_pendingNormalFolderId.clear();
    m_returnToFixedAfterPlacement = false;
    m_pendingPlacement = PendingPlacement::DynamicMarker;
    m_scene->beginAnnotationPlacement(m_pendingMarkerType);
    const QString operation = annotationAutoFinishPoints(m_pendingMarkerType) == 1
        ? QStringLiteral("单击地图确定位置，随后设置属性")
        : QStringLiteral("左键添加控制点，右键结束并设置属性");
    updateStatus(QStringLiteral("正在绘制阶段标注“%1”：%2")
        .arg(annotationDisplayName(m_pendingMarkerType), operation));
}

void ZqbsPage::receiveMapPoint(const GeoCoordinate &coordinate)
{
    if (m_pendingPlacement != PendingPlacement::NormalMarker
        && m_pendingPlacement != PendingPlacement::FixedMarker) return;
    DeploymentMarker marker;
    marker.id = newId(QStringLiteral("marker"));
    marker.name = m_pendingMarkerName;
    marker.category = QStringLiteral("unit-annotation");
    marker.geometryType = QStringLiteral("point");
    applyLegacyAnnotationStyleDefaults(marker);
    marker.position = coordinate;
    marker.points.append(coordinate);
    const SceneViewpoint sceneView = m_scene->currentViewpoint();
    marker.viewpoint.longitude = sceneView.longitude;
    marker.viewpoint.latitude = sceneView.latitude;
    marker.viewpoint.altitude = sceneView.altitude;
    marker.viewpoint.heading = sceneView.heading;
    marker.viewpoint.pitch = sceneView.pitch;
    marker.viewpoint.range = sceneView.range;
    marker.viewFlyTimeSeconds = 1.0;
    if (m_pendingPlacement == PendingPlacement::NormalMarker) {
        if (const auto *folder = normalFolder(m_data, m_pendingNormalFolderId)) {
            marker.id = newId(QStringLiteral("squad-marker"));
            marker.folderId = folder->id;
            marker.category = folder->name;
            m_data.normalMarkers.append(marker);
            if (marker.visible)
                revealNormalMarkerAncestors(m_data, marker.folderId);
            saveNormal();
        }
    } else if (m_pendingPlacement == PendingPlacement::FixedMarker) {
        m_data.markers.append(marker);
        saveFixedAnnotations();
    }
    m_pendingPlacement = PendingPlacement::None;
    m_pendingMarkerName.clear();
    m_pendingMarkerType = QStringLiteral("point");
    rebuildTree();
    updateScene();
}

void ZqbsPage::receiveAnnotation(const QString &kind,
                                 const QVector<GeoCoordinate> &points)
{
    if ((m_pendingPlacement != PendingPlacement::NormalMarker
         && m_pendingPlacement != PendingPlacement::FixedMarker
         && m_pendingPlacement != PendingPlacement::DynamicMarker) || points.isEmpty())
        return;
    if (m_markerPropertyDialog) {
        m_markerPropertyDialog->raise();
        m_markerPropertyDialog->activateWindow();
        m_pendingPlacement = PendingPlacement::None;
        m_pendingMarkerName.clear();
        m_pendingMarkerType = QStringLiteral("point");
        m_pendingMarkerCategory = QStringLiteral("unit-annotation");
        m_returnToFixedAfterPlacement = false;
        updateStatus(QStringLiteral("请先完成当前标注的属性编辑"), true);
        return;
    }
    DeploymentMarker marker;
    const bool normal = m_pendingPlacement == PendingPlacement::NormalMarker;
    const bool dynamic = m_pendingPlacement == PendingPlacement::DynamicMarker;
    const QString planId = m_pendingDynamicPlanId, stepId = m_pendingDynamicStepId;
    if (dynamic && !findDynamicStep(m_data, planId, stepId)) {
        m_pendingPlacement = PendingPlacement::None;
        m_pendingDynamicPlanId.clear(); m_pendingDynamicStepId.clear();
        updateStatus(QStringLiteral("所属预案阶段已不存在，草稿未保存"), true);
        return;
    }
    marker.id = newId(dynamic ? QStringLiteral("dynamic-marker")
        : (normal ? QStringLiteral("squad-marker") : QStringLiteral("marker")));
    marker.folderId = normal ? m_pendingNormalFolderId : QString();
    marker.name = m_pendingMarkerName;
    marker.category = m_pendingMarkerCategory;
    marker.geometryType = kind.isEmpty() ? m_pendingMarkerType : kind;
    marker.icon = defaultAnnotationResource(marker.geometryType);
    // PlotVisitProcessor uses red/0.8/2 for plotting nodes while ordinary
    // marker nodes use the FreeEarth marker defaults.
    applyLegacyAnnotationStyleDefaults(marker);
    const SceneViewpoint sceneView = m_scene->currentViewpoint();
    marker.points = referenceMarkerControlPoints(
        marker.geometryType, points, sceneView.range);
    marker.position = points.constFirst();
    // Image and particle resources are screen/range sized. Models and tilt
    // datasets use real model coordinates, so their reference default is a
    // direct 1.0 scale and must not be multiplied by the camera range.
    const double markSize = std::max(10.0, std::abs(sceneView.range) / 10.0);
    if (marker.geometryType == QStringLiteral("model")
        || marker.geometryType == QStringLiteral("tilt-model"))
        marker.pointSize = 10.0;
    else if (marker.geometryType == QStringLiteral("particle"))
        marker.pointSize = markSize;
    else if (marker.geometryType == QStringLiteral("billboard")
             || marker.geometryType == QStringLiteral("overlay"))
        marker.pointSize = markSize / 2.0;
    // The marker's stored viewpoint is the complete camera pose at the end
    // of drawing, not merely the first geometry coordinate. This preserves
    // framing, heading, pitch and distance when the tree item is activated.
    marker.viewpoint.longitude = sceneView.longitude;
    marker.viewpoint.latitude = sceneView.latitude;
    marker.viewpoint.altitude = sceneView.altitude;
    marker.viewpoint.heading = sceneView.heading;
    marker.viewpoint.pitch = sceneView.pitch;
    marker.viewpoint.range = sceneView.range;
    marker.viewFlyTimeSeconds = 1.0;
    if (marker.geometryType == QStringLiteral("text")
        || marker.geometryType == QStringLiteral("label")) {
        marker.text = marker.name;
    }
    if (marker.geometryType == QStringLiteral("label")) {
        marker.textSize = 18.0;
        marker.labelWidth = 130.0;
        marker.labelHeight = 30.0;
        marker.labelTextColor = QColor(QStringLiteral("#ffff00"));
        marker.labelBackgroundColor = QColor(QStringLiteral("#ffffff"));
        marker.labelBackgroundImage = QStringLiteral("texture/mark/label_title.png");
    }
    const bool returnToFixed = m_returnToFixedAfterPlacement;
    m_pendingPlacement = PendingPlacement::None;
    m_pendingDynamicPlanId.clear(); m_pendingDynamicStepId.clear();
    m_pendingMarkerName.clear();
    m_pendingMarkerType = QStringLiteral("point");
    m_pendingMarkerCategory = QStringLiteral("unit-annotation");
    m_returnToFixedAfterPlacement = false;
    QPointer<ZqbsPage> self(this);
    QDialog *propertyDialog = showDeploymentMarkerProperties(
        marker, markerViewpointProvider(m_scene), m_liveMarkers, m_leftPanel,
        [self, returnToFixed, normal, dynamic, planId, stepId, folderId = marker.folderId](bool accepted, const DeploymentMarker &result) {
            if (!self)
                return;
            self->m_scene->endMarkerPointEditing();
            self->m_markerPropertyDialog.clear();
            if (!accepted) {
                self->updateStatus(QStringLiteral("已取消新增标注，草稿未保存"));
                return;
            }
            if (dynamic) {
                DeploymentData candidate = self->m_data;
                auto *step = findDynamicStep(candidate, planId, stepId);
                if (!step) {
                    self->updateStatus(QStringLiteral("所属预案阶段已不存在，标注未保存"), true);
                    return;
                }
                DeploymentMarker saved = result;
                saved.folderId.clear();
                saved.category = QStringLiteral("plan");
                step->markers.append(saved);
                if (!self->commitDynamicPlans(candidate.plans)) return;
                self->m_selectedPlanId = planId; self->m_selectedStepId = stepId;
                self->m_filter->clear();
                self->refreshDynamicData();
                self->selectDynamicTreeItem(QStringLiteral("dynamic-marker"), saved.id,
                    planId + QLatin1Char('|') + stepId);
                self->updateStatus(QStringLiteral("已保存阶段标注“%1”的属性、视角和参数").arg(saved.name));
                return;
            }
            if (normal) {
                const auto *folder = normalFolder(self->m_data, folderId);
                if (!folder) {
                    self->updateStatus(QStringLiteral("所属部署分组文件夹已删除，标注未保存"), true);
                    return;
                }
                DeploymentMarker saved = result;
                saved.folderId = folderId;
                saved.category = folder->name;
                self->m_data.normalMarkers.append(saved);
                const bool revealed = saved.visible
                    && revealNormalMarkerAncestors(self->m_data, saved.folderId);
                self->saveNormal();
                self->rebuildTree();
                self->updateScene();
                if (self->m_domain == Domain::Normal) {
                    for (QTreeWidgetItemIterator it(self->m_tree); *it; ++it) {
                        if ((*it)->data(0, ItemTypeRole) == QStringLiteral("normal-marker")
                            && (*it)->data(0, ItemIdRole) == saved.id) {
                            self->m_tree->setCurrentItem(*it);
                            for (auto *parent = (*it)->parent(); parent; parent = parent->parent())
                                parent->setExpanded(true);
                            self->m_tree->scrollToItem(*it);
                            break;
                        }
                    }
                }
                self->updateStatus(revealed
                    ? QStringLiteral("已保存部署分组标注“%1”，并开启本地标注及所属文件夹显示").arg(saved.name)
                    : QStringLiteral("已保存部署分组标注“%1”").arg(saved.name));
                return;
            }
            const bool plotFolder = isPlotAnnotation(result.geometryType);
            const bool folderExists = std::any_of(
                self->m_data.sceneFolders.cbegin(), self->m_data.sceneFolders.cend(),
                [&result, plotFolder](const DeploymentSceneFolder &folder) {
                    return folder.plot == plotFolder && folder.name == result.category;
                });
            if (!folderExists) {
                DeploymentSceneFolder folder;
                folder.id = newId(QStringLiteral("folder"));
                folder.name = result.category;
                folder.plot = plotFolder;
                folder.viewpoint = result.viewpoint;
                self->m_data.sceneFolders.append(folder);
            }
            self->m_data.markers.append(result);
            self->saveFixedAnnotations();
            if (returnToFixed)
                self->setDomain(Domain::Fixed);
            self->rebuildTree();
            self->updateScene();
            self->updateStatus(QStringLiteral("已保存“%1”的属性、视角和参数")
                                   .arg(result.name));
        });
    m_markerPropertyDialog = propertyDialog;
    QPointer<QDialog> dialogGuard(propertyDialog);
    m_scene->beginMarkerPointEditing(marker,
        [dialogGuard](const QVector<GeoCoordinate> &changedPoints) {
            if (dialogGuard)
                updateDeploymentMarkerPropertyPoints(dialogGuard, changedPoints);
        });
    updateStatus(QStringLiteral("属性窗为非模式窗口；可拖动窗口，也可在地图上拖动控制点三轴"));
}

void ZqbsPage::receiveMeasurement(const QString &kind,
                                  const QVector<GeoCoordinate> &points,
                                  double value)
{
    if (points.size() < measurementMinimumPoints(kind))
        return;
    DeploymentMeasurement measurement;
    measurement.id = newId(QStringLiteral("measurement"));
    measurement.kind = kind == QStringLiteral("distance")
        ? QStringLiteral("space-distance")
        : (kind == QStringLiteral("area") ? QStringLiteral("shadow-area") : kind);
    applyLegacyMeasurementStyleDefaults(measurement);
    measurement.name = QStringLiteral("%1 %2")
        .arg(measurementDisplayName(measurement.kind))
        .arg(m_data.measurements.size() + 1);
    measurement.points = points;
    measurement.value = value;
    measurement.parameter = m_pendingMeasurementParameter;

    const auto sampleTerrain = [this](QVector<GeoCoordinate> samples,
                                      QVector<GeoCoordinate> &output) {
        QString error;
        if (!m_scene->sampleTerrainElevations(samples, error)) {
            QMessageBox::warning(m_leftPanel, QStringLiteral("高程采样失败"),
                QStringLiteral("无法完成地形分析：%1").arg(error));
            updateStatus(QStringLiteral("地形分析失败：%1").arg(error), true);
            return false;
        }
        output = std::move(samples);
        return true;
    };

    if (measurement.kind == QStringLiteral("space-distance")) {
        measurement.value = polylineDistance(points, true);
        measurement.resultText = formatMeasurementValue(measurement.kind, measurement.value);
    } else if (measurement.kind == QStringLiteral("height-distance")) {
        const double vertical = points.constLast().altitude
            - points.constFirst().altitude;
        measurement.value = vertical;
        measurement.resultText = formatMeasurementValue(measurement.kind, vertical);
        const GeoCoordinate &first = points.constFirst();
        const GeoCoordinate &last = points.constLast();
        GeoCoordinate corner = last.altitude <= first.altitude ? last : first;
        corner.altitude = std::max(first.altitude, last.altitude);
        measurement.analysisSegments = {first, corner, corner, last};
        measurement.analysisSegmentStates = {0, 0};
    } else if (measurement.kind == QStringLiteral("shadow-area")) {
        measurement.value = projectedPolygonArea(points);
        measurement.resultText = formatMeasurementValue(measurement.kind, measurement.value);
    } else if (measurement.kind == QStringLiteral("space-area")) {
        measurement.value = spatialPolygonArea(points);
        measurement.resultText = formatMeasurementValue(measurement.kind, measurement.value);
    } else if (measurement.kind == QStringLiteral("surface-distance")) {
        measurement.value = rhumbPolylineDistance(points);
        measurement.resultText = formatMeasurementValue(measurement.kind, measurement.value);
    } else if (measurement.kind == QStringLiteral("profile-analysis")) {
        QVector<GeoCoordinate> samples;
        const int count = qBound(2, qRound(measurement.parameter), 500);
        if (!sampleTerrain(densifyPolyline(points, count), samples))
            return;
        measurement.analysisPoints = samples;
        measurement.terrainBacked = true;
        double minimum = std::numeric_limits<double>::max();
        double maximum = std::numeric_limits<double>::lowest();
        double total = 0.0;
        for (qsizetype index = 0; index < samples.size(); ++index) {
            minimum = std::min(minimum, samples.at(index).altitude);
            maximum = std::max(maximum, samples.at(index).altitude);
            total += samples.at(index).altitude;
        }
        measurement.value = polylineDistance(samples, true);
        measurement.resultText = QStringLiteral("剖面长度 %1\n最低高程 %2 m\n最高高程 %3 m\n平均高程 %4 m\n采样点 %5")
            .arg(formatMeasurementValue(QStringLiteral("space-distance"), measurement.value))
            .arg(minimum, 0, 'f', 2).arg(maximum, 0, 'f', 2)
            .arg(total / samples.size(), 0, 'f', 2).arg(samples.size());
    } else if (measurement.kind == QStringLiteral("line-visible")) {
        int visibleTargets = 0;
        const GeoCoordinate observerControl = points.constFirst();
        for (qsizetype targetIndex = 1; targetIndex < points.size(); ++targetIndex) {
            QVector<GeoCoordinate> samples;
            if (!sampleTerrain(densifyPolyline(
                    {observerControl, points.at(targetIndex)}, 96), samples))
                return;
            measurement.analysisPoints += samples;
            const LineOfSightAnalysis analysis = analyzeLineOfSightTerrain(samples);
            if (!analysis.valid) {
                QMessageBox::warning(m_leftPanel, QStringLiteral("高程采样失败"),
                    QStringLiteral("通视剖面包含无效或 NoData 高程，未生成分析几何。"));
                updateStatus(QStringLiteral("通视分析失败：地形剖面不完整"), true);
                return;
            }
            if (analysis.targetVisible)
                ++visibleTargets;
            measurement.analysisSegments += analysis.segments;
            measurement.analysisSegmentStates += analysis.segmentStates;
        }
        measurement.terrainBacked = true;
        measurement.value = visibleTargets;
        measurement.resultText = QStringLiteral("可通视目标 %1 / %2")
            .arg(visibleTargets).arg(points.size() - 1);
    } else if (measurement.kind == QStringLiteral("gradient-analysis")) {
        const GeoCoordinate &a = points.constFirst();
        const GeoCoordinate &b = points.constLast();
        constexpr int GridSize = 50;
        QVector<GeoCoordinate> grid;
        grid.reserve(GridSize * GridSize);
        for (int row = 0; row < GridSize; ++row)
            for (int column = 0; column < GridSize; ++column)
                grid.append({a.longitude + (b.longitude - a.longitude) * column / (GridSize - 1),
                             a.latitude + (b.latitude - a.latitude) * row / (GridSize - 1), 0.0});
        if (!sampleTerrain(std::move(grid), measurement.analysisPoints))
            return;
        double maxSlope = 0.0;
        for (int row = 0; row + 1 < GridSize; ++row) {
            for (int column = 0; column + 1 < GridSize; ++column) {
                const GeoCoordinate &p = measurement.analysisPoints.at(row * GridSize + column);
                const GeoCoordinate &px = measurement.analysisPoints.at(row * GridSize + column + 1);
                const GeoCoordinate &py = measurement.analysisPoints.at((row + 1) * GridSize + column);
                const double dx = std::max(0.01, horizontalDistance(p, px));
                const double dy = std::max(0.01, horizontalDistance(p, py));
                maxSlope = std::max(maxSlope, std::atan(std::hypot(
                    (px.altitude - p.altitude) / dx,
                    (py.altitude - p.altitude) / dy))
                    * 180.0 / 3.14159265358979323846);
            }
        }
        measurement.terrainBacked = true;
        measurement.value = maxSlope;
        measurement.resultText = QStringLiteral("最大坡度 %1度\n采样网格 50 × 50")
            .arg(maxSlope, 0, 'f', 2);
    } else if (measurement.kind == QStringLiteral("radial-line-visible")) {
        const GeoCoordinate center = points.constFirst();
        const double radius = horizontalDistance(center, points.constLast());
        const double latitudeScale = 111320.0;
        const double longitudeScale = std::max(1000.0, latitudeScale
            * std::cos(center.latitude * 3.14159265358979323846 / 180.0));
        constexpr int RayCount = ViewshedRayCount;
        constexpr int DetectCount = ViewshedDetectCount;
        QVector<GeoCoordinate> allSamples;
        allSamples.reserve(RayCount * (DetectCount + 1));
        for (int ray = 0; ray < RayCount; ++ray) {
            const double angle = 2.0 * 3.14159265358979323846 * ray / RayCount;
            GeoCoordinate edge{center.longitude + radius * std::cos(angle) / longitudeScale,
                               center.latitude + radius * std::sin(angle) / latitudeScale, 0.0};
            allSamples += densifyPolyline({center, edge}, DetectCount + 1);
        }
        if (!sampleTerrain(allSamples, measurement.analysisPoints))
            return;
        const ViewshedAnalysis analysis = analyzeViewshedTerrain(
            measurement.analysisPoints, RayCount, DetectCount);
        if (!analysis.valid) {
            QMessageBox::warning(m_leftPanel, QStringLiteral("高程采样失败"),
                QStringLiteral("视域采样包含无效或 NoData 高程，未生成分析几何。"));
            updateStatus(QStringLiteral("视域分析失败：地形采样不完整"), true);
            return;
        }
        // Persist only the radial intervals. Circular completion geometry is
        // reproducible from the saved terrain samples and is rebuilt without
        // another terrain request when the scene/XML is restored.
        measurement.analysisSegments = analysis.radialSegments;
        measurement.analysisSegmentStates = analysis.radialSegmentStates;
        measurement.terrainBacked = true;
        measurement.value = analysis.visibleRayCount;
        measurement.resultText = QStringLiteral("通视方向 %1 / %2\n分析半径 %3")
            .arg(analysis.visibleRayCount).arg(RayCount)
            .arg(formatMeasurementValue(QStringLiteral("space-distance"), radius));
    } else if (measurement.kind == QStringLiteral("flood-analysis")
               || measurement.kind == QStringLiteral("fill-cut-analysis")
               || measurement.kind == QStringLiteral("contour-measure")) {
        const int GridSize = measurement.kind == QStringLiteral("contour-measure")
            ? 500 : 50;
        QVector<GeoCoordinate> analysisPolygon = points;
        if (measurement.kind == QStringLiteral("contour-measure")) {
            const GeoCoordinate &a = points.constFirst();
            const GeoCoordinate &b = points.constLast();
            analysisPolygon = {{a.longitude, a.latitude, 0.0},
                               {b.longitude, a.latitude, 0.0},
                               {b.longitude, b.latitude, 0.0},
                               {a.longitude, b.latitude, 0.0}};
        }
        double minLongitude = points.constFirst().longitude;
        double maxLongitude = minLongitude;
        double minLatitude = points.constFirst().latitude;
        double maxLatitude = minLatitude;
        for (const GeoCoordinate &point : points) {
            minLongitude = std::min(minLongitude, point.longitude);
            maxLongitude = std::max(maxLongitude, point.longitude);
            minLatitude = std::min(minLatitude, point.latitude);
            maxLatitude = std::max(maxLatitude, point.latitude);
        }
        QVector<GeoCoordinate> grid;
        grid.reserve(GridSize * GridSize);
        for (int row = 0; row < GridSize; ++row)
            for (int column = 0; column < GridSize; ++column)
                grid.append({minLongitude + (maxLongitude - minLongitude) * column / (GridSize - 1),
                             minLatitude + (maxLatitude - minLatitude) * row / (GridSize - 1), 0.0});
        QVector<GeoCoordinate> samples;
        if (!sampleTerrain(grid, samples))
            return;
        measurement.terrainBacked = true;
        double minimum = std::numeric_limits<double>::max();
        double maximum = std::numeric_limits<double>::lowest();
        double total = 0.0;
        int insideCount = 0;
        if (measurement.kind == QStringLiteral("contour-measure"))
            measurement.analysisPoints = samples;
        for (const GeoCoordinate &sample : samples) {
            if (!pointInPolygon(sample, analysisPolygon))
                continue;
            if (measurement.kind != QStringLiteral("contour-measure"))
                measurement.analysisPoints.append(sample);
            minimum = std::min(minimum, sample.altitude);
            maximum = std::max(maximum, sample.altitude);
            total += sample.altitude;
            ++insideCount;
        }
        if (insideCount == 0) {
            QMessageBox::warning(m_leftPanel, QStringLiteral("分析范围无效"),
                                 QStringLiteral("所选范围过小或无法获得有效地形采样点。"));
            return;
        }
        const double area = projectedPolygonArea(analysisPolygon);
        const double cellArea = area / insideCount;
        if (measurement.kind == QStringLiteral("flood-analysis")) {
            measurement.parameter = (maximum - minimum) / 20.0;
            measurement.secondaryParameter = minimum;
            measurement.value = 0.0;
            measurement.resultText = QStringLiteral("总面积 %1\n淹没面积 %2\n当前水位 %3 m\n范围最低/最高 %4 / %5 m")
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), area))
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), measurement.value))
                .arg(measurement.secondaryParameter, 0, 'f', 2)
                .arg(minimum, 0, 'f', 2).arg(maximum, 0, 'f', 2);
        } else if (measurement.kind == QStringLiteral("fill-cut-analysis")) {
            const double reference = std::abs(measurement.parameter) < 1e-9
                ? (minimum + maximum) / 2.0 : measurement.parameter;
            int fillCells = 0;
            int cutCells = 0;
            for (const GeoCoordinate &sample : measurement.analysisPoints) {
                const double difference = reference - sample.altitude;
                if (difference >= 0.0) {
                    ++fillCells;
                } else {
                    ++cutCells;
                }
            }
            // Preserve the supplied reference implementation's calculation:
            // total projected area times average terrain elevation, apportioned
            // by the sample counts above and below the reference plane.
            const double totalVolume = area * total / insideCount;
            const double fillVolume = totalVolume * fillCells / insideCount;
            const double cutVolume = totalVolume * cutCells / insideCount;
            measurement.parameter = reference;
            measurement.value = fillVolume + cutVolume;
            measurement.resultText = QStringLiteral("填方 %1 m^3 / 挖方 %2 m^3 / 总量 %3 m^3\n填方面积 %4 / 挖方面积 %5 / 总面积 %6\n基准高程 %7 m")
                .arg(fillVolume, 0, 'f', 1).arg(cutVolume, 0, 'f', 1)
                .arg(totalVolume, 0, 'f', 1)
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), fillCells * cellArea))
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), cutCells * cellArea))
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), area))
                .arg(reference, 0, 'f', 2);
        } else {
            const double interval = std::max(0.1, measurement.parameter);
            const double firstLevel = std::ceil(minimum / interval) * interval;
            const auto interpolate = [](const GeoCoordinate &a, const GeoCoordinate &b,
                                        double level) {
                const double denominator = b.altitude - a.altitude;
                const double t = std::abs(denominator) < 1e-9
                    ? 0.5 : qBound(0.0, (level - a.altitude) / denominator, 1.0);
                return GeoCoordinate{a.longitude + (b.longitude - a.longitude) * t,
                                     a.latitude + (b.latitude - a.latitude) * t, level};
            };
            int levelCount = 0;
            for (double level = firstLevel; level <= maximum; level += interval) {
                ++levelCount;
                for (int row = 0; row + 1 < GridSize; ++row) {
                    for (int column = 0; column + 1 < GridSize; ++column) {
                        const GeoCoordinate &a = samples.at(row * GridSize + column);
                        const GeoCoordinate &b = samples.at(row * GridSize + column + 1);
                        const GeoCoordinate &c = samples.at((row + 1) * GridSize + column + 1);
                        const GeoCoordinate &d = samples.at((row + 1) * GridSize + column);
                        GeoCoordinate center{(a.longitude + c.longitude) * 0.5,
                                             (a.latitude + c.latitude) * 0.5, 0.0};
                        if (!pointInPolygon(center, analysisPolygon))
                            continue;
                        QVector<GeoCoordinate> crossings;
                        const auto edge = [&crossings, &interpolate, level](
                            const GeoCoordinate &left, const GeoCoordinate &right) {
                            if ((left.altitude <= level && right.altitude > level)
                                || (right.altitude <= level && left.altitude > level))
                                crossings.append(interpolate(left, right, level));
                        };
                        edge(a, b); edge(b, c); edge(c, d); edge(d, a);
                        if (crossings.size() >= 2) {
                            measurement.analysisSegments.append(crossings.at(0));
                            measurement.analysisSegments.append(crossings.at(1));
                            if (crossings.size() == 4) {
                                measurement.analysisSegments.append(crossings.at(2));
                                measurement.analysisSegments.append(crossings.at(3));
                            }
                        }
                    }
                }
            }
            measurement.value = measurement.analysisSegments.size() / 2;
            measurement.resultText = QStringLiteral("生成等高线段 %1\n等高距 %2 m\n高程范围 %3–%4 m\n等高层级 %5")
                .arg(measurement.analysisSegments.size() / 2)
                .arg(interval, 0, 'f', 1).arg(minimum, 0, 'f', 2)
                .arg(maximum, 0, 'f', 2).arg(levelCount);
        }
    }
    m_data.measurements.append(measurement);
    saveFixed();
    rebuildTree();
    updateScene();
    showMeasurementDetails(m_data.measurements.constLast());
}

void ZqbsPage::showMeasurementDetails(
    const DeploymentMeasurement &measurement)
{
    if (m_measurementDialog)
        m_measurementDialog->close();
    m_measurementDialog = showDeploymentMeasurementResults(
        measurement, m_leftPanel,
        [this](DeploymentMeasurement &edited, QString &error) {
            auto found = std::find_if(m_data.measurements.begin(), m_data.measurements.end(),
                [&edited](const DeploymentMeasurement &value) {
                    return value.id == edited.id;
                });
            if (found == m_data.measurements.end()) {
                error = QStringLiteral("测量结果已被清除。");
                return false;
            }
            if (edited.kind == QStringLiteral("profile-analysis")
                && qRound(edited.parameter) != qRound(found->parameter)) {
                QVector<GeoCoordinate> samples = densifyPolyline(
                    edited.points, qBound(2, qRound(edited.parameter), 500));
                if (!m_scene->sampleTerrainElevations(samples, error))
                    return false;
                edited.analysisPoints = samples;
                edited.terrainBacked = true;
                const auto minimum = std::min_element(samples.begin(), samples.end(),
                    [](const GeoCoordinate &a, const GeoCoordinate &b) {
                        return a.altitude < b.altitude;
                    });
                const auto maximum = std::max_element(samples.begin(), samples.end(),
                    [](const GeoCoordinate &a, const GeoCoordinate &b) {
                        return a.altitude < b.altitude;
                    });
                double total = 0.0;
                for (const GeoCoordinate &point : samples) total += point.altitude;
                edited.value = polylineDistance(samples, true);
                edited.resultText = QStringLiteral("剖面长度 %1\n最低高程 %2 m\n最高高程 %3 m\n平均高程 %4 m\n采样点 %5")
                    .arg(formatMeasurementValue(QStringLiteral("space-distance"), edited.value))
                    .arg(minimum->altitude, 0, 'f', 2)
                    .arg(maximum->altitude, 0, 'f', 2)
                    .arg(total / samples.size(), 0, 'f', 2).arg(samples.size());
            } else if (edited.kind == QStringLiteral("contour-measure")
                       && std::abs(edited.parameter - found->parameter) > 0.01
                       && !rebuildContourSegments(edited, error)) {
                return false;
            }
            *found = edited;
            saveFixed();
            updateScene();
            updateStatus(QStringLiteral("已应用“%1”的测量参数和样式").arg(edited.name));
            return true;
        });
}

void ZqbsPage::treeItemChanged(QTreeWidgetItem *item, int)
{
    if (m_treeRebuilding || !item)
        return;
    const QString type = item->data(0, ItemTypeRole).toString();
    const QString id = item->data(0, ItemIdRole).toString();
    const bool visible = item->checkState(0) == Qt::Checked;
    if (type == QStringLiteral("layer")) {
        for (DeploymentLayer &value : m_data.layers) if (value.id == id) value.visible = visible;
        saveFixed();
    } else if (type == QStringLiteral("scene-system-root")) {
        for (DeploymentLayer &value : m_data.layers)
            if (value.runtimeLayer || value.systemLayer) value.visible = visible;
        saveFixed();
        rebuildTree();
    } else if (type == QStringLiteral("scene-system-folder")) {
        for (DeploymentLayer &value : m_data.layers)
            if (value.runtimeLayer) value.visible = visible;
        saveFixed();
        rebuildTree();
    } else if (type == QStringLiteral("scene-system-feature-folder")) {
        for (DeploymentLayer &value : m_data.layers)
            if (value.systemLayer) value.visible = visible;
        saveFixed();
        rebuildTree();
    } else if (type == QStringLiteral("scene-custom-root")) {
        for (DeploymentLayer &value : m_data.layers)
            if (!value.runtimeLayer && !value.systemLayer) value.visible = visible;
        saveFixed();
        rebuildTree();
    } else if (type == QStringLiteral("scene-custom-kind")) {
        const QString kind = item->data(0, ItemAuxRole).toString();
        for (DeploymentLayer &value : m_data.layers)
            if (!value.runtimeLayer && !value.systemLayer && value.kind == kind)
                value.visible = visible;
        saveFixed();
        rebuildTree();
    } else if (type == QStringLiteral("scene-mark-root")
               || type == QStringLiteral("scene-plot-root")) {
        const bool plot = type == QStringLiteral("scene-plot-root");
        for (DeploymentMarker &value : m_data.markers)
            if (isPlotAnnotation(value.geometryType) == plot) value.visible = visible;
        for (DeploymentSceneFolder &folder : m_data.sceneFolders)
            if (folder.plot == plot) folder.visible = visible;
        saveFixedAnnotations();
        rebuildTree();
    } else if (type == QStringLiteral("scene-mark-folder")
               || type == QStringLiteral("scene-plot-folder")) {
        const bool plot = type == QStringLiteral("scene-plot-folder");
        const QString category = item->data(0, ItemAuxRole).toString();
        for (DeploymentSceneFolder &folder : m_data.sceneFolders)
            if (folder.plot == plot && (folder.id == id || folder.name == category))
                folder.visible = visible;
        for (DeploymentMarker &value : m_data.markers)
            if (isPlotAnnotation(value.geometryType) == plot && value.category == category)
                value.visible = visible;
        saveFixedAnnotations();
        rebuildTree();
    } else if (type == QStringLiteral("measurement")) {
        for (DeploymentMeasurement &value : m_data.measurements) if (value.id == id) value.visible = visible;
        saveFixed();
    } else if (type == QStringLiteral("marker")) {
        for (DeploymentMarker &value : m_data.markers) if (value.id == id) value.visible = visible;
        saveFixedAnnotations();
    } else if (type == QStringLiteral("binding-rule")) {
        for (DeploymentBindingRule &value : m_data.bindingRules) if (value.entityType == id) value.visible = visible;
        saveNormal();
        for (DeploymentMarker &marker : m_liveMarkers)
            if (marker.bindingType == id) marker.visible = visible;
    } else if (type == QStringLiteral("normal-local-root")) {
        m_data.normalLocalVisible = visible;
        saveNormal();
    } else if (type == QStringLiteral("normal-folder")) {
        for (auto &folder : m_data.normalFolders) if (folder.id == id) folder.visible = visible;
        saveNormal();
    } else if (type == QStringLiteral("normal-marker")) {
        for (auto &marker : m_data.normalMarkers) if (marker.id == id) marker.visible = visible;
        saveNormal();
    }
    updateScene();
}

void ZqbsPage::treeSelectionChanged()
{
    if (m_treeRebuilding) return;
    QTreeWidgetItem *item = m_tree->currentItem();
    updateInspector(item);
    if (!item) return;
    const QString type = item->data(0, ItemTypeRole).toString();
    if (type == QStringLiteral("plan")) {
        m_selectedPlanId = item->data(0, ItemIdRole).toString();
        pausePlan();
        m_selectedStepId.clear();
        if (auto *plan = selectedPlan(); plan && !plan->steps.isEmpty())
            showPlanStep(0, false);
        else stopPlan();
    } else if (type == QStringLiteral("step")) {
        pausePlan();
        m_selectedPlanId = item->data(0, ItemAuxRole).toString();
        m_selectedStepId = item->data(0, ItemIdRole).toString();
        if (DeploymentPlan *plan = selectedPlan()) {
            const auto it = std::find_if(plan->steps.cbegin(), plan->steps.cend(),
                [this](const DeploymentPlanStep &step) { return step.id == m_selectedStepId; });
            m_currentStep = it == plan->steps.cend() ? 0 : int(it - plan->steps.cbegin());
            showPlanStep(m_currentStep, false);
        }
    } else if (type == QStringLiteral("dynamic-marker")) {
        pausePlan();
        const QString aux = item->data(0, ItemAuxRole).toString();
        m_selectedPlanId = aux.section(QLatin1Char('|'), 0, 0);
        m_selectedStepId = aux.section(QLatin1Char('|'), 1, 1);
        resetPlanPlayback(PlanState::Preview);
        if (auto *plan = findDynamicPlan(m_data, m_selectedPlanId)) {
            for (int i = 0; i < plan->steps.size(); ++i)
                if (plan->steps[i].id == m_selectedStepId) {
                    m_currentStep = i; m_dynamicMarkers = plan->steps[i].markers; break;
                }
        }
        rebuildTimeline(); updateScene();
    }
}

void ZqbsPage::treeItemActivated(QTreeWidgetItem *item, int)
{
    if (!item) return;
    const QString type = item->data(0, ItemTypeRole).toString();
    const QString id = item->data(0, ItemIdRole).toString();
    if (type == QStringLiteral("measurement")) {
        const auto found = std::find_if(m_data.measurements.cbegin(), m_data.measurements.cend(),
            [&id](const DeploymentMeasurement &value) { return value.id == id; });
        if (found != m_data.measurements.cend())
            showMeasurementDetails(*found);
    } else if (type == QStringLiteral("marker") || type == QStringLiteral("normal-marker")) {
        const auto &markers = type == QStringLiteral("normal-marker") ? m_data.normalMarkers : m_data.markers;
        const auto found = std::find_if(markers.cbegin(), markers.cend(),
            [&id](const DeploymentMarker &marker) { return marker.id == id; });
        if (found != markers.cend()) {
            const DeploymentViewpoint &saved = found->viewpoint;
            const bool savedViewValid = std::isfinite(saved.longitude)
                && std::isfinite(saved.latitude)
                && std::isfinite(saved.altitude)
                && std::isfinite(saved.heading)
                && std::isfinite(saved.pitch)
                && std::isfinite(saved.range)
                && saved.latitude >= -90.0 && saved.latitude <= 90.0
                && saved.longitude >= -180.0 && saved.longitude <= 180.0
                && saved.range > 0.0;
            SceneViewpoint view;
            if (savedViewValid) {
                view = {saved.longitude, saved.latitude, saved.altitude,
                        saved.heading, saved.pitch, saved.range, true};
            } else {
                view = m_scene->currentViewpoint();
                view.longitude = found->position.longitude;
                view.latitude = found->position.latitude;
                view.altitude = found->position.altitude;
                view.range = qMin(view.range, 3000.0);
                view.valid = true;
            }
            m_scene->flyTo(view, qMax(0.0, found->viewFlyTimeSeconds));
        }
    } else if (type == QStringLiteral("live")) {
        const auto found = std::find_if(m_liveMarkers.cbegin(), m_liveMarkers.cend(),
            [&id](const DeploymentMarker &marker) { return marker.id == id; });
        if (found != m_liveMarkers.cend()) {
            SceneViewpoint view = m_scene->currentViewpoint();
            view.longitude = found->position.longitude;
            view.latitude = found->position.latitude;
            view.altitude = found->position.altitude;
            view.range = qMin(view.range, 3000.0);
            view.valid = true;
            m_scene->flyTo(view);
        }
    } else if (type == QStringLiteral("dynamic-marker")) {
        const QString dynamicAux = item->data(0, ItemAuxRole).toString();
        const QString planId = dynamicAux.section(QLatin1Char('|'), 0, 0);
        const QString stepId = dynamicAux.section(QLatin1Char('|'), 1, 1);
        for (const DeploymentPlan &plan : m_data.plans) if (plan.id == planId)
            for (const DeploymentPlanStep &step : plan.steps) if (step.id == stepId) {
                const auto found = std::find_if(step.markers.cbegin(), step.markers.cend(),
                    [&id](const DeploymentMarker &marker) { return marker.id == id; });
                if (found != step.markers.cend()) {
                    const auto &saved = found->viewpoint;
                    SceneViewpoint view{saved.longitude, saved.latitude, saved.altitude,
                        saved.heading, saved.pitch, saved.range, saved.range > 0.0};
                    if (!view.valid) {
                        view = m_scene->currentViewpoint();
                        view.longitude = found->position.longitude;
                        view.latitude = found->position.latitude;
                        view.altitude = found->position.altitude;
                        view.range = qMin(view.range, 3000.0); view.valid = true;
                    }
                    m_scene->flyTo(view, qMax(0.0, found->viewFlyTimeSeconds));
                }
            }
    } else if (type == QStringLiteral("viewpoint")) {
        const auto found = std::find_if(m_data.viewpoints.cbegin(), m_data.viewpoints.cend(),
            [&id](const DeploymentViewpoint &view) { return view.id == id; });
        if (found != m_data.viewpoints.cend()) {
            SceneViewpoint view{found->longitude, found->latitude, found->altitude,
                found->heading, found->pitch, found->range, true};
            m_scene->flyTo(view, found->flyTimeSeconds);
        }
    } else if (type == QStringLiteral("step")) {
        treeSelectionChanged();
    }
}

void ZqbsPage::removeSelected()
{
    if (m_markerPropertyDialog) {
        m_markerPropertyDialog->raise();
        updateStatus(QStringLiteral("请先完成当前标注的属性编辑再删除"), true);
        return;
    }
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item) return;
    const QString type = item->data(0, ItemTypeRole).toString();
    const QString id = item->data(0, ItemIdRole).toString();
    const QString aux = item->data(0, ItemAuxRole).toString();
    if (type == QStringLiteral("plan") || type == QStringLiteral("step")
        || type == QStringLiteral("dynamic-marker")) {
        removeDynamicItem(type, id, aux);
        return;
    }
    if (type == QStringLiteral("scene-mark-folder")
        || type == QStringLiteral("scene-plot-folder")) {
        clearSceneBranch(type, aux);
        return;
    }
    if (type == QStringLiteral("scene-custom-root")
        || type == QStringLiteral("scene-viewpoint-root")
        || type == QStringLiteral("scene-mark-root")
        || type == QStringLiteral("scene-plot-root")) {
        clearSceneBranch(type);
        return;
    }
    if (type == QStringLiteral("scene-custom-kind")) {
        clearSceneBranch(type, aux);
        return;
    }
    if (type == QStringLiteral("scene-system-root")
        || type == QStringLiteral("scene-system-folder")
        || type == QStringLiteral("scene-system-feature-folder")) {
        updateStatus(QStringLiteral("系统图层不能删除，可取消勾选隐藏"), true);
        return;
    }
    if (type == QStringLiteral("live") || type == QStringLiteral("binding-rule")
        || type == QStringLiteral("group") || type == QStringLiteral("normal-local-root")) {
        updateStatus(QStringLiteral("系统根文件夹不能在本地删除，可取消勾选隐藏"), true);
        return;
    }
    if (type == QStringLiteral("normal-folder")) {
        const auto folders = normalFolderSubtree(m_data, id);
        const auto count = std::count_if(m_data.normalMarkers.cbegin(), m_data.normalMarkers.cend(),
            [&folders](const DeploymentMarker &marker) { return folders.contains(marker.folderId); });
        if (QMessageBox::question(m_leftPanel, QStringLiteral("删除部署分组文件夹"),
                QStringLiteral("确认删除“%1”及其子文件夹（共 %2 个文件夹、%3 个标注）？\n基础场景不受影响。")
                    .arg(item->text(0)).arg(folders.size()).arg(count)) != QMessageBox::Yes)
            return;
        removeNormalFolder(m_data, id);
        saveNormal();
        rebuildTree();
        updateScene();
        return;
    }
    if (type == QStringLiteral("layer")) {
        const auto layer = std::find_if(m_data.layers.cbegin(), m_data.layers.cend(),
            [&id](const DeploymentLayer &value) { return value.id == id; });
        if (layer != m_data.layers.cend()
            && (layer->runtimeLayer || layer->systemLayer)) {
            updateStatus(QStringLiteral("系统图层不能从这里删除，可取消勾选隐藏"), true);
            return;
        }
    }
    if (QMessageBox::question(m_leftPanel, QStringLiteral("确认删除"),
            QStringLiteral("确认删除“%1”？").arg(item->text(0))) != QMessageBox::Yes)
        return;
    if (type == QStringLiteral("layer")) {
        m_data.layers.erase(std::remove_if(m_data.layers.begin(), m_data.layers.end(),
            [&id](const DeploymentLayer &value) { return value.id == id; }), m_data.layers.end());
        saveFixed();
    } else if (type == QStringLiteral("measurement")) {
        m_data.measurements.erase(std::remove_if(m_data.measurements.begin(), m_data.measurements.end(),
            [&id](const DeploymentMeasurement &value) { return value.id == id; }), m_data.measurements.end());
        saveFixed();
    } else if (type == QStringLiteral("viewpoint")) {
        m_data.viewpoints.erase(std::remove_if(m_data.viewpoints.begin(), m_data.viewpoints.end(),
            [&id](const DeploymentViewpoint &value) { return value.id == id; }), m_data.viewpoints.end());
        saveFixed();
    } else if (type == QStringLiteral("marker")) {
        m_data.markers.erase(std::remove_if(m_data.markers.begin(), m_data.markers.end(),
            [&id](const DeploymentMarker &value) { return value.id == id; }), m_data.markers.end());
        saveFixedAnnotations();
    } else if (type == QStringLiteral("normal-marker")) {
        m_data.normalMarkers.erase(std::remove_if(m_data.normalMarkers.begin(), m_data.normalMarkers.end(),
            [&id](const DeploymentMarker &value) { return value.id == id; }), m_data.normalMarkers.end());
        saveNormal();
    }
    rebuildTree(); rebuildTimeline(); updateScene();
}

void ZqbsPage::editSelected()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item) return;
    const QString type = item->data(0, ItemTypeRole).toString();
    const QString id = item->data(0, ItemIdRole).toString();
    if (type == QStringLiteral("plan")) { editDynamicPlan(id); return; }
    if (type == QStringLiteral("step")) {
        editDynamicStep(item->data(0, ItemAuxRole).toString(), id);
        return;
    }
    if (type == QStringLiteral("normal-folder")) {
        editNormalFolder(id);
        return;
    }
    if (type == QStringLiteral("normal-local-root")
        || type == QStringLiteral("live") || type == QStringLiteral("binding-rule")) {
        updateInspector(item);
        updateStatus(QStringLiteral("此项由系统维护，可在数据树中控制显隐"));
        return;
    }
    if (type == QStringLiteral("layer")) {
        auto found = std::find_if(m_data.layers.begin(), m_data.layers.end(),
            [&id](const DeploymentLayer &value) { return value.id == id; });
        if (found == m_data.layers.end()) return;
        SkinnedDialog dialog(QStringLiteral("编辑图层"), m_leftPanel);
        auto *layout = new QVBoxLayout(createDeploymentDialogBody(dialog));
        auto *form = new QFormLayout;
        const bool editableDefinition = !found->runtimeLayer && !found->systemLayer;
        auto *nameEdit = new QLineEdit(found->name, &dialog);
        nameEdit->setReadOnly(found->systemLayer);
        auto *opacity = new QSpinBox(&dialog);
        opacity->setRange(0, 100);
        opacity->setSuffix(QStringLiteral(" %"));
        opacity->setValue(qRound((found->opacityEnabled ? found->opacity : 1.0) * 100.0));
        auto *source = new QLineEdit(found->source, &dialog);
        source->setReadOnly(found->runtimeLayer || found->systemLayer);
        auto *color = new QLineEdit(found->color.name(QColor::HexRgb), &dialog);
        auto *lineWidth = new QDoubleSpinBox(&dialog);
        lineWidth->setRange(0.1, 20.0);
        lineWidth->setValue(found->lineWidth);
        lineWidth->setSuffix(QStringLiteral(" px"));
        form->addRow(QStringLiteral("名称"), nameEdit);
        form->addRow(QStringLiteral("数据源"), source);
        form->addRow(QStringLiteral("透明度"), opacity);
        if (editableDefinition) {
            form->addRow(QStringLiteral("矢量颜色"), color);
            form->addRow(QStringLiteral("矢量线宽"), lineWidth);
        } else if (found->systemLayer) {
            auto *range = new QLineEdit(QStringLiteral("%1 ～ %2 米")
                .arg(found->minRange, 0, 'f', 0)
                .arg(found->maxRange, 0, 'f', 0), &dialog);
            range->setReadOnly(true);
            auto *style = new QLineEdit(found->styleName, &dialog);
            style->setReadOnly(true);
            form->addRow(QStringLiteral("显示范围"), range);
            form->addRow(QStringLiteral("配置样式"), style);
        }
        layout->addLayout(form);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted || nameEdit->text().trimmed().isEmpty()) return;
        const QColor layerColor(color->text().trimmed());
        if (editableDefinition && !layerColor.isValid()) {
            QMessageBox::warning(m_leftPanel, QStringLiteral("颜色无效"),
                                 QStringLiteral("请输入 #RRGGBB 格式的颜色。"));
            return;
        }
        const DeploymentLayer previous = *found;
        if (!found->systemLayer)
            found->name = nameEdit->text().trimmed();
        if (editableDefinition)
            found->source = source->text().trimmed();
        found->opacity = opacity->value() / 100.0;
        found->opacityEnabled = true;
        if (editableDefinition) {
            found->color = layerColor;
            found->lineWidth = lineWidth->value();
        }
        QString layerError;
        if (!m_scene->setDeploymentLayers(m_data.layers, layerError)) {
            *found = previous;
            QString restoreError;
            m_scene->setDeploymentLayers(m_data.layers, restoreError);
            QMessageBox::warning(m_leftPanel, QStringLiteral("图层加载失败"), layerError);
            return;
        }
        saveFixed(); rebuildTree(); updateScene();
        return;
    }
    if (type == QStringLiteral("marker") || type == QStringLiteral("normal-marker")
        || type == QStringLiteral("dynamic-marker")) {
        if (m_markerPropertyDialog) {
            m_markerPropertyDialog->raise();
            m_markerPropertyDialog->activateWindow();
            updateStatus(QStringLiteral("请先完成当前标注的属性编辑"), true);
            return;
        }
        const bool normal = type == QStringLiteral("normal-marker");
        const bool dynamic = type == QStringLiteral("dynamic-marker");
        const QString context = item->data(0, ItemAuxRole).toString();
        const QString planId = context.section(QLatin1Char('|'), 0, 0);
        const QString stepId = context.section(QLatin1Char('|'), 1, 1);
        if (dynamic && !prepareDynamicEdit()) return;
        auto *step = dynamic ? findDynamicStep(m_data, planId, stepId) : nullptr;
        if (dynamic && !step) return;
        auto &markers = dynamic ? step->markers : (normal ? m_data.normalMarkers : m_data.markers);
        auto found = std::find_if(markers.begin(), markers.end(),
            [&id](const DeploymentMarker &value) { return value.id == id; });
        if (found == markers.end()) return;
        const DeploymentMarker original = *found;
        QPointer<ZqbsPage> self(this);
        QDialog *propertyDialog = showDeploymentMarkerProperties(
            original, markerViewpointProvider(m_scene), m_liveMarkers, m_leftPanel,
            [self, id, normal, dynamic, planId, stepId](bool accepted, const DeploymentMarker &result) {
                if (!self)
                    return;
                self->m_scene->endMarkerPointEditing();
                self->m_markerPropertyDialog.clear();
                if (!accepted) {
                    self->updateStatus(QStringLiteral("已取消属性修改"));
                    return;
                }
                if (dynamic) {
                    DeploymentData candidate = self->m_data;
                    auto *marker = findDynamicMarker(candidate, planId, stepId, id);
                    if (!marker) {
                        self->updateStatus(QStringLiteral("所属步骤或标记已不存在，属性未保存"), true);
                        return;
                    }
                    *marker = result;
                    marker->id = id;
                    if (!self->commitDynamicPlans(candidate.plans)) return;
                    self->m_selectedPlanId = planId; self->m_selectedStepId = stepId;
                    self->refreshDynamicData();
                    self->selectDynamicTreeItem(QStringLiteral("dynamic-marker"), id, planId + QLatin1Char('|') + stepId);
                    return;
                }
                auto &markers = normal ? self->m_data.normalMarkers : self->m_data.markers;
                auto current = std::find_if(
                    markers.begin(), markers.end(),
                    [&id](const DeploymentMarker &value) { return value.id == id; });
                if (current == markers.end())
                    return;
                const QString folderId = current->folderId;
                *current = result;
                if (normal) {
                    current->folderId = folderId;
                    if (const auto *folder = normalFolder(self->m_data, folderId)) current->category = folder->name;
                    self->saveNormal();
                } else self->saveFixedAnnotations();
                self->rebuildTree();
                self->updateScene();
                self->updateStatus(QStringLiteral("已保存“%1”的属性修改")
                                       .arg(result.name));
            });
        m_markerPropertyDialog = propertyDialog;
        QPointer<QDialog> dialogGuard(propertyDialog);
        m_scene->beginMarkerPointEditing(original,
            [dialogGuard](const QVector<GeoCoordinate> &changedPoints) {
                if (dialogGuard)
                    updateDeploymentMarkerPropertyPoints(dialogGuard, changedPoints);
            });
        updateStatus(QStringLiteral("属性窗为非模式窗口；可在地图上拖动控制点三轴"));
        return;
    }
    if (type == QStringLiteral("viewpoint")) {
        editViewpoint(id);
        return;
    }
    if (type == QStringLiteral("scene-mark-folder")
        || type == QStringLiteral("scene-plot-folder")) {
        editSceneFolder(id);
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(m_leftPanel, QStringLiteral("编辑名称"),
        QStringLiteral("名称"), QLineEdit::Normal, item->text(0), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (type == QStringLiteral("measurement")) {
        for (DeploymentMeasurement &value : m_data.measurements) if (value.id == id) value.name = name;
        saveFixed();
    } else {
        updateStatus(QStringLiteral("该对象不支持本地改名"), true);
        return;
    }
    rebuildTree();
}

void ZqbsPage::showEnvironmentMenu()
{
    // Keep live preview and quick-toggle state in one editor while it is open.
    if (m_environmentDialog) {
        m_environmentDialog->show(); m_environmentDialog->raise();
        m_environmentDialog->activateWindow();
        return;
    }
    auto *palette = new DeploymentToolPalette(QStringLiteral("环境"),
        QStringLiteral(":/legacy/images/toolbar/environment.png"), 2, m_leftPanel);
    const auto applyToggle = [this](const QString &name) {
        saveFixed();
        updateScene();
        updateStatus(QStringLiteral("%1已%2")
            .arg(name, name == QStringLiteral("大气层")
                ? (m_data.environment.atmosphereVisible ? QStringLiteral("显示")
                                                        : QStringLiteral("隐藏"))
                : name == QStringLiteral("光照")
                    ? (m_data.environment.sunVisible ? QStringLiteral("显示")
                                                     : QStringLiteral("隐藏"))
                    : name == QStringLiteral("雨")
                        ? (m_data.environment.rainVisible ? QStringLiteral("显示")
                                                         : QStringLiteral("隐藏"))
                        : name == QStringLiteral("雪")
                            ? (m_data.environment.snowVisible ? QStringLiteral("显示")
                                                             : QStringLiteral("隐藏"))
                            : (m_data.environment.fogVisible ? QStringLiteral("显示")
                                                            : QStringLiteral("隐藏"))));
    };
    palette->addToggleTool(QStringLiteral("光照"), deploymentIcon(QStringLiteral("sun_light.png")),
                           m_data.environment.sunVisible,
                           [this, applyToggle](bool visible) {
        m_data.environment.sunVisible = visible;
        applyToggle(QStringLiteral("光照"));
    });
    palette->addToggleTool(QStringLiteral("大气层"), deploymentIcon(QStringLiteral("atmosphere.png")),
                           m_data.environment.atmosphereVisible,
                           [this, applyToggle](bool visible) {
        m_data.environment.atmosphereVisible = visible;
        applyToggle(QStringLiteral("大气层"));
    });
    palette->addToggleTool(QStringLiteral("雨"), deploymentIcon(QStringLiteral("rain.png")),
                           m_data.environment.rainVisible,
                           [this, applyToggle](bool visible) {
        m_data.environment.rainVisible = visible;
        if (visible && m_data.environment.rain <= 0)
            m_data.environment.rain = 50;
        applyToggle(QStringLiteral("雨"));
    });
    palette->addToggleTool(QStringLiteral("雪"), deploymentIcon(QStringLiteral("snow.png")),
                           m_data.environment.snowVisible,
                           [this, applyToggle](bool visible) {
        m_data.environment.snowVisible = visible;
        if (visible && m_data.environment.snow <= 0)
            m_data.environment.snow = 50;
        applyToggle(QStringLiteral("雪"));
    });
    palette->addToggleTool(QStringLiteral("雾"), deploymentIcon(QStringLiteral("fog.png")),
                           m_data.environment.fogVisible,
                           [this, applyToggle](bool visible) {
        m_data.environment.fogVisible = visible;
        if (visible && m_data.environment.fog <= 0)
            m_data.environment.fog = 50;
        applyToggle(QStringLiteral("雾"));
    });
    palette->addTool(QStringLiteral("设置"), deploymentIcon(QStringLiteral("setttings.png")),
        [this] { configureEnvironment(); });
    palette->showBeside(qobject_cast<QWidget *>(sender()));
}

void ZqbsPage::configureEnvironment(int initialTab)
{
    if (m_environmentDialog) {
        m_environmentDialog->selectPage(initialTab);
        m_environmentDialog->raise(); m_environmentDialog->activateWindow(); return;
    }
    EnvironmentSettings initial = m_data.environment;
    const QDateTime now = m_scene->environmentTime();
    if (now.isValid()) {
        initial.calendarDate = now.date().toString(Qt::ISODate);
        initial.timeOfDay = now.time().hour(); initial.minuteOfHour = now.time().minute();
        initial.secondOfMinute = now.time().second();
    }
    auto *dialog = new EnvironmentSettingsDialog(initial, m_leftPanel,
        [this](const EnvironmentSettings &preview) { m_scene->setEnvironmentSettings(preview); },
        [this](double longitude, double latitude, double height) {
            SceneViewpoint target;
            target.longitude = longitude; target.latitude = latitude; target.altitude = height / 4.0;
            target.heading = 0; target.pitch = 0; target.range = height / 4.0; target.valid = true;
            m_scene->flyTo(target, 3.0);
        }, [this] { return m_scene->environmentTime(); });
    m_environmentDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
        if (result == QDialog::Accepted) {
            m_data.environment = dialog->snapshot();
            saveFixed();
        }
        m_environmentDialog.clear();
        updateScene(); // Cancel restores saved settings, including time mode.
    });
    dialog->selectPage(initialTab); dialog->show(); dialog->raise();
}

void ZqbsPage::saveViewpoint()
{
    editViewpoint();
}

void ZqbsPage::editViewpoint(const QString &viewpointId)
{
    auto existing = viewpointId.isEmpty() ? m_data.viewpoints.end()
        : std::find_if(m_data.viewpoints.begin(), m_data.viewpoints.end(),
            [&viewpointId](const DeploymentViewpoint &view) { return view.id == viewpointId; });
    const SceneViewpoint currentScene = m_scene->currentViewpoint();
    DeploymentViewpoint value;
    if (existing != m_data.viewpoints.end()) {
        value = *existing;
    } else {
        value.id = newId(QStringLiteral("view"));
        value.name = QStringLiteral("新建视点 %1").arg(m_data.viewpoints.size() + 1);
        value.longitude = currentScene.longitude;
        value.latitude = currentScene.latitude;
        value.altitude = currentScene.altitude;
        value.heading = currentScene.heading;
        value.pitch = currentScene.pitch;
        value.range = currentScene.range;
    }

    SkinnedDialog dialog(existing == m_data.viewpoints.end()
        ? QStringLiteral("添加视点") : QStringLiteral("视点属性"), m_leftPanel);
    dialog.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(createDeploymentDialogBody(dialog));
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(value.name, &dialog);
    auto coordinate = [&dialog](double minimum, double maximum, int decimals, double initial) {
        auto *box = new QDoubleSpinBox(&dialog);
        box->setRange(minimum, maximum);
        box->setDecimals(decimals);
        box->setValue(initial);
        return box;
    };
    auto *longitude = coordinate(-180.0, 180.0, 8, value.longitude);
    auto *latitude = coordinate(-90.0, 90.0, 8, value.latitude);
    auto *height = coordinate(-11000000.0, 1000000000.0, 3, value.altitude);
    auto *pitch = coordinate(-180.0, 180.0, 3, value.pitch);
    auto *heading = coordinate(-360.0, 360.0, 3, value.heading);
    auto *range = coordinate(0.0, 1000000000.0, 3, value.range);
    auto *flyTime = coordinate(0.0, 3600.0, 2, value.flyTimeSeconds);
    longitude->setSuffix(QStringLiteral(" °"));
    latitude->setSuffix(QStringLiteral(" °"));
    height->setSuffix(QStringLiteral(" m"));
    pitch->setSuffix(QStringLiteral(" °"));
    heading->setSuffix(QStringLiteral(" °"));
    range->setSuffix(QStringLiteral(" m"));
    flyTime->setSuffix(QStringLiteral(" s"));
    form->addRow(QStringLiteral("图层名称"), name);
    form->addRow(QStringLiteral("经度"), longitude);
    form->addRow(QStringLiteral("纬度"), latitude);
    form->addRow(QStringLiteral("高度"), height);
    form->addRow(QStringLiteral("俯仰角"), pitch);
    form->addRow(QStringLiteral("航向角"), heading);
    form->addRow(QStringLiteral("视距"), range);
    form->addRow(QStringLiteral("飞行时间"), flyTime);
    layout->addLayout(form);
    auto *currentButton = new QPushButton(QStringLiteral("获取当前视角"), &dialog);
    layout->addWidget(currentButton, 0, Qt::AlignLeft);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(currentButton, &QPushButton::clicked, &dialog,
            [this, longitude, latitude, height, heading, pitch, range] {
        const SceneViewpoint current = m_scene->currentViewpoint();
        longitude->setValue(current.longitude);
        latitude->setValue(current.latitude);
        height->setValue(current.altitude);
        heading->setValue(current.heading);
        pitch->setValue(current.pitch);
        range->setValue(current.range);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty())
        return;
    value.name = name->text().trimmed();
    value.longitude = longitude->value();
    value.latitude = latitude->value();
    value.altitude = height->value();
    value.pitch = pitch->value();
    value.heading = heading->value();
    value.range = range->value();
    value.flyTimeSeconds = flyTime->value();
    if (existing == m_data.viewpoints.end())
        m_data.viewpoints.append(value);
    else
        *existing = value;
    saveFixed();
    setDomain(Domain::Fixed);
}

void ZqbsPage::showFlightDialog()
{
    if (m_flightDialog) {
        if (!m_flightDialog->isVisible())
            static_cast<FlightRoamDialog *>(m_flightDialog.data())->reactivate();
        m_flightDialog->show(); m_flightDialog->raise(); m_flightDialog->activateWindow();
        return;
    }
    pausePlan();
    m_scene->cancelDeploymentInteraction();
    if (m_markerPropertyDialog) m_markerPropertyDialog->reject();
    auto *dialog = new FlightRoamDialog(m_scene, m_leftPanel);
    m_flightDialog = dialog;
    dialog->show();
    dialog->move(m_scene->mapToGlobal(QPoint(20, 62)));
}

void ZqbsPage::showViewpointMenu()
{
    auto *palette = new DeploymentToolPalette(QStringLiteral("视角"),
        QStringLiteral(":/legacy/images/toolbar/viewpoint.png"), 2, m_leftPanel);
    palette->addTool(QStringLiteral("视点复位"), deploymentIcon(QStringLiteral("viewpoint_reset.png")),
                     [this] { m_scene->resetView(); });
    palette->addTool(QStringLiteral("视点正北"), deploymentIcon(QStringLiteral("viewpoint_north.png")), [this] {
        SceneViewpoint view = m_scene->currentViewpoint();
        view.heading = 0.0;
        view.valid = true;
        m_scene->flyTo(view, 0.5);
    });
    palette->addTool(QStringLiteral("视点垂直"), deploymentIcon(QStringLiteral("viewpoint_vert.png")), [this] {
        SceneViewpoint view = m_scene->currentViewpoint();
        view.pitch = -90.0;
        view.valid = true;
        m_scene->flyTo(view, 0.5);
    });
    palette->addTool(QStringLiteral("视点定位"), deploymentIcon(QStringLiteral("viewpoint_locate.png")),
                     [this] { showViewpointLocator(); });
    palette->addToggleTool(QStringLiteral("视点锁定"),
                           deploymentIcon(QStringLiteral("viewpoint_obtain.png")),
                           m_scene->viewLocked(), [this](bool locked) {
        m_scene->setViewLocked(locked);
        updateStatus(locked ? QStringLiteral("视点已锁定，鼠标和导航器不再改变地图视角")
                            : QStringLiteral("视点锁定已解除"));
    });
    palette->addTool(QStringLiteral("保存视角"), deploymentIcon(QStringLiteral("viewpoint_obtain.png")),
                     [this] { saveViewpoint(); });
    for (const DeploymentViewpoint &view : m_data.viewpoints) {
        palette->addTool(view.name, deploymentIcon(QStringLiteral("viewpoint_locate.png")),
                         [this, view] {
            m_scene->flyTo({view.longitude, view.latitude, view.altitude,
                            view.heading, view.pitch, view.range, true},
                           view.flyTimeSeconds);
        });
    }
    palette->showBeside(qobject_cast<QWidget *>(sender()));
}

void ZqbsPage::showViewpointLocator()
{
    if (m_viewpointLocateDialog) {
        m_viewpointLocateDialog->show();
        m_viewpointLocateDialog->raise();
        m_viewpointLocateDialog->activateWindow();
        return;
    }

    auto *dialog = new SkinnedDialog(QStringLiteral("视点定位"), m_leftPanel);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->resize(370, 285);
    m_viewpointLocateDialog = dialog;

    QWidget *body = createDeploymentDialogBody(*dialog);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(14);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);

    const SceneViewpoint current = m_scene->currentViewpoint();
    const auto coordinate = [body](double value, double minimum, double maximum,
                                   int decimals, const QString &suffix) {
        auto *box = new QDoubleSpinBox(body);
        box->setDecimals(decimals);
        box->setRange(minimum, maximum);
        box->setValue(value);
        box->setSuffix(suffix);
        box->setKeyboardTracking(false);
        return box;
    };
    auto *longitude = coordinate(current.valid ? current.longitude : 109.0,
                                 -180.0, 180.0, 6, QStringLiteral(" °"));
    auto *latitude = coordinate(current.valid ? current.latitude : 34.5,
                                -90.0, 90.0, 6, QStringLiteral(" °"));
    auto *height = coordinate(current.valid && current.range > 0.0
                                  ? current.range : 416000.0,
                              10.0, 1000000000.0, 1, QStringLiteral(" m"));
    auto *flyTime = coordinate(5.0, 0.0, 3600.0, 1, QStringLiteral(" s"));
    form->addRow(QStringLiteral("经度"), longitude);
    form->addRow(QStringLiteral("纬度"), latitude);
    form->addRow(QStringLiteral("视点高度"), height);
    form->addRow(QStringLiteral("飞行时间"), flyTime);
    layout->addLayout(form);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *locate = new QPushButton(QStringLiteral("定位"), body);
    locate->setMinimumWidth(92);
    buttons->addWidget(locate);
    layout->addLayout(buttons);
    connect(locate, &QPushButton::clicked, dialog,
            [this, longitude, latitude, height, flyTime] {
        SceneViewpoint target;
        target.longitude = longitude->value();
        target.latitude = latitude->value();
        target.altitude = 0.0;
        target.heading = 0.0;
        target.pitch = -90.0;
        target.range = height->value();
        target.valid = true;
        m_scene->flyTo(target, flyTime->value());
        updateStatus(QStringLiteral("正在定位到 %1°, %2°")
            .arg(target.longitude, 0, 'f', 6)
            .arg(target.latitude, 0, 'f', 6));
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void ZqbsPage::showToolMenu()
{
    auto *palette = new DeploymentToolPalette(QStringLiteral("工具"),
        QStringLiteral(":/legacy/images/toolbar/tool.png"), 2, m_leftPanel);
    palette->addToggleTool(QStringLiteral("鸟瞰图"), deploymentIcon(QStringLiteral("bird_eye.png")),
                           m_scene->airViewVisible(),
                           [this](bool visible) { m_scene->setAirViewVisible(visible); });
    palette->addToggleTool(QStringLiteral("导航器"), deploymentIcon(QStringLiteral("navigator.png")),
                           m_scene->navigatorVisible(),
                           [this](bool visible) { m_scene->setNavigatorVisible(visible); });
    palette->addToggleTool(QStringLiteral("比例尺"), deploymentIcon(QStringLiteral("scale.png")),
                           m_scene->scaleBarVisible(),
                           [this](bool visible) { m_scene->setScaleBarVisible(visible); });
    palette->addToggleTool(QStringLiteral("红外"), deploymentIcon(QStringLiteral("ir.png")),
                           m_scene->infraredVisible(),
                           [this](bool visible) { m_scene->setInfraredVisible(visible); });
    palette->addTool(QStringLiteral("黑板批注"), deploymentIcon(QStringLiteral("billboard.png")),
                     [this] { showBlackboardPanel(); });
    palette->addTool(QStringLiteral("地图截图"), deploymentIcon(QStringLiteral("scene_info.png")), [this] {
        const QString defaultName = QDir::home().filePath(
            QStringLiteral("演练截图_%1.png")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
        const QString path = QFileDialog::getSaveFileName(m_leftPanel,
            QStringLiteral("保存地图截图"), defaultName,
            QStringLiteral("PNG 图片 (*.png);;BMP 图片 (*.bmp)"));
        if (!path.isEmpty()) {
            QString error;
            if (m_scene->saveSceneSnapshot(path, error))
                updateStatus(QStringLiteral("地图截图已保存：%1").arg(path));
            else
                QMessageBox::warning(m_leftPanel, QStringLiteral("截图失败"), error);
        }
    });
    palette->addTool(QStringLiteral("保存部署数据"), deploymentIcon(QStringLiteral("save.png")), [this] {
        QString error;
        if (m_store.saveAll(m_data, error)) updateStatus(QStringLiteral("各类部署配置已分别保存"));
        else updateStatus(error, true);
    });
    palette->addTool(QStringLiteral("重新载入"), deploymentIcon(QStringLiteral("open.png")),
                     [this] { reloadXml(); });
    palette->addTool(QStringLiteral("数据文件"), deploymentIcon(QStringLiteral("folder.png")), [this] {
        QMessageBox::information(m_leftPanel, QStringLiteral("方案演练本地数据"),
            QStringLiteral("基础场景：%1\n部署标注：%2\n动态演练：%3\n\n坐标系：EPSG:4326 (WGS84)")
                .arg(m_store.fixedFilePath(), m_store.normalFilePath(), m_store.dynamicFilePath()));
    });
    palette->showBeside(qobject_cast<QWidget *>(sender()));
}

void ZqbsPage::showBlackboardPanel()
{
    if (m_blackboardPanel) {
        m_blackboardPanel->show();
        m_blackboardPanel->raise();
        m_blackboardPanel->activateWindow();
        return;
    }
    auto *panel = new QWidget(m_bottomPanel ? m_bottomPanel->window() : nullptr,
                              Qt::Tool | Qt::WindowStaysOnTopHint);
    m_blackboardPanel = panel;
    panel->setObjectName(QStringLiteral("deploymentBlackboardPanel"));
    panel->setAttribute(Qt::WA_DeleteOnClose);
    panel->setWindowTitle(QStringLiteral("地图黑板"));
    panel->setStyleSheet(panelStyle());
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    const std::pair<const char *, const char *> tools[] = {
        {"自由绘制", "freehand"}, {"折线", "polyline"},
        {"圆", "circle"}, {"矩形", "rectangle"}
    };
    auto *group = new QButtonGroup(panel);
    group->setExclusive(true);
    for (int index = 0; index < 4; ++index) {
        auto *button = new QPushButton(QString::fromUtf8(tools[index].first), panel);
        button->setCheckable(true);
        button->setProperty("blackboardTool", QString::fromLatin1(tools[index].second));
        if (index == 0)
            button->setChecked(true);
        group->addButton(button);
        layout->addWidget(button);
    }
    auto *color = new QPushButton(QStringLiteral("颜色"), panel);
    auto *clear = new QPushButton(QStringLiteral("清空"), panel);
    auto *exit = new QPushButton(QStringLiteral("退出"), panel);
    layout->addWidget(color);
    layout->addWidget(clear);
    layout->addWidget(exit);
    QColor *currentColor = new QColor(QStringLiteral("#ffef64"));
    connect(group, &QButtonGroup::buttonClicked, panel,
            [this](QAbstractButton *button) {
        m_scene->setBlackboardTool(button->property("blackboardTool").toString());
    });
    connect(color, &QPushButton::clicked, panel, [this, panel, currentColor] {
        const QColor selected = QColorDialog::getColor(*currentColor, panel,
            QStringLiteral("选择黑板颜色"));
        if (selected.isValid()) {
            *currentColor = selected;
            m_scene->setBlackboardColor(selected);
        }
    });
    connect(clear, &QPushButton::clicked, panel,
            [this] { m_scene->clearBlackboard(); });
    connect(exit, &QPushButton::clicked, panel, &QWidget::close);
    connect(panel, &QObject::destroyed, this, [this, currentColor] {
        delete currentColor;
        m_blackboardPanel = nullptr;
        m_scene->closeBlackboard();
    });
    m_scene->beginBlackboard(QStringLiteral("freehand"), *currentColor);
    panel->adjustSize();
    panel->show();
}

DeploymentPlan *ZqbsPage::selectedPlan()
{
    auto found = std::find_if(m_data.plans.begin(), m_data.plans.end(),
        [this](const DeploymentPlan &plan) { return plan.id == m_selectedPlanId; });
    if (found != m_data.plans.end()) return &*found;
    if (!m_data.plans.isEmpty()) {
        m_selectedPlanId = m_data.plans.first().id;
        return &m_data.plans.first();
    }
    return nullptr;
}

DeploymentPlanStep *ZqbsPage::selectedStep()
{
    DeploymentPlan *plan = selectedPlan();
    if (!plan) return nullptr;
    auto found = std::find_if(plan->steps.begin(), plan->steps.end(),
        [this](const DeploymentPlanStep &step) { return step.id == m_selectedStepId; });
    if (found != plan->steps.end()) return &*found;
    if (!plan->steps.isEmpty()) {
        m_selectedStepId = plan->steps.first().id;
        m_currentStep = 0;
        return &plan->steps.first();
    }
    return nullptr;
}

const DeploymentBindingRule *ZqbsPage::bindingRule(const QString &type) const
{
    const auto found = std::find_if(m_data.bindingRules.cbegin(), m_data.bindingRules.cend(),
        [&type](const DeploymentBindingRule &rule) { return rule.entityType == type; });
    return found == m_data.bindingRules.cend() ? nullptr : &*found;
}

void ZqbsPage::resetPlanPlayback(PlanState state)
{
    m_planTimer->stop();
    m_planClock.invalidate();
    m_planElapsedMs = 0;
    m_playbackPlanId.clear();
    m_playbackStepId.clear();
    m_focusedStepId.clear();
    m_focusedMarkerIndex = -2;
    m_timelineScrubbing = false;
    m_resumeAfterScrub = false;
    m_executedAutoActions.clear();
    m_planState = state;
    if (m_playButton) m_playButton->setText(QStringLiteral("播放"));
}

qint64 ZqbsPage::planElapsedMs() const
{
    return m_planElapsedMs + (m_planState == PlanState::Playing && m_planClock.isValid()
        ? m_planClock.elapsed() : 0);
}

void ZqbsPage::playPlan()
{
    if (m_markerPropertyDialog || m_pendingPlacement == PendingPlacement::DynamicMarker) {
        updateStatus(QStringLiteral("请先完成动态标记的添加或属性编辑，再播放预案"), true);
        return;
    }
    DeploymentPlan *plan = selectedPlan();
    if (!plan || plan->steps.isEmpty()) {
        updateStatus(QStringLiteral("当前预案没有演练步骤"), true);
        return;
    }
    bool hasDuration = false;
    for (const auto &step : plan->steps) {
        const auto error = dynamicStepTimingError(step);
        if (!error.isEmpty()) {
            updateStatus(error, true);
            QMessageBox::warning(m_leftPanel, QStringLiteral("演练时长冲突"), error);
            return;
        }
        hasDuration = hasDuration || dynamicStepDurationMs(step) > 0;
    }
    if (!hasDuration) {
        updateStatus(QStringLiteral("当前预案的顺序显示步骤均为空，请先添加动态标记"), true);
        return;
    }
    if (m_planState == PlanState::Completed) {
        // An explicit replay starts a new round; do not carry completed marks forward.
        stopPlan();
        if (!approveAutomaticPrograms(*plan)) return;
        showPlanStep(0, true);
        return;
    }
    if (!approveAutomaticPrograms(*plan)) return;
    // Resume a paused phase at its original reveal/deadline, not from the beginning.
    if (m_planState == PlanState::Paused && m_playbackPlanId == plan->id
        && m_playbackStepId == m_selectedStepId
        && findDynamicStep(m_data, m_playbackPlanId, m_playbackStepId)) {
        m_planState = PlanState::Playing;
        m_planClock.start();
        m_playButton->setText(QStringLiteral("暂停"));
        advancePlanPlayback();
        return;
    }
    if (m_currentStep < 0 || m_currentStep >= plan->steps.size()) m_currentStep = 0;
    showPlanStep(m_currentStep, true);
}

void ZqbsPage::pausePlan()
{
    if (m_planState == PlanState::Playing) {
        m_planElapsedMs = planElapsedMs();
        m_planState = PlanState::Paused;
        m_planClock.invalidate();
        if (auto *step = findDynamicStep(m_data, m_playbackPlanId, m_playbackStepId)) {
            updateDynamicPlaybackMarkers(*step, m_planElapsedMs);
            const auto index = dynamicStepFocusIndex(*step, m_planElapsedMs);
            if (step->sequentialDisplay && index >= 0
                && m_planElapsedMs < dynamicStepRevealMs(*step, index)) {
                m_scene->flyTo(m_scene->currentViewpoint(), 0.0);
                m_focusedMarkerIndex = -2; // Resume the remaining flight, not a fresh full flight.
            } else focusDynamicStep(*step, m_planElapsedMs);
        }
        updateTimelineProgress();
    }
    m_planTimer->stop();
    if (m_playButton) m_playButton->setText(m_planState == PlanState::Completed
        ? QStringLiteral("重新播放") : QStringLiteral("播放"));
}

void ZqbsPage::stopPlan()
{
    pausePlan();
    resetPlanPlayback(PlanState::Stopped);
    m_approvedAutoPrograms.clear();
    // A draft/editor overlay must not survive stopping a dynamic rehearsal.
    if (m_domain == Domain::Dynamic) {
        if (m_markerPropertyDialog) m_markerPropertyDialog->reject();
        m_scene->cancelDeploymentInteraction();
    }
    if (m_pendingPlacement == PendingPlacement::DynamicMarker) {
        m_pendingPlacement = PendingPlacement::None;
        m_pendingDynamicPlanId.clear(); m_pendingDynamicStepId.clear();
    }
    m_currentStep = -1;
    m_selectedStepId.clear();
    m_dynamicMarkers.clear();
    updateScene();
    rebuildTimeline();
    updateStatus(QStringLiteral("预案演练已停止，全部演练标记已隐藏"));
}

void ZqbsPage::showPlanStep(int index, bool startTimer)
{
    DeploymentPlan *plan = selectedPlan();
    if (!plan || index < 0 || index >= plan->steps.size()) return;
    if (startTimer) {
        const auto error = dynamicStepTimingError(plan->steps[index]);
        if (!error.isEmpty()) {
            pausePlan();
            updateStatus(error, true);
            QMessageBox::warning(m_leftPanel, QStringLiteral("演练时长冲突"), error);
            return;
        }
    }
    if (m_flightDialog) m_flightDialog->reject();
    pausePlan();
    resetPlanPlayback(startTimer ? PlanState::Playing : PlanState::Preview);
    m_currentStep = index;
    const DeploymentPlanStep &step = plan->steps[index];
    m_selectedStepId = step.id;
    m_playbackPlanId = plan->id;
    m_playbackStepId = step.id;
    m_dynamicMarkers = dynamicPlanMarkersAt(*plan, index, 0);
    if (startTimer) m_planClock.start();
    focusDynamicStep(step, 0, true, !startTimer);
    updateScene();
    rebuildTimeline();
    updateStatus(QStringLiteral("演练步骤 %1/%2：%3")
        .arg(index + 1).arg(plan->steps.size()).arg(step.name));
    if (startTimer) {
        m_playButton->setText(QStringLiteral("暂停"));
        executeAutomaticMarkerActions(step, 0);
        m_planTimer->start(qMin(33, dynamicStepNextDelayMs(step, 0)));
    }
}

void ZqbsPage::advancePlanPlayback()
{
    // A queued timeout after stop/pause/selection changes may not revive markers.
    if (m_planState != PlanState::Playing) return;
    auto *step = findDynamicStep(m_data, m_playbackPlanId, m_playbackStepId);
    if (!step || m_selectedPlanId != m_playbackPlanId || m_selectedStepId != m_playbackStepId) {
        stopPlan();
        return;
    }
    const qint64 elapsed = planElapsedMs();
    if (elapsed >= dynamicStepDurationMs(*step)) {
        // A delayed event-loop tick must still deliver the final reveal.
        executeAutomaticMarkerActions(*step, dynamicStepDurationMs(*step));
        nextPlanStep();
        return;
    }
    updateDynamicPlaybackMarkers(*step, elapsed);
    focusDynamicStep(*step, elapsed);
    updateTimelineProgress();
    executeAutomaticMarkerActions(*step, elapsed);
    m_planTimer->start(qMin(33, dynamicStepNextDelayMs(*step, planElapsedMs())));
}

void ZqbsPage::updateDynamicPlaybackMarkers(const DeploymentPlanStep &step, qint64 elapsedMs)
{
    const auto *plan = findDynamicPlan(m_data, m_playbackPlanId);
    if (!plan) return;
    const auto found = std::find_if(plan->steps.cbegin(), plan->steps.cend(),
        [&step](const DeploymentPlanStep &value) { return value.id == step.id; });
    if (found == plan->steps.cend()) return;
    auto markers = dynamicPlanMarkersAt(*plan, int(found - plan->steps.cbegin()), elapsedMs);
    bool changed = m_dynamicMarkers.size() != markers.size();
    if (!changed) {
        for (qsizetype index = 0; index < markers.size(); ++index)
            if (m_dynamicMarkers[index].id != markers[index].id
                || m_dynamicMarkers[index].visible != markers[index].visible) { changed = true; break; }
    }
    // Never rebuild the map on progress-only ticks, and never persist blink state.
    if (changed) {
        m_dynamicMarkers = std::move(markers);
        updateScene();
    }
}

void ZqbsPage::focusDynamicStep(const DeploymentPlanStep &step, qint64 elapsedMs,
                               bool force, bool instant)
{
    const auto index = dynamicStepFocusIndex(step, elapsedMs);
    if (!force && m_focusedStepId == step.id && m_focusedMarkerIndex == index) return;
    m_focusedStepId = step.id;
    m_focusedMarkerIndex = index;
    const auto view = dynamicStepViewpointAt(step, elapsedMs);
    if (!dynamicViewpointValid(view)) return;
    // Focus follows slot start, not reveal/blink. The mark stays hidden until
    // the saved flight ends; hide/show transitions never restart the camera.
    double seconds = dynamicStepFlightSeconds(step, elapsedMs, instant);
    if (!instant && step.sequentialDisplay && index >= 0)
        seconds = qMax(0.0, double(dynamicStepRevealMs(step, index) - elapsedMs) / 1000.0);
    // If a timer is late or resumes partway through flight, still arrive at
    // this slot\'s reveal deadline; never overlap the following blink/hold.
    m_scene->flyTo({view.longitude, view.latitude, view.altitude,
                   view.heading, view.pitch, view.range, true}, seconds);
}

void ZqbsPage::finishPlan()
{
    const auto *plan = selectedPlan();
    if (!plan || plan->steps.isEmpty()) { stopPlan(); return; }
    m_planTimer->stop();
    m_planClock.invalidate();
    m_planState = PlanState::Completed;
    m_currentStep = int(plan->steps.size()) - 1;
    const auto &step = plan->steps.last();
    m_selectedStepId = step.id;
    m_playbackPlanId = plan->id;
    m_playbackStepId = step.id;
    m_planElapsedMs = dynamicStepDurationMs(step);
    m_dynamicMarkers = dynamicPlanMarkersAt(*plan, m_currentStep, m_planElapsedMs);
    updateScene();
    focusDynamicStep(step, m_planElapsedMs);
    updateTimelineProgress();
    m_playButton->setText(QStringLiteral("重新播放"));
    updateStatus(QStringLiteral("预案演练已完成，全部标记保留；停止或重新播放可清空"));
}

void ZqbsPage::seekPlanTime(int index, qint64 elapsedMs)
{
    const auto *plan = selectedPlan();
    if (!plan || index < 0 || index >= plan->steps.size()) return;
    const auto &step = plan->steps[index];
    m_planTimer->stop();
    m_planClock.invalidate();
    m_planState = PlanState::Paused;
    m_currentStep = index;
    m_selectedStepId = step.id;
    m_playbackPlanId = plan->id;
    m_playbackStepId = step.id;
    m_planElapsedMs = qBound(qint64(0), elapsedMs, dynamicStepDurationMs(step));
    // Scrubbing is preview only. Resume delivers future reveals, not past ones.
    m_executedAutoActions.clear();
    executeAutomaticMarkerActions(step, m_planElapsedMs, true);
    updateDynamicPlaybackMarkers(step, m_planElapsedMs);
    focusDynamicStep(step, m_planElapsedMs, true, true);
    updateTimelineProgress();
    m_playButton->setText(QStringLiteral("播放"));
}

void ZqbsPage::scrubTimeline(const QPoint &globalPosition, int gesture)
{
    using Gesture = DynamicTimelineButton::SeekGesture;
    if (m_domain != Domain::Dynamic) return;
    const bool begin = gesture == static_cast<int>(Gesture::Begin);
    const bool end = gesture == static_cast<int>(Gesture::End);
    if (begin) {
        if (m_markerPropertyDialog || m_pendingPlacement == PendingPlacement::DynamicMarker) {
            updateStatus(QStringLiteral("请先完成动态标记的添加或属性编辑，再拖动时间轴"), true);
            return;
        }
        m_resumeAfterScrub = m_planState == PlanState::Playing;
        pausePlan();
        if (m_flightDialog) m_flightDialog->reject();
        m_timelineScrubbing = true;
    } else if (!m_timelineScrubbing) return;
    const auto *plan = selectedPlan();
    if (!plan || plan->steps.isEmpty() || m_stepButtons.isEmpty()) {
        m_timelineScrubbing = false;
        m_resumeAfterScrub = false;
        return;
    }
    // Map the captured mouse's global X across all phase blocks, allowing one
    // continuous drag through gaps and into adjacent phases.
    int target = qMin(int(m_stepButtons.size()), int(plan->steps.size())) - 1;
    for (int index = 0; index <= target; ++index) {
        const auto *button = m_stepButtons[index];
        const int right = button->mapToGlobal(QPoint(button->width() - 1, 0)).x();
        if (globalPosition.x() <= right) { target = index; break; }
    }
    const auto *button = m_stepButtons[target];
    const int x = button->mapFromGlobal(globalPosition).x();
    const double ratio = qBound(0.0, double(x) / qMax(1, button->width() - 1), 1.0);
    seekPlanTime(target, qRound64(ratio * dynamicStepDurationMs(plan->steps[target])));
    if (end) {
        const bool resume = m_resumeAfterScrub;
        m_timelineScrubbing = false;
        m_resumeAfterScrub = false;
        if (resume) playPlan();
    }
}

void ZqbsPage::nextPlanStep()
{
    DeploymentPlan *plan = selectedPlan();
    if (!plan || plan->steps.isEmpty()) { stopPlan(); return; }
    const bool wasPlaying = m_planState == PlanState::Playing;
    int next = m_currentStep + 1;
    if (next >= plan->steps.size()) {
        if (plan->loop) next = 0;
        else { finishPlan(); return; }
    }
    showPlanStep(next, wasPlaying);
}

void ZqbsPage::previousPlanStep()
{
    DeploymentPlan *plan = selectedPlan();
    if (!plan || plan->steps.isEmpty()) return;
    showPlanStep(qMax(0, m_currentStep - 1), m_planState == PlanState::Playing);
}

} // namespace Zqxxxt
