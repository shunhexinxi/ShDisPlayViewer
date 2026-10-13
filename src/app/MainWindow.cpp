#include "MainWindow.h"
#include "AppPaths.h"
#include "ProjectStore.h"
#include "modules/zqbs/ZqbsPage.h"
#include "modules/zqts/SceneWidget.h"
#include "ui/SkinnedDialog.h"
#include "ui/AppIcon.h"
#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QCursor>
#include <QDir>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <functional>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {
constexpr auto kViewerTitle = "ShDisplayViewer-三维地图演练系统";

QPixmap viewerIcon(int size)
{
    return Zqxxxt::applicationIcon().pixmap(size, size);
}

class ViewerTitleBar final : public QFrame {
public:
    ViewerTitleBar(QMainWindow *owner, std::function<void()> toggleFullscreen)
        : QFrame(owner), m_owner(owner), m_toggleFullscreen(std::move(toggleFullscreen)) {}
protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && !m_owner->isFullScreen() && m_owner->windowHandle()) {
            m_owner->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_toggleFullscreen();
            event->accept();
            return;
        }
        QFrame::mouseDoubleClickEvent(event);
    }
private:
    QMainWindow *m_owner;
    std::function<void()> m_toggleFullscreen;
};
}

namespace Zqxxxt {
MainWindow::MainWindow(const QString &project)
{
    resize(1440, 900);
    setWindowTitle(QString::fromUtf8(kViewerTitle));
    setWindowIcon(applicationIcon());
    setWindowFlag(Qt::FramelessWindowHint);
    m_mainMenuBar = new QMenuBar(this);
    auto *menu = m_mainMenuBar->addMenu(QStringLiteral("项目"));
    menu->setObjectName(QStringLiteral("projectMenu"));
    menu->addAction(QStringLiteral("新建演练项目…"), this, [this] {
        if (m_presentation) { message(QStringLiteral("请先退出演示模式。")); return; }
        const auto root = QFileDialog::getExistingDirectory(this, QStringLiteral("选择空目录新建项目"));
        if (root.isEmpty()) return;
        QString error;
        if (!ProjectStore::create(root, error)) message(error); else openProject(root);
    });
    menu->addAction(QStringLiteral("打开项目…"), this, [this] {
        if (m_presentation) { message(QStringLiteral("请先退出演示模式。")); return; }
        const auto root = QFileDialog::getExistingDirectory(this, QStringLiteral("打开演练项目目录"));
        if (!root.isEmpty()) openProject(root);
    });
    menu->addAction(QStringLiteral("保存项目"), this, [this] { save(); }, QKeySequence::Save)
        ->setObjectName(QStringLiteral("projectSave"));
    menu->addAction(QStringLiteral("另存为…"), this, [this] { exportProject(true); }, QKeySequence::SaveAs);
    m_presentationAction = menu->addAction(QStringLiteral("进入 / 退出演示副本"), this, [this] {
        togglePresentation();
        m_presentationAction->setChecked(bool(m_presentation));
    });
    m_presentationAction->setObjectName(QStringLiteral("projectPresentation"));
    m_presentationAction->setCheckable(true);
    menu->addAction(QStringLiteral("导出演练项目…"), this, [this] { exportProject(false); });
    menu->addAction(QStringLiteral("导入 Qt 动态标记 XML…"), this, [this] {
        if (m_presentation) { message(QStringLiteral("请先退出演示模式。")); return; }
        const auto source = QFileDialog::getExistingDirectory(this, QStringLiteral("选择含 dynamic-plans.xml 的 Config 目录（只读导入）"));
        if (source.isEmpty()) return;
        const auto destination = QFileDialog::getExistingDirectory(this, QStringLiteral("选择空目录存放独立副本"));
        if (destination.isEmpty()) return;
        QString error;
        if (!ProjectStore::importLegacy(source, destination, error)) message(error);
        else { openProject(destination); message(QStringLiteral("XML 已导入。原始配置未改动；请重新选择底图并检查资源，IMS 动作不执行。")); }
    });
    auto *tools = m_mainMenuBar->addMenu(QStringLiteral("资源"));
    tools->addAction(QStringLiteral("检查资源…"), this, [this] {
        if (m_page) message(ProjectStore::validateResources(m_page->data()).join('\n'));
    });
    tools->addAction(QStringLiteral("导入单个资源…"), this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, QStringLiteral("选择单个图标或模型文件（不会遍历附属目录）"));
        if (file.isEmpty()) return;
        QString relative, error;
        if (!ProjectStore::importAsset(m_project, file, relative, error)) message(error);
        else {
            QApplication::clipboard()->setText(relative);
            message(QStringLiteral("已登记资源，相对路径已复制到剪贴板，可粘贴到标记资源路径：\n%1\n多文件模型的附属文件需要逐个配置；地图数据集保持外部引用。").arg(relative));
        }
    });
    tools->addAction(QStringLiteral("选择地图 .earth 配置…"), this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, QStringLiteral("选择地图配置（保留其中的 TMS/标准服务设置）"), {}, QStringLiteral("Earth (*.earth)"));
        if (file.isEmpty() || !save()) return;
        QString error;
        if (!ProjectStore::setEarth(m_project, file, error)) message(error);
        else openProject(m_project);
    });
    tools->addAction(QStringLiteral("地图服务说明"), this, [this] {
        message(QStringLiteral("基础场景 → 图层 → 添加图层，可配置 TMS 服务和本地数据。其他地图驱动通过 .earth 文件配置，以当前 FreeEarthSDK 构建支持为准。网络地图需要服务可达，服务自身的凭据与许可仍由提供方决定。默认示例为空白离线地球，不请求 IMS 或外部底图。"));
    });
    auto *windowFullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(windowFullscreenShortcut, &QShortcut::activated, this, &MainWindow::toggleWindowFullscreen);
    auto *escapeMapFullscreen = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escapeMapFullscreen, &QShortcut::activated, this, [this] {
        if (m_mapFullscreen) setMapFullscreen(false);
        else if (isFullScreen()) toggleWindowFullscreen();
    });
    auto *help = m_mainMenuBar->addMenu(QStringLiteral("帮助"));
    help->setObjectName(QStringLiteral("helpMenu"));
    auto *manual = help->addAction(QStringLiteral("操作手册"));
    manual->setObjectName(QStringLiteral("helpManual"));
    manual->setData(QUrl::fromLocalFile(
        QDir(AppPaths::applicationDir()).filePath(QStringLiteral("doc/USER_GUIDE.pdf"))));
    connect(manual, &QAction::triggered, this, [this, manual] {
        setProperty("manualOpenSucceeded", false);
        const QUrl url = manual->data().toUrl();
        const QFileInfo file(url.toLocalFile());
        if (!file.isFile() || !file.isReadable()) {
            message(QStringLiteral("未找到或无法读取操作手册：\n%1\n请保留程序目录下的 doc/USER_GUIDE.pdf 文件。")
                .arg(QDir::toNativeSeparators(file.absoluteFilePath())));
            return;
        }
        const bool opened = QDesktopServices::openUrl(url);
        setProperty("manualOpenSucceeded", opened);
        if (!opened) message(QStringLiteral("无法打开操作手册，请安装或配置默认 PDF 阅读器。\n文件位置：%1")
            .arg(QDir::toNativeSeparators(file.absoluteFilePath())));
    });
    help->addAction(QStringLiteral("关于"), this, [this] { showAbout(); });
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("viewerWindowHeader"));
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    auto *titleBar = new ViewerTitleBar(this, [this] { toggleWindowFullscreen(); });
    titleBar->setParent(header);
    titleBar->setObjectName(QStringLiteral("viewerTitleBar"));
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet(QStringLiteral(
        "QFrame#viewerTitleBar{background:#1b5d99;border:0;border-bottom:1px solid #5798cb;}"));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 4, 0);
    titleLayout->setSpacing(8);
    auto *icon = new QLabel(titleBar);
    icon->setPixmap(viewerIcon(23));
    icon->setFixedSize(23, 23);
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLayout->addWidget(icon);
    auto *title = new QLabel(windowTitle(), titleBar);
    title->setObjectName(QStringLiteral("viewerTitleText"));
    title->setStyleSheet(QStringLiteral("color:white;background:transparent;font-weight:600;font-size:14px;"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(this, &QWidget::windowTitleChanged, title, &QLabel::setText);
    titleLayout->addWidget(title, 1);
    const auto makeButton = [titleBar, titleLayout](const QString &name, const QString &text, const QString &tip) {
        auto *button = new QToolButton(titleBar);
        button->setObjectName(name);
        button->setText(text);
        button->setToolTip(tip);
        button->setFixedSize(43, 32);
        button->setFocusPolicy(Qt::NoFocus);
        button->setStyleSheet(QStringLiteral(
            "QToolButton{color:white;background:transparent;border:0;border-radius:0;font-size:17px;}"
            "QToolButton:hover{background:#3883bd;}"
            "QToolButton:pressed{background:#0e477d;}"));
        titleLayout->addWidget(button);
        return button;
    };
    auto *minimize = makeButton(QStringLiteral("viewerMinimize"), QStringLiteral("—"), QStringLiteral("最小化"));
    m_maximizeButton = makeButton(QStringLiteral("viewerMaximize"), QStringLiteral("□"), QStringLiteral("窗口全屏（F11）"));
    auto *close = makeButton(QStringLiteral("viewerClose"), QStringLiteral("×"), QStringLiteral("关闭"));
    close->setStyleSheet(QStringLiteral(
        "QToolButton{color:white;background:transparent;border:0;border-radius:0;font-size:21px;}"
        "QToolButton:hover{background:#c44242;}"
        "QToolButton:pressed{background:#922929;}"));
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maximizeButton, &QToolButton::clicked, this, &MainWindow::toggleWindowFullscreen);
    connect(close, &QToolButton::clicked, this, &QWidget::close);
    headerLayout->addWidget(titleBar);
    m_mainMenuBar->setStyleSheet(QStringLiteral(
        "QMenuBar{color:#eaf6ff;background:#164a7d;border:0;padding:1px 3px;}"
        "QMenuBar::item{padding:4px 10px;background:transparent;}"
        "QMenuBar::item:selected{background:#2779b5;}"));
    headerLayout->addWidget(m_mainMenuBar);
    setMenuWidget(header);
    m_windowHeader = header;
#ifndef Q_OS_WIN
    // The native frame used to resize the Ubuntu window. Keep native edge
    // resizing while drawing our shared title bar and fullscreen controls.
    setMouseTracking(true);
    qApp->installEventFilter(this);
#endif
    QString error;
    if (!QFileInfo::exists(QDir(project).filePath(ProjectStore::manifestName()))
        && !ProjectStore::create(project, error)) { message(error); return; }
    openProject(project);
}
MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    if (m_page) m_page->setActive(false);
    delete m_page;
    delete takeCentralWidget();
}
void MainWindow::message(const QString &error) { QMessageBox::information(this, QStringLiteral("ShDisplayViewer"), error); }
void MainWindow::showAbout()
{
    SkinnedDialog dialog(QStringLiteral("关于"), this);
    dialog.setObjectName(QStringLiteral("viewerAboutDialog"));
    dialog.resize(440, 220);
    auto *row = new QHBoxLayout;
    auto *icon = new QLabel(&dialog);
    icon->setPixmap(viewerIcon(64));
    icon->setFixedSize(64, 64);
    icon->setAlignment(Qt::AlignTop);
    row->addWidget(icon);
    auto *details = new QLabel(&dialog);
    details->setObjectName(QStringLiteral("viewerAboutDetails"));
    details->setTextFormat(Qt::RichText);
    details->setTextInteractionFlags(Qt::TextBrowserInteraction);
    details->setOpenExternalLinks(true);
    details->setText(QStringLiteral(
        "版本：1.0.2<br>"
        "开发者：青岛顺和信息技术有限公司<br>"
        "支持网站：<a style='color:#8fe5ff' href='http://www.shunhexinxi.com'>www.shunhexinxi.com</a>"));
    row->addWidget(details, 1);
    dialog.contentLayout()->addLayout(row);
    auto *close = new QPushButton(QStringLiteral("关闭"), &dialog);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.contentLayout()->addWidget(close, 0, Qt::AlignRight);
    dialog.exec();
}
void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_maximizeButton) {
        m_maximizeButton->setText(isFullScreen() ? QStringLiteral("❐") : QStringLiteral("□"));
        m_maximizeButton->setToolTip(isFullScreen() ? QStringLiteral("还原") : QStringLiteral("窗口全屏（F11）"));
        unsetCursor();
    }
}
void MainWindow::toggleWindowFullscreen()
{
    if (m_mapFullscreen) { setMapFullscreen(false); return; }
    if (isFullScreen()) {
        if (m_windowFullscreenPreviousState.testFlag(Qt::WindowMaximized)) showMaximized();
        else {
            showNormal();
            if (m_windowFullscreenGeometry.isValid()) setGeometry(m_windowFullscreenGeometry);
        }
    } else {
        m_windowFullscreenPreviousState = windowState();
        m_windowFullscreenGeometry = geometry();
        showFullScreen();
    }
}
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
#ifndef Q_OS_WIN
    if ((event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)
        && !isFullScreen() && !isMaximized()) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->window() == this) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            const QPoint point = mapFromGlobal(mouse->globalPosition().toPoint());
            Qt::Edges edges;
            if (rect().contains(point)) {
                constexpr int border = 8;
                if (point.x() < border) edges |= Qt::LeftEdge;
                else if (point.x() >= width() - border) edges |= Qt::RightEdge;
                if (point.y() < border) edges |= Qt::TopEdge;
                else if (point.y() >= height() - border) edges |= Qt::BottomEdge;
            }
            if (edges) {
                const bool horizontal = edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge);
                const bool vertical = edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge);
                const bool diagonal = edges.testFlag(Qt::LeftEdge) == edges.testFlag(Qt::TopEdge);
                setCursor(horizontal && vertical ? (diagonal ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor)
                    : horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor);
                if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton
                    && windowHandle() && windowHandle()->startSystemResize(edges)) return true;
            } else unsetCursor();
        }
    }
