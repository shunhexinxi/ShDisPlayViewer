#include "modules/zqts/MapViewTools.h"

#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPropertyAnimation>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double EarthRadiusMeters = 6378137.0;
constexpr int NavigatorZoomTrackTop = 171;
constexpr int NavigatorZoomTrackBottom = 279;
constexpr int NavigatorZoomSliderHeight = 11;
constexpr int NavigatorZoomSliderMinTop = NavigatorZoomTrackTop;
constexpr int NavigatorZoomSliderMaxTop =
    NavigatorZoomTrackBottom - NavigatorZoomSliderHeight;
constexpr double NavigatorMinimumRangeMeters = 10.0;
constexpr double NavigatorMaximumRangeMeters = 100000000.0;

double radians(double degrees)
{
    return degrees * Pi / 180.0;
}

QPolygonF arrow(const QPointF &center, double angleDegrees, double radius)
{
    QPolygonF result;
    const double angle = radians(angleDegrees);
    const QPointF direction(std::cos(angle), std::sin(angle));
    const QPointF side(-direction.y(), direction.x());
    result << center + direction * radius
           << center - direction * radius * 0.50 + side * radius * 0.55
           << center - direction * radius * 0.50 - side * radius * 0.55;
    return result;
}

} // namespace

namespace Zqxxxt {

double geographicDistanceMeters(double longitude1, double latitude1,
                                double longitude2, double latitude2)
{
    const double lat1 = radians(latitude1);
    const double lat2 = radians(latitude2);
    const double deltaLatitude = lat2 - lat1;
    const double deltaLongitude = radians(longitude2 - longitude1);
    const double value = std::sin(deltaLatitude / 2.0)
            * std::sin(deltaLatitude / 2.0)
        + std::cos(lat1) * std::cos(lat2)
            * std::sin(deltaLongitude / 2.0)
            * std::sin(deltaLongitude / 2.0);
    return 2.0 * EarthRadiusMeters
        * std::atan2(std::sqrt(value), std::sqrt(std::max(0.0, 1.0 - value)));
}

QString formatMapScaleDistance(double meters)
{
    if (!std::isfinite(meters) || meters <= 0.0)
        return {};
    if (meters >= 1000.0)
        return QStringLiteral("%1Km").arg(meters / 1000.0, 0, 'f', 1);
    return QStringLiteral("%1m").arg(qMax(1.0, meters), 0, 'f', 0);
}

QImage makeInfraredImage(const QImage &source, double intensity)
{
    if (source.isNull())
        return {};
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const double gain = 0.75 + std::clamp(intensity, 0.0, 1.0) * 0.85;
    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const int luminance = qGray(line[x]);
            const int boosted = qBound(0,
                int((luminance - 38) * gain + 42), 255);
            // The reference CSulIR uses a white-hot intensity pass. Retain a
            // slight warm highlight so high-temperature areas remain clear.
            const int red = qBound(0, boosted + (boosted > 190 ? 18 : 0), 255);
            const int blue = qBound(0, boosted - (boosted > 190 ? 12 : 0), 255);
            line[x] = qRgba(red, boosted, blue, qAlpha(line[x]));
        }
    }
    return result;
}

AirViewOverlay::AirViewOverlay(QWidget *parent)
    : QWidget(parent),
      m_worldMap(QStringLiteral(":/deployment/tools/airview_world.jpg"))
{
    setObjectName(QStringLiteral("mapAirViewOverlay"));
    setFixedSize(280, 140);
    setCursor(Qt::CrossCursor);
    setToolTip(QStringLiteral("双击鸟瞰图可快速定位"));
}

void AirViewOverlay::setViewpoint(double longitude, double latitude,
                                  double heading, double rangeMeters)
{
    m_longitude = std::clamp(longitude, -180.0, 180.0);
    m_latitude = std::clamp(latitude, -90.0, 90.0);
    m_heading = heading;
    m_rangeMeters = std::max(1.0, rangeMeters);
    update();
}

void AirViewOverlay::setLocateHandler(
    std::function<void(double, double)> handler)
{
    m_locateHandler = std::move(handler);
}

QRectF AirViewOverlay::mapRect() const
{
    return rect().adjusted(3, 3, -3, -3);
}

void AirViewOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF target = mapRect();
    painter.fillRect(rect(), QColor(5, 24, 43, 235));
    if (!m_worldMap.isNull())
        painter.drawPixmap(target.toRect(), m_worldMap);
    else
        painter.fillRect(target, QColor(24, 67, 91));
    painter.setPen(QPen(QColor(205, 220, 229, 230), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(target);

    const QPointF center(target.left() + (m_longitude + 180.0) / 360.0
                                    * target.width(),
                         target.top() + (90.0 - m_latitude) / 180.0
                                    * target.height());
    const double longitudeSpan = std::clamp(m_rangeMeters / 111320.0,
                                            1.0, 120.0);
    const double latitudeSpan = longitudeSpan * 0.55;
    const double focusWidth = std::clamp(longitudeSpan / 360.0
                                         * target.width(), 10.0, 90.0);
    const double focusHeight = std::clamp(latitudeSpan / 180.0
                                          * target.height(), 7.0, 55.0);
    QRectF focus(center.x() - focusWidth / 2.0,
                 center.y() - focusHeight / 2.0,
                 focusWidth, focusHeight);
    focus = focus.intersected(target);
    painter.setPen(QPen(QColor(255, 64, 49, 235), 2));
    painter.setBrush(QColor(255, 45, 32, 38));
    painter.drawRect(focus);
    painter.translate(center);
    painter.rotate(m_heading);
    painter.setBrush(QColor(255, 230, 120));
    painter.drawPolygon(arrow(QPointF(), -90.0, 7.0));
}

void AirViewOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QRectF target = mapRect();
    if (!target.contains(event->position()) || !m_locateHandler) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const double longitude = (event->position().x() - target.left())
        / target.width() * 360.0 - 180.0;
    const double latitude = 90.0 - (event->position().y() - target.top())
        / target.height() * 180.0;
    m_locateHandler(longitude, latitude);
    event->accept();
}

NavigatorOverlay::NavigatorOverlay(QWidget *parent)
    : QWidget(parent),
      m_compassEdge(QStringLiteral(":/deployment/navigator/Compass_Edge.png")),
      m_compassNeedle(QStringLiteral(":/deployment/navigator/Compass.png")),
      m_directionEdge(QStringLiteral(":/deployment/navigator/DirectDiskEdge.png")),
      m_directionMove(QStringLiteral(":/deployment/navigator/DirectDiskMove.png")),
      m_directionRotate(QStringLiteral(":/deployment/navigator/DirectDiskRotate.png")),
      m_zoomEdge(QStringLiteral(":/deployment/navigator/ZoomEdge.png")),
      m_zoomIn(QStringLiteral(":/deployment/navigator/ZoomIn.png")),
      m_zoomOut(QStringLiteral(":/deployment/navigator/ZoomOut.png")),
      m_zoomPole(QStringLiteral(":/deployment/navigator/ZomPole.png")),
      m_zoomSlider(QStringLiteral(":/deployment/navigator/ZoomSlider.png"))
{
    setObjectName(QStringLiteral("mapNavigatorOverlay"));
    setFixedSize(90, 360);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setToolTip(QStringLiteral("双击指北针恢复正北；方向盘可平移、旋转和俯仰地图"));
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.34);
    setGraphicsEffect(m_opacityEffect);
    m_opacityAnimation = new QPropertyAnimation(
        m_opacityEffect, QByteArrayLiteral("opacity"), this);
    m_opacityAnimation->setDuration(150);
    m_opacityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_repeatTimer.setSingleShot(false);
    m_repeatTimer.setInterval(110);
    QObject::connect(&m_repeatTimer, &QTimer::timeout, this,
                     [this] { invokeCommand(); });
}

void NavigatorOverlay::setHeading(double degrees)
{
    m_heading = degrees;
    update();
}

void NavigatorOverlay::setRangeMeters(double meters)
{
    if (m_zoomDragging || !std::isfinite(meters) || meters <= 0.0)
        return;
    const double clamped = std::clamp(
        meters, NavigatorMinimumRangeMeters, NavigatorMaximumRangeMeters);
    const double normalized = std::log(
        clamped / NavigatorMinimumRangeMeters)
        / std::log(NavigatorMaximumRangeMeters / NavigatorMinimumRangeMeters);
    const int sliderTop = qRound(NavigatorZoomSliderMinTop
        + normalized * (NavigatorZoomSliderMaxTop - NavigatorZoomSliderMinTop));
    if (m_zoomSliderTop == sliderTop)
        return;
    m_zoomSliderTop = sliderTop;
    update();
}

void NavigatorOverlay::setCommandHandler(
    std::function<void(NavigatorCommand)> handler)
{
    m_commandHandler = std::move(handler);
}

