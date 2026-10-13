#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QMouseEvent>
#include <QKeyEvent>
#include <functional>

namespace Zqxxxt {

// Paint only the small phase block on timer ticks; no stylesheet replacement or
// timeline/tree reconstruction is needed for the continuous progress animation.
class DynamicTimelineButton final : public QPushButton
{
public:
    enum class SeekGesture { Begin, Move, End };
    std::function<void(const QPoint &, SeekGesture)> seekRequested;

    DynamicTimelineButton(const QString &text, QWidget *parent = nullptr)
        : QPushButton(text, parent)
    {
        setMinimumSize(120, 48);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setProperty("timelineStep", true);
    }

    void setPlaybackProgress(double progress, qint64 elapsedMs, qint64 durationMs, bool current)
    {
        m_progress = qBound(0.0, progress, 1.0);
        m_elapsedMs = qBound(qint64(0), elapsedMs, durationMs);
        m_durationMs = durationMs;
        m_current = current;
        setProperty("timelineProgress", m_progress);
        setProperty("current", current);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || !seekRequested) {
            QPushButton::mousePressEvent(event);
            return;
        }
        m_dragging = true;
        setFocus(Qt::MouseFocusReason);
        setDown(true);
        seekRequested(event->globalPosition().toPoint(), SeekGesture::Begin);
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_dragging || !seekRequested) { QPushButton::mouseMoveEvent(event); return; }
        seekRequested(event->globalPosition().toPoint(), SeekGesture::Move);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || !m_dragging || !seekRequested) {
            QPushButton::mouseReleaseEvent(event);
            return;
        }
        m_dragging = false;
        setDown(false);
        seekRequested(event->globalPosition().toPoint(), SeekGesture::End);
        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        double target = m_progress;
        if (event->key() == Qt::Key_Left) target -= 0.02;
        else if (event->key() == Qt::Key_Right) target += 0.02;
        else if (event->key() == Qt::Key_Home) target = 0;
        else if (event->key() == Qt::Key_End) target = 1;
        else { QPushButton::keyPressEvent(event); return; }
        if (seekRequested) {
            const QPoint position = mapToGlobal(QPoint(
                qRound(qBound(0.0, target, 1.0) * qMax(1, width() - 1)), height() / 2));
            seekRequested(position, SeekGesture::Begin);
            seekRequested(position, SeekGesture::End);
        }
        event->accept();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath shape; shape.addRoundedRect(bounds, 2, 2);
        painter.fillPath(shape, QColor(QStringLiteral("#0a355d")));
        painter.save();
        painter.setClipPath(shape);
        painter.fillRect(QRectF(bounds.left(), bounds.top(), bounds.width() * m_progress, bounds.height()),
                         QColor(QStringLiteral("#126d94")));
        painter.restore();
        painter.setPen(QPen(QColor(m_current || hasFocus() ? QStringLiteral("#5ae6ff")
                                                          : QStringLiteral("#326d98")), 1));
        painter.drawPath(shape);
        QFont labelFont = font(); labelFont.setBold(m_current);
        painter.setFont(labelFont);
        painter.setPen(QColor(m_current ? QStringLiteral("#ffffff") : QStringLiteral("#9bc7df")));
        const QRect content = rect().adjusted(11, 4, -11, -4);
        const int lineHeight = content.height() / 2;
        const QString number = text().section(QLatin1Char('\n'), 0, 0);
        const QString name = text().section(QLatin1Char('\n'), 1);
        painter.drawText(QRect(content.left(), content.top(), content.width(), lineHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, number);
        const QString timing = QStringLiteral("%1 / %2 秒")
            .arg(m_elapsedMs / 1000.0, 0, 'f', 1).arg(m_durationMs / 1000.0, 0, 'f', 0);
        if (content.width() > 150)
            painter.drawText(QRect(content.left(), content.top(), content.width(), lineHeight),
                             Qt::AlignRight | Qt::AlignVCenter, timing);
        painter.drawText(QRect(content.left(), content.top() + lineHeight, content.width(), lineHeight),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(name, Qt::ElideRight, content.width()));
    }

private:
    double m_progress = 0.0;
    qint64 m_elapsedMs = 0;
    qint64 m_durationMs = 1000;
    bool m_current = false;
    bool m_dragging = false;
};

} // namespace Zqxxxt
