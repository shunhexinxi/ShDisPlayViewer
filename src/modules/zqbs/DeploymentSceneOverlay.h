#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <QWidget>
#include <QPointF>
#include <QPair>

class QPainter;

namespace Zqxxxt {

class SceneWidget;

class DeploymentSceneOverlay final : public QWidget
{
public:
    explicit DeploymentSceneOverlay(SceneWidget *scene);

    void beginMarkerPlacement();
    void beginAnnotationPlacement(const QString &kind);
    void beginMeasurement(const QString &kind);
    void beginBlackboard(const QString &tool, const QColor &color);
    void setBlackboardTool(const QString &tool);
    void setBlackboardColor(const QColor &color);
    void clearBlackboard();
    void closeBlackboard();
    void cancelInteraction();
    void refresh();

    // Read-only interaction state used by the deterministic widget contract
    // tests and by drawDraft; it never samples terrain or mutates the tool.
    [[nodiscard]] bool measurementInteractionActive() const noexcept;
    [[nodiscard]] bool navigationInteractionActive() const noexcept;
    [[nodiscard]] QVector<GeoCoordinate> measurementDraftPoints() const;
    [[nodiscard]] QVector<QPair<GeoCoordinate, GeoCoordinate>>
        measurementPreviewSegments() const;

    [[nodiscard]] bool beginControlPointDrag(const QPointF &position);
    [[nodiscard]] bool updateControlPointDrag(const QPointF &position);
    [[nodiscard]] bool endControlPointDrag();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class Interaction { Navigate, Annotation, Measurement, Blackboard };

    struct BlackboardShape
    {
        QString kind;
        QColor color;
        QVector<QPointF> points;
    };

    void finishMeasurement();
    void finishAnnotation();
    void drawEnvironment(QPainter &painter);
    void drawMeasurements(QPainter &painter);
    void drawMarkers(QPainter &painter);
    void drawDraft(QPainter &painter);
    void drawBlackboard(QPainter &painter);
    void drawControlPointDraggers(QPainter &painter);
    void publishInstruction();
    [[nodiscard]] QString instruction() const;
    [[nodiscard]] double measurementValue() const;

    SceneWidget *m_scene = nullptr;
    Interaction m_interaction = Interaction::Navigate;
    QString m_annotationKind = QStringLiteral("point");
    QString m_measurementKind = QStringLiteral("space-distance");
    QVector<GeoCoordinate> m_draftPoints;
    QPointF m_measurementPressPosition;
    Qt::MouseButton m_measurementPressButton = Qt::NoButton;
    bool m_measurementStarted = false;
    bool m_suppressMeasurementRelease = false;
    QVector<BlackboardShape> m_blackboardShapes;
    BlackboardShape m_blackboardDraft;
    QString m_blackboardTool = QStringLiteral("freehand");
    QColor m_blackboardColor = QColor(QStringLiteral("#ffef64"));
    bool m_blackboardVisible = false;
    bool m_blackboardDrawing = false;
    enum class DraggerAxis { None, Free, X, Y, Z };
    DraggerAxis m_dragAxis = DraggerAxis::None;
    int m_dragPointIndex = -1;
    QPointF m_dragPressPosition;
    QPointF m_dragOriginScreen;
    GeoCoordinate m_dragOriginCoordinate;
    QString m_activeMarkerId;
    int m_activeControlPointIndex = -1;
};

} // namespace Zqxxxt
