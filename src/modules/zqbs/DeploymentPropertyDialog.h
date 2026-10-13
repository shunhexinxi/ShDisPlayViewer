#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <functional>
#include <optional>

class QDialog;
class QWidget;

namespace Zqxxxt {


using DeploymentMarkerEditFinished =
    std::function<void(bool accepted, const DeploymentMarker &marker)>;
// Evaluated on each click, since this modeless editor leaves the map usable.
using DeploymentViewpointProvider =
    std::function<std::optional<DeploymentViewpoint>()>;
using DeploymentMeasurementEditApplied =
    std::function<bool(DeploymentMeasurement &measurement, QString &error)>;

// Recreates the legacy FreeViewer mark/plot property workflow without taking
// a dependency on its Qt4-generated UI classes. This editor is deliberately
// modeless so the map and its control-point draggers remain usable.
QDialog *showDeploymentMarkerProperties(
    const DeploymentMarker &marker,
    DeploymentViewpointProvider currentViewpoint,
    const QVector<DeploymentMarker> &liveMarkers,
    QWidget *parent,
    DeploymentMarkerEditFinished finished);

// Builds and applies the legacy-style measurement result "样式" tab.  The
// caller owns the returned page and can place it in its result QTabWidget.
QWidget *createDeploymentMeasurementStylePage(
    const DeploymentMeasurement &measurement, QWidget *parent);
void applyDeploymentMeasurementStylePage(
    QWidget *stylePage, DeploymentMeasurement &measurement);

// Recreates the reference tool-specific result windows.  It remains modeless
// so the reference map annotations and control points stay visible.  Apply
// invokes the caller before changing the persisted/current measurement.
QDialog *showDeploymentMeasurementResults(
    const DeploymentMeasurement &measurement,
    QWidget *parent,
    DeploymentMeasurementEditApplied apply);

// Keeps the spatial-data table in sync when a control point is moved on map.
void updateDeploymentMarkerPropertyPoints(
    QDialog *dialog,
    const QVector<GeoCoordinate> &points);

} // namespace Zqxxxt
