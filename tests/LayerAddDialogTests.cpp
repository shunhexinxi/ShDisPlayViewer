#include "modules/zqbs/LayerAddDialog.h"
#include "app/AppPaths.h"
#include "ui/AppTheme.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QDir>
#include <QFont>
#include <QLineEdit>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <iostream>

static bool require(bool value, const char *message)
{ if (!value) std::cerr << "FAIL: " << message << '\n'; return value; }
template<class T> T *control(QWidget &dialog, const char *name)
{ return dialog.findChild<T *>(QString::fromLatin1(name)); }

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Zqxxxt::AppTheme::apply(app);
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    QTemporaryDir data;
    if (!data.isValid()) return 1;
    if (!Zqxxxt::AppPaths::initialize(data.path())) return 1;
    for (const QString &name : {QStringLiteral("test.tif"), QStringLiteral("test.shp"), QStringLiteral("tms.xml")}) {
        QFile file(data.filePath(name));
        if (!file.open(QIODevice::WriteOnly)) return 1;
        file.write("test fixture");
    }
    if (!QDir(data.path()).mkpath(QStringLiteral("flat/0/0"))) return 1;
    {
        QFile tile(data.filePath(QStringLiteral("flat/0/0/0.osgb")));
        if (!tile.open(QIODevice::WriteOnly)) return 1;
        tile.write("representative flatgis fixture");
    }
    auto capture = [&](QWidget &dialog, const QString &name) {
        dialog.show(); app.processEvents();
        if (argc > 1) {
            const QString path = QDir(QString::fromLocal8Bit(argv[1])).filePath(name + QStringLiteral(".png"));
            return dialog.grab().save(path);
        }
        return true;
    };
    Zqxxxt::LayerAddDialog imagery(QStringLiteral("imagery"), data.path());
    auto *driver = control<QComboBox>(imagery, "comboBox_driveType");
    if (!require(driver && driver->currentText() == QStringLiteral("gdal") && !driver->isEnabled(), "original raster GDAL default")) return 2;
    if (!require(!imagery.validationError().isEmpty(), "required name/source validation")) return 2;
    control<QLineEdit>(imagery, "lineEdit_layerName")->setText(QStringLiteral("测试影像图层"));
    control<QLineEdit>(imagery, "lineEdit_path")->setText(data.filePath(QStringLiteral("test.tif")));
    if (!require(imagery.validationError().isEmpty(), "valid raster parameters")) return 2;
    auto *cache = control<QCheckBox>(imagery, "checkBox_cache");
    auto *cacheName = control<QLineEdit>(imagery, "lineEdit_cacheName");
    if (!require(cache && !cache->isChecked() && !cacheName->isEnabled(), "cache disabled by default")) return 2;
    if (!require(cache->text() == QStringLiteral("启用缓存")
            && cache->styleSheet().contains(QStringLiteral("indicator:checked")),
            "cache checkbox has a prominent unchecked/checked treatment")) return 2;
    cache->setChecked(true);
    if (!require(cacheName->isEnabled() && cache->text() == QStringLiteral("缓存：已启用")
            && !imagery.validationError().isEmpty(), "cache fields enable and require values")) return 2;
    cacheName->setText(QStringLiteral("影像缓存"));
    if (!require(control<QLineEdit>(imagery, "lineEdit_cachePath")->isReadOnly()
        && QDir::fromNativeSeparators(control<QLineEdit>(imagery, "lineEdit_cachePath")->text())
            == Zqxxxt::AppPaths::earthCacheDir(), "cache path is fixed beside the application")) return 2;
    control<QSlider>(imagery, "hSlider_opacityDegree")->setValue(35);
    if (!require(imagery.validationError().isEmpty() && imagery.layer().cacheEnabled
            && imagery.layer().cacheConfigured && imagery.layer().opacity == 0.35, "raster cache and opacity values")) return 2;
    if (!capture(imagery, QStringLiteral("imagery"))) return 2;
    control<QCheckBox>(imagery, "checkBox_opacityLayer")->setChecked(false);
    if (!require(!imagery.layer().opacityEnabled && !control<QSlider>(imagery, "hSlider_opacityDegree")->isEnabled(), "opacity checkbox effect")) return 2;
    control<QRadioButton>(imagery, "radioBtn_sliceData")->setChecked(true);
    control<QLineEdit>(imagery, "lineEdit_path")->setText(data.path());
    if (!require(driver->currentText() == QStringLiteral("flatgis") && driver->isEnabled()
            && (driver->model()->flags(driver->model()->index(0, 0)) & Qt::ItemIsEnabled),
            "flatgis is the selectable sliced-data default")) return 2;
    driver->setCurrentText(QStringLiteral("tms"));
    if (!require(driver->currentText() == QStringLiteral("tms")
            && imagery.layer().source.endsWith(QStringLiteral("/tms.xml"))
            && imagery.layer().cachePath == QStringLiteral("cache/earth"), "TMS directory resolution does not change portable cache path")) return 2;
    cacheName->setText(QStringLiteral("../invalid"));
    if (!require(!imagery.validationError().isEmpty(), "invalid cache name rejected")) return 2;

    Zqxxxt::LayerAddDialog elevation(QStringLiteral("elevation"), data.path());
    if (!require(control<QCheckBox>(elevation, "checkBox_opacityLayer")->isHidden()
            && elevation.layer().opacity == 1.0, "elevation hides image opacity controls")) return 3;
    if (!capture(elevation, QStringLiteral("elevation"))) return 3;
    if (!require(elevation.height() < imagery.height(), "elevation window shrinks to its controls")) return 3;
    control<QRadioButton>(elevation, "radioBtn_sliceData")->setChecked(true);
    auto *elevationDriver = control<QComboBox>(elevation, "comboBox_driveType");
    control<QLineEdit>(elevation, "lineEdit_layerName")->setText(QStringLiteral("测试 FlatGIS 高程"));
    control<QLineEdit>(elevation, "lineEdit_path")->setText(data.filePath(QStringLiteral("flat")));
    if (!require(elevationDriver->currentText() == QStringLiteral("flatgis")
            && elevation.layer().source.endsWith(QStringLiteral("/flat"))
            && elevation.layer().kind == QStringLiteral("elevation")
            && elevation.layer().profile == QStringLiteral("global-geodetic")
            && elevation.layer().quadtreeType == QStringLiteral("google")
            && elevation.layer().format == QStringLiteral("osgb")
            && elevation.layer().minLevel == 0
            && elevation.layer().maxLevel == 0
            && elevation.layer().tileSize == 0
            && elevation.validationError().isEmpty(),
            "flatgis elevation metadata, directory and validation are incorrect")) return 3;

    Zqxxxt::LayerAddDialog vector(QStringLiteral("vector"), data.path());
    const auto value = vector.layer();
    auto *tabs = control<QTabWidget>(vector, "tabWidget");
    if (!require(tabs && tabs->count() == 2 && tabs->tabText(0) == QStringLiteral("样式")
            && tabs->tabText(1) == QStringLiteral("字体"), "reference vector tabs")) return 4;
    if (!require(value.minRange == 10000 && value.maxRange == 50000000
            && value.lineWidth == 2.0 && value.color == QColor(Qt::yellow)
            && value.opacity == 1.0 && value.fontSize == 18
            && value.fontColor == QColor(Qt::white) && value.fontHaloColor == QColor(56,58,63)
            && value.customVectorStyle, "reference vector defaults")) return 4;
    if (!capture(vector, QStringLiteral("vector-style"))) return 4;
    auto *minimum = control<QSpinBox>(vector, "spinBox_minDistance");
    auto *maximum = control<QSpinBox>(vector, "spinBox_maxDistance");
    auto *maximumLabel = control<QLabel>(vector, "label_maxDistance");
    std::cout << "Vector layout width=" << vector.width() << ", min-right=" << minimum->geometry().right()
        << ", max-label=" << maximumLabel->geometry().left() << ".." << maximumLabel->geometry().right()
        << ", max-spin=" << maximum->geometry().left() << ".." << maximum->geometry().right()
        << ", page-width=" << maximum->parentWidget()->width() << '\n';
    if (!require(minimum->geometry().right() < maximumLabel->geometry().left()
            && maximumLabel->geometry().right() < maximum->geometry().left()
            && maximum->geometry().right() < maximum->parentWidget()->width(),
            "LOD fields do not overlap or exceed the style page")) return 4;
    const QSize vectorSize = vector.size();
    tabs->setCurrentIndex(1);
    if (!capture(vector, QStringLiteral("vector-font"))) return 4;
    if (!require(vector.size() == vectorSize, "both tabs fit without resizing the dialog")) return 4;
    control<QLineEdit>(vector, "lineEdit_layerName")->setText(QStringLiteral("测试矢量图层"));
    control<QLineEdit>(vector, "lineEdit_path")->setText(data.filePath(QStringLiteral("test.shp")));
    control<QSpinBox>(vector, "spinBox_maxDistance")->setValue(1);
    if (!require(!vector.validationError().isEmpty(), "invalid LOD range rejected")) return 4;
    vector.accept();
    if (!require(vector.result() != QDialog::Accepted, "validation keeps dialog open")) return 4;
    control<QSpinBox>(vector, "spinBox_maxDistance")->setValue(50000000);
    control<QDoubleSpinBox>(vector, "dSpinBox_lineWidth")->setValue(0.1);
    vector.accept();
    if (!require(vector.result() == QDialog::Accepted && vector.layer().lineWidth == 0.1, "valid form accepts subpixel line width")) return 4;
    std::cout << "PASS imagery/elevation/vector layouts, defaults, conditional controls, input validation and values\n";
    return 0;
}
