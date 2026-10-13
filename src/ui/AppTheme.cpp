#include "ui/AppTheme.h"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QStyleFactory>

namespace Zqxxxt::AppTheme {

QString applicationStyleSheet()
{
    return QStringLiteral(R"QSS(
        QWidget {
            color:#eaf6ff;
        }
        QWidget:disabled { color:#7f9bb2; }
        QLabel { background:transparent; border:0; }
        QDialog, QMessageBox, QInputDialog, QColorDialog, QFileDialog, QFontDialog {
            color:#eaf6ff;
            background:#123f70;
        }
        QFrame[card="true"] {
            background:rgba(8,43,80,185);
            border:1px solid rgba(112,190,238,120);
            border-radius:5px;
        }
        QLineEdit, QPlainTextEdit, QTextEdit, QTextBrowser,
        QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit, QDateTimeEdit,
        QComboBox, QFontComboBox {
            color:#f1f9ff;
            background:#0d3967;
            border:1px solid #4d91c4;
            border-radius:4px;
            padding:5px 8px;
            min-height:20px;
            selection-background-color:#2786ca;
            selection-color:#ffffff;
        }
        QLineEdit:hover, QPlainTextEdit:hover, QTextEdit:hover, QTextBrowser:hover,
        QSpinBox:hover, QDoubleSpinBox:hover, QDateEdit:hover, QTimeEdit:hover,
        QDateTimeEdit:hover, QComboBox:hover, QFontComboBox:hover {
            border-color:#71c6ec;
            background:#104578;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
        QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus,
        QDateTimeEdit:focus, QComboBox:focus, QFontComboBox:focus {
            border:1px solid #72e1ff;
            background:#0b315b;
        }
        QLineEdit:disabled, QPlainTextEdit:disabled, QTextEdit:disabled,
        QSpinBox:disabled, QDoubleSpinBox:disabled, QDateTimeEdit:disabled,
        QComboBox:disabled, QFontComboBox:disabled {
            color:#8ca7bd;
            background:#244e73;
            border-color:#456b8a;
        }
        QComboBox, QFontComboBox { padding-right:28px; }
        QComboBox::drop-down, QFontComboBox::drop-down {
            width:25px;
            border:0;
            border-left:1px solid #4d91c4;
            background:#174f83;
        }
        QComboBox QAbstractItemView, QFontComboBox QAbstractItemView {
            color:#f2f9ff;
            background:#0b345f;
            border:1px solid #63b6e6;
            outline:0;
            padding:3px;
            selection-background-color:#267fc0;
            selection-color:#ffffff;
        }
        QAbstractItemView {
            color:#eaf6ff;
            background:#0d3967;
            alternate-background-color:#124777;
            border:1px solid #4d91c4;
            outline:0;
            selection-background-color:#267fc0;
            selection-color:#ffffff;
        }
        QAbstractItemView::item { min-height:26px; padding:3px 6px; }
        QAbstractItemView::item:hover { background:#1c679f; color:#ffffff; }
        QAbstractItemView::item:selected { background:#2786ca; color:#ffffff; }
        QTableWidget, QTreeWidget, QListWidget {
            gridline-color:#376f99;
            background:#0d3967;
            alternate-background-color:#124777;
        }
        QHeaderView { background:#174f83; }
        QHeaderView::section {
            color:#f4fbff;
            background:#174f83;
            border:0;
            border-right:1px solid #477fa7;
            border-bottom:1px solid #69acd5;
            padding:8px;
            font-weight:600;
        }
        QPushButton {
            color:#f5fbff;
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2478ba,stop:1 #15558f);
            border:1px solid #67b9e7;
            border-radius:4px;
            min-height:28px;
            min-width:72px;
            padding:3px 12px;
        }
        QPushButton:hover {
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #3196d6,stop:1 #1b6cac);
            border-color:#8ce4ff;
        }
        QPushButton:pressed, QPushButton:checked {
            background:#0c3d6c;
            border-color:#65ddff;
        }
        QPushButton:disabled {
            color:#809ab0;
            background:#285274;
            border-color:#466d8b;
        }
        QToolButton {
            color:#eaf6ff;
            background:rgba(18,70,116,175);
            border:1px solid rgba(93,169,216,135);
            border-radius:3px;
            padding:3px;
        }
        QToolButton:hover { background:#1d71ae; border-color:#78dbfa; }
        QToolButton:pressed, QToolButton:checked { background:#0c3b69; }
        QCheckBox, QRadioButton { color:#eaf6ff; spacing:7px; background:transparent; }
        QGroupBox {
            color:#9edcff;
            background:rgba(8,43,80,125);
            border:1px solid rgba(112,190,238,105);
            border-radius:5px;
            margin-top:12px;
            padding-top:12px;
            font-weight:600;
        }
        QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 6px; }
        QTabWidget::pane {
            background:rgba(8,43,80,120);
            border:1px solid #568fb7;
            border-radius:3px;
        }
        QTabBar::tab {
            color:#cfe9f8;
            background:#144979;
            border:1px solid #507fa2;
            padding:8px 18px;
        }
        QTabBar::tab:hover { color:#ffffff; background:#1d639e; }
        QTabBar::tab:selected { color:#ffffff; background:#287dbc; border-color:#76c9eb; }
        QMenu {
            color:#eaf6ff;
            background:#0c355f;
            border:1px solid #5ca5d2;
            padding:5px;
        }
        QMenu::item { padding:7px 30px 7px 12px; border-radius:3px; }
        QMenu::item:selected { color:#ffffff; background:#267fc0; }
        QMenu::item:disabled { color:#7895ab; }
        QMenu::separator { height:1px; background:#386b91; margin:4px 7px; }
        QScrollArea, QStackedWidget { background:transparent; border:0; }
        QScrollArea QWidget#qt_scrollarea_viewport { background:transparent; }
        QAbstractScrollArea::corner { background:#123f70; }
        QScrollBar:vertical {
            width:11px; background:#10385e; margin:0; border:0;
        }
        QScrollBar:horizontal {
            height:11px; background:#10385e; margin:0; border:0;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background:#4f92bf; border-radius:5px; min-height:28px; min-width:28px;
        }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background:#6bc3e8; }
        QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; border:0; }
        QSplitter::handle { background:#4380ab; }
        QSlider::groove:horizontal { height:5px; background:#0b355f; border-radius:2px; }
        QSlider::sub-page:horizontal { background:#3aa7db; border-radius:2px; }
        QSlider::handle:horizontal {
            width:14px; margin:-5px 0; background:#79e3ff;
            border:1px solid #d8f8ff; border-radius:7px;
        }
        QToolTip {
            color:#f5fbff;
            background:#092d52;
            border:1px solid #67b9e7;
            padding:5px 8px;
        }
        QFrame#moduleNavigation, QWidget#moduleNavigation, QWidget#wlcqSidebar,
        QFrame#managementNavigation {
            background:#0f3772;
            border:0;
            border-right:1px solid #2f75ad;
        }
        QPushButton[sideNavigation="true"] {
            color:#f4f9ff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #123a73,stop:1 #153f78);
            border:0;
            border-bottom:1px solid rgba(80,137,190,105);
            border-radius:0;
            min-height:41px;
            max-height:41px;
            min-width:0;
            padding:0 12px 0 9px;
            text-align:left;
            font-size:16px;
            font-weight:500;
        }
        QPushButton[sideNavigation="true"]:hover {
            color:#73d8ff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1b4c85,stop:1 #225d9a);
            border-bottom-color:#3c8ac0;
        }
        QPushButton[sideNavigation="true"]:checked {
            color:#ffffff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #235487,stop:1 #2d659c);
            border-left:4px solid #30c9ff;
            padding-left:5px;
        }
        QLabel[sideNavigationSection="true"] {
            color:#4bc9ff;
            background:#0c3062;
            border:0;
            border-bottom:1px solid rgba(66,125,177,85);
            min-height:24px;
            max-height:24px;
            padding:0 10px;
            font-size:11px;
            font-weight:600;
        }
    )QSS");
}

QString managementModuleStyleSheet()
{
    return QStringLiteral(R"QSS(
        QWidget[managementModule="true"] { background:#2468BC; }
        QFrame#managementNavigation {
            background:#0f3772;
            border:0;
            border-right:1px solid #2f75ad;
        }
        QLabel#managementNavigationCaption {
            color:#ffffff;
            background:transparent;
            font-size:20px;
            font-weight:600;
            padding:8px 7px 14px;
        }
        QLabel[managementNavigationSection="true"] {
            color:#4bc9ff;
            background:#0c3062;
            font-size:11px;
            padding:0 10px;
            min-height:24px;
            max-height:24px;
        }
        QPushButton[managementNavigationItem="true"] {
            color:#f4f9ff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #123a73,stop:1 #153f78);
            border:0;
            border-bottom:1px solid rgba(80,137,190,105);
            border-radius:0;
            min-height:41px;
            max-height:41px;
            min-width:0;
            padding:0 12px 0 9px;
            text-align:left;
            font-size:16px;
            font-weight:500;
        }
        QPushButton[managementNavigationItem="true"]:hover {
            color:#73d8ff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1b4c85,stop:1 #225d9a);
        }
        QPushButton[managementNavigationItem="true"]:checked {
            color:#ffffff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #235487,stop:1 #2d659c);
            border-left:4px solid #30c9ff;
            padding-left:5px;
        }
    )QSS");
}

QString managementContentStyleSheet()
{
    return applicationStyleSheet() + QStringLiteral(R"QSS(
        QWidget#managementPage { background:#2468BC; }
        QStackedWidget#managementContentStack {
            background:rgba(8,43,82,188);
            border:1px solid #4d91c4;
            border-radius:0;
        }
        QFrame#managementPanel, QFrame#toolbar, QFrame#summaryPanel {
            background:rgba(8,43,82,178);
            border:1px solid #4d91c4;
            border-radius:0;
        }
        QFrame#metricCard {
            background:#154a7d;
            border:1px solid #4d91c4;
            border-left:3px solid #64d9ff;
            border-radius:0;
        }
        QFrame#deviceBindingPanel {
            background:#0d3967;
            border:1px solid #4d91c4;
            border-radius:0;
        }
        QWidget#systemOperationsPage QGroupBox,
        QWidget#runtimeMonitorPage QTabWidget::pane {
            border:1px solid #4d91c4;
            border-radius:0;
        }
    )QSS");
}

QString deploymentPanelStyleSheet()
{
    return QStringLiteral(R"QSS(
        QWidget { color:#eaf6ff; font-family:'Microsoft YaHei UI','Microsoft YaHei'; }
        QWidget#deploymentLeftPanel, QWidget#deploymentRightPanel,
        QWidget#deploymentTopPanel, QWidget#deploymentBottomPanel,
        QWidget#deploymentBlackboardPanel, QWidget#deploymentDialogBody {
            background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 rgba(10,45,82,238),stop:1 rgba(20,79,132,232));
            border:1px solid rgba(104,190,235,145);
        }
        QWidget#deploymentTopPanel, QWidget#deploymentBottomPanel { background:#0d3967; }
        QWidget#deploymentTopPanel { border-bottom:1px solid #347fb5; }
        QWidget#deploymentSectionHeading { background:#0c3b69; border:0; border-bottom:1px solid #347db0; }
        QLabel#deploymentSectionTitle, QLabel#deploymentInspectorTitle {
            color:#f0faff; font-size:16px; font-weight:700; padding:5px 3px;
        }
        QLabel#deploymentSectionSubtitle { color:#6fd9f4; font-size:10px; }
        QLabel#deploymentMapHeading { color:#ffffff; font-size:18px; font-weight:700;
            background:rgba(3,27,53,185); border-left:3px solid #57dcff; padding:2px 10px; }
        QLabel#deploymentMapSubtitle { color:#8fd8ef; font-size:10px;
            background:rgba(3,27,53,150); padding:1px 13px; }
        QWidget#deploymentSceneTools { background:rgba(5,31,59,215); border:1px solid #4385b2; border-radius:5px; }
        QFrame#deploymentToolPalette { background:rgba(8,42,78,238); border:1px solid #45a9db; }
        QWidget#deploymentPaletteHeading { background:#164f7e; border:0; border-bottom:1px solid #40a8da; }
        QWidget#deploymentPaletteBody { background:rgba(9,48,87,242); border:0; }
        QLabel#deploymentPaletteTitle { color:#ffffff; font-size:13px; font-weight:700; }
        QToolButton#deploymentPaletteClose { color:#d8efff; background:transparent; border:0;
            min-width:22px; max-width:22px; min-height:22px; max-height:22px; font-size:18px; }
        QToolButton#deploymentPaletteClose:hover { color:#ffffff; background:#c74848; }
        QToolButton#deploymentPaletteTool { color:#e8f6ff; background:rgba(16,66,109,220);
            border:0; border-right:1px solid #2b7aaa; border-bottom:1px solid #2b7aaa;
            border-radius:0; padding:4px; font-size:12px; }
        QToolButton#deploymentPaletteTool:hover { color:#ffffff; background:#176d9f;
            border-color:#67dfff; }
        QToolButton#deploymentPaletteTool:pressed { background:#0b3f6d; }
        QPushButton[sceneTool="true"] { min-width:58px; padding:2px 4px; background:#0d3d6b; border-color:#3d7cac; }
        QPushButton[sceneTool="true"]:hover { background:#1b689f; border-color:#63daf7; }
        QPushButton[deploymentDomain="true"] { min-height:48px; text-align:left; padding:3px 14px;
            color:#9cc9e3; background:transparent; border:0; border-bottom:3px solid transparent;
            border-radius:0; font-size:12px; }
        QPushButton[deploymentDomain="true"]:hover { color:#ffffff; background:#0d3a67; }
        QPushButton[deploymentDomain="true"]:checked { color:#ffffff; background:#104574;
            border-bottom:3px solid #55ddff; font-weight:700; }
        QPushButton#deploymentSyncButton { background:#0d4b72; border-color:#48a4ce; }
        QWidget#deploymentSummaryPanel, QWidget#deploymentTimelinePanel { background:transparent; border:0; }
        QFrame#deploymentSummaryCard { background:#082f55; border:1px solid #2e709d;
            border-left:3px solid #48cbed; border-radius:3px; }
        QLabel#deploymentSummaryLabel { color:#8fbcd6; font-size:10px; }
        QLabel#deploymentSummaryValue { color:#eefaff; font-size:19px; font-weight:700; }
        QLabel#deploymentTimelineTitle { color:#6ee3ff; font-size:13px; font-weight:700; }
        QLabel#deploymentStepCounter { color:#ffe08b; font-weight:700; }
        QLabel#deploymentStepDescription, QLabel#deploymentTimelineEmpty { color:#9fcce2; }
        QWidget#deploymentTimelineSteps { background:#071f39; border:1px solid #2e638a; }
        QPushButton[timelineStep="true"] { min-height:39px; text-align:left; color:#9bc7df;
            background:#0a355d; border:1px solid #326d98; border-radius:2px; }
        QPushButton[timelineStep="true"][current="true"] { color:#ffffff; background:#126d94;
            border-color:#5ae6ff; font-weight:700; }
        QLabel#deploymentStatus { color:#8fbcd6; font-size:10px; }
        QLabel { color:#dceeff; background:transparent; border:0; }
        QLineEdit, QPlainTextEdit, QTextEdit, QTextBrowser,
        QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit, QDateTimeEdit,
        QComboBox, QFontComboBox, QTreeWidget, QListWidget, QTableWidget {
            color:#f1f9ff;
            background:#0c355f;
            border:1px solid #4f98d0;
            border-radius:3px;
            selection-background-color:#2784d3;
            selection-color:#ffffff;
            padding:5px;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
        QSpinBox:focus, QDoubleSpinBox:focus, QDateTimeEdit:focus, QComboBox:focus {
            border-color:#73e0ff;
        }
        QComboBox QAbstractItemView, QFontComboBox QAbstractItemView {
            color:#f5fbff;
            background:#0a3159;
            border:1px solid #67b9e7;
            outline:0;
            selection-background-color:#267fc0;
            selection-color:#ffffff;
        }
        QTreeWidget { alternate-background-color:#104271; }
        QTreeWidget::item { min-height:28px; }
        QTreeWidget::item:hover { background:#1c679f; }
        QTreeWidget::item:selected { color:#ffffff; background:#257fc8; }
        QPushButton {
            color:#ffffff;
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #277fbe,stop:1 #175a96);
            border:1px solid #72b9e8;
            border-radius:4px;
            min-height:28px;
            padding:3px 11px;
        }
        QPushButton:hover { background:#2788cd; border-color:#8de7ff; }
        QPushButton:pressed, QPushButton:checked { background:#0b345f; border-color:#66e0ff; }
        QPushButton:disabled { color:#819db5; background:#285274; border-color:#416b8b; }
        QTabWidget::pane { background:rgba(7,39,75,150); border:1px solid #5796c2; }
        QTabBar::tab { color:#cfe8f8; background:#124576; border:1px solid #4f89b2; padding:8px 16px; }
        QTabBar::tab:selected { color:#ffffff; background:#277dbd; border-color:#78cfef; }
        QMenu { color:#eaf6ff; background:#0b345f; border:1px solid #61add9; padding:5px; }
        QMenu::item { padding:7px 28px 7px 12px; }
        QMenu::item:selected { color:#ffffff; background:#267fc0; }
        QGroupBox { color:#8edfff; background:rgba(7,39,75,130); border:1px solid #5796c2;
            border-radius:4px; margin-top:12px; padding-top:12px; }
        QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 6px; }
        QCheckBox, QRadioButton { color:#eaf6ff; background:transparent; spacing:7px; }
        QSlider::groove:horizontal { height:5px; background:#0b355f; border-radius:2px; }
        QSlider::sub-page:horizontal { background:#37a8dc; border-radius:2px; }
        QSlider::handle:horizontal { width:14px; margin:-5px 0; background:#72e1ff; border-radius:7px; }
    )QSS");
}

void configureSideNavigationButton(QPushButton &button)
{
    auto navigationGlyph = [](const QColor &color) {
        QPixmap pixmap(18, 16);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(color, 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(1.5, 3.0, 14.0, 10.0), 1.2, 1.2);
        QPainterPath triangle;
        triangle.moveTo(7.0, 5.2);
        triangle.lineTo(12.0, 8.0);
        triangle.lineTo(7.0, 10.8);
        triangle.closeSubpath();
        painter.fillPath(triangle, color);
        return pixmap;
    };

    QIcon icon;
    icon.addPixmap(navigationGlyph(QColor(QStringLiteral("#f4f9ff"))),
                   QIcon::Normal, QIcon::Off);
    icon.addPixmap(navigationGlyph(QColor(QStringLiteral("#6ee1ff"))),
                   QIcon::Active, QIcon::Off);
    icon.addPixmap(navigationGlyph(QColor(QStringLiteral("#ffffff"))),
                   QIcon::Normal, QIcon::On);
    icon.addPixmap(navigationGlyph(QColor(QStringLiteral("#ffffff"))),
                   QIcon::Active, QIcon::On);
    button.setProperty("sideNavigation", true);
    button.setIcon(icon);
    button.setIconSize(QSize(18, 16));
    button.setFixedHeight(42);
    button.setFocusPolicy(Qt::NoFocus);
}

void apply(QApplication &application)
{
    if (QStyle *style = QStyleFactory::create(QStringLiteral("Fusion")))
        application.setStyle(style);

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#123f70")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#eaf6ff")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#0d3967")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#124777")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#092d52")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f5fbff")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#eaf6ff")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#185b9d")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#f5fbff")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2786ca")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#89abc2")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(QStringLiteral("#7f9bb2")));
    palette.setColor(QPalette::Disabled, QPalette::Text,
                     QColor(QStringLiteral("#7f9bb2")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(QStringLiteral("#7f9bb2")));
    application.setPalette(palette);
    application.setStyleSheet(applicationStyleSheet());
}

}
