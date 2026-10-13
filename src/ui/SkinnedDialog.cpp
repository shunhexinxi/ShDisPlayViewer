#include "ui/SkinnedDialog.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

QString dialogControlStyleSheet()
{
    return QStringLiteral(R"QSS(
        QDialog[skinnedDialog="true"] { color:#eaf6ff; background:#123f70;
            border:1px solid #4d91c4; }
        QLineEdit, QPlainTextEdit, QTextEdit, QTextBrowser,
        QSpinBox, QDoubleSpinBox, QDateEdit, QTimeEdit, QDateTimeEdit,
        QComboBox, QFontComboBox {
            color:#f1f9ff; background:#0d3967; border:1px solid #4d91c4;
            border-radius:4px; padding:5px 8px; min-height:20px;
            selection-background-color:#2786ca;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
        QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QTimeEdit:focus,
        QDateTimeEdit:focus, QComboBox:focus, QFontComboBox:focus {
            background:#0b315b; border-color:#72e1ff;
        }
        QComboBox QAbstractItemView, QFontComboBox QAbstractItemView,
        QTableWidget, QTreeWidget, QListWidget {
            color:#eaf6ff; background:#0d3967; border:1px solid #4d91c4;
            alternate-background-color:#124777; selection-background-color:#267fc0;
        }
        QHeaderView::section { color:#f4fbff; background:#174f83;
            border:0; border-right:1px solid #477fa7;
            border-bottom:1px solid #69acd5; padding:8px; }
        QPushButton { color:#f5fbff;
            background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2478ba,stop:1 #15558f);
            border:1px solid #67b9e7; border-radius:4px;
            min-height:28px; min-width:72px; padding:3px 12px; }
        QPushButton:hover { background:#3196d6; border-color:#8ce4ff; }
        QPushButton:disabled { color:#809ab0; background:#285274;
            border-color:#466d8b; }
    )QSS");
}

class DialogContentScrollArea final : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;

    QSize sizeHint() const override
    {
        if (const QWidget *body = widget())
            return body->sizeHint() + QSize(2 * frameWidth(), 2 * frameWidth());
        return QScrollArea::sizeHint();
    }
};

class SkinnedTitleBar final : public QFrame
{
public:
    explicit SkinnedTitleBar(QWidget *parent)
        : QFrame(parent)
    {
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        event->accept();
    }
};

}

namespace Zqxxxt {

SkinnedDialog::SkinnedDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    setProperty("skinnedDialog", true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    QDialog::setWindowTitle(title);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *titleBar = new SkinnedTitleBar(this);
    titleBar->setObjectName(QStringLiteral("skinDialogTitleBar"));
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet(QStringLiteral(
        "QFrame#skinDialogTitleBar{"
        "background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #2f7fc9,stop:1 #154a82);"
        "border:0;border-bottom:1px solid #8acfff;}"));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(7, 0, 4, 0);
    titleLayout->setSpacing(7);

    auto *icon = new QLabel(titleBar);
    icon->setObjectName(QStringLiteral("skinDialogTitleIcon"));
    icon->setFixedSize(25, 25);
    QPixmap logo(QStringLiteral(":/legacy/images/menu/menulogoimg.png"));
    if (logo.isNull() && parent)
        logo = parent->windowIcon().pixmap(icon->size());
    if (!logo.isNull())
        icon->setPixmap(logo.scaled(icon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral("background:transparent;border:0;"));
    titleLayout->addWidget(icon);

    m_titleLabel = new QLabel(title, titleBar);
    m_titleLabel->setObjectName(QStringLiteral("skinDialogTitle"));
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color:white;background:transparent;border:0;font-size:14px;font-weight:600;"));
    titleLayout->addWidget(m_titleLabel, 1);

    auto *close = new QToolButton(titleBar);
    close->setObjectName(QStringLiteral("skinDialogClose"));
    close->setFixedSize(28, 28);
    close->setFocusPolicy(Qt::NoFocus);
    close->setToolTip(QStringLiteral("关闭"));
    close->setStyleSheet(QStringLiteral(
        "QToolButton{border:0;background:transparent;image:url(:/legacy/images/window/close_normal.png);}"
        "QToolButton:hover{image:url(:/legacy/images/window/close_hover.png);}"
        "QToolButton:pressed{image:url(:/legacy/images/window/close_press.png);}"));
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(close);
    root->addWidget(titleBar);

    auto *body = new QWidget(this);
    body->setObjectName(QStringLiteral("skinDialogBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(QStringLiteral(
        "QWidget#skinDialogBody{background:#123f70;border:1px solid #4d91c4;border-top:0;}"));
    m_contentLayout = new QVBoxLayout(body);
    m_contentLayout->setContentsMargins(14, 12, 14, 14);
    m_contentLayout->setSpacing(8);
    auto *scroll = new DialogContentScrollArea(this);
    scroll->setObjectName(QStringLiteral("skinDialogScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    scroll->setWidgetResizable(true);
    scroll->setWidget(body);
    root->addWidget(scroll, 1);
}

void SkinnedDialog::setSkinTitle(const QString &title)
{
    QDialog::setWindowTitle(title);
    m_titleLabel->setText(title);
}

QVBoxLayout *SkinnedDialog::contentLayout() const
{
    return m_contentLayout;
}

void SkinnedDialog::showEvent(QShowEvent *event)
{
    if (!m_unifiedStyleApplied) {
        m_unifiedStyleApplied = true;
        setStyleSheet(styleSheet() + dialogControlStyleSheet());
    }
    QDialog::showEvent(event);
    if (m_initialGeometryApplied)
        return;
    m_initialGeometryApplied = true;

    const QPoint anchor = parentWidget()
        ? parentWidget()->mapToGlobal(parentWidget()->rect().center())
        : frameGeometry().center();
    QScreen *screen = QGuiApplication::screenAt(anchor);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry().adjusted(16, 16, -16, -16);
    resize(size().boundedTo(available.size()));
    move(available.center() - rect().center());
}

}
