#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <QString>

namespace Zqxxxt {

struct ExpandedAnnotationGeometry
{
    QString geometryType;
    QVector<GeoCoordinate> points;
};

[[nodiscard]] bool isPlotAnnotation(const QString &kind);
[[nodiscard]] bool annotationIsArea(const QString &kind);
[[nodiscard]] int annotationMinimumPoints(const QString &kind);
[[nodiscard]] int annotationAutoFinishPoints(const QString &kind);
[[nodiscard]] QVector<GeoCoordinate> referenceMarkerControlPoints(
    const QString &kind, const QVector<GeoCoordinate> &clickedPoints,
    double viewRangeMeters);
[[nodiscard]] QString annotationDisplayName(const QString &kind);
[[nodiscard]] ExpandedAnnotationGeometry expandAnnotationGeometry(
    const QString &kind, const QVector<GeoCoordinate> &controlPoints);

} // namespace Zqxxxt