void NavigatorOverlay::setZoomHandler(std::function<void(double)> handler)
{
    m_zoomHandler = std::move(handler);
}

int NavigatorOverlay::zoomSliderTop() const noexcept
{
    return m_zoomSliderTop;
}

void NavigatorOverlay::animateVisibility(bool fullyVisible)
{
    if (!m_opacityEffect || !m_opacityAnimation)
        return;
    m_opacityAnimation->stop();
    m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
    m_opacityAnimation->setEndValue(fullyVisible ? 1.0 : 0.34);
    m_opacityAnimation->start();
}

void NavigatorOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF compassRect(0.0, 0.0, 90.0, 90.0);
    painter.save();
    painter.translate(compassRect.center());
    painter.rotate(-m_heading);
    painter.translate(-compassRect.center());
    if (!m_compassEdge.isNull())
        painter.drawPixmap(compassRect.toRect(), m_compassEdge);
    if (!m_compassNeedle.isNull())
        painter.drawPixmap(QRect(14, 14, 62, 62), m_compassNeedle);
    painter.restore();

    const QRect rotateRect(9, 97, 72, 54);
    if (!m_directionEdge.isNull())
        painter.drawPixmap(rotateRect, m_directionEdge);
    if (!m_directionRotate.isNull())
        painter.drawPixmap(rotateRect, m_directionRotate);

    const QRect zoomRect(36, 158, 18, 135);
    if (!m_zoomEdge.isNull())
        painter.drawPixmap(zoomRect, m_zoomEdge);
    if (!m_zoomPole.isNull())
        painter.drawPixmap(QRect(43, 171, 4, 108), m_zoomPole);
    if (!m_zoomIn.isNull())
        painter.drawPixmap(QRect(36, 158, 18, 14), m_zoomIn);
    if (!m_zoomOut.isNull())
        painter.drawPixmap(QRect(36, 279, 18, 14), m_zoomOut);
    if (!m_zoomSlider.isNull())
        painter.drawPixmap(QRect(36, m_zoomSliderTop, 18,
                                 NavigatorZoomSliderHeight), m_zoomSlider);

    const QRect moveRect(9, 299, 72, 54);
    if (!m_directionEdge.isNull())
        painter.drawPixmap(moveRect, m_directionEdge);
    if (!m_directionMove.isNull())
        painter.drawPixmap(moveRect, m_directionMove);
}

void NavigatorOverlay::enterEvent(QEnterEvent *event)
{
    animateVisibility(true);
    QWidget::enterEvent(event);
}

NavigatorCommand NavigatorOverlay::commandAt(const QPointF &position,
                                               bool *valid) const
{
    *valid = true;
    if (QRectF(9, 97, 72, 54).contains(position)) {
        const QPointF delta = position - QPointF(45, 124);
        if (std::abs(delta.x()) >= std::abs(delta.y()))
            return delta.x() < 0.0 ? NavigatorCommand::RotateLeft
                                   : NavigatorCommand::RotateRight;
        return delta.y() < 0.0 ? NavigatorCommand::PitchUp
                               : NavigatorCommand::PitchDown;
    }
    if (QRectF(27, 153, 36, 18).contains(position))
        return NavigatorCommand::ZoomIn;
    if (QRectF(27, 279, 36, 18).contains(position))
        return NavigatorCommand::ZoomOut;
    if (QRectF(9, 299, 72, 54).contains(position)) {
        const QPointF delta = position - QPointF(45, 326);
        if (std::abs(delta.x()) >= std::abs(delta.y()))
            return delta.x() < 0.0 ? NavigatorCommand::PanWest
                                   : NavigatorCommand::PanEast;
        return delta.y() < 0.0 ? NavigatorCommand::PanNorth
                               : NavigatorCommand::PanSouth;
    }
    *valid = false;
    return NavigatorCommand::North;
}

void NavigatorOverlay::invokeCommand()
{
    if (m_commandHandler)
        m_commandHandler(m_activeCommand);
}

void NavigatorOverlay::updateZoomSlider(double pointerY, bool applyRange)
{
    const int sliderTop = qBound(
        NavigatorZoomSliderMinTop,
        qRound(pointerY - NavigatorZoomSliderHeight * 0.5),
        NavigatorZoomSliderMaxTop);
    if (m_zoomSliderTop != sliderTop) {
        m_zoomSliderTop = sliderTop;
        update();
    }
    if (!applyRange || !m_zoomHandler)
        return;
    const double normalized = double(m_zoomSliderTop - NavigatorZoomSliderMinTop)
        / double(NavigatorZoomSliderMaxTop - NavigatorZoomSliderMinTop);
    const double range = NavigatorMinimumRangeMeters * std::pow(
        NavigatorMaximumRangeMeters / NavigatorMinimumRangeMeters, normalized);
    m_zoomHandler(range);
}

