#include "modules/zqbs/LayerAddDialog.h"
#include "app/AppPaths.h"
#include "ui_RasterLayerForm.h"
#include "ui_VectorLayerForm.h"
#include <QColorDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace Zqxxxt {
namespace {
void compactSpacerHints(QLayout *layout)
{
    for (int index = 0; index < layout->count(); ++index) {
        QLayoutItem *item = layout->itemAt(index);
        if (item->layout()) compactSpacerHints(item->layout());
        else if (item->widget() && item->widget()->layout()) compactSpacerHints(item->widget()->layout());
        else if (auto *spacer = item->spacerItem()) {
            if (spacer->expandingDirections().testFlag(Qt::Vertical))
                spacer->changeSize(0, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);
            else
                spacer->changeSize(spacer->sizeHint().width(), 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        }
    }
    layout->invalidate();
}

void colorButton(QPushButton *button, const QColor &color)
{
    button->setProperty("layerColor", color);
    button->setStyleSheet(QStringLiteral("QPushButton{background:%1;border:1px solid #7797b4;min-width:23px;min-height:23px;padding:0;}")
        .arg(color.name(QColor::HexRgb)));
    button->setToolTip(color.name(QColor::HexRgb));
}
QColor buttonColor(QPushButton *button)
{ return button->property("layerColor").value<QColor>(); }

int flatGisMaximumLevel(const QString &root)
{
    int result = -1;
    const QRegularExpression numeric(QStringLiteral("^[0-9]+$"));
    const QFileInfoList entries = QDir(root).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::Name | QDir::LocaleAware);
    for (const QFileInfo &entry : entries) {
        if (!numeric.match(entry.fileName()).hasMatch())
            continue;
        bool ok = false;
        const int level = entry.fileName().toInt(&ok);
        if (ok && level >= 0 && level <= 30)
            result = qMax(result, level);
    }
    return result;
}
}

