#include "modules/zqbs/DeploymentPropertyDialog.h"

#include "modules/zqbs/MapAnalysis.h"
#include "app/AppPaths.h"
#include "app/MarkerActionEditor.h"
#include "modules/zqbs/PlotGeometry.h"
#include "ui/AppTheme.h"
#include "ui/SkinnedDialog.h"

#include <QAbstractItemView>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QPointF>
#include <QRadioButton>
#include <QScrollArea>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTimer>
#include <QUrlQuery>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double MetersPerDegree = 111320.0;

QWidget *createBody(Zqxxxt::SkinnedDialog &dialog)
{
    auto *body = new QWidget(&dialog);
    body->setObjectName(QStringLiteral("deploymentPropertyDialogBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(Zqxxxt::AppTheme::deploymentPanelStyleSheet());
    dialog.contentLayout()->addWidget(body);
    return body;
}

QDoubleSpinBox *realSpin(QWidget *parent, const QString &name, int decimals,
                         double minimum, double maximum, double value,
                         const QString &suffix = {})
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(name);
    spin->setDecimals(decimals);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

void updateColorButton(QToolButton *button, const QColor &color)
{
    const QColor safe = color.isValid() ? color : QColor(QStringLiteral("#37d6ff"));
    button->setProperty("selectedColor", safe);
    button->setText(QString());
    button->setToolTip(safe.name(QColor::HexRgb).toUpper());
    button->setFixedSize(28, 24);
    button->setStyleSheet(QStringLiteral(
        "QToolButton { background:%1; border:1px solid #8cccf3; padding:0; }")
        .arg(safe.name(QColor::HexRgb)));
}

QToolButton *colorButton(QWidget *parent, const QString &name,
                         const std::shared_ptr<QColor> &value)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(name);
    updateColorButton(button, *value);
    QObject::connect(button, &QToolButton::clicked, button,
        [button, value] {
            const QColor chosen = QColorDialog::getColor(
                *value, button, QStringLiteral("选择颜色"));
            if (chosen.isValid()) {
                *value = chosen;
                updateColorButton(button, *value);
            }
        });
    return button;
}

QString chooseMarkerResource(QWidget *parent, const QString &currentPath,
                             const QStringList &nameFilters)
{
    QFileDialog chooser(parent);
    // The platform picker may open behind the frameless, modeless property
    // window on Linux. Keep this picker inside Qt's own window hierarchy.
    chooser.setOption(QFileDialog::DontUseNativeDialog);
    chooser.setObjectName(QStringLiteral("markerResourceFileDialog"));
    chooser.setWindowTitle(QStringLiteral("选择标记资源"));
    chooser.setFileMode(QFileDialog::ExistingFile);
    chooser.setNameFilters(nameFilters);

    const QString value = currentPath.trimmed();
    const QFileInfo current(value.isEmpty() ? QString()
        : (QFileInfo(value).isAbsolute() ? value : Zqxxxt::AppPaths::assetPath(value)));
    if (current.isFile()) {
        chooser.setDirectory(current.absolutePath());
        chooser.selectFile(current.fileName());
    } else if (current.isDir()) {
        chooser.setDirectory(current.absoluteFilePath());
    } else {
        const QString dataDir = Zqxxxt::AppPaths::dataDir();
        chooser.setDirectory(QFileInfo(dataDir).isDir() ? dataDir : QDir::homePath());
    }

    if (chooser.exec() != QDialog::Accepted)
        return {};
    return chooser.selectedFiles().value(0);
}

double longitudeScale(double latitude)
{
    return std::max(1000.0,
        MetersPerDegree * std::cos(latitude * Pi / 180.0));
}

QPointF localOffset(const Zqxxxt::GeoCoordinate &center,
                    const Zqxxxt::GeoCoordinate &point)
{
    return {(point.longitude - center.longitude) * longitudeScale(center.latitude),
            (point.latitude - center.latitude) * MetersPerDegree};
}

Zqxxxt::GeoCoordinate fromOffset(const Zqxxxt::GeoCoordinate &center,
                                 const QPointF &offset, double altitude)
{
    return {center.longitude + offset.x() / longitudeScale(center.latitude),
            center.latitude + offset.y() / MetersPerDegree, altitude};
}

double angleDegrees(const Zqxxxt::GeoCoordinate &center,
                    const Zqxxxt::GeoCoordinate &point)
{
    const QPointF offset = localOffset(center, point);
    double value = std::atan2(offset.y(), offset.x()) * 180.0 / Pi;
    if (value < 0.0)
        value += 360.0;
    return value;
}

Zqxxxt::GeoCoordinate radialPoint(const Zqxxxt::GeoCoordinate &center,
                                  double angle, double radius)
{
    const double radians = angle * Pi / 180.0;
    return fromOffset(center,
        QPointF(std::cos(radians) * radius, std::sin(radians) * radius),
        center.altitude);
}

bool hasShapeParameters(const QString &kind)
{
    return kind == QStringLiteral("circle") || kind == QStringLiteral("ellipse")
        || kind == QStringLiteral("rectangle") || kind == QStringLiteral("arc")
        || kind == QStringLiteral("sector");
}

bool hasAreaStyle(const QString &kind)
{
    return Zqxxxt::annotationIsArea(kind) || kind == QStringLiteral("arc");
}

QString bindingName(int domain)
{
    switch (domain) {
    case 0: return QStringLiteral("组织");
    case 1: return QStringLiteral("哨位");
    case 2: return QStringLiteral("设备");
    case 3: return QStringLiteral("摄像机");
    case 4: return QStringLiteral("报警");
    default: return QStringLiteral("对象");
    }
}

QTableWidget *measurementCoordinateTable(
    const QVector<Zqxxxt::GeoCoordinate> &points, QWidget *parent,
    const QString &objectName)
{
    auto *table = new QTableWidget(points.size(), 4, parent);
    table->setObjectName(objectName);
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

class MeasurementProfileChart final : public QWidget
{
public:
    explicit MeasurementProfileChart(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("measurementProfileChart"));
        setMinimumSize(520, 210);
    }

    void setPoints(const QVector<Zqxxxt::GeoCoordinate> &points)
    {
        m_points = points;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(QStringLiteral("#0b3767")));
        const QRectF chart = QRectF(rect()).adjusted(52, 18, -18, -34);
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
            const double x = chart.left() + chart.width() * index
                / (m_points.size() - 1);
            const double y = chart.bottom() - chart.height()
                * (m_points.at(index).altitude - minimum) / (maximum - minimum);
            if (index == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        painter.setPen(QPen(QColor(QStringLiteral("#55e8ff")), 2));
        painter.drawPath(path);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(2, chart.top() - 8, 46, 20), Qt::AlignRight,
                         QString::number(maximum, 'f', 1));
        painter.drawText(QRectF(2, chart.bottom() - 10, 46, 20), Qt::AlignRight,
                         QString::number(minimum, 'f', 1));
    }

private:
    QVector<Zqxxxt::GeoCoordinate> m_points;
};

struct ElevationSummary
{
    Zqxxxt::GeoCoordinate minimumPoint;
    Zqxxxt::GeoCoordinate maximumPoint;
    double sum = 0.0;
    bool valid = false;
};

ElevationSummary elevationSummary(const QVector<Zqxxxt::GeoCoordinate> &points)
{
    ElevationSummary result;
    for (const Zqxxxt::GeoCoordinate &point : points) {
        if (!std::isfinite(point.altitude))
            continue;
        if (!result.valid || point.altitude < result.minimumPoint.altitude)
            result.minimumPoint = point;
        if (!result.valid || point.altitude > result.maximumPoint.altitude)
            result.maximumPoint = point;
        result.sum += point.altitude;
        result.valid = true;
    }
    return result;
}

QString pointText(const Zqxxxt::GeoCoordinate &point)
{
    return QStringLiteral("%1, %2, %3 m")
        .arg(point.longitude, 0, 'f', 6)
        .arg(point.latitude, 0, 'f', 6)
        .arg(point.altitude, 0, 'f', 2);
}

QLineEdit *readOnlyLine(QWidget *parent, const QString &name, const QString &value)
{
    auto *line = new QLineEdit(value, parent);
    line->setObjectName(name);
    line->setReadOnly(true);
    return line;
}

} // namespace