void NavigatorOverlay::mousePressEvent(QMouseEvent *event)
{
    bool valid = false;
    m_activeCommand = commandAt(event->position(), &valid);
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (!valid && QRectF(27, NavigatorZoomTrackTop, 36,
                         NavigatorZoomTrackBottom - NavigatorZoomTrackTop)
                         .contains(event->position())) {
        m_pressed = true;
        m_zoomDragging = true;
        m_repeatTimer.stop();
        updateZoomSlider(event->position().y(), true);
        event->accept();
        return;
    }
    if (!valid) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressed = true;
    invokeCommand();
    m_repeatTimer.start();
    event->accept();
}

void NavigatorOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
    m_zoomDragging = false;
    m_repeatTimer.stop();
    event->accept();
}

void NavigatorOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_zoomDragging) {
        updateZoomSlider(event->position().y(), true);
        event->accept();
        return;
    }
    bool valid = false;
    const NavigatorCommand command = commandAt(event->position(), &valid);
    if (valid) {
        m_activeCommand = command;
        invokeCommand();
    }
    event->accept();
}

void NavigatorOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (QRectF(0, 0, 90, 90).contains(event->position())
        && event->button() == Qt::LeftButton) {
        m_activeCommand = NavigatorCommand::North;
        invokeCommand();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void NavigatorOverlay::leaveEvent(QEvent *event)
{
    m_pressed = false;
    m_zoomDragging = false;
    m_repeatTimer.stop();
    animateVisibility(false);
    QWidget::leaveEvent(event);
}

ScaleBarOverlay::ScaleBarOverlay(QWidget *parent)
    : QWidget(parent),
      m_scaleImage(QStringLiteral(":/deployment/scale/mapScale.png"))
{
    setObjectName(QStringLiteral("mapScaleBarOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    // The application-wide QWidget skin otherwise paints the blue panel
    // behind this child widget.  Keep only the scale texture and text so the
    // live map remains visible through the whole overlay rectangle.
    setStyleSheet(QStringLiteral(
        "#mapScaleBarOverlay { background:transparent; border:none; }"));
    setFixedSize(300, 50);
}

void ScaleBarOverlay::setSegmentDistance(double meters)
{
    if (meters > 0.0 && meters < 1.0)
        meters = 1.0;
    if (qFuzzyCompare(m_segmentDistanceMeters, meters))
        return;
    m_segmentDistanceMeters = meters;
    update();
}

double ScaleBarOverlay::segmentDistance() const noexcept
{
    return m_segmentDistanceMeters;
}

void ScaleBarOverlay::paintEvent(QPaintEvent *)
{
    if (!std::isfinite(m_segmentDistanceMeters)
        || m_segmentDistanceMeters <= 0.0) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!m_scaleImage.isNull())
        painter.drawPixmap(QRect(30, 25, 240, 21), m_scaleImage);
    painter.setPen(QPen(Qt::black, 3));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
    const QString label = formatMapScaleDistance(m_segmentDistanceMeters);
    painter.drawText(QRect(190, 2, 100, 20),
                     Qt::AlignCenter, label);
    painter.setPen(Qt::white);
    painter.drawText(QRect(190, 2, 100, 20),
                     Qt::AlignCenter, label);
}

InfraredOverlay::InfraredOverlay(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("mapInfraredOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void InfraredOverlay::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void InfraredOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), Qt::black);
    if (!m_frame.isNull())
        painter.drawImage(rect(), m_frame);
}

ZoomFocusOverlay::ZoomFocusOverlay(QWidget *parent)
    : QWidget(parent),
      m_focusIcon(QStringLiteral(":/deployment/mouse/center.png"))
{
    setObjectName(QStringLiteral("mapZoomFocusOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(48, 48);
}

void ZoomFocusOverlay::setFocusPosition(const QPointF &position)
{
    move(qRound(position.x() - width() / 2.0),
         qRound(position.y() - height() / 2.0));
}

void ZoomFocusOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!m_focusIcon.isNull())
        painter.drawPixmap(rect(), m_focusIcon);
}

} // namespace Zqxxxt