LayerAddDialog::LayerAddDialog(const QString &kind, const QString &dataDirectory, QWidget *parent)
    : SkinnedDialog(kind == QStringLiteral("imagery") ? QStringLiteral("影像图层")
        : (kind == QStringLiteral("elevation") ? QStringLiteral("高程图层") : QStringLiteral("矢量图层")), parent),
      m_kind(kind), m_dataDirectory(dataDirectory)
{
    setObjectName(QStringLiteral("deploymentLayerAddDialog"));
    auto *body = new QWidget(this);
    contentLayout()->addWidget(body);
    if (m_kind == QStringLiteral("vector")) {
        m_vector = std::make_unique<Ui::CVectorLayerDialog>();
        m_vector->setupUi(body);
        m_vector->lineEdit_layerName->setMaxLength(25);
        m_vector->comboBox_driveType->clear();
        m_vector->comboBox_driveType->addItem(QStringLiteral("ogr"));
        m_vector->comboBox_driveType->setEnabled(false);
        // These are hidden by the reference CVectorLayerDialog::InitData too.
        m_vector->checkBox_dodge->hide();
        m_vector->label_fontFormat->hide();
        m_vector->lineEdit_fontFormat->hide();
        for (auto *distance : {m_vector->spinBox_minDistance, m_vector->spinBox_maxDistance})
            distance->setMinimumWidth(distance->fontMetrics().horizontalAdvance(QStringLiteral("999999999")) + 52);
        colorButton(m_vector->btn_lineColor, QColor(255, 255, 0));
        colorButton(m_vector->btn_fontColor, QColor(255, 255, 255));
        colorButton(m_vector->btn_shadowColor, QColor(56, 58, 63));
        for (auto *button : {m_vector->btn_lineColor, m_vector->btn_fontColor, m_vector->btn_shadowColor}) {
            connect(button, &QPushButton::clicked, this, [this, button] {
                const QColor color = QColorDialog::getColor(buttonColor(button), this,
                    QStringLiteral("选择颜色"), QColorDialog::DontUseNativeDialog);
                if (color.isValid()) colorButton(button, color);
            });
        }
        connect(m_vector->btn_find_path, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择矢量数据"),
                m_vector->lineEdit_path->text().isEmpty() ? m_dataDirectory : m_vector->lineEdit_path->text(),
                QStringLiteral("矢量数据 (*.shp *.geojson *.json *.kml);;所有文件 (*.*)"));
            if (!path.isEmpty()) m_vector->lineEdit_path->setText(QDir::toNativeSeparators(path));
        });
        connect(m_vector->btn_find_icon, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择图标"),
                m_vector->lineEdit_icon->text().isEmpty() ? m_dataDirectory : m_vector->lineEdit_icon->text(),
                QStringLiteral("图像 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*.*)"));
            if (!path.isEmpty()) m_vector->lineEdit_icon->setText(QDir::toNativeSeparators(path));
        });
        connect(m_vector->btn_ok, &QPushButton::clicked, this, &LayerAddDialog::accept);
        connect(m_vector->btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    } else {
        // The example's context menu uses ImageLayerDialog for BOTH raster
        // kinds; ElevationLayerDialog is an unused older alternative.
        m_raster = std::make_unique<Ui::CImageLayerDialog>();
        m_raster->setupUi(body);
        m_raster->stackedWidget->setCurrentIndex(1);
        m_raster->groupBox->hide();
        m_raster->lineEdit_layerName->setMaxLength(25);
        m_raster->lineEdit_cacheName->setMaxLength(25);
        m_raster->lineEdit_cachePath->setText(QDir::toNativeSeparators(AppPaths::earthCacheDir()));
        m_raster->lineEdit_cachePath->setReadOnly(true);
        m_raster->lineEdit_cachePath->setToolTip(QStringLiteral("缓存统一保存在程序所在目录的 cache/earth 中。"));
        m_raster->btn_find_cachePath->hide();
        m_raster->radioBtn_singleData->setChecked(true);
        m_raster->hSlider_opacityDegree->setRange(0, 100);
        m_raster->hSlider_opacityDegree->setValue(100);
        m_raster->label_opacityDegreeData->setText(QStringLiteral("1.0"));
        const bool imagery = m_kind == QStringLiteral("imagery");
        for (auto *widget : QList<QWidget *>{m_raster->label_display,
            m_raster->checkBox_opacityLayer, m_raster->label_opacityDegree,
            m_raster->hSlider_opacityDegree, m_raster->label_opacityDegreeData})
            widget->setVisible(imagery);
        m_raster->checkBox_cache->setMinimumHeight(34);
        m_raster->checkBox_cache->setCursor(Qt::PointingHandCursor);
        m_raster->checkBox_cache->setStyleSheet(QStringLiteral(R"QSS(
            QCheckBox#checkBox_cache {
                color:#eaf6ff;
                font-weight:600;
                spacing:10px;
                padding:5px 10px;
                background:#0b3d70;
                border:1px solid #4e95c8;
                border-radius:4px;
            }
            QCheckBox#checkBox_cache:hover {
                color:#ffffff;
                background:#115286;
                border-color:#8acfff;
            }
            QCheckBox#checkBox_cache::indicator {
                width:18px;
                height:18px;
                background:#082e56;
                border:2px solid #9dcff1;
                border-radius:3px;
            }
            QCheckBox#checkBox_cache::indicator:checked {
                background:#dff4ff;
                border:5px solid #2480bd;
            }
        )QSS"));
        const auto cacheChanged = [this](bool enabled) {
            m_raster->checkBox_cache->setText(enabled
                ? QStringLiteral("缓存：已启用")
                : QStringLiteral("启用缓存"));
            m_raster->lineEdit_cacheName->setEnabled(enabled);
            m_raster->lineEdit_cachePath->setEnabled(enabled);
            m_raster->label_cacheName->setEnabled(enabled);
            m_raster->label_cachePath->setEnabled(enabled);
        };
        connect(m_raster->checkBox_cache, &QCheckBox::toggled, this, cacheChanged);
        cacheChanged(false);
        connect(m_raster->checkBox_opacityLayer, &QCheckBox::toggled, this, [this](bool enabled) {
            m_raster->hSlider_opacityDegree->setEnabled(enabled);
            m_raster->label_opacityDegree->setEnabled(enabled);
            m_raster->label_opacityDegreeData->setEnabled(enabled);
        });
        connect(m_raster->hSlider_opacityDegree, &QSlider::valueChanged, this, [this](int value) {
            m_raster->label_opacityDegreeData->setText(QString::number(value / 100.0, 'f', 2));
        });
        connect(m_raster->radioBtn_singleData, &QRadioButton::toggled, this,
            [this] { updateRasterDriver(); });
        connect(m_raster->comboBox_driveType, &QComboBox::currentTextChanged,
            this, [this] { updateRasterSourceHint(); });
        updateRasterDriver();
        connect(m_raster->btn_find_path, &QPushButton::clicked, this, [this] {
            const QString initial = m_raster->lineEdit_path->text().isEmpty()
                ? m_dataDirectory : m_raster->lineEdit_path->text();
            const QString path = m_raster->radioBtn_singleData->isChecked()
                ? QFileDialog::getOpenFileName(this, QStringLiteral("选择原始数据"), initial,
                    QStringLiteral("栅格数据 (*.tif *.tiff *.img *.vrt *.dem *.png *.jpg *.jpeg);;所有文件 (*.*)"))
                : QFileDialog::getExistingDirectory(this, QStringLiteral("选择切片目录"), initial);
            if (!path.isEmpty()) m_raster->lineEdit_path->setText(QDir::toNativeSeparators(path));
        });
        connect(m_raster->btn_ok, &QPushButton::clicked, this, &LayerAddDialog::accept);
        connect(m_raster->btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    }
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("layerValidationError"));
    m_error->setTextFormat(Qt::PlainText);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color:#ffcf70;padding:6px;"));
    m_error->hide();
    contentLayout()->addWidget(m_error);
    for (auto *button : findChildren<QPushButton *>()) button->setAutoDefault(false);
    // Keep reference layout proportions without the old hard-coded pixel size.
    const int pathWidth = fontMetrics().horizontalAdvance(QString(30, QLatin1Char('0')));
    (m_vector ? m_vector->lineEdit_path : m_raster->lineEdit_path)->setMinimumWidth(pathWidth);
    compactSpacerHints(body->layout());
    ensurePolished();
    if (m_vector) {
        // Tab pages are still hidden here. Publish their layout requirements
        // explicitly so the outer dialog does not keep a stale, narrower hint.
        int pageWidth = 0;
        for (QWidget *page : {m_vector->tab_style, m_vector->tab_font}) {
            compactSpacerHints(page->layout());
            const QSize required = page->layout()->totalSizeHint()
                .expandedTo(page->layout()->totalMinimumSize());
            page->setMinimumSize(required);
            pageWidth = qMax(pageWidth, required.width());
        }
        const QMargins margins = m_vector->tab_style->layout()->contentsMargins();
        pageWidth = qMax(pageWidth, m_vector->label_minDistance->sizeHint().width()
            + m_vector->label_maxDistance->sizeHint().width()
            + m_vector->spinBox_minDistance->minimumWidth() + m_vector->spinBox_maxDistance->minimumWidth()
            + margins.left() + margins.right() + 5 * m_vector->tab_style->layout()->spacing());
        m_vector->tabWidget->setMinimumWidth(pageWidth + 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth));
    }
    layout()->setSizeConstraint(QLayout::SetNoConstraint);
    adjustSize();
    const QMargins margins = contentLayout()->contentsMargins();
    const int scrollbarWidth = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int contentWidth = body->layout()->totalSizeHint().width()
        + margins.left() + margins.right() + scrollbarWidth;
    resize(qMax(width(), contentWidth), height());
}

