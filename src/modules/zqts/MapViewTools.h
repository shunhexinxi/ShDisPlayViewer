#pragma once

#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>

class QEvent;
class QEnterEvent;
class QGraphicsOpacityEffect;
class QMouseEvent;
class QPaintEvent;
class QPropertyAnimation;

namespace Zqxxxt {

struct GeoCoordinate;

enum class NavigatorCommand
{
    North,
    PanNorth,
    PanSouth,
    PanWest,
    PanEast,
    RotateLeft,
    RotateRight,
    PitchUp,
    PitchDown,
    ZoomIn,
    ZoomOut
};

[[nodiscard]] double geographicDistanceMeters(double longitude1,
                                               double latitude1,
                                               double longitude2,
                                               double latitude2);
[[nodiscard]] QString formatMapScaleDistance(double meters);
[[nodiscard]] QImage makeInfraredImage(const QImage &source,
                                       double intensity = 0.6);

class AirViewOverlay final : public QWidget
{
public:
    explicit AirViewOverlay(QWidget *parent = nullptr);

    void setViewpoint(double longitude, double latitude, double heading,
                      double rangeMeters);
    void setLocateHandler(std::function<void(double, double)> handler);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QRectF mapRect() const;

    QPixmap m_worldMap;
    std::function<void(double, double)> m_locateHandler;
    double m_longitude = 0.0;
    double m_latitude = 0.0;
    double m_heading = 0.0;
    double m_rangeMeters = 1000000.0;
};

class NavigatorOverlay final : public QWidget
{
public:
    explicit NavigatorOverlay(QWidget *parent = nullptr);

    void setHeading(double degrees);
    void setRangeMeters(double meters);
    void setCommandHandler(std::function<void(NavigatorCommand)> handler);
    void setZoomHandler(std::function<void(double)> handler);
    [[nodiscard]] int zoomSliderTop() const noexcept;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    NavigatorCommand commandAt(const QPointF &position, bool *valid) const;
    void invokeCommand();
    void updateZoomSlider(double pointerY, bool applyRange);
    void animateVisibility(bool fullyVisible);

    std::function<void(NavigatorCommand)> m_commandHandler;
    std::function<void(double)> m_zoomHandler;
    QTimer m_repeatTimer;
    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
    QPropertyAnimation *m_opacityAnimation = nullptr;
    NavigatorCommand m_activeCommand = NavigatorCommand::North;
    QPixmap m_compassEdge;
    QPixmap m_compassNeedle;
    QPixmap m_directionEdge;
    QPixmap m_directionMove;
    QPixmap m_directionRotate;
    QPixmap m_zoomEdge;
    QPixmap m_zoomIn;
    QPixmap m_zoomOut;
    QPixmap m_zoomPole;
    QPixmap m_zoomSlider;
    double m_heading = 0.0;
    int m_zoomSliderTop = 221;
    bool m_pressed = false;
    bool m_zoomDragging = false;
};

class ScaleBarOverlay final : public QWidget
{
public:
    explicit ScaleBarOverlay(QWidget *parent = nullptr);

    static constexpr int segmentWidth() noexcept { return 60; }
    static constexpr int sampleStartX() noexcept { return 210; }
    void setSegmentDistance(double meters);
    [[nodiscard]] double segmentDistance() const noexcept;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_scaleImage;
    double m_segmentDistanceMeters = 0.0;
};

class InfraredOverlay final : public QWidget
{
public:
    explicit InfraredOverlay(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
};

class ZoomFocusOverlay final : public QWidget
{
public:
    explicit ZoomFocusOverlay(QWidget *parent = nullptr);

    void setFocusPosition(const QPointF &position);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_focusIcon;
};

} // namespace Zqxxxt
