#pragma once

#include "modules/zqbs/DeploymentTypes.h"
#include "modules/zqbs/DeploymentXmlStore.h"

#include <QJsonObject>
#include <QHash>
#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSet>

class QButtonGroup;
class QDialog;
class QEvent;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPoint;
class QPushButton;
class QSlider;
class QTextBrowser;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace Zqxxxt {
class EnvironmentSettingsDialog;

class SceneWidget;

// Independent rehearsal controls; no IMS session or authorization dependency.
class ZqbsPage final : public QObject
{
    Q_OBJECT

public:
    explicit ZqbsPage(SceneWidget *scene,
                      QObject *parent = nullptr);

    [[nodiscard]] QWidget *leftPanel() const;
    [[nodiscard]] QWidget *rightPanel() const;
    [[nodiscard]] QWidget *topPanel() const;
    [[nodiscard]] QWidget *bottomPanel() const;
    // The cumulative dynamic prefix currently submitted to the shared map.
    [[nodiscard]] const QVector<DeploymentMarker> &activeDynamicMarkers() const { return m_dynamicMarkers; }

    void setActive(bool active);
    void setMapFullScreen(bool fullScreen);
    void activate();
    bool saveProject(QString &error) { return m_store.saveAll(m_data, error); }
    const DeploymentData &data() const { return m_data; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void statusChanged(const QString &message);
    void mapFullScreenRequested(bool fullScreen);

private:
    enum class Domain { Fixed = 0, Normal = 1, Dynamic = 2 };
    enum class PendingPlacement { None, FixedMarker, NormalMarker, DynamicMarker };
    enum class PlanState { Stopped, Preview, Playing, Paused, Completed };

    void createPanels();
    void createSceneOverlays();
    QWidget *createLeftPanel();
    QWidget *createRightPanel();
    QWidget *createTopPanel();
    QWidget *createBottomPanel();
    QPushButton *createDomainButton(const QString &text, Domain domain);
    QPushButton *createToolButton(const QString &text);

    void setDomain(Domain domain);
    void rebuildTree();
    void rebuildTimeline();
    void updateTimelineProgress();
    void updateInspector(QTreeWidgetItem *item);
    void updateScene();
    void updateDomainChrome();
    void updateBottomSummary();
    void positionSceneOverlays();
    void updateStatus(const QString &message, bool error = false);
    void saveFixed();
    void saveNormal();
    void saveFixedAnnotations();
    void editNormalFolder(const QString &folderId = {}, const QString &parentId = {});
    void reloadXml();

    void showLayerMenu();
    void addLayer(const QString &presetKind = {});
    void showAnnotationMenu();
    void addMarker(const QString &presetType = {}, const QString &category = {},
                   bool keepFixedDomain = false, const QString &normalFolderId = {});
    void showSceneTreeContextMenu(const QPoint &position);
    void addSceneFolder(bool plot);
    void editSceneFolder(const QString &folderId);
    void clearSceneBranch(const QString &branchType, const QString &branchId = {});
    void editViewpoint(const QString &viewpointId = {});
    void showMeasurementMenu();
    void addMeasurement(const QString &kind);
    void addPlan();
    void editDynamicPlan(QString planId = {});
    void editDynamicStep(QString planId, QString stepId = {});
    bool prepareDynamicEdit();
    bool commitDynamicPlans(const QVector<DeploymentPlan> &plans);
    void refreshDynamicData();
    void selectDynamicTreeItem(const QString &type, const QString &id, const QString &aux = {});
    void moveSelectedDynamicMarkerUp(const QString &planId, const QString &stepId, const QString &markerId);
    void removeDynamicItem(const QString &type, const QString &id, const QString &aux);
    void addStep();
    void addDynamicMarker(const QString &kind = {});
    void removeSelected();
    void editSelected();
    void showEnvironmentMenu();
    void configureEnvironment(int initialTab = 0);
    void saveViewpoint();
    void showViewpointMenu();
    void showFlightDialog();
    void showViewpointLocator();
    void showToolMenu();
    void showBlackboardPanel();
    void showMeasurementDetails(const DeploymentMeasurement &measurement);

    void receiveMapPoint(const GeoCoordinate &coordinate);
    void receiveAnnotation(const QString &kind,
                           const QVector<GeoCoordinate> &points);
    void receiveMeasurement(const QString &kind,
                            const QVector<GeoCoordinate> &points,
                            double value);
    void treeItemChanged(QTreeWidgetItem *item, int column);
    void treeSelectionChanged();
    void treeItemActivated(QTreeWidgetItem *item, int column);

    void updateAssociatedMarkers(bool force = false);
    void handleMarkerTrigger(const QString &markerId, const QString &trigger,
                             const QPoint &globalPosition);
    void executeMarkerAction(const QJsonObject &action, bool automatic = false);
    void executeAutomaticMarkerActions(const DeploymentPlanStep &step,
                                       qint64 elapsedMs, bool consumeOnly = false);
    bool approveAutomaticPrograms(const DeploymentPlan &plan);

    void playPlan();
    void pausePlan();
    void stopPlan();
    void showPlanStep(int index, bool startTimer);
    void nextPlanStep();
    void previousPlanStep();
    void finishPlan();
    void scrubTimeline(const QPoint &globalPosition, int gesture);
    void seekPlanTime(int index, qint64 elapsedMs);
    void focusDynamicStep(const DeploymentPlanStep &step, qint64 elapsedMs,
                          bool force = false, bool instant = false);
    void advancePlanPlayback();
    void updateDynamicPlaybackMarkers(const DeploymentPlanStep &step, qint64 elapsedMs);
    void resetPlanPlayback(PlanState state);
    [[nodiscard]] qint64 planElapsedMs() const;
    [[nodiscard]] DeploymentPlan *selectedPlan();
    [[nodiscard]] DeploymentPlanStep *selectedStep();
    [[nodiscard]] const DeploymentBindingRule *bindingRule(const QString &type) const;

    SceneWidget *m_scene = nullptr;
    DeploymentXmlStore m_store;
    DeploymentData m_data;
    QVector<DeploymentMarker> m_liveMarkers;
    QVector<DeploymentMarker> m_dynamicMarkers;
    QWidget *m_leftPanel = nullptr;
    QWidget *m_rightPanel = nullptr;
    QWidget *m_topPanel = nullptr;
    QWidget *m_bottomPanel = nullptr;
    QWidget *m_timelinePanel = nullptr;
    QWidget *m_summaryPanel = nullptr;
    QWidget *m_sceneTools = nullptr;
    QWidget *m_sceneToolContent = nullptr;
    QPushButton *m_sceneToolToggle = nullptr;
    QPushButton *m_fullScreenButton = nullptr;
    QWidget *m_timelineStepsPanel = nullptr;
    QHBoxLayout *m_timelineStepsLayout = nullptr;
    QButtonGroup *m_domainButtons = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_filter = nullptr;
    QTextBrowser *m_inspector = nullptr;
    QLabel *m_domainTitle = nullptr;
    QLabel *m_domainSubtitle = nullptr;
    QLabel *m_inspectorTitle = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_stepDescription = nullptr;
    QLabel *m_stepCounter = nullptr;
    QSlider *m_timeline = nullptr;
    QPushButton *m_playButton = nullptr;
    QVector<QPushButton *> m_stepButtons;
    QVector<QLabel *> m_summaryLabels;
    QVector<QLabel *> m_summaryValues;
    QTimer *m_planTimer = nullptr;
    QPointer<QDialog> m_markerPropertyDialog;
    QPointer<QDialog> m_measurementDialog;
    QPointer<QDialog> m_viewpointLocateDialog;
    QPointer<QDialog> m_flightDialog;
    QPointer<EnvironmentSettingsDialog> m_environmentDialog;
    bool m_sceneToolsExpanded = true;
    bool m_sceneToolsExpandedBeforeFullScreen = true;
    bool m_mapFullScreen = false;
    Domain m_domain = Domain::Fixed;
    PendingPlacement m_pendingPlacement = PendingPlacement::None;
    QString m_pendingMarkerName;
    QString m_pendingMarkerType = QStringLiteral("point");
    QString m_pendingMarkerCategory = QStringLiteral("unit-annotation");
    QString m_pendingNormalFolderId;
    QString m_pendingDynamicPlanId;
    QString m_pendingDynamicStepId;
    bool m_returnToFixedAfterPlacement = false;
    double m_pendingMeasurementParameter = 0.0;
    QWidget *m_blackboardPanel = nullptr;
    QString m_lastLayerError;
    QString m_selectedPlanId;
    QString m_selectedStepId;
    int m_currentStep = -1;
    PlanState m_planState = PlanState::Stopped;
    QElapsedTimer m_planClock;
    qint64 m_planElapsedMs = 0;
    QString m_playbackPlanId;
    QString m_playbackStepId;
    QString m_focusedStepId;
    qsizetype m_focusedMarkerIndex = -2;
    bool m_timelineScrubbing = false;
    bool m_resumeAfterScrub = false;
    QSet<QString> m_executedAutoActions;
    QByteArray m_approvedAutoPrograms;
    bool m_active = false;
    bool m_treeRebuilding = false;
};

} // namespace Zqxxxt