LayerAddDialog::~LayerAddDialog() = default;

void LayerAddDialog::updateRasterDriver()
{
    auto *driver = m_raster->comboBox_driveType;
    driver->clear();
    const bool original = m_raster->radioBtn_singleData->isChecked();
    if (original) driver->addItem(QStringLiteral("gdal"));
    else {
        driver->addItem(QStringLiteral("flatgis"));
        driver->addItem(QStringLiteral("tms"));
        driver->setCurrentIndex(0);
    }
    driver->setEnabled(!original);
    updateRasterSourceHint();
}

void LayerAddDialog::updateRasterSourceHint()
{
    if (m_raster->radioBtn_singleData->isChecked()) {
        m_raster->lineEdit_path->setToolTip(QStringLiteral("本地栅格文件路径"));
    } else if (m_raster->comboBox_driveType->currentText() == QStringLiteral("flatgis")) {
        m_raster->lineEdit_path->setToolTip(m_kind == QStringLiteral("elevation")
            ? QStringLiteral("FlatGIS 高程根目录或服务地址；本地目录格式为 层级/列/行.osgb")
            : QStringLiteral("FlatGIS 影像根目录或服务地址"));
    } else {
        m_raster->lineEdit_path->setToolTip(
            QStringLiteral("TMS 切片目录、tms.xml 文件或服务地址"));
    }
}

QString LayerAddDialog::resolveSource(const QString &path) const
{
    const QString value = path.trimmed();
    if (value.isEmpty() || value.contains(QStringLiteral("://"))) return value;
    QString resolved = QDir(m_dataDirectory).absoluteFilePath(QDir::fromNativeSeparators(value));
    return resolved;
}

