#pragma once

#include "modules/zqbs/FlightRouteTypes.h"
#include "ui/SkinnedDialog.h"

#include <QPointer>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QTimer;

namespace Zqxxxt {
class SceneWidget;
class FlightRouteOverlay;

class FlightRoamDialog final : public SkinnedDialog
{
    Q_OBJECT
public:
    explicit FlightRoamDialog(SceneWidget *scene, QWidget *parent = nullptr);
    ~FlightRoamDialog() override;
    void reactivate();
    void done(int result) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void loadInitialDocument();
    void loadRoute(int index);
    void refreshRouteList();
    void refreshWaypointTable(int selectedRow = -1);
    void refreshLinkageEditor();
    void refreshOverlay();
    void refreshStatus();
    void beginPointPlacement(bool insert);
    void stopPointPlacement();
    void addPoint(const GeoCoordinate &coordinate);
    void deletePoint();
    void addAnnotation();
    void saveLinkage();
    void clearLinkage();
    void applyPointCell(int row, int column);
    void chooseRouteColor();
    void newDocument();
    void openDocument();
    bool saveDocument(bool saveAs = false);
    void applyCurrentRoute();
    void executeLinkage(const FlightWaypointLinkage &linkage, bool test);
    void openVideo(const QString &content, const QString &parameter);
    void stopSession();
    FlightRouteDefinition *currentRoute();
    FlightRouteWaypoint *currentWaypoint();
    QString defaultXmlPath() const;

    QPointer<SceneWidget> m_scene;
    QPointer<FlightRouteOverlay> m_overlay;
    QPointer<SkinnedDialog> m_videoDialog;
    QTimer *m_timer = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_routeTable = nullptr;
    QTableWidget *m_waypointTable = nullptr;
    QComboBox *m_routeCombo = nullptr;
    QComboBox *m_linkageType = nullptr;
    QComboBox *m_markerStyle = nullptr;
    QLineEdit *m_linkageContent = nullptr;
    QLineEdit *m_linkageParameter = nullptr;
    QLineEdit *m_routeName = nullptr;
    QDoubleSpinBox *m_lineWidth = nullptr;
    QPushButton *m_routeColor = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_reset = nullptr;
    QCheckBox *m_routeVisible = nullptr;
    QRadioButton *m_cockpit = nullptr;
    QRadioButton *m_free = nullptr;
    QRadioButton *m_follow = nullptr;
    QLabel *m_state = nullptr;
    QLabel *m_fileState = nullptr;
    QSpinBox *m_speed = nullptr;
    FlightRouteDocument m_document;
    QString m_filePath;
    int m_selectedRoute = 0;
    int m_insertBefore = -1;
    bool m_placingPoints = false;
    bool m_loadingUi = false;
    bool m_dirty = false;
    int m_overlayRefreshTick = 0;
};
}