#endif
    return QMainWindow::eventFilter(watched, event);
}
#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType)
    const auto *native = static_cast<MSG *>(message);
    if (native->message == WM_GETMINMAXINFO) {
        const HMONITOR monitor = MonitorFromWindow(native->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo {sizeof(MONITORINFO)};
        if (GetMonitorInfoW(monitor, &monitorInfo)) {
            auto *limits = reinterpret_cast<MINMAXINFO *>(native->lParam);
            const RECT &work = monitorInfo.rcWork;
            const RECT &screen = monitorInfo.rcMonitor;
            limits->ptMaxPosition.x = work.left - screen.left;
            limits->ptMaxPosition.y = work.top - screen.top;
            limits->ptMaxSize.x = work.right - work.left;
            limits->ptMaxSize.y = work.bottom - work.top;
            *result = 0;
            return true;
        }
    }
    if (native->message == WM_NCHITTEST && !isMaximized() && !isFullScreen()) {
        RECT bounds {};
        if (GetWindowRect(native->hwnd, &bounds)) {
            constexpr int edge = 8;
            const int x = GET_X_LPARAM(native->lParam);
            const int y = GET_Y_LPARAM(native->lParam);
            const bool left = x < bounds.left + edge;
            const bool right = x >= bounds.right - edge;
            const bool top = y < bounds.top + edge;
            const bool bottom = y >= bounds.bottom - edge;
            if (top && left) *result = HTTOPLEFT;
            else if (top && right) *result = HTTOPRIGHT;
            else if (bottom && left) *result = HTBOTTOMLEFT;
            else if (bottom && right) *result = HTBOTTOMRIGHT;
            else if (left) *result = HTLEFT;
            else if (right) *result = HTRIGHT;
            else if (top) *result = HTTOP;
            else if (bottom) *result = HTBOTTOM;
            else return QMainWindow::nativeEvent(eventType, message, result);
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif
bool MainWindow::earthReady() const { return m_scene && m_scene->earthReady(); }
int MainWindow::activeDynamicCount() const { return m_page ? int(m_page->activeDynamicMarkers().size()) : 0; }
bool MainWindow::save()
{
    if (!m_page) return false;
    QString error;
    if (!m_page->saveProject(error)) { message(error); return false; }
    statusBar()->showMessage(m_presentation ? QStringLiteral("已保存到演示临时副本，原方案未改动；可另存为保留。") : QStringLiteral("项目已保存"), 6000);
    return true;
}
bool MainWindow::openProject(const QString &path)
{
    if (m_mapFullscreen) setMapFullscreen(false);
    QString error;
    if (!ProjectStore::isProject(path, error)) { message(error); return false; }
    DeploymentXmlStore candidate(QDir(path).filePath("Data/Config"));
    DeploymentData checked;
    if (!candidate.load(checked, error)) { message(error); return false; }
    if (!AppPaths::initialize({}, path, &error)) { message(error); return false; }
    if (m_page) {
        m_page->setActive(false);
        delete m_page; m_page = nullptr;
        delete takeCentralWidget(); m_scene = nullptr;
    }
    m_project = QDir(path).absolutePath();
    auto *body = new QWidget(this);
    auto *layout = new QVBoxLayout(body); layout->setContentsMargins(4, 4, 4, 4); layout->setSpacing(4);
    m_bodyLayout = layout;
    m_scene = new SceneWidget(body);
    m_page = new ZqbsPage(m_scene, this);
    layout->addWidget(m_page->topPanel());
    auto *center = new QHBoxLayout;
    center->addWidget(m_page->leftPanel()); center->addWidget(m_scene, 1); center->addWidget(m_page->rightPanel());
    layout->addLayout(center, 1); layout->addWidget(m_page->bottomPanel());
    setCentralWidget(body);
    connect(m_page, &ZqbsPage::statusChanged, this, [this](const QString &text) { statusBar()->showMessage(text, 8000); });
    connect(m_page, &ZqbsPage::mapFullScreenRequested,
            this, &MainWindow::setMapFullscreen);
    m_page->setActive(true);
    m_presentationAction->setChecked(bool(m_presentation));
    return true;
}
void MainWindow::setMapFullscreen(bool fullScreen)
{
    if (fullScreen == m_mapFullscreen || !m_page || !m_scene || !m_bodyLayout)
        return;
    if (fullScreen) {
        m_mapFullscreenPreviousState = windowState();
        m_mapFullscreenGeometry = geometry();
        m_mapFullscreenMargins = m_bodyLayout->contentsMargins();
        m_mapFullscreenSpacing = m_bodyLayout->spacing();
        m_mapFullscreenVisibility.clear();
        const auto hideTemporarily = [this](QWidget *widget) {
            if (!widget) return;
            m_mapFullscreenVisibility.append({widget, widget->isVisible()});
            widget->hide();
        };
        hideTemporarily(m_page->topPanel());
        hideTemporarily(m_page->leftPanel());
        hideTemporarily(m_page->rightPanel());
        hideTemporarily(m_page->bottomPanel());
        hideTemporarily(m_windowHeader);
        hideTemporarily(statusBar());
        m_bodyLayout->setContentsMargins(0, 0, 0, 0);
        m_bodyLayout->setSpacing(0);
        m_mapFullscreen = true;
        m_page->setMapFullScreen(true);
        showFullScreen();
        m_scene->setFocus();
        return;
    }

    m_mapFullscreen = false;
    if (m_mapFullscreenPreviousState.testFlag(Qt::WindowFullScreen))
        showFullScreen();
    else if (m_mapFullscreenPreviousState.testFlag(Qt::WindowMaximized))
        showMaximized();
    else {
        showNormal();
        setGeometry(m_mapFullscreenGeometry);
    }
    m_bodyLayout->setContentsMargins(m_mapFullscreenMargins);
    m_bodyLayout->setSpacing(m_mapFullscreenSpacing);
    for (const auto &[widget, wasVisible] : m_mapFullscreenVisibility)
        if (widget) widget->setVisible(wasVisible);
    m_mapFullscreenVisibility.clear();
    m_page->setMapFullScreen(false);
    m_scene->setFocus();
}
void MainWindow::exportProject(bool openCopy)
{
    if (!save()) return;
    const auto destination = QFileDialog::getExistingDirectory(this, QStringLiteral("选择空目录保存项目副本"));
    if (destination.isEmpty()) return;
    QString error;
    if (!ProjectStore::clone(m_project, destination, error)) { message(error); return; }
    if (openCopy) {
        if (m_presentation) {
            auto previous = std::move(m_presentation);
            if (openProject(destination)) m_editProject.clear();
            else m_presentation = std::move(previous);
        }
        else openProject(destination);
    }
    message(QStringLiteral("项目副本已保存。仅包含项目配置和登记的资源；请在目标电脑检查外部地图、程序路径与 RTSP 地址。配置可能含您填写的服务凭据，公开前请人工脱敏。"));
}
void MainWindow::togglePresentation()
{
    if (!m_page) return;
    if (m_presentation) {
        const auto source = m_editProject;
        if (openProject(source)) { m_presentation.reset(); m_editProject.clear(); }
        return;
    }
    if (!save()) return;
    auto temporary = std::make_unique<QTemporaryDir>(
        QDir(AppPaths::temporaryDir()).filePath(QStringLiteral("presentation-XXXXXX")));
    QString error;
    if (!temporary->isValid() || !ProjectStore::clone(m_project, temporary->path(), error)) { message(error); return; }
    m_editProject = m_project;
    const auto destination = temporary->path(); m_presentation = std::move(temporary);
    if (!openProject(destination)) { m_presentation.reset(); m_editProject.clear(); }
}
}