namespace Zqxxxt {

QDialog *showDeploymentMarkerProperties(
    const DeploymentMarker &marker,
    DeploymentViewpointProvider currentViewpoint,
    const QVector<DeploymentMarker> &liveMarkers,
    QWidget *parent,
    DeploymentMarkerEditFinished finished)
{
    DeploymentMarker edited = marker;
    if (edited.points.isEmpty())
        edited.points.append(edited.position);
    if (!edited.fillColor.isValid())
        edited.fillColor = edited.color;

    auto *dialog = new SkinnedDialog(QStringLiteral("%1属性")
        .arg(annotationDisplayName(edited.geometryType)), parent);
    dialog->setObjectName(QStringLiteral("deploymentMarkerPropertyDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowOpacity(1.0);
    // 20261009 hello: every entry uses the existing deployment-marker size.
    // Folder membership and marker type must not change the editor geometry.
    const QScreen *screen = dialog->screen() ? dialog->screen() : QGuiApplication::primaryScreen();
    const QSize available = screen ? screen->availableGeometry().size() : QSize(1280, 900);
    dialog->resize(qBound(760, available.width() - 60, 1180),
                   qBound(620, available.height() - 70, 820));
    auto *root = new QVBoxLayout(createBody(*dialog));
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);
    dialog->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    dialog->contentLayout()->setSizeConstraint(QLayout::SetNoConstraint);
    root->setSizeConstraint(QLayout::SetNoConstraint);

    auto *identity = new QGroupBox(QStringLiteral("基本信息"), dialog);
    auto *identityForm = new QFormLayout(identity);
    auto *id = new QLineEdit(edited.id, identity);
    id->setObjectName(QStringLiteral("markerPropertyId"));
    id->setReadOnly(true);
    auto *title = new QLineEdit(edited.name, identity);
    title->setObjectName(QStringLiteral("markerPropertyTitle"));
    title->setMaxLength(25);
    identityForm->addRow(QStringLiteral("ID"), id);
    identityForm->addRow(QStringLiteral("标题"), title);
    root->addWidget(identity);

    auto *tabs = new QTabWidget(dialog);
    tabs->setObjectName(QStringLiteral("markerPropertyTabs"));
    root->addWidget(tabs, 1);

    // Legacy ViewPointStyleWidget: view range plus longitude, latitude,
    // height, pitch, heading and fly time.
    auto *viewPage = new QWidget(tabs);
    auto *viewLayout = new QVBoxLayout(viewPage);
    auto *rangeGroup = new QGroupBox(QStringLiteral("可视范围"), viewPage);
    auto *rangeForm = new QFormLayout(rangeGroup);
    auto *rangeLow = realSpin(rangeGroup, QStringLiteral("viewRangeLow"), 2,
        0.0, 100000000.0, edited.viewRangeLowMeters, QStringLiteral(" m"));
    auto *rangeHigh = realSpin(rangeGroup, QStringLiteral("viewRangeHigh"), 2,
        0.0, 100000000.0, edited.viewRangeHighMeters, QStringLiteral(" m"));
    rangeForm->addRow(QStringLiteral("最近可见距离"), rangeLow);
    rangeForm->addRow(QStringLiteral("最远可见距离"), rangeHigh);
    viewLayout->addWidget(rangeGroup);

    auto *viewGroup = new QGroupBox(QStringLiteral("视点"), viewPage);
    auto *viewForm = new QFormLayout(viewGroup);
    auto *viewLongitude = realSpin(viewGroup, QStringLiteral("viewLongitude"),
        6, -180.0, 180.0, edited.viewpoint.longitude, QStringLiteral(" °"));
    auto *viewLatitude = realSpin(viewGroup, QStringLiteral("viewLatitude"),
        6, -90.0, 90.0, edited.viewpoint.latitude, QStringLiteral(" °"));
    auto *viewHeight = realSpin(viewGroup, QStringLiteral("viewHeight"),
        3, -20000.0, 1000000000.0, edited.viewpoint.altitude,
        QStringLiteral(" m"));
    auto *viewRange = realSpin(viewGroup, QStringLiteral("viewRange"),
        2, 1.0, 100000000.0, edited.viewpoint.range,
        QStringLiteral(" m"));
    auto *viewPitch = realSpin(viewGroup, QStringLiteral("viewPitch"),
        3, -180.0, 180.0, edited.viewpoint.pitch, QStringLiteral(" °"));
    auto *viewHeading = realSpin(viewGroup, QStringLiteral("viewHeading"),
        3, -180.0, 180.0, edited.viewpoint.heading, QStringLiteral(" °"));
    auto *viewFlyTime = realSpin(viewGroup, QStringLiteral("viewFlyTime"),
        2, 0.0, 99.99, edited.viewFlyTimeSeconds, QStringLiteral(" s"));
    viewForm->addRow(QStringLiteral("经度"), viewLongitude);
    viewForm->addRow(QStringLiteral("纬度"), viewLatitude);
    viewForm->addRow(QStringLiteral("高度"), viewHeight);
    viewForm->addRow(QStringLiteral("视距"), viewRange);
    viewForm->addRow(QStringLiteral("俯仰角"), viewPitch);
    viewForm->addRow(QStringLiteral("航向角"), viewHeading);
    viewForm->addRow(QStringLiteral("飞行时间"), viewFlyTime);
    auto *viewButtons = new QHBoxLayout;
    auto *currentButton = new QPushButton(QStringLiteral("获取当前视角"), viewGroup);
    currentButton->setObjectName(QStringLiteral("captureMarkerViewpoint"));
    currentButton->setEnabled(bool(currentViewpoint));
    auto *resetButton = new QPushButton(QStringLiteral("重置"), viewGroup);
    viewButtons->addStretch();
    viewButtons->addWidget(currentButton);
    viewButtons->addWidget(resetButton);
    viewForm->addRow(viewButtons);
    auto *captureStatus = new QLabel(viewGroup);
    captureStatus->setObjectName(QStringLiteral("markerViewpointCaptureStatus"));
    captureStatus->setWordWrap(true);
    captureStatus->hide();
    viewForm->addRow(captureStatus);
    viewLayout->addWidget(viewGroup);
    viewLayout->addStretch();
    tabs->addTab(viewPage, QStringLiteral("视角"));

    const auto setViewControls = [=](const DeploymentViewpoint &view, double flyTime) {
        viewLongitude->setValue(view.longitude);
        viewLatitude->setValue(view.latitude);
        viewHeight->setValue(view.altitude);
        viewRange->setValue(view.range);
        viewPitch->setValue(view.pitch);
        viewHeading->setValue(view.heading);
        viewFlyTime->setValue(flyTime);
    };
    QObject::connect(currentButton, &QPushButton::clicked, dialog,
        [=] {
            const auto view = currentViewpoint ? currentViewpoint() : std::nullopt;
            if (!view) {
                captureStatus->setText(QStringLiteral("当前地图视角不可用，已保留原有数值。请等待地图就绪后重试。"));
                captureStatus->show();
                return;
            }
            setViewControls(*view, 1.0);
            captureStatus->hide();
        });
    QObject::connect(resetButton, &QPushButton::clicked, dialog, [=] {
        DeploymentViewpoint reset;
        reset.longitude = 0.0;
        reset.latitude = 0.0;
        reset.altitude = 0.0;
        reset.heading = -20.0;
        reset.pitch = -90.0;
        reset.range = 10000.0;
        setViewControls(reset, 1.0);
        rangeLow->setValue(0.0);
        rangeHigh->setValue(100000000.0);
    });

    auto *descriptionPage = new QWidget(tabs);
    auto *descriptionLayout = new QVBoxLayout(descriptionPage);
    auto *category = new QLineEdit(edited.category, descriptionPage);
    category->setObjectName(QStringLiteral("markerPropertyCategory"));
    if (!edited.folderId.isEmpty()) {
        category->setReadOnly(true);
        category->setToolTip(QStringLiteral("所属中队文件夹由场景树管理；重命名请使用文件夹的属性菜单"));
    }
    auto *descriptionForm = new QFormLayout;
    descriptionForm->addRow(QStringLiteral("分类文件夹"), category);
    descriptionLayout->addLayout(descriptionForm);
    auto *description = new QTextEdit(descriptionPage);
    description->setObjectName(QStringLiteral("markerPropertyDescription"));
    description->setPlainText(edited.description);
    description->setPlaceholderText(QStringLiteral("输入标注说明…"));
    descriptionLayout->addWidget(description, 1);
    tabs->addTab(descriptionPage, QStringLiteral("描述"));

    auto *linkPage = new QWidget(tabs);
    auto *linkLayout = new QVBoxLayout(linkPage);
    linkLayout->setContentsMargins(6, 6, 6, 6);
    auto *associationEditor = new MarkerActionEditor(edited.association, linkPage);
    linkLayout->addWidget(associationEditor);
    tabs->addTab(linkPage, QStringLiteral("关联"));

    auto *stylePage = new QWidget(tabs);
    auto *styleLayout = new QVBoxLayout(stylePage);
    styleLayout->setContentsMargins(8, 8, 8, 8);
    styleLayout->setSpacing(8);
    auto lineColor = std::make_shared<QColor>(edited.color);
    auto fillColor = std::make_shared<QColor>(edited.fillColor);
    auto *lineGroup = new QGroupBox(QStringLiteral("线"), stylePage);
    lineGroup->setObjectName(QStringLiteral("markerLineStyleGroup"));
    auto *lineGrid = new QGridLayout(lineGroup);
    auto *lineColorButton = colorButton(lineGroup,
        QStringLiteral("markerLineColor"), lineColor);
    auto *lineOpacity = realSpin(lineGroup, QStringLiteral("markerLineOpacity"),
        1, 0.0, 1.0, edited.lineOpacity);
    lineOpacity->setSingleStep(0.1);
    auto *lineWidth = realSpin(lineGroup, QStringLiteral("markerLineWidth"),
        1, 0.1, 10.0, edited.lineWidth, QStringLiteral(" px"));
    lineWidth->setSingleStep(0.1);
    lineGrid->addWidget(new QLabel(QStringLiteral("颜色"), lineGroup), 0, 0);
    lineGrid->addWidget(lineColorButton, 0, 1);
    lineGrid->setColumnStretch(2, 1);
    lineGrid->addWidget(new QLabel(QStringLiteral("宽度"), lineGroup), 0, 3);
    lineGrid->addWidget(lineWidth, 0, 4);
    lineGrid->addWidget(new QLabel(QStringLiteral("透明度"), lineGroup), 1, 0);
    lineGrid->addWidget(lineOpacity, 1, 1);

    auto *areaGroup = new QGroupBox(QStringLiteral("区域"), stylePage);
    areaGroup->setObjectName(QStringLiteral("markerAreaStyleGroup"));
    auto *areaGrid = new QGridLayout(areaGroup);
    auto *fillColorButton = colorButton(areaGroup,
        QStringLiteral("markerFillColor"), fillColor);
    auto *fillOpacity = realSpin(areaGroup, QStringLiteral("markerFillOpacity"),
        1, 0.0, 1.0, edited.fillOpacity);
    fillOpacity->setSingleStep(0.1);
    areaGrid->addWidget(new QLabel(QStringLiteral("透明度"), areaGroup), 0, 0);
    areaGrid->addWidget(fillOpacity, 0, 1);
    areaGrid->setColumnStretch(2, 1);
    areaGrid->addWidget(new QLabel(QStringLiteral("颜色"), areaGroup), 0, 3);
    areaGrid->addWidget(fillColorButton, 0, 4);

    auto *spatialGroup = new QGroupBox(QStringLiteral("空间数据"), stylePage);
    spatialGroup->setObjectName(QStringLiteral("markerSpatialStyleGroup"));
    auto *spatialLayout = new QVBoxLayout(spatialGroup);
    auto *spatialForm = new QFormLayout;
    auto *altitudeMode = new QComboBox(spatialGroup);
    altitudeMode->setObjectName(QStringLiteral("markerAltitudeMode"));
    altitudeMode->addItem(QStringLiteral("紧贴地表"), 0);
    altitudeMode->addItem(QStringLiteral("相对地表"), 1);
    altitudeMode->addItem(QStringLiteral("海拔高度"), 2);
    altitudeMode->setEditable(true);
    altitudeMode->setInsertPolicy(QComboBox::NoInsert);
    altitudeMode->setCurrentIndex(qBound(0, edited.altitudeMode, 2));
    auto *heightOffset = realSpin(spatialGroup, QStringLiteral("markerHeightOffset"),
        0, -1000.0, 1000.0, 0.0, QStringLiteral(" m"));
    heightOffset->setSingleStep(10.0);
    auto *addHeight = new QPushButton(QStringLiteral("增加"), spatialGroup);
    auto *heightRow = new QHBoxLayout;
    heightRow->addWidget(heightOffset, 1);
    heightRow->addWidget(addHeight);
    spatialForm->addRow(QStringLiteral("高度模式"), altitudeMode);
    spatialForm->addRow(QStringLiteral("Z 值统一增加"), heightRow);
    spatialLayout->addLayout(spatialForm);
    auto *pointsTable = new QTableWidget(edited.points.size(), 3, spatialGroup);
    pointsTable->setObjectName(QStringLiteral("markerControlPointTable"));
    pointsTable->setHorizontalHeaderLabels({QStringLiteral("X"),
                                             QStringLiteral("Y"),
                                             QStringLiteral("Z")});
    pointsTable->setToolTip(QStringLiteral("X=经度，Y=纬度，Z=高度"));
    pointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pointsTable->verticalHeader()->setVisible(false);
    pointsTable->setAlternatingRowColors(true);
    for (qsizetype row = 0; row < edited.points.size(); ++row) {
        const GeoCoordinate &point = edited.points.at(row);
        pointsTable->setItem(row, 0, new QTableWidgetItem(
            QString::number(point.longitude, 'f', 6)));
        pointsTable->setItem(row, 1, new QTableWidgetItem(
            QString::number(point.latitude, 'f', 6)));
        pointsTable->setItem(row, 2, new QTableWidgetItem(
            QString::number(point.altitude, 'f', 2)));
    }
    pointsTable->setMinimumHeight(112);
    spatialLayout->addWidget(pointsTable, 1);
    QObject::connect(addHeight, &QPushButton::clicked, dialog, [=] {
        const double offset = heightOffset->value();
        for (int row = 0; row < pointsTable->rowCount(); ++row) {
            bool ok = false;
            const double oldValue = pointsTable->item(row, 2)->text().toDouble(&ok);
            if (ok)
                pointsTable->item(row, 2)->setText(QString::number(oldValue + offset, 'f', 2));
        }
        heightOffset->setValue(0.0);
    });

    QGroupBox *centerGroup = nullptr;
    QGroupBox *shapeGroup = nullptr;
    QDoubleSpinBox *centerLongitude = nullptr;
    QDoubleSpinBox *centerLatitude = nullptr;
    QDoubleSpinBox *centerHeight = nullptr;
    QDoubleSpinBox *parameterA = nullptr;
    QDoubleSpinBox *parameterB = nullptr;
    QDoubleSpinBox *parameterC = nullptr;
    if (hasShapeParameters(edited.geometryType)) {
        centerGroup = new QGroupBox(QStringLiteral("中心"), stylePage);
        centerGroup->setObjectName(QStringLiteral("markerCenterStyleGroup"));
        auto *centerForm = new QFormLayout(centerGroup);
        QString shapeTitle = QStringLiteral("参数");
        if (edited.geometryType == QStringLiteral("circle"))
            shapeTitle = QStringLiteral("圆");
        else if (edited.geometryType == QStringLiteral("ellipse"))
            shapeTitle = QStringLiteral("椭圆");
        else if (edited.geometryType == QStringLiteral("rectangle"))
            shapeTitle = QStringLiteral("矩形");
        else if (edited.geometryType == QStringLiteral("arc"))
            shapeTitle = QStringLiteral("弧形");
        else if (edited.geometryType == QStringLiteral("sector"))
            shapeTitle = QStringLiteral("扇形");
        shapeGroup = new QGroupBox(shapeTitle, stylePage);
        shapeGroup->setObjectName(QStringLiteral("markerShapeStyleGroup"));
        auto *shapeForm = new QFormLayout(shapeGroup);
        GeoCoordinate center = edited.points.constFirst();
        if (edited.geometryType == QStringLiteral("rectangle") && edited.points.size() >= 2) {
            center.longitude = (edited.points.at(0).longitude + edited.points.at(1).longitude) * 0.5;
            center.latitude = (edited.points.at(0).latitude + edited.points.at(1).latitude) * 0.5;
            center.altitude = (edited.points.at(0).altitude + edited.points.at(1).altitude) * 0.5;
        }
        centerLongitude = realSpin(centerGroup, QStringLiteral("shapeCenterLongitude"),
            6, -180.0, 180.0, center.longitude, QStringLiteral(" °"));
        centerLatitude = realSpin(centerGroup, QStringLiteral("shapeCenterLatitude"),
            6, -90.0, 90.0, center.latitude, QStringLiteral(" °"));
        centerHeight = realSpin(centerGroup, QStringLiteral("shapeCenterHeight"),
            4, -999999999.0, 999999999.0, center.altitude, QStringLiteral(" m"));
        centerForm->addRow(QStringLiteral("经度"), centerLongitude);
        centerForm->addRow(QStringLiteral("纬度"), centerLatitude);
        centerForm->addRow(QStringLiteral("高度"), centerHeight);
        if (edited.geometryType == QStringLiteral("circle") && edited.points.size() >= 2) {
            parameterA = realSpin(shapeGroup, QStringLiteral("shapeRadius"), 6,
                1.0, 999999999.0, horizontalDistance(edited.points.at(0), edited.points.at(1)),
                QStringLiteral(" m"));
            shapeForm->addRow(QStringLiteral("半径"), parameterA);
        } else if (edited.geometryType == QStringLiteral("ellipse") && edited.points.size() >= 3) {
            parameterA = realSpin(shapeGroup, QStringLiteral("shapeLongRadius"), 6,
                1.0, 999999999.0, horizontalDistance(edited.points.at(0), edited.points.at(1)),
                QStringLiteral(" m"));
            parameterB = realSpin(shapeGroup, QStringLiteral("shapeShortRadius"), 6,
                1.0, 999999999.0, horizontalDistance(edited.points.at(0), edited.points.at(2)),
                QStringLiteral(" m"));
            shapeForm->addRow(QStringLiteral("长半径"), parameterA);
            shapeForm->addRow(QStringLiteral("短半径"), parameterB);
        } else if (edited.geometryType == QStringLiteral("rectangle") && edited.points.size() >= 2) {
            const QPointF delta = localOffset(edited.points.at(0), edited.points.at(1));
            parameterA = realSpin(shapeGroup, QStringLiteral("shapeHeight"), 6,
                1.0, 999999999.0, std::abs(delta.y()), QStringLiteral(" m"));
            parameterB = realSpin(shapeGroup, QStringLiteral("shapeWidth"), 6,
                1.0, 999999999.0, std::abs(delta.x()), QStringLiteral(" m"));
            shapeForm->addRow(QStringLiteral("高度"), parameterA);
            shapeForm->addRow(QStringLiteral("宽度"), parameterB);
        } else if ((edited.geometryType == QStringLiteral("arc")
                    || edited.geometryType == QStringLiteral("sector"))
                   && edited.points.size() >= 3) {
            parameterA = realSpin(shapeGroup, QStringLiteral("shapeStartAngle"), 6,
                0.0, 360.0, angleDegrees(edited.points.at(0), edited.points.at(1)),
                QStringLiteral(" °"));
            parameterB = realSpin(shapeGroup, QStringLiteral("shapeEndAngle"), 6,
                0.0, 360.0, angleDegrees(edited.points.at(0), edited.points.at(2)),
                QStringLiteral(" °"));
            parameterC = realSpin(shapeGroup, QStringLiteral("shapeRadius"), 6,
                1.0, 999999999.0, horizontalDistance(edited.points.at(0), edited.points.at(1)),
                QStringLiteral(" m"));
            shapeForm->addRow(QStringLiteral("起始角"), parameterA);
            shapeForm->addRow(QStringLiteral("终止角"), parameterB);
            shapeForm->addRow(QStringLiteral("半径"), parameterC);
        }
    }

    QDoubleSpinBox *pointSize = nullptr;
    QDoubleSpinBox *textSize = nullptr;
    QLineEdit *imagePath = nullptr;
    QLineEdit *textValue = nullptr;
    QGroupBox *resourceGroup = nullptr;
    QGroupBox *textGroup = nullptr;
    const bool resourceMarker = edited.geometryType == QStringLiteral("billboard")
        || edited.geometryType == QStringLiteral("overlay")
        || edited.geometryType == QStringLiteral("particle");
    const bool modelMarker = edited.geometryType == QStringLiteral("model");
    const bool tiltMarker = edited.geometryType == QStringLiteral("tilt-model");
    const bool pointMarker = edited.geometryType == QStringLiteral("point");
    const bool textMarker = edited.geometryType == QStringLiteral("text");
    const bool labelMarker = edited.geometryType == QStringLiteral("label");
    if (pointMarker || textMarker || resourceMarker) {
        if (pointMarker || resourceMarker) {
            QString resourceTitle = QStringLiteral("图片");
            if (edited.geometryType == QStringLiteral("billboard"))
                resourceTitle = QStringLiteral("广告牌");
            else if (edited.geometryType == QStringLiteral("overlay"))
                resourceTitle = QStringLiteral("遮盖标签");
            else if (edited.geometryType == QStringLiteral("particle"))
                resourceTitle = QStringLiteral("粒子效果");
            resourceGroup = new QGroupBox(resourceTitle, stylePage);
            resourceGroup->setObjectName(QStringLiteral("markerResourceStyleGroup"));
            auto *resourceForm = new QFormLayout(resourceGroup);
            pointSize = realSpin(resourceGroup, QStringLiteral("markerPointSize"),
                1, 1.0, 1000000.0, edited.pointSize,
                resourceMarker ? QString() : QStringLiteral(" px"));
            imagePath = new QLineEdit(edited.icon, resourceGroup);
            imagePath->setObjectName(QStringLiteral("markerImagePath"));
            auto *resourceRow = new QWidget(resourceGroup);
            auto *resourceLayout = new QHBoxLayout(resourceRow);
            resourceLayout->setContentsMargins(0, 0, 0, 0);
            resourceLayout->setSpacing(6);
            auto *browseResource = new QPushButton(QStringLiteral("浏览…"), resourceRow);
            browseResource->setObjectName(QStringLiteral("markerResourceBrowse"));
            resourceLayout->addWidget(imagePath, 1);
            resourceLayout->addWidget(browseResource);
            resourceForm->addRow(resourceMarker ? QStringLiteral("资源路径")
                                                : QStringLiteral("文件路径"), resourceRow);
            resourceForm->addRow(resourceMarker ? QStringLiteral("缩放值")
                                                : QStringLiteral("图片大小"), pointSize);
            QObject::connect(browseResource, &QPushButton::clicked, dialog,
                [imagePath, dialog, kind = edited.geometryType] {
                    const QString chosen = chooseMarkerResource(dialog,
                        imagePath->text(), kind == QStringLiteral("particle")
                            ? QStringList{QStringLiteral("粒子效果配置 (*.xml)"),
                                          QStringLiteral("所有文件 (*)")}
                            : QStringList{QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.gif)"),
                                          QStringLiteral("所有文件 (*)")});
                    if (!chosen.isEmpty())
                        imagePath->setText(QDir::toNativeSeparators(chosen));
                });
        }
        if (pointMarker || textMarker || resourceMarker) {
            QString textTitle = textMarker ? QStringLiteral("文字样式") : QStringLiteral("文字");
            textGroup = new QGroupBox(textTitle, stylePage);
            textGroup->setObjectName(QStringLiteral("markerTextStyleGroup"));
            auto *textForm = new QFormLayout(textGroup);
            textSize = realSpin(textGroup, QStringLiteral("markerTextSize"),
            1, 1.0, 64.0, edited.textSize, QStringLiteral(" px"));
            textValue = new QLineEdit(edited.text.isEmpty() ? edited.name : edited.text,
                                      textGroup);
            textValue->setObjectName(QStringLiteral("markerText"));
            auto *textColorButton = colorButton(textGroup,
                QStringLiteral("markerTextColor"), lineColor);
            textForm->addRow(QStringLiteral("内容"), textValue);
            textForm->addRow(QStringLiteral("大小"), textSize);
            textForm->addRow(QStringLiteral("颜色"), textColorButton);
        }
    }

    QGroupBox *modelGroup = nullptr;
    QDoubleSpinBox *modelScale = nullptr;
    QDoubleSpinBox *modelRotationX = nullptr;
    QDoubleSpinBox *modelRotationY = nullptr;
    QDoubleSpinBox *modelRotationZ = nullptr;
    QDoubleSpinBox *modelLongitude = nullptr;
    QDoubleSpinBox *modelLatitude = nullptr;
    QDoubleSpinBox *modelHeight = nullptr;
    QRadioButton *modelLocalSource = nullptr;
    QRadioButton *modelServiceSource = nullptr;
    QLineEdit *modelLocalPath = nullptr;
    QLineEdit *modelServicePath = nullptr;
    QDoubleSpinBox *modelLocalLodLow = nullptr;
    QDoubleSpinBox *modelLocalLodHigh = nullptr;
    QDoubleSpinBox *modelServiceLodLow = nullptr;
    QDoubleSpinBox *modelServiceLodHigh = nullptr;
    if (modelMarker) {
        modelGroup = new QGroupBox(stylePage);
        modelGroup->setObjectName(QStringLiteral("markerModelStyleGroup"));
        modelGroup->setFlat(true);
        auto *modelLayout = new QVBoxLayout(modelGroup);
        modelLayout->setContentsMargins(0, 0, 0, 0);
        modelLayout->setSpacing(8);

        auto *postureGroup = new QGroupBox(QStringLiteral("姿态"), modelGroup);
        postureGroup->setObjectName(QStringLiteral("modelPostureGroup"));
        auto *postureForm = new QFormLayout(postureGroup);
        modelScale = realSpin(postureGroup, QStringLiteral("modelScale"),
            4, 0.0, 1000000.0, std::max(0.0, edited.pointSize / 10.0));
        auto *rotationRow = new QWidget(postureGroup);
        auto *rotationLayout = new QHBoxLayout(rotationRow);
        rotationLayout->setContentsMargins(0, 0, 0, 0);
        rotationLayout->setSpacing(5);
        modelRotationX = realSpin(rotationRow, QStringLiteral("modelRotationX"),
            4, -360.0, 360.0, edited.modelRotationXDegrees);
        modelRotationY = realSpin(rotationRow, QStringLiteral("modelRotationY"),
            4, -360.0, 360.0, edited.modelRotationYDegrees);
        modelRotationZ = realSpin(rotationRow, QStringLiteral("modelRotationZ"),
            4, -360.0, 360.0, edited.modelRotationZDegrees);
        modelRotationX->setToolTip(QStringLiteral("X 轴旋转角度"));
        modelRotationY->setToolTip(QStringLiteral("Y 轴旋转角度"));
        modelRotationZ->setToolTip(QStringLiteral("Z 轴旋转角度"));
        rotationLayout->addWidget(modelRotationX, 1);
        rotationLayout->addWidget(modelRotationY, 1);
        rotationLayout->addWidget(modelRotationZ, 1);
        postureForm->addRow(QStringLiteral("缩放比例"), modelScale);
        postureForm->addRow(QStringLiteral("旋转角度"), rotationRow);
        modelLayout->addWidget(postureGroup);

        auto *modelCenterBox = new QGroupBox(QStringLiteral("中心"), modelGroup);
        modelCenterBox->setObjectName(QStringLiteral("modelCenterGroup"));
        auto *centerForm = new QFormLayout(modelCenterBox);
        const GeoCoordinate position = edited.points.isEmpty()
            ? edited.position : edited.points.constFirst();
        modelLongitude = realSpin(modelCenterBox, QStringLiteral("modelLongitude"),
            6, -180.0, 180.0, position.longitude, QStringLiteral(" °"));
        modelLongitude->setSingleStep(0.000001);
        modelLatitude = realSpin(modelCenterBox, QStringLiteral("modelLatitude"),
            6, -90.0, 90.0, position.latitude, QStringLiteral(" °"));
        modelLatitude->setSingleStep(0.000001);
        modelHeight = realSpin(modelCenterBox, QStringLiteral("modelHeight"),
            4, -8000.0, 10000000.0, position.altitude, QStringLiteral(" m"));
        centerForm->addRow(QStringLiteral("经度"), modelLongitude);
        centerForm->addRow(QStringLiteral("纬度"), modelLatitude);
        centerForm->addRow(QStringLiteral("高度"), modelHeight);
        modelLayout->addWidget(modelCenterBox);

        auto *sourceButtons = new QHBoxLayout;
        modelLocalSource = new QRadioButton(QStringLiteral("本地模型"), modelGroup);
        modelServiceSource = new QRadioButton(QStringLiteral("服务模型"), modelGroup);
        modelLocalSource->setObjectName(QStringLiteral("modelLocalSource"));
        modelServiceSource->setObjectName(QStringLiteral("modelServiceSource"));
        sourceButtons->addWidget(modelLocalSource);
        sourceButtons->addWidget(modelServiceSource);
        sourceButtons->addStretch();
        modelLayout->addLayout(sourceButtons);

        auto *localGroup = new QGroupBox(QStringLiteral("本地模型"), modelGroup);
        localGroup->setObjectName(QStringLiteral("modelLocalGroup"));
        auto *localForm = new QFormLayout(localGroup);
        auto *localPathRow = new QWidget(localGroup);
        auto *localPathLayout = new QHBoxLayout(localPathRow);
        localPathLayout->setContentsMargins(0, 0, 0, 0);
        localPathLayout->setSpacing(6);
        modelLocalPath = new QLineEdit(localPathRow);
        modelLocalPath->setObjectName(QStringLiteral("modelLocalPath"));
        if (edited.modelSourceType == 0)
            modelLocalPath->setText(edited.icon);
        auto *browseModel = new QPushButton(QStringLiteral("浏览"), localPathRow);
        browseModel->setObjectName(QStringLiteral("modelLocalBrowse"));
        localPathLayout->addWidget(modelLocalPath, 1);
        localPathLayout->addWidget(browseModel);
        auto *localLodRow = new QWidget(localGroup);
        auto *localLodLayout = new QHBoxLayout(localLodRow);
        localLodLayout->setContentsMargins(0, 0, 0, 0);
        localLodLayout->setSpacing(6);
        modelLocalLodLow = realSpin(localLodRow, QStringLiteral("modelLocalLodLow"),
            2, 0.0, 100000000.0, edited.modelLodLowMeters);
        modelLocalLodHigh = realSpin(localLodRow, QStringLiteral("modelLocalLodHigh"),
            2, 0.0, 100000000.0, edited.modelLodHighMeters);
        localLodLayout->addWidget(modelLocalLodLow, 1);
        localLodLayout->addWidget(new QLabel(QStringLiteral("高 LOD"), localLodRow));
        localLodLayout->addWidget(modelLocalLodHigh, 1);
        localForm->addRow(QStringLiteral("路径"), localPathRow);
        localForm->addRow(QStringLiteral("低 LOD"), localLodRow);
        modelLayout->addWidget(localGroup);

        auto *serviceGroup = new QGroupBox(QStringLiteral("服务模型"), modelGroup);
        serviceGroup->setObjectName(QStringLiteral("modelServiceGroup"));
        auto *serviceForm = new QFormLayout(serviceGroup);
        auto *servicePreset = new QComboBox(serviceGroup);
        servicePreset->setObjectName(QStringLiteral("modelServicePreset"));
        servicePreset->addItem(QStringLiteral("自定义"), QString());
        auto *servicePathRow = new QWidget(serviceGroup);
        auto *servicePathLayout = new QHBoxLayout(servicePathRow);
        servicePathLayout->setContentsMargins(0, 0, 0, 0);
        servicePathLayout->setSpacing(6);
        modelServicePath = new QLineEdit(servicePathRow);
        modelServicePath->setObjectName(QStringLiteral("modelServicePath"));
        modelServicePath->setPlaceholderText(QStringLiteral("请输入模型服务地址"));
        modelServicePath->setText(edited.modelServiceUrl.isEmpty()
            && edited.modelSourceType == 1 ? edited.icon : edited.modelServiceUrl);
        auto *loadModelService = new QPushButton(QStringLiteral("加载"), servicePathRow);
        loadModelService->setObjectName(QStringLiteral("modelServiceLoad"));
        servicePathLayout->addWidget(modelServicePath, 1);
        servicePathLayout->addWidget(loadModelService);
        auto *serviceLodRow = new QWidget(serviceGroup);
        auto *serviceLodLayout = new QHBoxLayout(serviceLodRow);
        serviceLodLayout->setContentsMargins(0, 0, 0, 0);
        serviceLodLayout->setSpacing(6);
        modelServiceLodLow = realSpin(serviceLodRow,
            QStringLiteral("modelServiceLodLow"), 2, 0.0, 100000000.0,
            edited.modelLodLowMeters);
        modelServiceLodHigh = realSpin(serviceLodRow,
            QStringLiteral("modelServiceLodHigh"), 2, 0.0, 100000000.0,
            edited.modelLodHighMeters);
        serviceLodLayout->addWidget(modelServiceLodLow, 1);
        serviceLodLayout->addWidget(new QLabel(QStringLiteral("高 LOD"), serviceLodRow));
        serviceLodLayout->addWidget(modelServiceLodHigh, 1);
        serviceForm->addRow(QStringLiteral("选择模型"), servicePreset);
        serviceForm->addRow(QStringLiteral("路径"), servicePathRow);
        serviceForm->addRow(QStringLiteral("低 LOD"), serviceLodRow);
        auto *serviceMore = new QLabel(
            QStringLiteral("更多服务模型请访问 FreeServer"), serviceGroup);
        serviceMore->setObjectName(QStringLiteral("modelServiceMore"));
        serviceMore->setStyleSheet(QStringLiteral("color:#7f9ebc;"));
        serviceForm->addRow(serviceMore);
        modelLayout->addWidget(serviceGroup);
        modelLayout->addStretch();

        const auto updateModelSourceUi = [=] {
            const bool service = modelServiceSource->isChecked();
            localGroup->setVisible(!service);
            serviceGroup->setVisible(service);
        };
        if (edited.modelSourceType == 1)
            modelServiceSource->setChecked(true);
        else
            modelLocalSource->setChecked(true);
        int presetIndex = servicePreset->count() - 1;
        for (int index = 0; index < servicePreset->count() - 1; ++index) {
            if (servicePreset->itemData(index).toString()
                == modelServicePath->text().trimmed()) {
                presetIndex = index;
                break;
            }
        }
        servicePreset->setCurrentIndex(presetIndex);
        modelServicePath->setReadOnly(presetIndex < servicePreset->count() - 1);
        updateModelSourceUi();
        QObject::connect(modelLocalSource, &QRadioButton::toggled, dialog,
            [=](bool) { updateModelSourceUi(); });
        QObject::connect(modelServiceSource, &QRadioButton::toggled, dialog,
            [=](bool) { updateModelSourceUi(); });
        QObject::connect(browseModel, &QPushButton::clicked, dialog, [=] {
            const QString chosen = QFileDialog::getOpenFileName(dialog,
                QStringLiteral("选择本地模型"), modelLocalPath->text(),
                QStringLiteral("模型文件 (*.ive);;所有文件 (*.*)"));
            if (!chosen.isEmpty())
                modelLocalPath->setText(QDir::toNativeSeparators(chosen));
        });
        QObject::connect(servicePreset,
            qOverload<int>(&QComboBox::currentIndexChanged), dialog, [=](int index) {
                const QString url = servicePreset->itemData(index).toString();
                modelServicePath->setReadOnly(!url.isEmpty());
                if (!url.isEmpty())
                    modelServicePath->setText(url);
                else {
                    modelServicePath->clear();
                    modelServicePath->setFocus();
                }
            });
        QObject::connect(loadModelService, &QPushButton::clicked, dialog, [=] {
            if (modelServicePath->text().trimmed().isEmpty()) {
                QMessageBox::warning(dialog, QStringLiteral("服务地址为空"),
                    QStringLiteral("请输入模型服务地址。"));
                modelServicePath->setFocus();
            }
        });
    }

    QGroupBox *tiltGroup = nullptr;
    QDoubleSpinBox *tiltScale = nullptr;
    QDoubleSpinBox *tiltRotation = nullptr;
    QDoubleSpinBox *tiltLongitude = nullptr;
    QDoubleSpinBox *tiltLatitude = nullptr;
    QDoubleSpinBox *tiltHeight = nullptr;
    QRadioButton *tiltFileSource = nullptr;
    QRadioButton *tiltDataSource = nullptr;
    QRadioButton *tiltServiceSource = nullptr;
    QLineEdit *tiltLocalPath = nullptr;
    QLineEdit *tiltServicePath = nullptr;
    if (tiltMarker) {
        // The reference TiltModelStyleWidget is a compact, borderless style
        // page: transform, center and source controls in that order.
        tiltGroup = new QGroupBox(stylePage);
        tiltGroup->setObjectName(QStringLiteral("markerTiltModelStyleGroup"));
        tiltGroup->setFlat(true);
        auto *tiltLayout = new QVBoxLayout(tiltGroup);
        tiltLayout->setContentsMargins(0, 0, 0, 0);
        tiltLayout->setSpacing(8);

        auto *transformWidget = new QWidget(tiltGroup);
        auto *transformForm = new QFormLayout(transformWidget);
        transformForm->setContentsMargins(0, 0, 0, 0);
        tiltScale = realSpin(transformWidget, QStringLiteral("tiltModelScale"),
            4, 0.0, 1000000.0, std::max(0.0, edited.pointSize / 10.0));
        tiltRotation = realSpin(transformWidget,
            QStringLiteral("tiltModelRotation"), 4, -360.0, 360.0,
            edited.tiltRotationDegrees, QStringLiteral(" °"));
        transformForm->addRow(QStringLiteral("缩放比例"), tiltScale);
        transformForm->addRow(QStringLiteral("旋转角度（Z 轴）"), tiltRotation);
        tiltLayout->addWidget(transformWidget);

        auto *center = new QGroupBox(QStringLiteral("中心"), tiltGroup);
        center->setObjectName(QStringLiteral("tiltModelCenterGroup"));
        auto *centerForm = new QFormLayout(center);
        const GeoCoordinate position = edited.points.isEmpty()
            ? edited.position : edited.points.constFirst();
        tiltLongitude = realSpin(center, QStringLiteral("tiltModelLongitude"),
            6, -180.0, 180.0, position.longitude, QStringLiteral(" °"));
        tiltLongitude->setSingleStep(0.000001);
        tiltLatitude = realSpin(center, QStringLiteral("tiltModelLatitude"),
            6, -90.0, 90.0, position.latitude, QStringLiteral(" °"));
        tiltLatitude->setSingleStep(0.000001);
        tiltHeight = realSpin(center, QStringLiteral("tiltModelHeight"),
            4, -8000.0, 1000000.0, position.altitude, QStringLiteral(" m"));
        centerForm->addRow(QStringLiteral("经度"), tiltLongitude);
        centerForm->addRow(QStringLiteral("纬度"), tiltLatitude);
        centerForm->addRow(QStringLiteral("高度"), tiltHeight);
        tiltLayout->addWidget(center);

        auto *sourceGroup = new QWidget(tiltGroup);
        sourceGroup->setObjectName(QStringLiteral("tiltModelSourceGroup"));
        auto *sourceLayout = new QVBoxLayout(sourceGroup);
        sourceLayout->setContentsMargins(0, 0, 0, 0);
        auto *sourceButtons = new QHBoxLayout;
        tiltFileSource = new QRadioButton(QStringLiteral("配置文件"), sourceGroup);
        tiltDataSource = new QRadioButton(QStringLiteral("数据目录"), sourceGroup);
        tiltServiceSource = new QRadioButton(QStringLiteral("服务地址"), sourceGroup);
        tiltFileSource->setObjectName(QStringLiteral("tiltModelFileSource"));
        tiltDataSource->setObjectName(QStringLiteral("tiltModelDataSource"));
        tiltServiceSource->setObjectName(QStringLiteral("tiltModelServiceSource"));
        sourceButtons->addWidget(tiltFileSource);
        sourceButtons->addWidget(tiltDataSource);
        sourceButtons->addWidget(tiltServiceSource);
        sourceButtons->addStretch();
        sourceLayout->addLayout(sourceButtons);

        auto *localRow = new QWidget(sourceGroup);
        localRow->setObjectName(QStringLiteral("tiltModelLocalRow"));
        auto *localLayout = new QHBoxLayout(localRow);
        localLayout->setContentsMargins(0, 0, 0, 0);
        localLayout->setSpacing(6);
        tiltLocalPath = new QLineEdit(edited.icon, localRow);
        tiltLocalPath->setObjectName(QStringLiteral("tiltModelLocalPath"));
        tiltLocalPath->setReadOnly(true);
        auto *browseTilt = new QPushButton(QStringLiteral("浏览"), localRow);
        browseTilt->setObjectName(QStringLiteral("tiltModelBrowse"));
        localLayout->addWidget(tiltLocalPath, 1);
        localLayout->addWidget(browseTilt);
        sourceLayout->addWidget(localRow);

        auto *serviceRow = new QWidget(sourceGroup);
        serviceRow->setObjectName(QStringLiteral("tiltModelServiceRow"));
        auto *serviceLayout = new QHBoxLayout(serviceRow);
        serviceLayout->setContentsMargins(0, 0, 0, 0);
        serviceLayout->setSpacing(6);
        auto *servicePreset = new QComboBox(serviceRow);
        servicePreset->setObjectName(QStringLiteral("tiltModelServicePreset"));
        servicePreset->addItem(QStringLiteral("自定义"), QString());
        tiltServicePath = new QLineEdit(serviceRow);
        tiltServicePath->setObjectName(QStringLiteral("tiltModelServicePath"));
        tiltServicePath->setPlaceholderText(QStringLiteral("请输入模型服务地址"));
        tiltServicePath->setText(edited.tiltServiceUrl.isEmpty()
            && edited.tiltSourceType == 2 ? edited.icon : edited.tiltServiceUrl);
        auto *loadService = new QPushButton(QStringLiteral("加载"), serviceRow);
        loadService->setObjectName(QStringLiteral("tiltModelLoadService"));
        serviceLayout->addWidget(servicePreset);
        serviceLayout->addWidget(tiltServicePath, 1);
        serviceLayout->addWidget(loadService);
        sourceLayout->addWidget(serviceRow);

        auto *serviceLink = new QLabel(sourceGroup);
        serviceLink->setObjectName(QStringLiteral("tiltModelServiceLink"));
        serviceLink->setText(QStringLiteral("请配置您有权访问的模型服务地址。"));
        sourceLayout->addWidget(serviceLink);
        tiltLayout->addWidget(sourceGroup);
        tiltLayout->addStretch();

        const auto updateTiltSourceUi = [=] {
            const bool service = tiltServiceSource->isChecked();
            localRow->setVisible(!service);
            serviceRow->setVisible(service);
            serviceLink->setVisible(service);
        };
        if (edited.tiltSourceType == 1)
            tiltDataSource->setChecked(true);
        else if (edited.tiltSourceType == 2)
            tiltServiceSource->setChecked(true);
        else
            tiltFileSource->setChecked(true);
        const QString presetUrl = servicePreset->itemData(0).toString();
        const bool presetSelected = edited.tiltSourceType == 2
            && tiltServicePath->text().trimmed() == presetUrl;
        servicePreset->setCurrentIndex(presetSelected ? 0 : 1);
        tiltServicePath->setReadOnly(presetSelected);
        updateTiltSourceUi();
        QObject::connect(tiltFileSource, &QRadioButton::toggled, dialog,
            [=](bool) { updateTiltSourceUi(); });
        QObject::connect(tiltDataSource, &QRadioButton::toggled, dialog,
            [=](bool) { updateTiltSourceUi(); });
        QObject::connect(tiltServiceSource, &QRadioButton::toggled, dialog,
            [=](bool) { updateTiltSourceUi(); });
        QObject::connect(browseTilt, &QPushButton::clicked, dialog, [=] {
            QString chosen;
            if (tiltDataSource->isChecked()) {
                chosen = QFileDialog::getExistingDirectory(dialog,
                    QStringLiteral("选择倾斜摄影数据目录"), tiltLocalPath->text());
            } else {
                chosen = QFileDialog::getOpenFileName(dialog,
                    QStringLiteral("选择倾斜摄影配置文件"), tiltLocalPath->text(),
                    QStringLiteral("倾斜摄影配置 (*.fgt *.xml *.json *.osgb *.ive);;所有文件 (*.*)"));
            }
            if (!chosen.isEmpty())
                tiltLocalPath->setText(QDir::toNativeSeparators(chosen));
        });
        QObject::connect(servicePreset,
            qOverload<int>(&QComboBox::currentIndexChanged), dialog, [=](int index) {
                const QString url = servicePreset->itemData(index).toString();
                tiltServicePath->setReadOnly(!url.isEmpty());
                if (!url.isEmpty())
                    tiltServicePath->setText(url);
                else {
                    tiltServicePath->clear();
                    tiltServicePath->setFocus();
                }
            });
        QObject::connect(loadService, &QPushButton::clicked, dialog, [=] {
            if (tiltServicePath->text().trimmed().isEmpty()) {
                QMessageBox::warning(dialog, QStringLiteral("服务地址为空"),
                    QStringLiteral("请输入倾斜摄影模型服务地址。"));
                tiltServicePath->setFocus();
            }
        });
    }

    QGroupBox *labelGroup = nullptr;
    QDoubleSpinBox *labelLongitude = nullptr;
    QDoubleSpinBox *labelLatitude = nullptr;
    QLineEdit *labelItemText = nullptr;
    QDoubleSpinBox *labelItemTextSize = nullptr;
    QDoubleSpinBox *labelItemWidth = nullptr;
    QDoubleSpinBox *labelItemHeight = nullptr;
    QLineEdit *labelItemImage = nullptr;
    QComboBox *labelItemAlignment = nullptr;
    QTreeWidget *labelTree = nullptr;
    auto labelTitleStyle = std::make_shared<DeploymentLabelItem>();
    auto labelItems = std::make_shared<QVector<DeploymentLabelItem>>(edited.labelItems);
    labelItems->erase(std::remove_if(labelItems->begin(), labelItems->end(),
        [](const DeploymentLabelItem &item) {
            return item.text.trimmed().isEmpty()
                || isInternalLabelPlaceholder(item.text);
        }), labelItems->end());
    auto labelCurrentIndex = std::make_shared<int>(-1);
    auto labelTextColorValue = std::make_shared<QColor>(edited.labelTextColor);
    auto labelBackgroundColorValue =
        std::make_shared<QColor>(edited.labelBackgroundColor);
    QToolButton *labelTextColorButton = nullptr;
    QToolButton *labelBackgroundColorButton = nullptr;
    std::function<void()> storeCurrentLabelStyle;
    if (labelMarker) {
        labelTitleStyle->text = edited.text.isEmpty() ? edited.name : edited.text;
        labelTitleStyle->width = edited.labelWidth;
        labelTitleStyle->height = edited.labelHeight;
        labelTitleStyle->textSize = edited.textSize;
        labelTitleStyle->textColor = edited.labelTextColor;
        labelTitleStyle->backgroundColor = edited.labelBackgroundColor;
        labelTitleStyle->backgroundImage = edited.labelBackgroundImage;
        labelTitleStyle->alignment = edited.labelAlignment;
        labelGroup = new QGroupBox(QStringLiteral("标牌内容与样式"), stylePage);
        labelGroup->setObjectName(QStringLiteral("markerLabelStyleGroup"));
        auto *labelGrid = new QGridLayout(labelGroup);
        labelGrid->setContentsMargins(8, 8, 8, 8);
        labelGrid->setSpacing(8);

        auto *newItemGroup = new QGroupBox(labelGroup);
        auto *newItemLayout = new QVBoxLayout(newItemGroup);
        auto *newItemRow = new QHBoxLayout;
        auto *newItemText = new QLineEdit(newItemGroup);
        newItemText->setObjectName(QStringLiteral("labelNewItemText"));
        newItemText->setMaxLength(512);
        newItemText->setPlaceholderText(QStringLiteral("新子项内容，如 电压：{电压} {电压单位}"));
        newItemRow->addWidget(new QLabel(QStringLiteral("新增子项"), newItemGroup));
        newItemRow->addWidget(newItemText, 1);
        newItemLayout->addLayout(newItemRow);
        auto *labelButtons = new QHBoxLayout;
        auto *addLabelItem = new QPushButton(QStringLiteral("添加"), newItemGroup);
        auto *deleteLabelItem = new QPushButton(QStringLiteral("删除"), newItemGroup);
        auto *clearLabelItems = new QPushButton(QStringLiteral("清空"), newItemGroup);
        addLabelItem->setObjectName(QStringLiteral("labelAddItem"));
        deleteLabelItem->setObjectName(QStringLiteral("labelDeleteItem"));
        clearLabelItems->setObjectName(QStringLiteral("labelClearItems"));
        labelButtons->addStretch();
        labelButtons->addWidget(addLabelItem);
        labelButtons->addWidget(deleteLabelItem);
        labelButtons->addWidget(clearLabelItems);
        newItemLayout->addLayout(labelButtons);
        labelGrid->addWidget(newItemGroup, 0, 0);

        auto *positionGroup = new QGroupBox(QStringLiteral("位置"), labelGroup);
        auto *positionForm = new QFormLayout(positionGroup);
        const GeoCoordinate labelPosition = edited.points.isEmpty()
            ? edited.position : edited.points.constFirst();
        labelLongitude = realSpin(positionGroup, QStringLiteral("labelLongitude"),
            6, -180.0, 180.0, labelPosition.longitude, QStringLiteral(" °"));
        labelLatitude = realSpin(positionGroup, QStringLiteral("labelLatitude"),
            6, -90.0, 90.0, labelPosition.latitude, QStringLiteral(" °"));
        positionForm->addRow(QStringLiteral("经度"), labelLongitude);
        positionForm->addRow(QStringLiteral("纬度"), labelLatitude);
        labelGrid->addWidget(positionGroup, 0, 1);

        auto *treeGroup = new QGroupBox(labelGroup);
        auto *treeLayout = new QVBoxLayout(treeGroup);
        labelTree = new QTreeWidget(treeGroup);
        labelTree->setObjectName(QStringLiteral("labelItemTree"));
        labelTree->setHeaderHidden(true);
        labelTree->setRootIsDecorated(true);
        labelTree->setSelectionMode(QAbstractItemView::SingleSelection);
        treeLayout->addWidget(labelTree);
        labelGrid->addWidget(treeGroup, 1, 0);

        auto *itemGroup = new QGroupBox(QStringLiteral("内容 / 样式"), labelGroup);
        auto *itemForm = new QFormLayout(itemGroup);
        labelItemText = new QLineEdit(itemGroup);
        labelItemText->setObjectName(QStringLiteral("labelItemText"));
        labelItemText->setMaxLength(512);
        labelItemText->setToolTip(QStringLiteral("支持 {name}、{标牌字段}、{标牌字段单位}；字段需在关联页映射并勾选显示。"));
        labelItemTextSize = realSpin(itemGroup, QStringLiteral("labelItemTextSize"),
            1, 1.0, 64.0, labelTitleStyle->textSize, QStringLiteral(" px"));
        labelItemWidth = realSpin(itemGroup, QStringLiteral("labelItemWidth"),
            1, 1.0, 1920.0, labelTitleStyle->width, QStringLiteral(" px"));
        labelItemHeight = realSpin(itemGroup, QStringLiteral("labelItemHeight"),
            1, 1.0, 1080.0, labelTitleStyle->height, QStringLiteral(" px"));
        labelItemImage = new QLineEdit(itemGroup);
        labelItemImage->setObjectName(QStringLiteral("labelItemImage"));
        auto *imageRow = new QWidget(itemGroup);
        auto *imageLayout = new QHBoxLayout(imageRow);
        imageLayout->setContentsMargins(0, 0, 0, 0);
        imageLayout->setSpacing(6);
        auto *browseLabelImage = new QPushButton(QStringLiteral("…"), imageRow);
        browseLabelImage->setFixedWidth(32);
        imageLayout->addWidget(labelItemImage, 1);
        imageLayout->addWidget(browseLabelImage);
        labelItemAlignment = new QComboBox(itemGroup);
        labelItemAlignment->setObjectName(QStringLiteral("labelItemAlignment"));
        labelItemAlignment->addItems({QStringLiteral("左对齐"),
                                      QStringLiteral("居中"),
                                      QStringLiteral("右对齐")});
        labelTextColorButton = colorButton(itemGroup,
            QStringLiteral("labelItemTextColor"), labelTextColorValue);
        labelBackgroundColorButton = colorButton(itemGroup,
            QStringLiteral("labelItemBackgroundColor"), labelBackgroundColorValue);
        itemForm->addRow(QStringLiteral("内容"), labelItemText);
        itemForm->addRow(QStringLiteral("文字大小"), labelItemTextSize);
        itemForm->addRow(QStringLiteral("标牌宽度"), labelItemWidth);
        itemForm->addRow(QStringLiteral("标牌高度"), labelItemHeight);
        itemForm->addRow(QStringLiteral("背景图片"), imageRow);
        itemForm->addRow(QStringLiteral("对齐方式"), labelItemAlignment);
        itemForm->addRow(QStringLiteral("文字颜色"), labelTextColorButton);
        itemForm->addRow(QStringLiteral("背景颜色"), labelBackgroundColorButton);
        labelGrid->addWidget(itemGroup, 1, 1);
        labelGrid->setRowStretch(1, 1);
        labelGrid->setColumnStretch(0, 1);
        labelGrid->setColumnStretch(1, 2);
        auto *nodeHint = new QLabel(QStringLiteral(
            "父节点和各子节点可在“内容”中独立填写占位符，例如 电压：{电压} {电压单位}。关联页选择“标牌多数据点”，映射同名字段并勾选“显示”；缺失或过期值显示 --。关联页模板只覆盖父节点。"), labelGroup);
        nodeHint->setWordWrap(true);
        labelGrid->addWidget(nodeHint, 2, 0, 1, 2);

        const auto loadLabelStyle = [=](const DeploymentLabelItem &item) {
            labelItemText->setText(item.text);
            labelItemTextSize->setValue(item.textSize);
            labelItemWidth->setValue(item.width);
            labelItemHeight->setValue(item.height);
            labelItemImage->setText(item.backgroundImage);
            labelItemAlignment->setCurrentIndex(qBound(0, item.alignment, 2));
            *labelTextColorValue = item.textColor;
            *labelBackgroundColorValue = item.backgroundColor;
            updateColorButton(labelTextColorButton, *labelTextColorValue);
            updateColorButton(labelBackgroundColorButton,
                              *labelBackgroundColorValue);
        };
        storeCurrentLabelStyle = [=] {
            DeploymentLabelItem value;
            value.text = labelItemText->text().trimmed();
            value.textSize = labelItemTextSize->value();
            value.width = labelItemWidth->value();
            value.height = labelItemHeight->value();
            value.backgroundImage = labelItemImage->text().trimmed();
            value.alignment = labelItemAlignment->currentIndex();
            value.textColor = *labelTextColorValue;
            value.backgroundColor = *labelBackgroundColorValue;
            if (*labelCurrentIndex < 0)
                *labelTitleStyle = value;
            else if (*labelCurrentIndex < labelItems->size())
                (*labelItems)[*labelCurrentIndex] = value;
            // currentItemChanged already points at the NEW item. Update the
            // previous index we just saved, otherwise sibling names are swapped.
            if (QTreeWidgetItem *rootItem = labelTree->topLevelItem(0)) {
                QTreeWidgetItem *savedItem = *labelCurrentIndex < 0 ? rootItem
                    : rootItem->child(*labelCurrentIndex);
                if (savedItem) savedItem->setText(0, value.text);
            }
        };
        const auto rebuildLabelTree = [=] {
            labelTree->clear();
            auto *rootItem = new QTreeWidgetItem(labelTree,
                QStringList(labelTitleStyle->text));
            rootItem->setData(0, Qt::UserRole, -1);
            for (int index = 0; index < labelItems->size(); ++index) {
                auto *item = new QTreeWidgetItem(rootItem,
                    QStringList(labelItems->at(index).text));
                item->setData(0, Qt::UserRole, index);
            }
            rootItem->setExpanded(true);
            *labelCurrentIndex = -1;
            loadLabelStyle(*labelTitleStyle);
            labelTree->setCurrentItem(rootItem);
        };
        QObject::connect(labelTree, &QTreeWidget::currentItemChanged, dialog,
            [=](QTreeWidgetItem *current, QTreeWidgetItem *) {
                if (!current)
                    return;
                storeCurrentLabelStyle();
                *labelCurrentIndex = current->data(0, Qt::UserRole).toInt();
                if (*labelCurrentIndex < 0)
                    loadLabelStyle(*labelTitleStyle);
                else if (*labelCurrentIndex < labelItems->size())
                    loadLabelStyle(labelItems->at(*labelCurrentIndex));
            });
        QObject::connect(addLabelItem, &QPushButton::clicked, dialog, [=] {
            storeCurrentLabelStyle();
            const QString intendedText = newItemText->text().trimmed();
            if (intendedText.isEmpty()
                || isInternalLabelPlaceholder(intendedText)) {
                return;
            }
            DeploymentLabelItem item;
            item.text = intendedText;
            labelItems->append(item);
            newItemText->clear();
            rebuildLabelTree();
            if (QTreeWidgetItem *rootItem = labelTree->topLevelItem(0))
                labelTree->setCurrentItem(rootItem->child(rootItem->childCount() - 1));
        });
        QObject::connect(deleteLabelItem, &QPushButton::clicked, dialog, [=] {
            storeCurrentLabelStyle();
            if (*labelCurrentIndex >= 0 && *labelCurrentIndex < labelItems->size())
                labelItems->removeAt(*labelCurrentIndex);
            rebuildLabelTree();
        });
        QObject::connect(clearLabelItems, &QPushButton::clicked, dialog, [=] {
            storeCurrentLabelStyle();
            labelItems->clear();
            rebuildLabelTree();
        });
        QObject::connect(browseLabelImage, &QPushButton::clicked, dialog, [=] {
            const QString chosen = QFileDialog::getOpenFileName(dialog,
                QStringLiteral("选择标牌背景图片"), labelItemImage->text(),
                QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*.*)"));
            if (!chosen.isEmpty())
                labelItemImage->setText(QDir::toNativeSeparators(chosen));
        });
        rebuildLabelTree();
        if (associationEditor) {
            const auto syncLabelPreview = [=] {
                QString parentText = labelTitleStyle->text;
                auto children = *labelItems;
                if (*labelCurrentIndex < 0) parentText = labelItemText->text();
                else if (*labelCurrentIndex < children.size())
                    children[*labelCurrentIndex].text = labelItemText->text();
                associationEditor->setLabelContents(parentText, children);
            };
            QObject::connect(labelItemText, &QLineEdit::textChanged, associationEditor, syncLabelPreview);
            QObject::connect(labelTree, &QTreeWidget::currentItemChanged, associationEditor, syncLabelPreview);
            QObject::connect(addLabelItem, &QPushButton::clicked, associationEditor, syncLabelPreview);
            QObject::connect(deleteLabelItem, &QPushButton::clicked, associationEditor, syncLabelPreview);
            QObject::connect(clearLabelItems, &QPushButton::clicked, associationEditor, syncLabelPreview);
            syncLabelPreview();
        }
    }

    // Keep the grouping and ordering used by the reference StyleWidget set.
    // The old application used a different widget for every marker type;
    // these groups preserve that layout while sharing the save logic.
    lineGroup->hide();
    areaGroup->hide();
    spatialGroup->hide();
    if (centerGroup) centerGroup->hide();
    if (shapeGroup) shapeGroup->hide();
    if (resourceGroup) resourceGroup->hide();
    if (textGroup) textGroup->hide();
    if (modelGroup) modelGroup->hide();
    if (tiltGroup) tiltGroup->hide();
    if (labelGroup) labelGroup->hide();
    const auto addStyleGroup = [styleLayout](QGroupBox *group, int stretch = 0) {
        if (!group)
            return;
        group->show();
        styleLayout->addWidget(group, stretch);
    };
    if (hasShapeParameters(edited.geometryType)) {
        addStyleGroup(lineGroup);
        addStyleGroup(centerGroup);
        addStyleGroup(shapeGroup);
        if (hasAreaStyle(edited.geometryType))
            addStyleGroup(areaGroup);
    } else if (pointMarker) {
        spatialGroup->setTitle(QStringLiteral("位置"));
        addStyleGroup(spatialGroup, 1);
        addStyleGroup(resourceGroup);
        addStyleGroup(textGroup);
    } else if (textMarker) {
        spatialGroup->setTitle(QStringLiteral("位置"));
        addStyleGroup(textGroup);
        addStyleGroup(spatialGroup, 1);
    } else if (labelMarker) {
        addStyleGroup(labelGroup, 1);
    } else if (modelMarker) {
        addStyleGroup(modelGroup, 1);
    } else if (tiltMarker) {
        addStyleGroup(tiltGroup, 1);
    } else if (resourceMarker) {
        spatialGroup->setTitle(QStringLiteral("中心"));
        addStyleGroup(resourceGroup);
        addStyleGroup(textGroup);
        addStyleGroup(spatialGroup, 1);
    } else {
        addStyleGroup(lineGroup);
        addStyleGroup(spatialGroup, 1);
        if (hasAreaStyle(edited.geometryType))
            addStyleGroup(areaGroup);
    }
    styleLayout->addStretch();
    tabs->addTab(stylePage, QStringLiteral("样式"));
    tabs->setCurrentWidget(stylePage);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    root->addWidget(buttons);
    auto result = std::make_shared<DeploymentMarker>(edited);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, [=] {
        if (title->text().trimmed().isEmpty()) {
            QMessageBox::warning(dialog, QStringLiteral("标题不能为空"),
                                 QStringLiteral("请输入标注或标绘标题。"));
            title->setFocus();
            return;
        }
        QString associationError;
        if (associationEditor && !associationEditor->validate(&associationError)) {
            tabs->setCurrentWidget(linkPage);
            QMessageBox::warning(dialog, QStringLiteral("关联配置无效"), associationError);
            return;
        }
        if (rangeLow->value() > rangeHigh->value()) {
            QMessageBox::warning(dialog, QStringLiteral("可视范围无效"),
                QStringLiteral("最近可见距离不能大于最远可见距离。"));
            rangeLow->setFocus();
            return;
        }
        if (modelMarker) {
            QDoubleSpinBox *low = modelServiceSource->isChecked()
                ? modelServiceLodLow : modelLocalLodLow;
            QDoubleSpinBox *high = modelServiceSource->isChecked()
                ? modelServiceLodHigh : modelLocalLodHigh;
            if (low->value() > high->value()) {
                QMessageBox::warning(dialog, QStringLiteral("LOD 范围无效"),
                    QStringLiteral("低 LOD 不能大于高 LOD。"));
                low->setFocus();
                return;
            }
        }
        if (labelMarker && storeCurrentLabelStyle)
            storeCurrentLabelStyle();
        DeploymentMarker &accepted = *result;
        accepted.name = title->text().trimmed();
        accepted.category = category->text().trimmed();
        accepted.description = description->toPlainText();
        accepted.viewpoint.longitude = viewLongitude->value();
        accepted.viewpoint.latitude = viewLatitude->value();
        accepted.viewpoint.altitude = viewHeight->value();
        accepted.viewpoint.pitch = viewPitch->value();
        accepted.viewpoint.heading = viewHeading->value();
        accepted.viewpoint.range = viewRange->value();
        accepted.viewFlyTimeSeconds = viewFlyTime->value();
        accepted.viewRangeLowMeters = rangeLow->value();
        accepted.viewRangeHighMeters = rangeHigh->value();
        accepted.color = *lineColor;
        accepted.lineOpacity = lineOpacity->value();
        accepted.lineWidth = lineWidth->value();
        accepted.fillColor = *fillColor;
        accepted.fillOpacity = fillOpacity->value();
        accepted.altitudeMode = altitudeMode->currentData().toInt();
        accepted.association = associationEditor->value();
        accepted.bindingType.clear();
        accepted.bindingId.clear();

        const bool mapPointsEdited = dialog->property("markerPointsEdited").toBool();
        if (hasShapeParameters(accepted.geometryType) && centerLongitude
            && !mapPointsEdited) {
            GeoCoordinate center{centerLongitude->value(), centerLatitude->value(),
                                 centerHeight->value()};
            if (accepted.geometryType == QStringLiteral("circle") && parameterA) {
                const double angle = accepted.points.size() >= 2
                    ? angleDegrees(accepted.points.at(0), accepted.points.at(1)) : 0.0;
                accepted.points = {center, radialPoint(center, angle, parameterA->value())};
            } else if (accepted.geometryType == QStringLiteral("ellipse")
                       && parameterA && parameterB) {
                const double longAngle = accepted.points.size() >= 2
                    ? angleDegrees(accepted.points.at(0), accepted.points.at(1)) : 0.0;
                const double shortAngle = accepted.points.size() >= 3
                    ? angleDegrees(accepted.points.at(0), accepted.points.at(2)) : 90.0;
                accepted.points = {center,
                    radialPoint(center, longAngle, parameterA->value()),
                    radialPoint(center, shortAngle, parameterB->value())};
            } else if (accepted.geometryType == QStringLiteral("rectangle")
                       && parameterA && parameterB) {
                const double halfWidth = parameterB->value() * 0.5;
                const double halfHeight = parameterA->value() * 0.5;
                accepted.points = {
                    fromOffset(center, QPointF(-halfWidth, -halfHeight), center.altitude),
                    fromOffset(center, QPointF(halfWidth, halfHeight), center.altitude)};
            } else if ((accepted.geometryType == QStringLiteral("arc")
                        || accepted.geometryType == QStringLiteral("sector"))
                       && parameterA && parameterB && parameterC) {
                accepted.points = {center,
                    radialPoint(center, parameterA->value(), parameterC->value()),
                    radialPoint(center, parameterB->value(), parameterC->value())};
            }
        } else {
            QVector<GeoCoordinate> points;
            points.reserve(pointsTable->rowCount());
            for (int row = 0; row < pointsTable->rowCount(); ++row) {
                bool lonOk = false;
                bool latOk = false;
                bool altOk = false;
                const QTableWidgetItem *lonItem = pointsTable->item(row, 0);
                const QTableWidgetItem *latItem = pointsTable->item(row, 1);
                const QTableWidgetItem *altItem = pointsTable->item(row, 2);
                const double longitude = lonItem ? lonItem->text().toDouble(&lonOk) : 0.0;
                const double latitude = latItem ? latItem->text().toDouble(&latOk) : 0.0;
                const double altitude = altItem ? altItem->text().toDouble(&altOk) : 0.0;
                if (lonOk && latOk && altOk)
                    points.append({qBound(-180.0, longitude, 180.0),
                                   qBound(-90.0, latitude, 90.0), altitude});
            }
            if (!points.isEmpty())
                accepted.points = points;
        }
        if (accepted.geometryType == QStringLiteral("rectangle")
            && accepted.points.size() >= 2) {
            accepted.position.longitude =
                (accepted.points.at(0).longitude + accepted.points.at(1).longitude) * 0.5;
            accepted.position.latitude =
                (accepted.points.at(0).latitude + accepted.points.at(1).latitude) * 0.5;
            accepted.position.altitude =
                (accepted.points.at(0).altitude + accepted.points.at(1).altitude) * 0.5;
        } else {
            accepted.position = accepted.points.constFirst();
        }
        if (pointSize) accepted.pointSize = pointSize->value();
        if (textSize) accepted.textSize = textSize->value();
        if (imagePath) accepted.icon = imagePath->text().trimmed();
        if (textValue) accepted.text = textValue->text().trimmed();
        if (labelMarker) {
            accepted.text = labelTitleStyle->text.isEmpty()
                ? accepted.name : labelTitleStyle->text;
            accepted.textSize = labelTitleStyle->textSize;
            accepted.labelWidth = labelTitleStyle->width;
            accepted.labelHeight = labelTitleStyle->height;
            accepted.labelTextColor = labelTitleStyle->textColor;
            accepted.labelBackgroundColor = labelTitleStyle->backgroundColor;
            accepted.labelBackgroundImage = labelTitleStyle->backgroundImage;
            accepted.labelAlignment = labelTitleStyle->alignment;
            accepted.labelItems = *labelItems;
            if (labelLongitude && labelLatitude) {
                accepted.position.longitude = labelLongitude->value();
                accepted.position.latitude = labelLatitude->value();
                if (accepted.points.isEmpty())
                    accepted.points.append(accepted.position);
                else {
                    accepted.points[0].longitude = accepted.position.longitude;
                    accepted.points[0].latitude = accepted.position.latitude;
                    accepted.points[0].altitude = accepted.position.altitude;
                }
            }
        }
        if (modelMarker) {
            accepted.pointSize = modelScale->value() * 10.0;
            accepted.modelRotationXDegrees = modelRotationX->value();
            accepted.modelRotationYDegrees = modelRotationY->value();
            accepted.modelRotationZDegrees = modelRotationZ->value();
            accepted.modelSourceType = modelServiceSource->isChecked() ? 1 : 0;
            accepted.modelServiceUrl = accepted.modelSourceType == 1
                ? modelServicePath->text().trimmed() : QString();
            accepted.icon = accepted.modelSourceType == 1
                ? accepted.modelServiceUrl : modelLocalPath->text().trimmed();
            accepted.modelLodLowMeters = accepted.modelSourceType == 1
                ? modelServiceLodLow->value() : modelLocalLodLow->value();
            accepted.modelLodHighMeters = accepted.modelSourceType == 1
                ? modelServiceLodHigh->value() : modelLocalLodHigh->value();
            accepted.position.longitude = modelLongitude->value();
            accepted.position.latitude = modelLatitude->value();
            accepted.position.altitude = modelHeight->value();
            if (accepted.points.isEmpty())
                accepted.points.append(accepted.position);
            else
                accepted.points[0] = accepted.position;
        }
        if (tiltMarker) {
            accepted.pointSize = tiltScale->value() * 10.0;
            accepted.tiltRotationDegrees = tiltRotation->value();
            accepted.tiltSourceType = tiltServiceSource->isChecked() ? 2
                : (tiltDataSource->isChecked() ? 1 : 0);
            accepted.tiltServiceUrl = accepted.tiltSourceType == 2
                ? tiltServicePath->text().trimmed() : QString();
            accepted.icon = accepted.tiltSourceType == 2
                ? accepted.tiltServiceUrl : tiltLocalPath->text().trimmed();
            accepted.position.longitude = tiltLongitude->value();
            accepted.position.latitude = tiltLatitude->value();
            accepted.position.altitude = tiltHeight->value();
            if (accepted.points.isEmpty())
                accepted.points.append(accepted.position);
            else
                accepted.points[0] = accepted.position;
        }
        if ((accepted.geometryType == QStringLiteral("text")
             || accepted.geometryType == QStringLiteral("label"))
            && accepted.text.isEmpty()) {
            accepted.text = accepted.name;
        }
        dialog->accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     dialog, &QDialog::reject);

    auto completion = std::make_shared<DeploymentMarkerEditFinished>(
        std::move(finished));
    QObject::connect(dialog, &QDialog::finished, dialog,
        [result, completion](int code) {
            if (*completion)
                (*completion)(code == QDialog::Accepted, *result);
        });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return dialog;
}

QWidget *createDeploymentMeasurementStylePage(
    const DeploymentMeasurement &measurement, QWidget *parent)
{
    auto *stylePage = new QWidget(parent);
    stylePage->setObjectName(QStringLiteral("measurementStylePage"));
    auto *styleLayout = new QVBoxLayout(stylePage);
    styleLayout->setContentsMargins(8, 8, 8, 8);
    styleLayout->setSpacing(8);

    const auto addLineGroup = [stylePage, styleLayout](
        const QString &title, const QString &prefix, const QColor &initialColor,
        double initialWidth, double initialOpacity) {
        auto *group = new QGroupBox(title, stylePage);
        group->setObjectName(prefix + QStringLiteral("StyleGroup"));
        auto *grid = new QGridLayout(group);
        auto color = std::make_shared<QColor>(initialColor);
        auto *swatch = colorButton(group, prefix + QStringLiteral("Color"), color);
        auto *opacity = realSpin(group, prefix + QStringLiteral("Opacity"),
            1, 0.0, 1.0, initialOpacity);
        opacity->setSingleStep(0.1);
        auto *width = realSpin(group, prefix + QStringLiteral("Width"),
            1, 0.1, 10.0, initialWidth, QStringLiteral(" px"));
        width->setSingleStep(0.1);
        grid->addWidget(new QLabel(QStringLiteral("颜色"), group), 0, 0);
        grid->addWidget(swatch, 0, 1);
        grid->setColumnStretch(2, 1);
        grid->addWidget(new QLabel(QStringLiteral("宽度"), group), 0, 3);
        grid->addWidget(width, 0, 4);
        grid->addWidget(new QLabel(QStringLiteral("透明度"), group), 1, 0);
        grid->addWidget(opacity, 1, 1);
        styleLayout->addWidget(group);
    };

    addLineGroup(measurementIsArea(measurement.kind)
                     ? QStringLiteral("边界线") : QStringLiteral("测量线"),
                 QStringLiteral("measurementLine"), measurement.lineColor,
                 measurement.lineWidth, measurement.lineOpacity);

    if (measurementIsArea(measurement.kind)) {
        auto *areaGroup = new QGroupBox(QStringLiteral("区域"), stylePage);
        areaGroup->setObjectName(QStringLiteral("measurementAreaStyleGroup"));
        auto *areaGrid = new QGridLayout(areaGroup);
        auto fillColor = std::make_shared<QColor>(measurement.fillColor);
        auto *fillSwatch = colorButton(areaGroup,
            QStringLiteral("measurementFillColor"), fillColor);
        auto *fillOpacity = realSpin(areaGroup,
            QStringLiteral("measurementFillOpacity"), 1, 0.0, 1.0,
            measurement.fillOpacity);
        fillOpacity->setSingleStep(0.1);
        areaGrid->addWidget(new QLabel(QStringLiteral("透明度"), areaGroup), 0, 0);
        areaGrid->addWidget(fillOpacity, 0, 1);
        areaGrid->setColumnStretch(2, 1);
        areaGrid->addWidget(new QLabel(QStringLiteral("颜色"), areaGroup), 0, 3);
        areaGrid->addWidget(fillSwatch, 0, 4);
        styleLayout->addWidget(areaGroup);
    }

    if (measurementHasAnalysisStyle(measurement.kind)) {
        addLineGroup(QStringLiteral("分析结果"),
                     QStringLiteral("measurementAnalysis"),
                     measurement.analysisColor,
                     measurement.analysisLineWidth,
                     measurement.analysisOpacity);
    }
    styleLayout->addStretch();
    return stylePage;
}

void applyDeploymentMeasurementStylePage(
    QWidget *stylePage, DeploymentMeasurement &measurement)
{
    if (!stylePage)
        return;
    const auto colorValue = [stylePage](const QString &name,
                                        const QColor &fallback) {
        if (auto *button = stylePage->findChild<QToolButton *>(name)) {
            const QColor value = button->property("selectedColor").value<QColor>();
            if (value.isValid())
                return value;
        }
        return fallback;
    };
    const auto spinValue = [stylePage](const QString &name, double fallback) {
        if (auto *spin = stylePage->findChild<QDoubleSpinBox *>(name))
            return spin->value();
        return fallback;
    };

    measurement.lineColor = colorValue(
        QStringLiteral("measurementLineColor"), measurement.lineColor);
    measurement.lineWidth = spinValue(
        QStringLiteral("measurementLineWidth"), measurement.lineWidth);
    measurement.lineOpacity = spinValue(
        QStringLiteral("measurementLineOpacity"), measurement.lineOpacity);
    measurement.fillColor = colorValue(
        QStringLiteral("measurementFillColor"), measurement.fillColor);
    measurement.fillOpacity = spinValue(
        QStringLiteral("measurementFillOpacity"), measurement.fillOpacity);
    measurement.analysisColor = colorValue(
        QStringLiteral("measurementAnalysisColor"), measurement.analysisColor);
    measurement.analysisLineWidth = spinValue(
        QStringLiteral("measurementAnalysisWidth"),
        measurement.analysisLineWidth);
    measurement.analysisOpacity = spinValue(
        QStringLiteral("measurementAnalysisOpacity"), measurement.analysisOpacity);
}

QDialog *showDeploymentMeasurementResults(
    const DeploymentMeasurement &measurement,
    QWidget *parent,
    DeploymentMeasurementEditApplied apply)
{
    auto edited = std::make_shared<DeploymentMeasurement>(measurement);
    auto completion = std::make_shared<DeploymentMeasurementEditApplied>(
        std::move(apply));
    auto *dialog = new SkinnedDialog(measurementDisplayName(edited->kind), parent);
    dialog->setObjectName(QStringLiteral("measurementResultDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowOpacity(0.90);
    dialog->setWindowIcon(QIcon(QStringLiteral(":/legacy/images/toolbar/measure.png")));
    const bool radialResult = edited->kind == QStringLiteral("radial-line-visible");
    dialog->resize(edited->kind == QStringLiteral("profile-analysis") ? 760
                       : (radialResult ? 433 : 560),
                   edited->kind == QStringLiteral("profile-analysis") ? 690
                       : (radialResult ? 326 : 560));

    auto *root = new QVBoxLayout(createBody(*dialog));
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);
    auto *tabs = new QTabWidget(dialog);
    tabs->setObjectName(QStringLiteral("measurementResultTabs"));
    auto *resultPage = new QWidget(tabs);
    resultPage->setObjectName(QStringLiteral("measurementResultPage"));
    auto *resultLayout = new QVBoxLayout(resultPage);
    resultLayout->setContentsMargins(7, 7, 7, 7);
    resultLayout->setSpacing(8);

    const bool profile = edited->kind == QStringLiteral("profile-analysis");
    const bool flood = edited->kind == QStringLiteral("flood-analysis");
    const bool fillCut = edited->kind == QStringLiteral("fill-cut-analysis");
    const bool contour = edited->kind == QStringLiteral("contour-measure");
    const bool specialized = profile || flood || fillCut || contour;

    QLineEdit *valueLine = nullptr;
    QLineEdit *countLine = nullptr;
    QTableWidget *controlTable = nullptr;
    MeasurementProfileChart *profileChart = nullptr;
    QTableWidget *sampleTable = nullptr;
    QSpinBox *profileCount = nullptr;
    QLabel *profileStart = nullptr;
    QLabel *profileEnd = nullptr;
    QLabel *profileMaximum = nullptr;
    QLabel *profileMinimum = nullptr;
    QLabel *profileInterval = nullptr;
    QDoubleSpinBox *floodDelta = nullptr;
    QLabel *floodTotalArea = nullptr;
    QLabel *floodArea = nullptr;
    QLabel *floodWater = nullptr;
    QCheckBox *floodBorder = nullptr;
    QPushButton *floodPause = nullptr;
    QPushButton *floodStart = nullptr;
    QPushButton *floodReset = nullptr;
    QDoubleSpinBox *fillReference = nullptr;
    QLabel *fillSummary = nullptr;
    QDoubleSpinBox *contourInterval = nullptr;
    QLabel *contourRange = nullptr;

    if (!specialized) {
        auto *summary = new QGroupBox(QStringLiteral("测量结果"), resultPage);
        summary->setObjectName(QStringLiteral("measurementDistanceResultGroup"));
        auto *form = new QFormLayout(summary);
        countLine = readOnlyLine(summary,
            QStringLiteral("measurementControlPointCount"),
            QString::number(edited->points.size()));
        valueLine = readOnlyLine(summary, QStringLiteral("measurementResultValue"),
            edited->resultText.isEmpty()
                ? formatMeasurementValue(edited->kind, edited->value)
                : edited->resultText.section(QLatin1Char('\n'), 0, 0));
        const bool radial = edited->kind == QStringLiteral("radial-line-visible");
        const bool hideValue = edited->kind == QStringLiteral("line-visible")
            || edited->kind == QStringLiteral("gradient-analysis") || radial;
        if (!radial)
            form->addRow(QStringLiteral("控制点个数"), countLine);
        else
            countLine->hide();
        if (!hideValue)
            form->addRow(QStringLiteral("测量值"), valueLine);
        else
            valueLine->hide();
        if (!radial)
            resultLayout->addWidget(summary);
        else
            summary->hide();
        const QVector<GeoCoordinate> displayedPoints = radial
            ? edited->points.mid(0, 1) : edited->points;
        controlTable = measurementCoordinateTable(displayedPoints, resultPage,
            QStringLiteral("measurementControlPointTable"));
        resultLayout->addWidget(controlTable, 1);
    } else if (profile) {
        const ElevationSummary summary = elevationSummary(edited->analysisPoints);
        auto *group = new QGroupBox(QStringLiteral("剖面参数"), resultPage);
        auto *grid = new QGridLayout(group);
        profileStart = new QLabel(edited->analysisPoints.isEmpty() ? QString()
            : pointText(edited->analysisPoints.constFirst()), group);
        profileEnd = new QLabel(edited->analysisPoints.isEmpty() ? QString()
            : pointText(edited->analysisPoints.constLast()), group);
        profileMaximum = new QLabel(summary.valid ? pointText(summary.maximumPoint) : QString(), group);
        profileMinimum = new QLabel(summary.valid ? pointText(summary.minimumPoint) : QString(), group);
        profileInterval = new QLabel(group);
        profileCount = new QSpinBox(group);
        profileCount->setObjectName(QStringLiteral("measurementProfileSampleCount"));
        profileCount->setRange(2, 500);
        profileCount->setValue(qBound(2, qRound(edited->parameter), 500));
        const double length = polylineDistance(edited->analysisPoints, true);
        profileInterval->setText(QStringLiteral("%1 km")
            .arg(length / std::max<qsizetype>(1, edited->analysisPoints.size() - 1) / 1000.0,
                 0, 'f', 3));
        grid->addWidget(new QLabel(QStringLiteral("起点"), group), 0, 0);
        grid->addWidget(profileStart, 0, 1);
        grid->addWidget(new QLabel(QStringLiteral("终点"), group), 1, 0);
        grid->addWidget(profileEnd, 1, 1);
        grid->addWidget(new QLabel(QStringLiteral("最高点"), group), 2, 0);
        grid->addWidget(profileMaximum, 2, 1);
        grid->addWidget(new QLabel(QStringLiteral("最低点"), group), 3, 0);
        grid->addWidget(profileMinimum, 3, 1);
        grid->addWidget(new QLabel(QStringLiteral("采样间隔"), group), 4, 0);
        grid->addWidget(profileInterval, 4, 1);
        grid->addWidget(new QLabel(QStringLiteral("采样点数"), group), 5, 0);
        grid->addWidget(profileCount, 5, 1);
        auto *analyze = new QPushButton(QStringLiteral("分析"), group);
        analyze->setObjectName(QStringLiteral("measurementProfileAnalyze"));
        grid->addWidget(analyze, 5, 2);
        resultLayout->addWidget(group);
        profileChart = new MeasurementProfileChart(resultPage);
        profileChart->setPoints(edited->analysisPoints);
        resultLayout->addWidget(profileChart, 1);
        sampleTable = measurementCoordinateTable(edited->analysisPoints, resultPage,
            QStringLiteral("measurementProfileTable"));
        sampleTable->setMaximumHeight(180);
        resultLayout->addWidget(sampleTable);
    } else {
        const ElevationSummary summary = elevationSummary(edited->analysisPoints);
        auto *group = new QGroupBox(measurementDisplayName(edited->kind), resultPage);
        auto *form = new QFormLayout(group);
        form->addRow(QStringLiteral("最高点"),
            new QLabel(summary.valid ? pointText(summary.maximumPoint) : QString(), group));
        form->addRow(QStringLiteral("最低点"),
            new QLabel(summary.valid ? pointText(summary.minimumPoint) : QString(), group));
        if (flood) {
            const double area = projectedPolygonArea(edited->points);
            floodTotalArea = new QLabel(formatMeasurementValue(QStringLiteral("shadow-area"), area), group);
            floodArea = new QLabel(formatMeasurementValue(QStringLiteral("shadow-area"), edited->value), group);
            floodWater = new QLabel(QStringLiteral("%1 m").arg(edited->secondaryParameter, 0, 'f', 2), group);
            floodDelta = realSpin(group, QStringLiteral("measurementFloodDelta"), 2,
                                  -1000000000.0, 1000000000.0, edited->parameter,
                                  QStringLiteral(" m"));
            floodBorder = new QCheckBox(QStringLiteral("显示边界"), group);
            floodBorder->setObjectName(QStringLiteral("measurementFloodBorder"));
            floodBorder->setChecked(edited->showAnalysisBorder);
            form->addRow(QStringLiteral("总面积"), floodTotalArea);
            form->addRow(QStringLiteral("淹没面积"), floodArea);
            form->addRow(QStringLiteral("水位增量"), floodDelta);
            form->addRow(QStringLiteral("当前水位"), floodWater);
            form->addRow(floodBorder);
            auto *row = new QHBoxLayout;
            floodPause = new QPushButton(QStringLiteral("暂停"), group);
            floodStart = new QPushButton(QStringLiteral("淹没"), group);
            floodReset = new QPushButton(QStringLiteral("重置"), group);
            floodPause->setObjectName(QStringLiteral("measurementFloodPause"));
            floodStart->setObjectName(QStringLiteral("measurementFloodStart"));
            floodReset->setObjectName(QStringLiteral("measurementFloodReset"));
            row->addStretch(); row->addWidget(floodPause); row->addWidget(floodStart);
            row->addWidget(floodReset);
            form->addRow(row);
        } else if (fillCut) {
            fillReference = realSpin(group, QStringLiteral("measurementFillReference"),
                                     2, -10000.0, 10000.0, edited->parameter,
                                     QStringLiteral(" m"));
            fillSummary = new QLabel(edited->resultText, group);
            fillSummary->setObjectName(QStringLiteral("measurementFillSummary"));
            fillSummary->setWordWrap(true);
            auto *analyze = new QPushButton(QStringLiteral("分析"), group);
            analyze->setObjectName(QStringLiteral("measurementFillAnalyze"));
            form->addRow(QStringLiteral("基准高程"), fillReference);
            form->addRow(QStringLiteral("填挖方结果"), fillSummary);
            form->addRow(analyze);
        } else if (contour) {
            contourRange = new QLabel(summary.valid
                ? QStringLiteral("%1 – %2 m").arg(summary.minimumPoint.altitude, 0, 'f', 2)
                    .arg(summary.maximumPoint.altitude, 0, 'f', 2) : QString(), group);
            contourInterval = realSpin(group, QStringLiteral("measurementContourInterval"),
                                       0, 100.0, 1000.0,
                                       std::clamp(edited->parameter, 100.0, 1000.0),
                                       QStringLiteral(" m"));
            contourInterval->setSingleStep(100.0);
            auto *measureButton = new QPushButton(QStringLiteral("测量"), group);
            measureButton->setObjectName(QStringLiteral("measurementContourMeasure"));
            form->addRow(QStringLiteral("高程范围"), contourRange);
            form->addRow(QStringLiteral("等高距"), contourInterval);
            form->addRow(measureButton);
        }
        resultLayout->addWidget(group);
    }

    tabs->addTab(resultPage, QStringLiteral("结果"));
    QWidget *stylePage = createDeploymentMeasurementStylePage(*edited, tabs);
    tabs->addTab(stylePage, QStringLiteral("样式"));
    root->addWidget(tabs, 1);

    const auto applyNow = [dialog, edited, completion, stylePage]() {
        applyDeploymentMeasurementStylePage(stylePage, *edited);
        QString error;
        if (*completion && !(*completion)(*edited, error)) {
            QMessageBox::warning(dialog, QStringLiteral("无法应用测量参数"), error);
            return false;
        }
        return true;
    };

    if (profileCount) {
        auto *analyze = resultPage->findChild<QPushButton *>(
            QStringLiteral("measurementProfileAnalyze"));
        QObject::connect(analyze, &QPushButton::clicked, dialog,
            [=] {
                edited->parameter = profileCount->value();
                if (!applyNow()) return;
                const ElevationSummary summary = elevationSummary(edited->analysisPoints);
                profileStart->setText(edited->analysisPoints.isEmpty() ? QString()
                    : pointText(edited->analysisPoints.constFirst()));
                profileEnd->setText(edited->analysisPoints.isEmpty() ? QString()
                    : pointText(edited->analysisPoints.constLast()));
                profileMaximum->setText(summary.valid ? pointText(summary.maximumPoint) : QString());
                profileMinimum->setText(summary.valid ? pointText(summary.minimumPoint) : QString());
                profileInterval->setText(QStringLiteral("%1 km").arg(
                    polylineDistance(edited->analysisPoints, true)
                        / std::max<qsizetype>(1, edited->analysisPoints.size() - 1) / 1000.0,
                    0, 'f', 3));
                profileChart->setPoints(edited->analysisPoints);
            });
    }

    const auto updateFlood = [=] {
        const ElevationSummary summary = elevationSummary(edited->analysisPoints);
        if (!summary.valid || edited->analysisPoints.isEmpty()) return;
        const double totalArea = projectedPolygonArea(edited->points);
        int flooded = 0;
        for (const GeoCoordinate &point : edited->analysisPoints)
            if (point.altitude <= edited->secondaryParameter) ++flooded;
        edited->value = totalArea * flooded / edited->analysisPoints.size();
        edited->parameter = floodDelta->value();
        edited->showAnalysisBorder = floodBorder->isChecked();
        edited->resultText = QStringLiteral("总面积 %1\n淹没面积 %2\n当前水位 %3 m\n范围最低/最高 %4 / %5 m")
            .arg(formatMeasurementValue(QStringLiteral("shadow-area"), totalArea))
            .arg(formatMeasurementValue(QStringLiteral("shadow-area"), edited->value))
            .arg(edited->secondaryParameter, 0, 'f', 2)
            .arg(summary.minimumPoint.altitude, 0, 'f', 2)
            .arg(summary.maximumPoint.altitude, 0, 'f', 2);
        floodTotalArea->setText(formatMeasurementValue(QStringLiteral("shadow-area"), totalArea));
        floodArea->setText(formatMeasurementValue(QStringLiteral("shadow-area"), edited->value));
        floodWater->setText(QStringLiteral("%1 m").arg(edited->secondaryParameter, 0, 'f', 2));
        applyNow();
    };
    if (floodDelta) {
        auto *timer = new QTimer(dialog);
        timer->setObjectName(QStringLiteral("measurementFloodTimer"));
        timer->setInterval(100);
        QObject::connect(timer, &QTimer::timeout, dialog, [=] {
            const ElevationSummary summary = elevationSummary(edited->analysisPoints);
            if (!summary.valid) { timer->stop(); return; }
            edited->secondaryParameter += floodDelta->value() * 0.1;
            if ((floodDelta->value() >= 0.0
                 && edited->secondaryParameter >= summary.maximumPoint.altitude)
                || (floodDelta->value() < 0.0
                    && edited->secondaryParameter <= summary.minimumPoint.altitude)) {
                edited->secondaryParameter = floodDelta->value() >= 0.0
                    ? summary.maximumPoint.altitude : summary.minimumPoint.altitude;
                timer->stop();
            }
            updateFlood();
        });
        QObject::connect(floodStart, &QPushButton::clicked, dialog, [=] {
            edited->analysisSurfaceVisible = true;
            updateFlood();
            timer->start();
        });
        QObject::connect(floodPause, &QPushButton::clicked, timer, &QTimer::stop);
        QObject::connect(floodReset, &QPushButton::clicked, dialog, [=] {
            timer->stop();
            const ElevationSummary summary = elevationSummary(edited->analysisPoints);
            if (summary.valid) edited->secondaryParameter = summary.minimumPoint.altitude;
            edited->analysisSurfaceVisible = false;
            updateFlood();
        });
        QObject::connect(floodBorder, &QCheckBox::toggled, dialog,
                         [=] { updateFlood(); });
    }

    if (fillReference) {
        auto *analyze = resultPage->findChild<QPushButton *>(
            QStringLiteral("measurementFillAnalyze"));
        QObject::connect(analyze, &QPushButton::clicked, dialog, [=] {
            const ElevationSummary summary = elevationSummary(edited->analysisPoints);
            if (!summary.valid || edited->analysisPoints.isEmpty()) return;
            const double area = projectedPolygonArea(edited->points);
            const double reference = fillReference->value();
            int fillCells = 0, cutCells = 0;
            for (const GeoCoordinate &point : edited->analysisPoints) {
                if (point.altitude <= reference) ++fillCells; else ++cutCells;
            }
            const double totalVolume = area * summary.sum / edited->analysisPoints.size();
            const double fillVolume = totalVolume * fillCells / edited->analysisPoints.size();
            const double cutVolume = totalVolume * cutCells / edited->analysisPoints.size();
            edited->parameter = reference;
            edited->value = fillVolume + cutVolume;
            edited->resultText = QStringLiteral("填方 %1 m^3 / 挖方 %2 m^3 / 总量 %3 m^3\n填方面积 %4 / 挖方面积 %5 / 总面积 %6\n基准高程 %7 m")
                .arg(fillVolume, 0, 'f', 1).arg(cutVolume, 0, 'f', 1)
                .arg(totalVolume, 0, 'f', 1)
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), area * fillCells / edited->analysisPoints.size()))
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), area * cutCells / edited->analysisPoints.size()))
                .arg(formatMeasurementValue(QStringLiteral("shadow-area"), area))
                .arg(reference, 0, 'f', 2);
            fillSummary->setText(edited->resultText);
            applyNow();
        });
    }
    if (contourInterval) {
        auto *measureButton = resultPage->findChild<QPushButton *>(
            QStringLiteral("measurementContourMeasure"));
        QObject::connect(measureButton, &QPushButton::clicked, dialog, [=] {
            edited->parameter = contourInterval->value();
            applyNow();
        });
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply
                                         | QDialogButtonBox::Close, dialog);
    buttons->setObjectName(QStringLiteral("measurementResultButtons"));
    buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("应用"));
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    QObject::connect(buttons->button(QDialogButtonBox::Apply),
                     &QPushButton::clicked, dialog, [applyNow] { applyNow(); });
    QObject::connect(buttons->button(QDialogButtonBox::Close),
                     &QPushButton::clicked, dialog, &QDialog::close);
    root->addWidget(buttons);
    if (radialResult) {
        // DistanceWidget.ui is 433x326. QTabWidget otherwise propagates the
        // wider style page's size hint and silently expands this result view.
        dialog->setMinimumSize(0, 0);
        dialog->layout()->setSizeConstraint(QLayout::SetNoConstraint);
        dialog->contentLayout()->setSizeConstraint(QLayout::SetNoConstraint);
        root->setSizeConstraint(QLayout::SetNoConstraint);
        dialog->resize(433, 326);
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return dialog;
}

void updateDeploymentMarkerPropertyPoints(
    QDialog *dialog,
    const QVector<GeoCoordinate> &points)
{
    if (!dialog)
        return;
    auto *table = dialog->findChild<QTableWidget *>(
        QStringLiteral("markerControlPointTable"));
    if (!table)
        return;
    table->setUpdatesEnabled(false);
    table->setRowCount(points.size());
    for (qsizetype row = 0; row < points.size(); ++row) {
        const GeoCoordinate &point = points.at(row);
        const QString values[] = {
            QString::number(point.longitude, 'f', 6),
            QString::number(point.latitude, 'f', 6),
            QString::number(point.altitude, 'f', 2)};
        for (int column = 0; column < 3; ++column) {
            QTableWidgetItem *item = table->item(row, column);
            if (!item) {
                item = new QTableWidgetItem;
                table->setItem(row, column, item);
            }
            item->setText(values[column]);
        }
    }
    table->setUpdatesEnabled(true);
    table->viewport()->update();
    dialog->setProperty("markerPointsEdited", true);
}

} // namespace Zqxxxt