DeploymentLayer LayerAddDialog::layer() const
{
    DeploymentLayer result;
    result.kind = m_kind;
    if (m_vector) {
        result.name = m_vector->lineEdit_layerName->text().trimmed();
        result.driver = QStringLiteral("ogr");
        result.source = resolveSource(m_vector->lineEdit_path->text());
        result.color = buttonColor(m_vector->btn_lineColor);
        result.lineWidth = m_vector->dSpinBox_lineWidth->value();
        result.opacity = m_vector->dspinBox_lineOpacity->value();
        result.minRange = m_vector->spinBox_minDistance->value();
        result.maxRange = m_vector->spinBox_maxDistance->value();
        result.customVectorStyle = true;
        result.fontSize = m_vector->spinBox_fontSize->value();
        result.fontColor = buttonColor(m_vector->btn_fontColor);
        result.fontHaloColor = buttonColor(m_vector->btn_shadowColor);
        result.pointIcon = resolveSource(m_vector->lineEdit_icon->text());
    } else {
        result.name = m_raster->lineEdit_layerName->text().trimmed();
        result.driver = m_raster->comboBox_driveType->currentText();
        result.source = resolveSource(m_raster->lineEdit_path->text());
        if (result.driver == QStringLiteral("flatgis")) {
            result.profile = QStringLiteral("global-geodetic");
            // The deployed legacy FlatGIS directory convention numbers rows
            // from north to south; this driver's "google" mode means no flip.
            result.quadtreeType = QStringLiteral("google");
            result.format = m_kind == QStringLiteral("elevation")
                ? QStringLiteral("osgb") : QStringLiteral("jpg");
            if (!result.source.contains(QStringLiteral("://")))
                result.maxLevel = flatGisMaximumLevel(result.source);
        }
        if (result.driver == QStringLiteral("tms") && QFileInfo(result.source).isDir()) {
            for (const QString &name : {QStringLiteral("tms.xml"), QStringLiteral("tilemapresource.xml")}) {
                const QString descriptor = QDir(result.source).filePath(name);
                if (QFileInfo(descriptor).isFile()) { result.source = descriptor; break; }
            }
        }
        result.opacityEnabled = m_kind == QStringLiteral("imagery") && m_raster->checkBox_opacityLayer->isChecked();
        result.opacity = m_kind == QStringLiteral("imagery") ? m_raster->hSlider_opacityDegree->value() / 100.0 : 1.0;
        result.cacheEnabled = m_raster->checkBox_cache->isChecked();
        result.cacheConfigured = true;
        result.cacheName = m_raster->lineEdit_cacheName->text().trimmed();
        result.cachePath = QStringLiteral("cache/earth");
    }
    return result;
}

QString LayerAddDialog::validationError() const
{
    const auto value = layer();
    if (value.name.isEmpty()) return QStringLiteral("请输入图层名称。");
    if (value.source.isEmpty()) return QStringLiteral("请选择数据路径或输入服务地址。");
    if (!value.source.contains(QStringLiteral("://"))) {
        const QFileInfo sourceInfo(value.source);
        if (value.driver == QStringLiteral("flatgis")) {
            if (!sourceInfo.isDir())
                return QStringLiteral("FlatGIS 数据源应为存在的本地根目录或服务地址。");
            if (value.kind == QStringLiteral("elevation")
                && !QFileInfo(QDir(value.source).filePath(
                    QStringLiteral("0/0/0.osgb"))).isFile()) {
                return QStringLiteral(
                    "FlatGIS 高程目录无效；未找到代表瓦片 0/0/0.osgb。");
            }
        } else if (!sourceInfo.isFile()) {
            return QStringLiteral(
                "数据文件不存在；TMS 切片目录中应包含 tms.xml 或 tilemapresource.xml。");
        }
    }
    if (value.cacheEnabled) {
        if (value.cacheName.isEmpty()) return QStringLiteral("启用缓存时，请填写缓存名称。");
        if (value.cacheName.contains(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")))
            || value.cacheName == QStringLiteral(".") || value.cacheName == QStringLiteral(".."))
            return QStringLiteral("缓存名称不能包含路径分隔符或文件名保留字符。");
        if (!QFileInfo(AppPaths::earthCacheDir()).isWritable())
            return QStringLiteral("程序所在目录的 cache/earth 不可写，请检查运行目录权限。");
    }
    if (m_vector && value.maxRange < value.minRange) return QStringLiteral("LOD 最大距离不能小于最小距离。");
    if (m_vector && !value.pointIcon.isEmpty() && !QFileInfo(value.pointIcon).isFile())
        return QStringLiteral("图标文件不存在。");
    return {};
}

void LayerAddDialog::accept()
{
    const QString error = validationError();
    m_error->setText(error);
    m_error->setVisible(!error.isEmpty());
    if (error.isEmpty()) SkinnedDialog::accept();
    else adjustSize();
}
}
