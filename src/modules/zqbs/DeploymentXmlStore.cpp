#include "modules/zqbs/DeploymentXmlStore.h"

#include "app/AppPaths.h"
#include "modules/zqts/MapHomeViewpoint.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace {

QString newId(const QString &prefix)
{
    return prefix + QLatin1Char('-')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

bool boolAttribute(const QDomElement &element, const QString &name, bool fallback)
{
    if (!element.hasAttribute(name))
        return fallback;
    const QString value = element.attribute(name).trimmed().toLower();
    return value == QStringLiteral("true") || value == QStringLiteral("1")
        || value == QStringLiteral("yes");
}

double doubleAttribute(const QDomElement &element, const QString &name, double fallback)
{
    bool ok = false;
    const double value = element.attribute(name).toDouble(&ok);
    return ok ? value : fallback;
}

int intAttribute(const QDomElement &element, const QString &name, int fallback)
{
    bool ok = false;
    const int value = element.attribute(name).toInt(&ok);
    return ok ? value : fallback;
}

Zqxxxt::GeoCoordinate readPoint(const QDomElement &element)
{
    Zqxxxt::GeoCoordinate point;
    point.longitude = doubleAttribute(element, QStringLiteral("longitude"), 0.0);
    point.latitude = doubleAttribute(element, QStringLiteral("latitude"), 0.0);
    point.altitude = doubleAttribute(element, QStringLiteral("altitude"), 0.0);
    return point;
}

void writePoint(QDomDocument &document, QDomElement &parent,
                const Zqxxxt::GeoCoordinate &point,
                const QString &tagName = QStringLiteral("point"))
{
    QDomElement element = document.createElement(tagName);
    element.setAttribute(QStringLiteral("longitude"), QString::number(point.longitude, 'f', 8));
    element.setAttribute(QStringLiteral("latitude"), QString::number(point.latitude, 'f', 8));
    element.setAttribute(QStringLiteral("altitude"), QString::number(point.altitude, 'f', 2));
    parent.appendChild(element);
}

Zqxxxt::DeploymentViewpoint readViewpoint(const QDomElement &element)
{
    Zqxxxt::DeploymentViewpoint view;
    view.id = element.attribute(QStringLiteral("id"));
    view.name = element.attribute(QStringLiteral("name"));
    view.longitude = doubleAttribute(element, QStringLiteral("longitude"), 121.414);
    view.latitude = doubleAttribute(element, QStringLiteral("latitude"), 31.1828);
    view.altitude = doubleAttribute(element, QStringLiteral("altitude"), 0.0);
    view.heading = doubleAttribute(element, QStringLiteral("heading"), 0.0);
    view.pitch = doubleAttribute(element, QStringLiteral("pitch"), -35.0);
    view.range = doubleAttribute(element, QStringLiteral("range"), 1500000.0);
    view.flyTimeSeconds = doubleAttribute(element, QStringLiteral("flyTimeSeconds"), 3.0);
    return view;
}

void writeViewpointAttributes(QDomElement &element,
                              const Zqxxxt::DeploymentViewpoint &view)
{
    element.setAttribute(QStringLiteral("id"), view.id);
    element.setAttribute(QStringLiteral("name"), view.name);
    element.setAttribute(QStringLiteral("longitude"), QString::number(view.longitude, 'f', 8));
    element.setAttribute(QStringLiteral("latitude"), QString::number(view.latitude, 'f', 8));
    element.setAttribute(QStringLiteral("altitude"), QString::number(view.altitude, 'f', 2));
    element.setAttribute(QStringLiteral("heading"), QString::number(view.heading, 'f', 2));
    element.setAttribute(QStringLiteral("pitch"), QString::number(view.pitch, 'f', 2));
    element.setAttribute(QStringLiteral("range"), QString::number(view.range, 'f', 2));
    element.setAttribute(QStringLiteral("flyTimeSeconds"),
                         QString::number(view.flyTimeSeconds, 'f', 2));
}

Zqxxxt::DeploymentMarker readMarker(const QDomElement &element)
{
    Zqxxxt::DeploymentMarker marker;
    marker.id = element.attribute(QStringLiteral("id"));
    marker.folderId = element.attribute(QStringLiteral("folderId"));
    marker.name = element.attribute(QStringLiteral("name"));
    marker.category = element.attribute(QStringLiteral("category"), QStringLiteral("manual"));
    marker.geometryType = element.attribute(QStringLiteral("geometryType"), QStringLiteral("point"));
    Zqxxxt::applyLegacyAnnotationStyleDefaults(marker);
    marker.icon = element.attribute(QStringLiteral("icon"), QStringLiteral("pin"));
    marker.color = QColor(element.attribute(QStringLiteral("color"),
                                            marker.color.name(QColor::HexRgb)));
    if (!marker.color.isValid())
        marker.color = QColor(QStringLiteral("#ffff00"));
    marker.fillColor = QColor(element.attribute(QStringLiteral("fillColor"),
                                                marker.fillColor.name(QColor::HexRgb)));
    if (!marker.fillColor.isValid())
        marker.fillColor = marker.color;
    marker.visible = boolAttribute(element, QStringLiteral("visible"), true);
    marker.bindingType = element.attribute(QStringLiteral("bindingType"));
    marker.bindingId = element.attribute(QStringLiteral("bindingId"));
    marker.association = QJsonDocument::fromJson(
        element.attribute(QStringLiteral("association")).toUtf8()).object();
    marker.position.longitude = doubleAttribute(element, QStringLiteral("longitude"), 0.0);
    marker.position.latitude = doubleAttribute(element, QStringLiteral("latitude"), 0.0);
    marker.position.altitude = doubleAttribute(element, QStringLiteral("altitude"), 0.0);
    marker.text = element.attribute(QStringLiteral("text"));
    marker.description = element.attribute(QStringLiteral("description"));
    marker.lineWidth = doubleAttribute(element, QStringLiteral("lineWidth"), marker.lineWidth);
    marker.lineOpacity = doubleAttribute(element, QStringLiteral("lineOpacity"), marker.lineOpacity);
    marker.fillOpacity = doubleAttribute(element, QStringLiteral("fillOpacity"), marker.fillOpacity);
    marker.pointSize = doubleAttribute(element, QStringLiteral("pointSize"), marker.pointSize);
    marker.textSize = doubleAttribute(element, QStringLiteral("textSize"), marker.textSize);
    marker.labelWidth = doubleAttribute(element, QStringLiteral("labelWidth"), marker.labelWidth);
    marker.labelHeight = doubleAttribute(element, QStringLiteral("labelHeight"), marker.labelHeight);
    marker.labelTextColor = QColor(element.attribute(QStringLiteral("labelTextColor"),
        marker.labelTextColor.name(QColor::HexArgb)));
    marker.labelBackgroundColor = QColor(element.attribute(
        QStringLiteral("labelBackgroundColor"),
        marker.labelBackgroundColor.name(QColor::HexArgb)));
    marker.labelBackgroundImage = element.attribute(
        QStringLiteral("labelBackgroundImage"), marker.labelBackgroundImage);
    marker.labelAlignment = intAttribute(element, QStringLiteral("labelAlignment"),
                                         marker.labelAlignment);
    marker.modelSourceType = intAttribute(element, QStringLiteral("modelSourceType"),
                                          marker.modelSourceType);
    marker.modelRotationXDegrees = doubleAttribute(element,
        QStringLiteral("modelRotationXDegrees"), marker.modelRotationXDegrees);
    marker.modelRotationYDegrees = doubleAttribute(element,
        QStringLiteral("modelRotationYDegrees"), marker.modelRotationYDegrees);
    marker.modelRotationZDegrees = doubleAttribute(element,
        QStringLiteral("modelRotationZDegrees"), marker.modelRotationZDegrees);
    marker.modelLodLowMeters = doubleAttribute(element,
        QStringLiteral("modelLodLowMeters"), marker.modelLodLowMeters);
    marker.modelLodHighMeters = doubleAttribute(element,
        QStringLiteral("modelLodHighMeters"), marker.modelLodHighMeters);
    marker.modelServiceUrl = element.attribute(QStringLiteral("modelServiceUrl"));
    marker.tiltSourceType = intAttribute(element, QStringLiteral("tiltSourceType"),
                                         marker.tiltSourceType);
    marker.tiltRotationDegrees = doubleAttribute(element,
        QStringLiteral("tiltRotationDegrees"), marker.tiltRotationDegrees);
    marker.tiltServiceUrl = element.attribute(QStringLiteral("tiltServiceUrl"));
    marker.altitudeMode = element.attribute(QStringLiteral("altitudeMode"),
                                            QStringLiteral("0")).toInt();
    marker.viewpoint.longitude = doubleAttribute(element,
        QStringLiteral("viewLongitude"), marker.position.longitude);
    marker.viewpoint.latitude = doubleAttribute(element,
        QStringLiteral("viewLatitude"), marker.position.latitude);
    marker.viewpoint.altitude = doubleAttribute(element,
        QStringLiteral("viewAltitude"), marker.position.altitude);
    marker.viewpoint.heading = doubleAttribute(element,
        QStringLiteral("viewHeading"), -20.0);
    marker.viewpoint.pitch = doubleAttribute(element,
        QStringLiteral("viewPitch"), -90.0);
    marker.viewpoint.range = doubleAttribute(element,
        QStringLiteral("viewRange"), 10000.0);
    marker.viewFlyTimeSeconds = doubleAttribute(element,
        QStringLiteral("viewFlyTime"), 1.0);
    marker.viewRangeLowMeters = doubleAttribute(element,
        QStringLiteral("viewRangeLow"), 0.0);
    marker.viewRangeHighMeters = doubleAttribute(element,
        QStringLiteral("viewRangeHigh"), 100000000.0);
    for (QDomElement point = element.firstChildElement(QStringLiteral("point"));
         !point.isNull(); point = point.nextSiblingElement(QStringLiteral("point"))) {
        marker.points.append(readPoint(point));
    }
    for (QDomElement row = element.firstChildElement(QStringLiteral("labelItem"));
         !row.isNull(); row = row.nextSiblingElement(QStringLiteral("labelItem"))) {
        Zqxxxt::DeploymentLabelItem item;
        item.text = row.attribute(QStringLiteral("text"), item.text);
        item.width = doubleAttribute(row, QStringLiteral("width"), item.width);
        item.height = doubleAttribute(row, QStringLiteral("height"), item.height);
        item.textSize = doubleAttribute(row, QStringLiteral("textSize"), item.textSize);
        item.textColor = QColor(row.attribute(QStringLiteral("textColor"),
            item.textColor.name(QColor::HexArgb)));
        item.backgroundColor = QColor(row.attribute(QStringLiteral("backgroundColor"),
            item.backgroundColor.name(QColor::HexArgb)));
        item.backgroundImage = row.attribute(QStringLiteral("backgroundImage"),
                                             item.backgroundImage);
        item.alignment = intAttribute(row, QStringLiteral("alignment"), item.alignment);
        marker.labelItems.append(item);
    }
    if (marker.points.isEmpty())
        marker.points.append(marker.position);
    else
        marker.position = marker.points.constFirst();
    return marker;
}

void writeMarker(QDomDocument &document, QDomElement &parent,
                 const Zqxxxt::DeploymentMarker &marker)
{
    QDomElement element = document.createElement(QStringLiteral("marker"));
    element.setAttribute(QStringLiteral("id"), marker.id);
    element.setAttribute(QStringLiteral("name"), marker.name);
    element.setAttribute(QStringLiteral("category"), marker.category);
    if (!marker.folderId.isEmpty()) element.setAttribute(QStringLiteral("folderId"), marker.folderId);
    element.setAttribute(QStringLiteral("geometryType"), marker.geometryType);
    element.setAttribute(QStringLiteral("icon"), marker.icon);
    element.setAttribute(QStringLiteral("color"), marker.color.name(QColor::HexRgb));
    element.setAttribute(QStringLiteral("fillColor"), marker.fillColor.name(QColor::HexRgb));
    element.setAttribute(QStringLiteral("visible"), boolText(marker.visible));
    element.setAttribute(QStringLiteral("longitude"), QString::number(marker.position.longitude, 'f', 8));
    element.setAttribute(QStringLiteral("latitude"), QString::number(marker.position.latitude, 'f', 8));
    element.setAttribute(QStringLiteral("altitude"), QString::number(marker.position.altitude, 'f', 2));
    element.setAttribute(QStringLiteral("lineWidth"), QString::number(marker.lineWidth, 'f', 2));
    element.setAttribute(QStringLiteral("lineOpacity"), QString::number(marker.lineOpacity, 'f', 3));
    element.setAttribute(QStringLiteral("fillOpacity"), QString::number(marker.fillOpacity, 'f', 3));
    element.setAttribute(QStringLiteral("pointSize"), QString::number(marker.pointSize, 'f', 2));
    element.setAttribute(QStringLiteral("textSize"), QString::number(marker.textSize, 'f', 2));
    element.setAttribute(QStringLiteral("labelWidth"), QString::number(marker.labelWidth, 'f', 2));
    element.setAttribute(QStringLiteral("labelHeight"), QString::number(marker.labelHeight, 'f', 2));
    element.setAttribute(QStringLiteral("labelTextColor"),
                         marker.labelTextColor.name(QColor::HexArgb));
    element.setAttribute(QStringLiteral("labelBackgroundColor"),
                         marker.labelBackgroundColor.name(QColor::HexArgb));
    element.setAttribute(QStringLiteral("labelBackgroundImage"),
                         marker.labelBackgroundImage);
    element.setAttribute(QStringLiteral("labelAlignment"), marker.labelAlignment);
    element.setAttribute(QStringLiteral("modelSourceType"), marker.modelSourceType);
    element.setAttribute(QStringLiteral("modelRotationXDegrees"),
                         QString::number(marker.modelRotationXDegrees, 'f', 4));
    element.setAttribute(QStringLiteral("modelRotationYDegrees"),
                         QString::number(marker.modelRotationYDegrees, 'f', 4));
    element.setAttribute(QStringLiteral("modelRotationZDegrees"),
                         QString::number(marker.modelRotationZDegrees, 'f', 4));
    element.setAttribute(QStringLiteral("modelLodLowMeters"),
                         QString::number(marker.modelLodLowMeters, 'f', 3));
    element.setAttribute(QStringLiteral("modelLodHighMeters"),
                         QString::number(marker.modelLodHighMeters, 'f', 3));
    if (!marker.modelServiceUrl.isEmpty())
        element.setAttribute(QStringLiteral("modelServiceUrl"), marker.modelServiceUrl);
    element.setAttribute(QStringLiteral("tiltSourceType"), marker.tiltSourceType);
    element.setAttribute(QStringLiteral("tiltRotationDegrees"),
                         QString::number(marker.tiltRotationDegrees, 'f', 4));
    if (!marker.tiltServiceUrl.isEmpty())
        element.setAttribute(QStringLiteral("tiltServiceUrl"), marker.tiltServiceUrl);
    element.setAttribute(QStringLiteral("altitudeMode"), marker.altitudeMode);
    element.setAttribute(QStringLiteral("viewLongitude"),
                         QString::number(marker.viewpoint.longitude, 'f', 8));
    element.setAttribute(QStringLiteral("viewLatitude"),
                         QString::number(marker.viewpoint.latitude, 'f', 8));
    element.setAttribute(QStringLiteral("viewAltitude"),
                         QString::number(marker.viewpoint.altitude, 'f', 2));
    element.setAttribute(QStringLiteral("viewHeading"),
                         QString::number(marker.viewpoint.heading, 'f', 3));
    element.setAttribute(QStringLiteral("viewPitch"),
                         QString::number(marker.viewpoint.pitch, 'f', 3));
    element.setAttribute(QStringLiteral("viewRange"),
                         QString::number(marker.viewpoint.range, 'f', 2));
    element.setAttribute(QStringLiteral("viewFlyTime"),
                         QString::number(marker.viewFlyTimeSeconds, 'f', 2));
    element.setAttribute(QStringLiteral("viewRangeLow"),
                         QString::number(marker.viewRangeLowMeters, 'f', 2));
    element.setAttribute(QStringLiteral("viewRangeHigh"),
                         QString::number(marker.viewRangeHighMeters, 'f', 2));
    if (!marker.text.isEmpty())
        element.setAttribute(QStringLiteral("text"), marker.text);
    if (!marker.description.isEmpty())
        element.setAttribute(QStringLiteral("description"), marker.description);
    if (!marker.bindingType.isEmpty())
        element.setAttribute(QStringLiteral("bindingType"), marker.bindingType);
    if (!marker.bindingId.isEmpty())
        element.setAttribute(QStringLiteral("bindingId"), marker.bindingId);
    if (!marker.association.isEmpty())
        element.setAttribute(QStringLiteral("association"), QString::fromUtf8(
            QJsonDocument(marker.association).toJson(QJsonDocument::Compact)));
    const QVector<Zqxxxt::GeoCoordinate> points = marker.points.isEmpty()
        ? QVector<Zqxxxt::GeoCoordinate>{marker.position} : marker.points;
    for (const Zqxxxt::GeoCoordinate &point : points)
        writePoint(document, element, point);
    for (const Zqxxxt::DeploymentLabelItem &item : marker.labelItems) {
        QDomElement row = document.createElement(QStringLiteral("labelItem"));
        row.setAttribute(QStringLiteral("text"), item.text);
        row.setAttribute(QStringLiteral("width"), QString::number(item.width, 'f', 2));
        row.setAttribute(QStringLiteral("height"), QString::number(item.height, 'f', 2));
        row.setAttribute(QStringLiteral("textSize"), QString::number(item.textSize, 'f', 2));
        row.setAttribute(QStringLiteral("textColor"), item.textColor.name(QColor::HexArgb));
        row.setAttribute(QStringLiteral("backgroundColor"),
                         item.backgroundColor.name(QColor::HexArgb));
        row.setAttribute(QStringLiteral("backgroundImage"), item.backgroundImage);
        row.setAttribute(QStringLiteral("alignment"), item.alignment);
        element.appendChild(row);
    }
    parent.appendChild(element);
}

bool readDocument(const QString &path, QDomDocument &document, QString &errorMessage);
bool writeDocument(const QString &path, const QDomDocument &document, QString &errorMessage);

QString childText(const QDomElement &parent, const QString &tagName,
                  const QString &fallback = {})
{
    const QDomElement child = parent.firstChildElement(tagName);
    return child.isNull() ? fallback : child.text().trimmed();
}

void appendTextElement(QDomDocument &document, QDomElement &parent,
                       const QString &tagName, const QString &value)
{
    QDomElement element = document.createElement(tagName);
    element.appendChild(document.createTextNode(value));
    parent.appendChild(element);
}

QString legacyMarkerTag(const QString &kind)
{
    static const QHash<QString, QString> tags{
        {QStringLiteral("point"), QStringLiteral("PointMark")},
        {QStringLiteral("legacy-point"), QStringLiteral("PointMark")},
        {QStringLiteral("line"), QStringLiteral("LineMark")},
        {QStringLiteral("polygon"), QStringLiteral("PolygonMark")},
        {QStringLiteral("model"), QStringLiteral("ModelMark")},
        {QStringLiteral("billboard"), QStringLiteral("BillBoardMark")},
        {QStringLiteral("overlay"), QStringLiteral("OverLayMark")},
        {QStringLiteral("rectangle"), QStringLiteral("RectMark")},
        {QStringLiteral("circle"), QStringLiteral("CircleMark")},
        {QStringLiteral("ellipse"), QStringLiteral("EllipseMark")},
        {QStringLiteral("arc"), QStringLiteral("ArcMark")},
        {QStringLiteral("sector"), QStringLiteral("SectorMark")},
        {QStringLiteral("text"), QStringLiteral("TextMark")},
        {QStringLiteral("tilt-model"), QStringLiteral("TiltModelMark")},
        {QStringLiteral("particle"), QStringLiteral("ParticleMark")},
        {QStringLiteral("label"), QStringLiteral("LabelMark")},
        {QStringLiteral("plot-straight-arrow"), QStringLiteral("StraightArrowMark")},
        {QStringLiteral("plot-straight-multi-arrow"), QStringLiteral("StraightMoreArrowMark")},
        {QStringLiteral("plot-dovetail-diagonal-arrow"), QStringLiteral("DovetailDiagonalArrowMark")},
        {QStringLiteral("plot-dovetail-diagonal-multi-arrow"), QStringLiteral("DovetailDiagonalMoreArrowMark")},
        {QStringLiteral("plot-dovetail-straight-arrow"), QStringLiteral("DoveTailStraightArrowMark")},
        {QStringLiteral("plot-dovetail-straight-multi-arrow"), QStringLiteral("DoveTailStraightMoreArrowMark")},
        {QStringLiteral("plot-diagonal-arrow"), QStringLiteral("DiagonalArrowMark")},
        {QStringLiteral("plot-diagonal-multi-arrow"), QStringLiteral("DiagonalMoreArrowMark")},
        {QStringLiteral("plot-double-arrow"), QStringLiteral("DoubleArrowMark")},
        {QStringLiteral("plot-closed-curve"), QStringLiteral("CloseCurveMark")},
        {QStringLiteral("plot-gathering-place"), QStringLiteral("GatheringPlaceMark")},
        {QStringLiteral("plot-rounded-rect"), QStringLiteral("RoundedRectMark")},
        {QStringLiteral("plot-bezier-arrow"), QStringLiteral("BezierCurveArrowMark")},
        {QStringLiteral("plot-polyline-arrow"), QStringLiteral("PolylineArrowMark")},
        {QStringLiteral("plot-cardinal-arrow"), QStringLiteral("CardinalCurveArrowMark")},
        {QStringLiteral("plot-sector-search"), QStringLiteral("SectorSearchMark")},
        {QStringLiteral("plot-parallel-search"), QStringLiteral("ParallelSearchMark")},
        {QStringLiteral("plot-straight-line-arrow"), QStringLiteral("StraightLineArrowMark")}
    };
    return tags.value(kind, kind.startsWith(QStringLiteral("plot-"))
        ? QStringLiteral("StraightArrowMark") : QStringLiteral("PointMark"));
}

QString kindForLegacyMarkerTag(const QDomElement &element, bool plot)
{
    const QString extension = element.attribute(QStringLiteral("zqxxxt-kind"));
    if (!extension.isEmpty())
        return extension;
    static const QHash<QString, QString> kinds{
        {QStringLiteral("PointMark"), QStringLiteral("point")},
        {QStringLiteral("LineMark"), QStringLiteral("line")},
        {QStringLiteral("PolygonMark"), QStringLiteral("polygon")},
        {QStringLiteral("ModelMark"), QStringLiteral("model")},
        {QStringLiteral("LodModelMark"), QStringLiteral("model")},
        {QStringLiteral("BillBoardMark"), QStringLiteral("billboard")},
        {QStringLiteral("BillboardMark"), QStringLiteral("billboard")},
        {QStringLiteral("OverLayMark"), QStringLiteral("overlay")},
        {QStringLiteral("OverlayMark"), QStringLiteral("overlay")},
        {QStringLiteral("RectMark"), QStringLiteral("rectangle")},
        {QStringLiteral("CircleMark"), QStringLiteral("circle")},
        {QStringLiteral("EllipseMark"), QStringLiteral("ellipse")},
        {QStringLiteral("ArcMark"), QStringLiteral("arc")},
        {QStringLiteral("SectorMark"), QStringLiteral("sector")},
        {QStringLiteral("TextMark"), QStringLiteral("text")},
        {QStringLiteral("TiltModelMark"), QStringLiteral("tilt-model")},
        {QStringLiteral("ParticleMark"), QStringLiteral("particle")},
        {QStringLiteral("LabelMark"), QStringLiteral("label")},
        {QStringLiteral("StraightArrowMark"), QStringLiteral("plot-straight-arrow")},
        {QStringLiteral("StraightMoreArrowMark"), QStringLiteral("plot-straight-multi-arrow")},
        {QStringLiteral("DovetailDiagonalArrowMark"), QStringLiteral("plot-dovetail-diagonal-arrow")},
        {QStringLiteral("DovetailDiagonalMoreArrowMark"), QStringLiteral("plot-dovetail-diagonal-multi-arrow")},
        {QStringLiteral("DoveTailStraightArrowMark"), QStringLiteral("plot-dovetail-straight-arrow")},
        {QStringLiteral("DoveTailStraightMoreArrowMark"), QStringLiteral("plot-dovetail-straight-multi-arrow")},
        {QStringLiteral("DiagonalArrowMark"), QStringLiteral("plot-diagonal-arrow")},
        {QStringLiteral("DiagonalMoreArrowMark"), QStringLiteral("plot-diagonal-multi-arrow")},
        {QStringLiteral("DoubleArrowMark"), QStringLiteral("plot-double-arrow")},
        {QStringLiteral("CloseCurveMark"), QStringLiteral("plot-closed-curve")},
        {QStringLiteral("GatheringPlaceMark"), QStringLiteral("plot-gathering-place")},
        {QStringLiteral("RoundedRectMark"), QStringLiteral("plot-rounded-rect")},
        {QStringLiteral("BezierCurveArrowMark"), QStringLiteral("plot-bezier-arrow")},
        {QStringLiteral("PolylineArrowMark"), QStringLiteral("plot-polyline-arrow")},
        {QStringLiteral("CardinalCurveArrowMark"), QStringLiteral("plot-cardinal-arrow")},
        {QStringLiteral("SectorSearchMark"), QStringLiteral("plot-sector-search")},
        {QStringLiteral("ParallelSearchMark"), QStringLiteral("plot-parallel-search")},
        {QStringLiteral("StraightLineArrowMark"), QStringLiteral("plot-straight-line-arrow")}
    };
    return kinds.value(element.tagName(), plot
        ? QStringLiteral("plot-straight-arrow") : QStringLiteral("point"));
}

void appendLegacyColor(QDomDocument &document, QDomElement &parent,
                       const QString &tagName, const QColor &color, double alpha)
{
    QDomElement element = document.createElement(tagName);
    element.setAttribute(QStringLiteral("r"), QString::number(color.redF(), 'f', 6));
    element.setAttribute(QStringLiteral("g"), QString::number(color.greenF(), 'f', 6));
    element.setAttribute(QStringLiteral("b"), QString::number(color.blueF(), 'f', 6));
    element.setAttribute(QStringLiteral("a"), QString::number(alpha, 'f', 6));
    parent.appendChild(element);
}

QColor readLegacyColor(const QDomElement &parent, const QString &tagName,
                       const QColor &fallback, double *alpha = nullptr)
{
    const QDomElement element = parent.firstChildElement(tagName);
    if (element.isNull())
        return fallback;
    QColor color;
    color.setRgbF(doubleAttribute(element, QStringLiteral("r"), fallback.redF()),
                  doubleAttribute(element, QStringLiteral("g"), fallback.greenF()),
                  doubleAttribute(element, QStringLiteral("b"), fallback.blueF()));
    if (alpha)
        *alpha = doubleAttribute(element, QStringLiteral("a"), *alpha);
    return color;
}

void appendLegacyViewpoint(QDomDocument &document, QDomElement &parent,
                           const Zqxxxt::DeploymentViewpoint &view,
                           double flyTimeSeconds)
{
    QDomElement element = document.createElement(QStringLiteral("viewPoint"));
    element.setAttribute(QStringLiteral("lon"), QString::number(view.longitude, 'f', 8));
    element.setAttribute(QStringLiteral("lat"), QString::number(view.latitude, 'f', 8));
    element.setAttribute(QStringLiteral("hei"), QString::number(view.altitude, 'f', 3));
    element.setAttribute(QStringLiteral("heading"), QString::number(view.heading, 'f', 3));
    element.setAttribute(QStringLiteral("pitch"), QString::number(view.pitch, 'f', 3));
    element.setAttribute(QStringLiteral("range"), QString::number(view.range, 'f', 3));
    element.setAttribute(QStringLiteral("time"), QString::number(flyTimeSeconds, 'f', 3));
    parent.appendChild(element);
}

Zqxxxt::DeploymentViewpoint readLegacyViewpoint(const QDomElement &parent)
{
    const QDomElement element = parent.firstChildElement(QStringLiteral("viewPoint"));
    Zqxxxt::DeploymentViewpoint view;
    if (element.isNull())
        return view;
    view.longitude = doubleAttribute(element, QStringLiteral("lon"), view.longitude);
    view.latitude = doubleAttribute(element, QStringLiteral("lat"), view.latitude);
    view.altitude = doubleAttribute(element, QStringLiteral("hei"), view.altitude);
    view.heading = doubleAttribute(element, QStringLiteral("heading"), view.heading);
    view.pitch = doubleAttribute(element, QStringLiteral("pitch"), view.pitch);
    view.range = doubleAttribute(element, QStringLiteral("range"), view.range);
    view.flyTimeSeconds = doubleAttribute(element, QStringLiteral("time"), 3.0);
    return view;
}

void writeLegacyMarker(QDomDocument &document, QDomElement &parent,
                       const Zqxxxt::DeploymentMarker &marker)
{
    QDomElement element = document.createElement(legacyMarkerTag(marker.geometryType));
    element.setAttribute(QStringLiteral("title"), marker.name);
    element.setAttribute(QStringLiteral("visible"), boolText(marker.visible));
    element.setAttribute(QStringLiteral("description"), marker.description);
    element.setAttribute(QStringLiteral("width"), QString::number(marker.lineWidth, 'f', 3));
    element.setAttribute(QStringLiteral("size"), QString::number(marker.textSize, 'f', 3));
    if (!marker.icon.isEmpty())
        element.setAttribute(QStringLiteral("path"), marker.icon);
    // These namespaced-by-convention attributes are ignored by the old SDK,
    // while preserving all fields required by the new property dialog.
    element.setAttribute(QStringLiteral("zqxxxt-id"), marker.id);
    element.setAttribute(QStringLiteral("zqxxxt-kind"), marker.geometryType);
    element.setAttribute(QStringLiteral("zqxxxt-category"), marker.category);
    element.setAttribute(QStringLiteral("zqxxxt-icon"), marker.icon);
    element.setAttribute(QStringLiteral("zqxxxt-text"), marker.text);
    element.setAttribute(QStringLiteral("zqxxxt-altitude-mode"), marker.altitudeMode);
    element.setAttribute(QStringLiteral("zqxxxt-point-size"), QString::number(marker.pointSize, 'f', 3));
    element.setAttribute(QStringLiteral("zqxxxt-label-width"), QString::number(marker.labelWidth, 'f', 3));
    element.setAttribute(QStringLiteral("zqxxxt-label-height"), QString::number(marker.labelHeight, 'f', 3));
    element.setAttribute(QStringLiteral("zqxxxt-label-text-color"),
                         marker.labelTextColor.name(QColor::HexArgb));
    element.setAttribute(QStringLiteral("zqxxxt-label-background-color"),
                         marker.labelBackgroundColor.name(QColor::HexArgb));
    element.setAttribute(QStringLiteral("zqxxxt-label-background-image"),
                         marker.labelBackgroundImage);
    element.setAttribute(QStringLiteral("zqxxxt-label-alignment"), marker.labelAlignment);
    element.setAttribute(QStringLiteral("zqxxxt-model-source-type"), marker.modelSourceType);
    element.setAttribute(QStringLiteral("zqxxxt-model-rotation-x"),
                         QString::number(marker.modelRotationXDegrees, 'f', 4));
    element.setAttribute(QStringLiteral("zqxxxt-model-rotation-y"),
                         QString::number(marker.modelRotationYDegrees, 'f', 4));
    element.setAttribute(QStringLiteral("zqxxxt-model-rotation-z"),
                         QString::number(marker.modelRotationZDegrees, 'f', 4));
    element.setAttribute(QStringLiteral("zqxxxt-model-lod-low"),
                         QString::number(marker.modelLodLowMeters, 'f', 3));
    element.setAttribute(QStringLiteral("zqxxxt-model-lod-high"),
                         QString::number(marker.modelLodHighMeters, 'f', 3));
    if (!marker.modelServiceUrl.isEmpty())
        element.setAttribute(QStringLiteral("zqxxxt-model-service-url"),
                             marker.modelServiceUrl);
    element.setAttribute(QStringLiteral("zqxxxt-tilt-source-type"), marker.tiltSourceType);
    element.setAttribute(QStringLiteral("zqxxxt-tilt-rotation"),
                         QString::number(marker.tiltRotationDegrees, 'f', 4));
    if (!marker.tiltServiceUrl.isEmpty())
        element.setAttribute(QStringLiteral("zqxxxt-tilt-service-url"),
                             marker.tiltServiceUrl);
    element.setAttribute(QStringLiteral("zqxxxt-view-low"), QString::number(marker.viewRangeLowMeters, 'f', 3));
    element.setAttribute(QStringLiteral("zqxxxt-view-high"), QString::number(marker.viewRangeHighMeters, 'f', 3));
    if (!marker.bindingType.isEmpty())
        element.setAttribute(QStringLiteral("zqxxxt-binding-type"), marker.bindingType);
    if (!marker.bindingId.isEmpty())
        element.setAttribute(QStringLiteral("zqxxxt-binding-id"), marker.bindingId);
    if (!marker.association.isEmpty())
        element.setAttribute(QStringLiteral("zqxxxt-association"), QString::fromUtf8(
            QJsonDocument(marker.association).toJson(QJsonDocument::Compact)));
    appendLegacyColor(document, element, QStringLiteral("color"),
                      marker.color, marker.lineOpacity);
    appendLegacyColor(document, element, QStringLiteral("fillColor"),
                      marker.fillColor, marker.fillOpacity);
    appendLegacyViewpoint(document, element, marker.viewpoint,
                          marker.viewFlyTimeSeconds);
    const QVector<Zqxxxt::GeoCoordinate> points = marker.points.isEmpty()
        ? QVector<Zqxxxt::GeoCoordinate>{marker.position} : marker.points;
    if (!points.isEmpty()) {
        QDomElement position = document.createElement(QStringLiteral("position"));
        position.setAttribute(QStringLiteral("lon"), QString::number(points.constFirst().longitude, 'f', 8));
        position.setAttribute(QStringLiteral("lat"), QString::number(points.constFirst().latitude, 'f', 8));
        position.setAttribute(QStringLiteral("hei"), QString::number(points.constFirst().altitude, 'f', 3));
        element.appendChild(position);
    }
    for (const Zqxxxt::GeoCoordinate &point : points) {
        QDomElement vertex = document.createElement(QStringLiteral("vertex"));
        vertex.setAttribute(QStringLiteral("lon"), QString::number(point.longitude, 'f', 8));
        vertex.setAttribute(QStringLiteral("lat"), QString::number(point.latitude, 'f', 8));
        vertex.setAttribute(QStringLiteral("hei"), QString::number(point.altitude, 'f', 3));
        element.appendChild(vertex);
    }
    for (const Zqxxxt::DeploymentLabelItem &item : marker.labelItems) {
        QDomElement row = document.createElement(QStringLiteral("labelItem"));
        row.setAttribute(QStringLiteral("text"), item.text);
        row.setAttribute(QStringLiteral("width"), QString::number(item.width, 'f', 2));
        row.setAttribute(QStringLiteral("height"), QString::number(item.height, 'f', 2));
        row.setAttribute(QStringLiteral("textSize"), QString::number(item.textSize, 'f', 2));
        row.setAttribute(QStringLiteral("textColor"), item.textColor.name(QColor::HexArgb));
        row.setAttribute(QStringLiteral("backgroundColor"),
                         item.backgroundColor.name(QColor::HexArgb));
        row.setAttribute(QStringLiteral("backgroundImage"), item.backgroundImage);
        row.setAttribute(QStringLiteral("alignment"), item.alignment);
        element.appendChild(row);
    }
    parent.appendChild(element);
}

Zqxxxt::DeploymentMarker readLegacyMarker(const QDomElement &element,
                                           const QString &folderName,
                                           bool plot)
{
    Zqxxxt::DeploymentMarker marker;
    marker.id = element.attribute(QStringLiteral("zqxxxt-id"), newId(QStringLiteral("marker")));
    marker.name = element.attribute(QStringLiteral("title"), QStringLiteral("未命名标记"));
    marker.category = element.attribute(QStringLiteral("zqxxxt-category"), folderName);
    marker.geometryType = kindForLegacyMarkerTag(element, plot);
    Zqxxxt::applyLegacyAnnotationStyleDefaults(marker);
    marker.icon = element.attribute(QStringLiteral("zqxxxt-icon"),
                                    element.attribute(QStringLiteral("path")));
    marker.text = element.attribute(QStringLiteral("zqxxxt-text"));
    marker.description = element.attribute(QStringLiteral("description"));
    marker.visible = boolAttribute(element, QStringLiteral("visible"), true);
    marker.lineWidth = doubleAttribute(element, QStringLiteral("width"),
        doubleAttribute(element, QStringLiteral("line_width"), marker.lineWidth));
    marker.textSize = doubleAttribute(element, QStringLiteral("size"), marker.textSize);
    marker.pointSize = doubleAttribute(element, QStringLiteral("zqxxxt-point-size"), marker.pointSize);
    marker.labelWidth = doubleAttribute(element, QStringLiteral("zqxxxt-label-width"), marker.labelWidth);
    marker.labelHeight = doubleAttribute(element, QStringLiteral("zqxxxt-label-height"), marker.labelHeight);
    marker.labelTextColor = QColor(element.attribute(
        QStringLiteral("zqxxxt-label-text-color"),
        marker.labelTextColor.name(QColor::HexArgb)));
    marker.labelBackgroundColor = QColor(element.attribute(
        QStringLiteral("zqxxxt-label-background-color"),
        marker.labelBackgroundColor.name(QColor::HexArgb)));
    marker.labelBackgroundImage = element.attribute(
        QStringLiteral("zqxxxt-label-background-image"), marker.labelBackgroundImage);
    marker.labelAlignment = intAttribute(element,
        QStringLiteral("zqxxxt-label-alignment"), marker.labelAlignment);
    marker.modelSourceType = intAttribute(element,
        QStringLiteral("zqxxxt-model-source-type"), marker.modelSourceType);
    marker.modelRotationXDegrees = doubleAttribute(element,
        QStringLiteral("zqxxxt-model-rotation-x"), marker.modelRotationXDegrees);
    marker.modelRotationYDegrees = doubleAttribute(element,
        QStringLiteral("zqxxxt-model-rotation-y"), marker.modelRotationYDegrees);
    marker.modelRotationZDegrees = doubleAttribute(element,
        QStringLiteral("zqxxxt-model-rotation-z"), marker.modelRotationZDegrees);
    marker.modelLodLowMeters = doubleAttribute(element,
        QStringLiteral("zqxxxt-model-lod-low"), marker.modelLodLowMeters);
    marker.modelLodHighMeters = doubleAttribute(element,
        QStringLiteral("zqxxxt-model-lod-high"), marker.modelLodHighMeters);
    marker.modelServiceUrl = element.attribute(
        QStringLiteral("zqxxxt-model-service-url"));
    marker.tiltSourceType = intAttribute(element,
        QStringLiteral("zqxxxt-tilt-source-type"), marker.tiltSourceType);
    marker.tiltRotationDegrees = doubleAttribute(element,
        QStringLiteral("zqxxxt-tilt-rotation"), marker.tiltRotationDegrees);
    marker.tiltServiceUrl = element.attribute(
        QStringLiteral("zqxxxt-tilt-service-url"));
    marker.altitudeMode = intAttribute(element, QStringLiteral("zqxxxt-altitude-mode"), 0);
    marker.viewRangeLowMeters = doubleAttribute(element, QStringLiteral("zqxxxt-view-low"), 0.0);
    marker.viewRangeHighMeters = doubleAttribute(element, QStringLiteral("zqxxxt-view-high"), 100000000.0);
    marker.bindingType = element.attribute(QStringLiteral("zqxxxt-binding-type"));
    marker.bindingId = element.attribute(QStringLiteral("zqxxxt-binding-id"));
    marker.association = QJsonDocument::fromJson(
        element.attribute(QStringLiteral("zqxxxt-association")).toUtf8()).object();
    marker.color = readLegacyColor(element, QStringLiteral("color"),
                                   marker.color, &marker.lineOpacity);
    marker.fillColor = readLegacyColor(element, QStringLiteral("fillColor"),
                                       marker.fillColor, &marker.fillOpacity);
    marker.viewpoint = readLegacyViewpoint(element);
    marker.viewFlyTimeSeconds = marker.viewpoint.flyTimeSeconds;
    for (QDomElement vertex = element.firstChildElement(QStringLiteral("vertex"));
         !vertex.isNull(); vertex = vertex.nextSiblingElement(QStringLiteral("vertex"))) {
        marker.points.append({doubleAttribute(vertex, QStringLiteral("lon"), 0.0),
                              doubleAttribute(vertex, QStringLiteral("lat"), 0.0),
                              doubleAttribute(vertex, QStringLiteral("hei"), 0.0)});
    }
    for (QDomElement row = element.firstChildElement(QStringLiteral("labelItem"));
         !row.isNull(); row = row.nextSiblingElement(QStringLiteral("labelItem"))) {
        Zqxxxt::DeploymentLabelItem item;
        item.text = row.attribute(QStringLiteral("text"), item.text);
        item.width = doubleAttribute(row, QStringLiteral("width"), item.width);
        item.height = doubleAttribute(row, QStringLiteral("height"), item.height);
        item.textSize = doubleAttribute(row, QStringLiteral("textSize"), item.textSize);
        item.textColor = QColor(row.attribute(QStringLiteral("textColor"),
            item.textColor.name(QColor::HexArgb)));
        item.backgroundColor = QColor(row.attribute(QStringLiteral("backgroundColor"),
            item.backgroundColor.name(QColor::HexArgb)));
        item.backgroundImage = row.attribute(QStringLiteral("backgroundImage"),
                                             item.backgroundImage);
        item.alignment = intAttribute(row, QStringLiteral("alignment"), item.alignment);
        marker.labelItems.append(item);
    }
    if (marker.points.isEmpty()) {
        const QDomElement position = element.firstChildElement(QStringLiteral("position"));
        if (!position.isNull()) {
            marker.points.append({doubleAttribute(position, QStringLiteral("lon"), 0.0),
                                  doubleAttribute(position, QStringLiteral("lat"), 0.0),
                                  doubleAttribute(position, QStringLiteral("hei"), 0.0)});
        }
    }
    if (marker.points.isEmpty())
        marker.points.append(Zqxxxt::GeoCoordinate{});
    marker.position = marker.points.constFirst();
    if (marker.text.isEmpty()
        && (marker.geometryType == QStringLiteral("text")
            || marker.geometryType == QStringLiteral("label")))
        marker.text = marker.name;
    return marker;
}

void readLegacyMarkerChildren(const QDomElement &parent, bool plot,
                              const QString &folderName,
                              Zqxxxt::DeploymentData &data)
{
    for (QDomElement child = parent.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement()) {
        if (child.tagName() == QStringLiteral("Folder")) {
            Zqxxxt::DeploymentSceneFolder folder;
            folder.id = child.attribute(QStringLiteral("zqxxxt-id"), newId(QStringLiteral("folder")));
            folder.name = child.attribute(QStringLiteral("title"), QStringLiteral("文件夹"));
            folder.description = child.attribute(QStringLiteral("description"));
            folder.visible = boolAttribute(child, QStringLiteral("visible"), true);
            folder.plot = plot;
            folder.viewpoint = readLegacyViewpoint(child);
            data.sceneFolders.append(folder);
            readLegacyMarkerChildren(child, plot, folder.name, data);
        } else if (child.tagName() != QStringLiteral("viewPoint")) {
            data.markers.append(readLegacyMarker(child, folderName, plot));
        }
    }
}

bool loadLegacyMarkers(const QString &path, bool plot,
                       Zqxxxt::DeploymentData &data, QString &errorMessage)
{
    QDomDocument document;
    if (!readDocument(path, document, errorMessage))
        return false;
    if (document.documentElement().tagName() != QStringLiteral("document")) {
        errorMessage = QStringLiteral("%1 的根节点不是实例工程 document 格式").arg(path);
        return false;
    }
    readLegacyMarkerChildren(document.documentElement(), plot,
        plot ? QStringLiteral("战术标绘") : QStringLiteral("地图标记"), data);
    return true;
}

bool saveLegacyMarkers(const QString &path, bool plot,
                       const Zqxxxt::DeploymentData &data, QString &errorMessage)
{
    QDomDocument document;
    QDomProcessingInstruction declaration = document.createProcessingInstruction(
        QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\""));
    document.appendChild(declaration);
    QDomElement root = document.createElement(QStringLiteral("document"));
    document.appendChild(root);
    QHash<QString, QDomElement> folders;
    for (const Zqxxxt::DeploymentSceneFolder &folder : data.sceneFolders) {
        if (folder.plot != plot || folder.name.trimmed().isEmpty())
            continue;
        QDomElement element = document.createElement(QStringLiteral("Folder"));
        element.setAttribute(QStringLiteral("title"), folder.name);
        element.setAttribute(QStringLiteral("visible"), boolText(folder.visible));
        element.setAttribute(QStringLiteral("description"), folder.description);
        element.setAttribute(QStringLiteral("zqxxxt-id"), folder.id);
        appendLegacyViewpoint(document, element, folder.viewpoint,
                              folder.viewpoint.flyTimeSeconds);
        root.appendChild(element);
        folders.insert(folder.name, element);
    }
    for (const Zqxxxt::DeploymentMarker &marker : data.markers) {
        if (marker.geometryType.startsWith(QStringLiteral("plot-")) != plot)
            continue;
        QString folderName = marker.category.trimmed();
        if (folderName.isEmpty())
            folderName = plot ? QStringLiteral("战术标绘") : QStringLiteral("地图标记");
        QDomElement folder = folders.value(folderName);
        if (folder.isNull()) {
            folder = document.createElement(QStringLiteral("Folder"));
            folder.setAttribute(QStringLiteral("title"), folderName);
            folder.setAttribute(QStringLiteral("visible"), QStringLiteral("true"));
            folder.setAttribute(QStringLiteral("description"), QString());
            Zqxxxt::DeploymentViewpoint view;
            appendLegacyViewpoint(document, folder, view, view.flyTimeSeconds);
            root.appendChild(folder);
            folders.insert(folderName, folder);
        }
        writeLegacyMarker(document, folder, marker);
    }
    return writeDocument(path, document, errorMessage);
}

QString resolveFeatureResource(const QString &configPath,
                               const QString &configuredPath)
{
    QString value = QDir::fromNativeSeparators(configuredPath.trimmed());
    if (value.isEmpty() || value.contains(QStringLiteral("://")))
        return value;
    if (QFileInfo(value).isAbsolute())
        return QDir::cleanPath(value);

    const QDir configDirectory = QFileInfo(configPath).absoluteDir();
    const QDir dataDirectory(configDirectory.absoluteFilePath(QStringLiteral("..")));
    QStringList candidates;
    candidates.append(configDirectory.absoluteFilePath(value));
    if (!value.startsWith(QStringLiteral("../")))
        candidates.append(dataDirectory.absoluteFilePath(value));

    QString dataRelative = value;
    while (dataRelative.startsWith(QStringLiteral("../")))
        dataRelative.remove(0, 3);
    candidates.append(QDir(Zqxxxt::AppPaths::dataDir()).absoluteFilePath(dataRelative));
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
    }
    return QDir::cleanPath(candidates.constFirst());
}

QString cssProperty(const QString &css, const QString &property)
{
    const QRegularExpression expression(
        QStringLiteral("(?:^|[\\s;{])%1\\s*:\\s*[\\\"']?([^;\\\"'\\r\\n]+)")
            .arg(QRegularExpression::escape(property)),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(css);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString resolveCssResource(const QString &css, const QString &property,
                           const QString &configPath)
{
    const QString configured = cssProperty(css, property);
    if (configured.isEmpty())
        return css;
    QString result = css;
    result.replace(configured,
        QDir::fromNativeSeparators(resolveFeatureResource(configPath, configured)),
        Qt::CaseSensitive);
    return result;
}

bool loadSystemFeatureLayers(const QString &path,
                             Zqxxxt::DeploymentData &data,
                             QString &errorMessage)
{
    QDomDocument document;
    if (!readDocument(path, document, errorMessage))
        return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("layers")) {
        errorMessage = QStringLiteral("%1 的根节点不是旧版系统矢量 layers 格式").arg(path);
        return false;
    }

    QHash<QString, Zqxxxt::DeploymentLayer> savedById;
    QHash<QString, Zqxxxt::DeploymentLayer> savedByName;
    for (const Zqxxxt::DeploymentLayer &saved : data.layers) {
        if (!saved.systemLayer
            && !saved.id.startsWith(QStringLiteral("system-feature-"))) {
            continue;
        }
        savedById.insert(saved.id, saved);
        savedByName.insert(saved.name, saved);
    }
    data.layers.erase(std::remove_if(data.layers.begin(), data.layers.end(),
        [](const Zqxxxt::DeploymentLayer &layer) {
            return layer.systemLayer
                || layer.id.startsWith(QStringLiteral("system-feature-"));
        }), data.layers.end());

    int index = 0;
    for (QDomElement element = root.firstChildElement(QStringLiteral("layer"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("layer"))) {
        ++index;
        Zqxxxt::DeploymentLayer layer;
        layer.id = QStringLiteral("system-feature-%1").arg(
            index, 2, 10, QLatin1Char('0'));
        layer.name = element.attribute(QStringLiteral("name")).trimmed();
        layer.kind = QStringLiteral("vector");
        layer.driver = QStringLiteral("ogr");
        layer.systemLayer = true;

        const QDomElement features = element.firstChildElement(QStringLiteral("features"));
        layer.source = resolveFeatureResource(path,
            childText(features, QStringLiteral("url")));
        const QDomElement layout = element.firstChildElement(QStringLiteral("layout"));
        layer.cropFeatures = boolAttribute(layout, QStringLiteral("crop_features"), false);
        layer.tileSizeFactor = childText(layout, QStringLiteral("tile_size_factor"),
                                         QStringLiteral("15")).toDouble();
        if (layer.tileSizeFactor <= 0.0)
            layer.tileSizeFactor = 15.0;
        const QDomElement level = layout.firstChildElement(QStringLiteral("level"));
        layer.minRange = doubleAttribute(level, QStringLiteral("min_range"), 0.0);
        layer.maxRange = doubleAttribute(level, QStringLiteral("max_range"), 100000000.0);
        layer.styleName = level.attribute(QStringLiteral("style"),
                                          QStringLiteral("default")).trimmed();

        const QDomElement style = element.firstChildElement(QStringLiteral("styles"))
            .firstChildElement(QStringLiteral("style"));
        layer.styleCss = style.text().trimmed();
        layer.styleCss = resolveCssResource(layer.styleCss,
                                             QStringLiteral("icon"), path);
        layer.styleCss = resolveCssResource(layer.styleCss,
                                             QStringLiteral("text-font"), path);
        const bool labelLayer = layer.styleCss.contains(
            QStringLiteral("text-content"), Qt::CaseInsensitive);
        const QColor parsedColor(cssProperty(layer.styleCss,
            labelLayer ? QStringLiteral("text-fill") : QStringLiteral("stroke")));
        if (parsedColor.isValid())
            layer.color = parsedColor;
        bool widthOk = false;
        const double parsedWidth = cssProperty(layer.styleCss,
            QStringLiteral("stroke-width")).toDouble(&widthOk);
        if (widthOk)
            layer.lineWidth = parsedWidth;
        layer.visible = childText(element, QStringLiteral("visible"),
                                  QStringLiteral("true"))
            .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0;

        const Zqxxxt::DeploymentLayer *saved = nullptr;
        const auto byId = savedById.constFind(layer.id);
        if (byId != savedById.cend())
            saved = &byId.value();
        else {
            const auto byName = savedByName.constFind(layer.name);
            if (byName != savedByName.cend())
                saved = &byName.value();
        }
        if (saved) {
            layer.visible = saved->visible;
            layer.opacity = saved->opacity;
        }
        if (layer.name.isEmpty() || layer.source.isEmpty()
            || layer.styleCss.isEmpty()) {
            errorMessage = QStringLiteral("系统矢量图层第 %1 项缺少名称、数据源或样式")
                .arg(index);
            return false;
        }
        data.layers.append(layer);
    }
    // Standalone projects may have no system layers, or any number of valid layers.
    // The old client's fixed ten-layer dataset is not a Viewer dependency.
    return true;
}

void readLayerDialogOptions(const QDomElement &element, Zqxxxt::DeploymentLayer &layer)
{
    layer.profile = element.attribute(QStringLiteral("profile"),
        childText(element, QStringLiteral("zqxxxt-profile"), layer.profile));
    layer.quadtreeType = element.attribute(QStringLiteral("quadtreeType"),
        childText(element, QStringLiteral("zqxxxt-quadtree-type"), layer.quadtreeType));
    layer.format = element.attribute(QStringLiteral("format"),
        childText(element, QStringLiteral("zqxxxt-format"), layer.format));
    layer.minLevel = intAttribute(element, QStringLiteral("minLevel"),
        childText(element, QStringLiteral("zqxxxt-min-level"),
                  QString::number(layer.minLevel)).toInt());
    layer.maxLevel = intAttribute(element, QStringLiteral("maxLevel"),
        childText(element, QStringLiteral("zqxxxt-max-level"),
                  QString::number(layer.maxLevel)).toInt());
    layer.tileSize = intAttribute(element, QStringLiteral("tileSize"),
        childText(element, QStringLiteral("zqxxxt-tile-size"),
                  QString::number(layer.tileSize)).toInt());
    layer.opacityEnabled = boolAttribute(element, QStringLiteral("opacityEnabled"),
        childText(element, QStringLiteral("hastransparency"), QStringLiteral("true")) != QStringLiteral("false"));
    layer.cacheConfigured = boolAttribute(element, QStringLiteral("cacheConfigured"),
        !element.firstChildElement(QStringLiteral("cache_enable")).isNull());
    layer.cacheEnabled = boolAttribute(element, QStringLiteral("cacheEnabled"),
        childText(element, QStringLiteral("cache_enable"), QStringLiteral("false")) == QStringLiteral("true"));
    layer.cachePath = element.attribute(QStringLiteral("cachePath"), childText(element, QStringLiteral("cachepath")));
    layer.cacheName = element.attribute(QStringLiteral("cacheName"), childText(element, QStringLiteral("cache_name")));
    layer.minRange = doubleAttribute(element, QStringLiteral("minRange"),
        childText(element, QStringLiteral("min_range"), QString::number(layer.minRange)).toDouble());
    layer.maxRange = doubleAttribute(element, QStringLiteral("maxRange"),
        childText(element, QStringLiteral("max_range"), QString::number(layer.maxRange)).toDouble());
    layer.styleCss = childText(element, QStringLiteral("zqxxxt-style-css"));
    layer.customVectorStyle = boolAttribute(element, QStringLiteral("customVectorStyle"), false);
    layer.fontSize = intAttribute(element, QStringLiteral("fontSize"), 18);
    layer.fontColor = QColor(element.attribute(QStringLiteral("fontColor"), QStringLiteral("#ffffff")));
    layer.fontHaloColor = QColor(element.attribute(QStringLiteral("fontHaloColor"), QStringLiteral("#383a3f")));
    layer.pointIcon = element.attribute(QStringLiteral("pointIcon"));
}

void writeLayerDialogOptions(QDomDocument &document, QDomElement &element,
                             const Zqxxxt::DeploymentLayer &layer, bool legacy)
{
    element.setAttribute(QStringLiteral("profile"), layer.profile);
    element.setAttribute(QStringLiteral("quadtreeType"), layer.quadtreeType);
    element.setAttribute(QStringLiteral("format"), layer.format);
    element.setAttribute(QStringLiteral("minLevel"), layer.minLevel);
    element.setAttribute(QStringLiteral("maxLevel"), layer.maxLevel);
    element.setAttribute(QStringLiteral("tileSize"), layer.tileSize);
    element.setAttribute(QStringLiteral("opacityEnabled"), boolText(layer.opacityEnabled));
    element.setAttribute(QStringLiteral("cacheConfigured"), boolText(layer.cacheConfigured));
    element.setAttribute(QStringLiteral("cacheEnabled"), boolText(layer.cacheEnabled));
    element.setAttribute(QStringLiteral("cachePath"), layer.cachePath);
    element.setAttribute(QStringLiteral("cacheName"), layer.cacheName);
    element.setAttribute(QStringLiteral("minRange"), QString::number(layer.minRange, 'f', 2));
    element.setAttribute(QStringLiteral("maxRange"), QString::number(layer.maxRange, 'f', 2));
    element.setAttribute(QStringLiteral("customVectorStyle"), boolText(layer.customVectorStyle));
    element.setAttribute(QStringLiteral("fontSize"), layer.fontSize);
    element.setAttribute(QStringLiteral("fontColor"), layer.fontColor.name(QColor::HexRgb));
    element.setAttribute(QStringLiteral("fontHaloColor"), layer.fontHaloColor.name(QColor::HexRgb));
    element.setAttribute(QStringLiteral("pointIcon"), layer.pointIcon);
    if (!layer.styleCss.isEmpty()) appendTextElement(document, element, QStringLiteral("zqxxxt-style-css"), layer.styleCss);
    if (legacy) {
        appendTextElement(document, element, QStringLiteral("zqxxxt-profile"), layer.profile);
        appendTextElement(document, element, QStringLiteral("zqxxxt-quadtree-type"), layer.quadtreeType);
        appendTextElement(document, element, QStringLiteral("zqxxxt-format"), layer.format);
        appendTextElement(document, element, QStringLiteral("zqxxxt-min-level"), QString::number(layer.minLevel));
        appendTextElement(document, element, QStringLiteral("zqxxxt-max-level"), QString::number(layer.maxLevel));
        appendTextElement(document, element, QStringLiteral("zqxxxt-tile-size"), QString::number(layer.tileSize));
        appendTextElement(document, element, QStringLiteral("hastransparency"), boolText(layer.opacityEnabled));
        appendTextElement(document, element, QStringLiteral("cache_enable"), boolText(layer.cacheEnabled));
        appendTextElement(document, element, QStringLiteral("cachepath"), layer.cachePath);
        appendTextElement(document, element, QStringLiteral("cache_name"), layer.cacheName);
        appendTextElement(document, element, QStringLiteral("min_range"), QString::number(layer.minRange));
        appendTextElement(document, element, QStringLiteral("max_range"), QString::number(layer.maxRange));
    }
}

bool loadLegacyCustomLayers(const QString &path, Zqxxxt::DeploymentData &data,
                            QString &errorMessage)
{
    QDomDocument document;
    if (!readDocument(path, document, errorMessage))
        return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("layers")) {
        errorMessage = QStringLiteral("%1 的根节点不是实例工程 layers 格式").arg(path);
        return false;
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("layer"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("layer"))) {
        Zqxxxt::DeploymentLayer layer;
        layer.id = element.attribute(QStringLiteral("zqxxxt-id"), newId(QStringLiteral("layer")));
        layer.name = childText(element, QStringLiteral("name"), QStringLiteral("未命名图层"));
        const int layerType = childText(element, QStringLiteral("layer-type"), QStringLiteral("1")).toInt();
        layer.kind = layerType == 0 ? QStringLiteral("imagery")
            : (layerType == 2 ? QStringLiteral("elevation") : QStringLiteral("vector"));
        const QDomElement source = element.firstChildElement(QStringLiteral("data_source"));
        layer.driver = childText(source, QStringLiteral("driver"),
            layer.kind == QStringLiteral("vector") ? QStringLiteral("ogr") : QStringLiteral("gdal"));
        layer.source = childText(source, QStringLiteral("url"));
        layer.visible = childText(element, QStringLiteral("visible"), QStringLiteral("true"))
            .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0;
        layer.opacity = childText(element, QStringLiteral("opacity"), QStringLiteral("1")).toDouble();
        layer.lineWidth = childText(element, QStringLiteral("zqxxxt-line-width"), QStringLiteral("2")).toDouble();
        readLayerDialogOptions(element, layer);
        layer.color = QColor(childText(element, QStringLiteral("zqxxxt-color"), QStringLiteral("#37d6ff")));
        if (!layer.color.isValid())
            layer.color = QColor(QStringLiteral("#37d6ff"));
        if (!layer.source.isEmpty())
            data.layers.append(layer);
    }
    return true;
}

bool saveLegacyCustomLayers(const QString &path,
                            const Zqxxxt::DeploymentData &data,
                            QString &errorMessage)
{
    QDomDocument document;
    QDomProcessingInstruction declaration = document.createProcessingInstruction(
        QStringLiteral("xml"), QStringLiteral("version=\"1.0\""));
    document.appendChild(declaration);
    QDomElement root = document.createElement(QStringLiteral("layers"));
    document.appendChild(root);
    for (const Zqxxxt::DeploymentLayer &layer : data.layers) {
        if (layer.runtimeLayer || layer.systemLayer)
            continue;
        QDomElement element = document.createElement(QStringLiteral("layer"));
        element.setAttribute(QStringLiteral("zqxxxt-id"), layer.id);
        writeLayerDialogOptions(document, element, layer, true);
        appendTextElement(document, element, QStringLiteral("name"), layer.name);
        appendTextElement(document, element, QStringLiteral("visible"), boolText(layer.visible));
        appendTextElement(document, element, QStringLiteral("layer-type"),
            layer.kind == QStringLiteral("imagery") ? QStringLiteral("0")
            : (layer.kind == QStringLiteral("elevation") ? QStringLiteral("2") : QStringLiteral("1")));
        appendTextElement(document, element, QStringLiteral("opacity"),
                          QString::number(layer.opacity, 'f', 3));
        appendTextElement(document, element, QStringLiteral("zqxxxt-color"),
                          layer.color.name(QColor::HexRgb));
        appendTextElement(document, element, QStringLiteral("zqxxxt-line-width"),
                          QString::number(layer.lineWidth, 'f', 3));
        QDomElement source = document.createElement(QStringLiteral("data_source"));
        appendTextElement(document, source, QStringLiteral("driver"), layer.driver);
        appendTextElement(document, source, QStringLiteral("url"), layer.source);
        element.appendChild(source);
        root.appendChild(element);
    }
    return writeDocument(path, document, errorMessage);
}

bool loadLegacyViewpoints(const QString &path, Zqxxxt::DeploymentData &data,
                          QString &errorMessage)
{
    QDomDocument document;
    if (!readDocument(path, document, errorMessage))
        return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("viewpoints")) {
        errorMessage = QStringLiteral("%1 的根节点不是实例工程 viewpoints 格式").arg(path);
        return false;
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("viewpoint"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("viewpoint"))) {
        Zqxxxt::DeploymentViewpoint view;
        view.id = element.attribute(QStringLiteral("zqxxxt-id"), newId(QStringLiteral("view")));
        view.name = childText(element, QStringLiteral("name"), QStringLiteral("未命名视点"));
        view.longitude = childText(element, QStringLiteral("lon"), QStringLiteral("0")).toDouble();
        view.latitude = childText(element, QStringLiteral("lat"), QStringLiteral("0")).toDouble();
        view.altitude = childText(element, QStringLiteral("hei"), QStringLiteral("0")).toDouble();
        view.heading = childText(element, QStringLiteral("heading"), QStringLiteral("0")).toDouble();
        view.pitch = childText(element, QStringLiteral("pitch"), QStringLiteral("-35")).toDouble();
        view.range = childText(element, QStringLiteral("range"), QStringLiteral("0")).toDouble();
        view.flyTimeSeconds = childText(element, QStringLiteral("time"), QStringLiteral("3")).toDouble();
        data.viewpoints.append(view);
    }
    return true;
}

bool saveLegacyViewpoints(const QString &path,
                          const Zqxxxt::DeploymentData &data,
                          QString &errorMessage)
{
    QDomDocument document;
    QDomProcessingInstruction declaration = document.createProcessingInstruction(
        QStringLiteral("xml"), QStringLiteral("version=\"1.0\""));
    document.appendChild(declaration);
    QDomElement root = document.createElement(QStringLiteral("viewpoints"));
    document.appendChild(root);
    for (const Zqxxxt::DeploymentViewpoint &view : data.viewpoints) {
        QDomElement element = document.createElement(QStringLiteral("viewpoint"));
        element.setAttribute(QStringLiteral("zqxxxt-id"), view.id);
        appendTextElement(document, element, QStringLiteral("lon"), QString::number(view.longitude, 'f', 8));
        appendTextElement(document, element, QStringLiteral("lat"), QString::number(view.latitude, 'f', 8));
        appendTextElement(document, element, QStringLiteral("hei"), QString::number(view.altitude, 'f', 3));
        appendTextElement(document, element, QStringLiteral("heading"), QString::number(view.heading, 'f', 3));
        appendTextElement(document, element, QStringLiteral("pitch"), QString::number(view.pitch, 'f', 3));
        appendTextElement(document, element, QStringLiteral("range"), QString::number(view.range, 'f', 3));
        appendTextElement(document, element, QStringLiteral("time"), QString::number(view.flyTimeSeconds, 'f', 3));
        appendTextElement(document, element, QStringLiteral("name"), view.name);
        root.appendChild(element);
    }
    return writeDocument(path, document, errorMessage);
}

bool readDocument(const QString &path, QDomDocument &document, QString &errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("无法读取 %1：%2").arg(path, file.errorString());
        return false;
    }
    QDomDocument::ParseResult parsed = document.setContent(&file);
    if (!parsed) {
        errorMessage = QStringLiteral("XML 格式错误 %1（第 %2 行，第 %3 列）：%4")
            .arg(path).arg(parsed.errorLine).arg(parsed.errorColumn).arg(parsed.errorMessage);
        return false;
    }
    return true;
}

bool writeDocument(const QString &path, const QDomDocument &document, QString &errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("无法写入 %1：%2").arg(path, file.errorString());
        return false;
    }
    if (file.write(document.toByteArray(2)) < 0 || !file.commit()) {
        errorMessage = QStringLiteral("保存 %1 失败：%2").arg(path, file.errorString());
        return false;
    }
    return true;
}

} // namespace

namespace Zqxxxt {

DeploymentXmlStore::DeploymentXmlStore(QString directory)
{
    if (directory.isEmpty())
        directory = qEnvironmentVariable("SHDISPLAY_DEPLOYMENT_DATA_DIR");
    if (directory.isEmpty())
        directory = QDir(AppPaths::dataDir()).filePath(QStringLiteral("Config"));
    m_directory = QDir(directory).absolutePath();
}

QString DeploymentXmlStore::directory() const { return m_directory; }
QString DeploymentXmlStore::fixedFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("fixed-spatial.xml"));
}
QString DeploymentXmlStore::fixedAnnotationsFilePath() const
{
    // Compatibility mirror of the fixed marks/plots from earlier releases.
    // It is deliberately NOT the new squadron annotation store.
    return QDir(m_directory).filePath(QStringLiteral("normal-annotations.xml"));
}
QString DeploymentXmlStore::normalFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("squad-annotations.xml"));
}
QString DeploymentXmlStore::dynamicFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("dynamic-plans.xml"));
}
QString DeploymentXmlStore::customLayerFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("customLayerConfig.xml"));
}
QString DeploymentXmlStore::marksFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("marksConfig.xml"));
}
QString DeploymentXmlStore::plotsFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("plotsConfig.xml"));
}
QString DeploymentXmlStore::viewpointFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("viewPointConfig.xml"));
}
QString DeploymentXmlStore::systemConfigFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("systemConfig.xml"));
}
QString DeploymentXmlStore::featureConfigFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("featureConfig.xml"));
}

bool DeploymentXmlStore::ensureFiles(QString &errorMessage) const
{
    if (!QDir().mkpath(m_directory)) {
        errorMessage = QStringLiteral("无法创建方案演练数据目录：%1").arg(m_directory);
        return false;
    }
    if (!QFile::exists(systemConfigFilePath())) {
        QDomDocument systemDocument;
        QDomProcessingInstruction declaration = systemDocument.createProcessingInstruction(
            QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"UTF-8\""));
        systemDocument.appendChild(declaration);
        QDomElement system = systemDocument.createElement(QStringLiteral("system"));
        systemDocument.appendChild(system);
        QDomElement common = systemDocument.createElement(QStringLiteral("common"));
        common.setAttribute(QStringLiteral("title"), QStringLiteral("ShDisplayViewer"));
        common.setAttribute(QStringLiteral("earth"), QStringLiteral("earth/FreeEarth_flat.earth"));
        common.setAttribute(QStringLiteral("marks"), QStringLiteral("Config/marksConfig.xml"));
        common.setAttribute(QStringLiteral("plots"), QStringLiteral("Config/plotsConfig.xml"));
        common.setAttribute(QStringLiteral("customLayer"), QStringLiteral("Config/customLayerConfig.xml"));
        common.setAttribute(QStringLiteral("featureLayer"), QStringLiteral("Config/featureConfig.xml"));
        system.appendChild(common);
        QDomElement viewPoints = systemDocument.createElement(QStringLiteral("viewPoints"));
        system.appendChild(viewPoints);
        QDomElement initial = systemDocument.createElement(QStringLiteral("viewPointBegin"));
        initial.setAttribute(QStringLiteral("lon"), QStringLiteral("121.414"));
        initial.setAttribute(QStringLiteral("lat"), QStringLiteral("31.1828"));
        initial.setAttribute(QStringLiteral("hei"), QStringLiteral("1500000"));
        initial.setAttribute(QStringLiteral("heading"), QStringLiteral("0"));
        initial.setAttribute(QStringLiteral("pitch"), QStringLiteral("-35"));
        initial.setAttribute(QStringLiteral("range"), QStringLiteral("1500000"));
        initial.setAttribute(QStringLiteral("time"), QStringLiteral("0"));
        viewPoints.appendChild(initial);
        QDomElement ending = initial.cloneNode(true).toElement();
        ending.setTagName(QStringLiteral("viewPointEnd"));
        // New configurations must use the same Home pose as the reference
        // engine; keep the independent startup viewpoint unchanged.
        const MapHomeViewpoint home;
        ending.setAttribute(QStringLiteral("lon"), home.longitude);
        ending.setAttribute(QStringLiteral("lat"), home.latitude);
        ending.setAttribute(QStringLiteral("hei"), home.altitude);
        ending.setAttribute(QStringLiteral("heading"), home.heading);
        ending.setAttribute(QStringLiteral("pitch"), home.pitch);
        ending.setAttribute(QStringLiteral("range"), home.range);
        viewPoints.appendChild(ending);
        if (!writeDocument(systemConfigFilePath(), systemDocument, errorMessage))
            return false;
    }
    {
        QFile systemFile(systemConfigFilePath());
        if (systemFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QDomDocument systemDocument;
            if (systemDocument.setContent(&systemFile)) {
                systemFile.close();
                QDomElement common = systemDocument.documentElement()
                    .firstChildElement(QStringLiteral("common"));
                const QString earth = common.attribute(QStringLiteral("earth")).trimmed();
                bool changed = false;
                if (!common.isNull() && earth.compare(
                        QStringLiteral("earth/ShDisplayViewer.earth"),
                        Qt::CaseInsensitive) == 0) {
                    common.setAttribute(QStringLiteral("earth"),
                                        QStringLiteral("earth/FreeEarth_flat.earth"));
                    changed = true;
                }
                if (!common.isNull() && !common.hasAttribute(QStringLiteral("featureLayer"))) {
                    common.setAttribute(QStringLiteral("featureLayer"),
                                        QStringLiteral("Config/featureConfig.xml"));
                    changed = true;
                }
                if (changed && !writeDocument(systemConfigFilePath(), systemDocument, errorMessage))
                    return false;
            }
        }
    }
    DeploymentData defaults;
    defaults.environment = EnvironmentSettings{};
    DeploymentViewpoint defaultView;
    defaultView.id = QStringLiteral("default-view");
    defaultView.name = QStringLiteral("默认视角");
    defaults.viewpoints.append(defaultView);
    DeploymentPlan plan;
    plan.id = QStringLiteral("sample-plan");
    plan.name = QStringLiteral("示例演练方案");
    plan.description = QStringLiteral("可编辑的本地时间轴预案");
    DeploymentPlanStep step;
    step.id = QStringLiteral("sample-step-1");
    step.name = QStringLiteral("第一阶段");
    step.description = QStringLiteral("设置视角并显示本阶段动态标记");
    step.durationSeconds = 8;
    step.viewpoint = defaultView;
    plan.steps.append(step);
    defaults.plans.append(plan);

    if (!QFile::exists(fixedFilePath()) && !saveFixed(defaults, errorMessage))
        return false;
    if (!QFile::exists(fixedAnnotationsFilePath())) {
        // A missing old mirror must never erase the authoritative scene files.
        if (QFile::exists(marksFilePath()) && !loadLegacyMarkers(marksFilePath(), false, defaults, errorMessage))
            return false;
        if (QFile::exists(plotsFilePath()) && !loadLegacyMarkers(plotsFilePath(), true, defaults, errorMessage))
            return false;
        if (!saveFixedAnnotations(defaults, errorMessage)) return false;
    }
    if (!QFile::exists(dynamicFilePath()) && !saveDynamic(defaults, errorMessage))
        return false;
    return true;
}

bool DeploymentXmlStore::load(DeploymentData &data, QString &errorMessage)
{
    data = DeploymentData{};
    if (!ensureFiles(errorMessage))
        return false;
    const bool needsFixedLegacy = !QFile::exists(customLayerFilePath())
        || !QFile::exists(viewpointFilePath());
    const bool needsMarkerLegacy = !QFile::exists(marksFilePath())
        || !QFile::exists(plotsFilePath());
    if (!loadFixed(data, errorMessage) || !loadFixedAnnotations(data, errorMessage)
        || !loadNormal(data, errorMessage)
        || !loadDynamic(data, errorMessage)
        || (QFile::exists(featureConfigFilePath())
            && !loadSystemFeatureLayers(featureConfigFilePath(), data, errorMessage)))
        return false;
    if (needsFixedLegacy && !saveFixed(data, errorMessage))
        return false;
    if (needsMarkerLegacy && !saveFixedAnnotations(data, errorMessage))
        return false;
    return true;
}

bool DeploymentXmlStore::loadFixed(DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document;
    if (!readDocument(fixedFilePath(), document, errorMessage))
        return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("fixedSpatial")
        || root.attribute(QStringLiteral("schemaVersion")) != QStringLiteral("1")) {
        errorMessage = QStringLiteral("fixed-spatial.xml 的根节点或版本不受支持");
        return false;
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("layers")).firstChildElement(QStringLiteral("layer"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("layer"))) {
        DeploymentLayer layer;
        layer.id = element.attribute(QStringLiteral("id"), newId(QStringLiteral("layer")));
        layer.name = element.attribute(QStringLiteral("name"), QStringLiteral("未命名图层"));
        layer.kind = element.attribute(QStringLiteral("kind"), QStringLiteral("vector"));
        layer.driver = element.attribute(QStringLiteral("driver"));
        layer.source = element.attribute(QStringLiteral("source"));
        layer.color = QColor(element.attribute(QStringLiteral("color"), QStringLiteral("#37d6ff")));
        if (!layer.color.isValid())
            layer.color = QColor(QStringLiteral("#37d6ff"));
        layer.visible = boolAttribute(element, QStringLiteral("visible"), true);
        layer.opacity = doubleAttribute(element, QStringLiteral("opacity"), 1.0);
        layer.lineWidth = doubleAttribute(element, QStringLiteral("lineWidth"), 2.0);
        layer.runtimeLayer = boolAttribute(element, QStringLiteral("runtimeLayer"), false);
        layer.systemLayer = boolAttribute(element, QStringLiteral("systemLayer"), false);
        layer.styleName = element.attribute(QStringLiteral("styleName"),
                                            QStringLiteral("default"));
        layer.minRange = doubleAttribute(element, QStringLiteral("minRange"), 0.0);
        layer.maxRange = doubleAttribute(element, QStringLiteral("maxRange"), 100000000.0);
        layer.tileSizeFactor = doubleAttribute(element, QStringLiteral("tileSizeFactor"), 15.0);
        layer.cropFeatures = boolAttribute(element, QStringLiteral("cropFeatures"), false);
        readLayerDialogOptions(element, layer);
        if (layer.driver.isEmpty()) {
            layer.driver = layer.runtimeLayer ? QStringLiteral("earth")
                : (layer.kind == QStringLiteral("vector") ? QStringLiteral("ogr")
                   : QStringLiteral("gdal"));
        }
        // Remove the two list-only placeholders created by the first UI draft.
        // They never referred to a readable dataset and must not masquerade as
        // real map layers after the osgEarth-backed manager is available.
        if ((layer.id == QStringLiteral("runtime-imagery")
             && layer.source == QStringLiteral("local:base-imagery"))
            || (layer.id == QStringLiteral("deployment-boundary")
                && layer.source == QStringLiteral("local:boundary"))) {
            continue;
        }
        data.layers.append(layer);
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("measurements")).firstChildElement(QStringLiteral("measurement"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("measurement"))) {
        DeploymentMeasurement measurement;
        measurement.id = element.attribute(QStringLiteral("id"), newId(QStringLiteral("measure")));
        measurement.name = element.attribute(QStringLiteral("name"), QStringLiteral("未命名测量"));
        measurement.kind = element.attribute(QStringLiteral("kind"), QStringLiteral("distance"));
        applyLegacyMeasurementStyleDefaults(measurement);
        measurement.value = doubleAttribute(element, QStringLiteral("value"), 0.0);
        measurement.parameter = doubleAttribute(element, QStringLiteral("parameter"), 0.0);
        measurement.secondaryParameter = doubleAttribute(
            element, QStringLiteral("secondaryParameter"), 0.0);
        measurement.showAnalysisBorder = boolAttribute(
            element, QStringLiteral("showAnalysisBorder"), true);
        measurement.analysisSurfaceVisible = boolAttribute(
            element, QStringLiteral("analysisSurfaceVisible"), true);
        measurement.resultText = element.attribute(QStringLiteral("resultText"));
        measurement.terrainBacked = boolAttribute(element, QStringLiteral("terrainBacked"), false);
        measurement.visible = boolAttribute(element, QStringLiteral("visible"), true);
        measurement.lineColor = QColor(element.attribute(
            QStringLiteral("lineColor"), measurement.lineColor.name(QColor::HexRgb)));
        if (!measurement.lineColor.isValid())
            measurement.lineColor = QColor(QStringLiteral("#ffff00"));
        measurement.lineWidth = doubleAttribute(
            element, QStringLiteral("lineWidth"), measurement.lineWidth);
        measurement.lineOpacity = doubleAttribute(
            element, QStringLiteral("lineOpacity"), measurement.lineOpacity);
        measurement.fillColor = QColor(element.attribute(
            QStringLiteral("fillColor"), measurement.fillColor.name(QColor::HexRgb)));
        if (!measurement.fillColor.isValid())
            measurement.fillColor = QColor(QStringLiteral("#e6e600"));
        measurement.fillOpacity = doubleAttribute(
            element, QStringLiteral("fillOpacity"), measurement.fillOpacity);
        measurement.analysisColor = QColor(element.attribute(
            QStringLiteral("analysisColor"),
            measurement.analysisColor.name(QColor::HexRgb)));
        if (!measurement.analysisColor.isValid())
            measurement.analysisColor = QColor(QStringLiteral("#00ff00"));
        measurement.analysisLineWidth = doubleAttribute(
            element, QStringLiteral("analysisLineWidth"),
            measurement.analysisLineWidth);
        measurement.analysisOpacity = doubleAttribute(
            element, QStringLiteral("analysisOpacity"),
            measurement.analysisOpacity);
        for (QDomElement point = element.firstChildElement(QStringLiteral("point"));
             !point.isNull(); point = point.nextSiblingElement(QStringLiteral("point"))) {
            measurement.points.append(readPoint(point));
        }
        for (QDomElement point = element.firstChildElement(QStringLiteral("analysisPoint"));
             !point.isNull(); point = point.nextSiblingElement(QStringLiteral("analysisPoint")))
            measurement.analysisPoints.append(readPoint(point));
        for (QDomElement point = element.firstChildElement(QStringLiteral("analysisSegmentPoint"));
             !point.isNull(); point = point.nextSiblingElement(QStringLiteral("analysisSegmentPoint")))
            measurement.analysisSegments.append(readPoint(point));
        const QStringList states = element.attribute(
            QStringLiteral("analysisSegmentStates")).split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &state : states)
            measurement.analysisSegmentStates.append(state.toInt());
        data.measurements.append(measurement);
    }
    const QDomElement environment = root.firstChildElement(QStringLiteral("environment"));
    if (!environment.isNull()) {
        EnvironmentSettings &value = data.environment;
        const EnvironmentSettings defaults;
        value.timeOfDay = intAttribute(environment, QStringLiteral("timeOfDay"), defaults.timeOfDay);
        value.calendarDate = environment.attribute(QStringLiteral("calendarDate"));
        value.secondOfMinute = qBound(0, intAttribute(environment, QStringLiteral("secondOfMinute"), 0), 59);
        value.syncSystemTime = boolAttribute(environment, QStringLiteral("syncSystemTime"), false);
        value.animateTime = boolAttribute(environment, QStringLiteral("animateTime"), false);
        value.timeSpeed = qBound(1, intAttribute(environment, QStringLiteral("timeSpeed"), 1), 3600);
        value.minuteOfHour = intAttribute(environment, QStringLiteral("minuteOfHour"), defaults.minuteOfHour);
        value.fog = intAttribute(environment, QStringLiteral("fog"), defaults.fog);
        value.rain = intAttribute(environment, QStringLiteral("rain"), defaults.rain);
        value.snow = intAttribute(environment, QStringLiteral("snow"), defaults.snow);
        value.brightness = intAttribute(environment, QStringLiteral("brightness"), defaults.brightness);
        // Earlier client builds had no separate visibility attributes, so a
        // non-zero intensity also meant enabled.
        value.fogVisible = boolAttribute(environment, QStringLiteral("fogVisible"), value.fog > 0);
        value.rainVisible = boolAttribute(environment, QStringLiteral("rainVisible"), value.rain > 0);
        value.snowVisible = boolAttribute(environment, QStringLiteral("snowVisible"), value.snow > 0);
        value.sunVisible = boolAttribute(environment, QStringLiteral("sunVisible"), defaults.sunVisible);
        value.atmosphereVisible = boolAttribute(environment, QStringLiteral("atmosphereVisible"), defaults.atmosphereVisible);
        value.moonVisible = boolAttribute(environment, QStringLiteral("moonVisible"), defaults.moonVisible);
        value.starsVisible = boolAttribute(environment, QStringLiteral("starsVisible"), defaults.starsVisible);
        value.nebulaVisible = boolAttribute(environment, QStringLiteral("nebulaVisible"), defaults.nebulaVisible);
        value.ambientIntensity = doubleAttribute(environment, QStringLiteral("ambientIntensity"), defaults.ambientIntensity);
        value.rainGlobal = boolAttribute(environment, QStringLiteral("rainGlobal"), defaults.rainGlobal);
        value.rainLongitude = doubleAttribute(environment, QStringLiteral("rainLongitude"), defaults.rainLongitude);
        value.rainLatitude = doubleAttribute(environment, QStringLiteral("rainLatitude"), defaults.rainLatitude);
        value.rainRadiusMeters = doubleAttribute(environment, QStringLiteral("rainRadiusMeters"), defaults.rainRadiusMeters);
        value.rainHeightMeters = doubleAttribute(environment, QStringLiteral("rainHeightMeters"), defaults.rainHeightMeters);
        value.snowGlobal = boolAttribute(environment, QStringLiteral("snowGlobal"), defaults.snowGlobal);
        value.snowLongitude = doubleAttribute(environment, QStringLiteral("snowLongitude"), defaults.snowLongitude);
        value.snowLatitude = doubleAttribute(environment, QStringLiteral("snowLatitude"), defaults.snowLatitude);
        value.snowRadiusMeters = doubleAttribute(environment, QStringLiteral("snowRadiusMeters"), defaults.snowRadiusMeters);
        value.snowHeightMeters = doubleAttribute(environment, QStringLiteral("snowHeightMeters"), defaults.snowHeightMeters);
        value.cloudVisible = boolAttribute(environment, QStringLiteral("cloudVisible"), defaults.cloudVisible);
        value.cloudAltitudeMeters = doubleAttribute(environment, QStringLiteral("cloudAltitudeMeters"), defaults.cloudAltitudeMeters);
        value.cloudThicknessMeters = doubleAttribute(environment, QStringLiteral("cloudThicknessMeters"), defaults.cloudThicknessMeters);
        value.cloudDensity = intAttribute(environment, QStringLiteral("cloudDensity"), defaults.cloudDensity);
        value.cloudOpacity = intAttribute(environment, QStringLiteral("cloudOpacity"), defaults.cloudOpacity);
        value.cloudSpeed = doubleAttribute(environment, QStringLiteral("cloudSpeed"), defaults.cloudSpeed);
        value.cloudDirectionDegrees = intAttribute(environment, QStringLiteral("cloudDirectionDegrees"), defaults.cloudDirectionDegrees);
        value.cloudType = environment.attribute(QStringLiteral("cloudType"), defaults.cloudType);
        const auto readColor = [&environment](const QString &name, const QColor &fallback) {
            const QColor color(environment.attribute(name, fallback.name(QColor::HexRgb)));
            return color.isValid() ? color : fallback;
        };
        value.atmosphereColor = readColor(QStringLiteral("atmosphereColor"), defaults.atmosphereColor);
        value.atmosphereOpacity = doubleAttribute(environment, QStringLiteral("atmosphereOpacity"), defaults.atmosphereOpacity);
        value.fogColor = readColor(QStringLiteral("fogColor"), defaults.fogColor);
        value.fogOpacity = doubleAttribute(environment, QStringLiteral("fogOpacity"), defaults.fogOpacity);
        value.rainColor = readColor(QStringLiteral("rainColor"), defaults.rainColor);
        value.rainLineWidth = doubleAttribute(environment, QStringLiteral("rainLineWidth"), defaults.rainLineWidth);
        value.rainOpacity = doubleAttribute(environment, QStringLiteral("rainOpacity"), defaults.rainOpacity);
        value.snowColor = readColor(QStringLiteral("snowColor"), defaults.snowColor);
        value.snowParticleSize = doubleAttribute(environment, QStringLiteral("snowParticleSize"), defaults.snowParticleSize);
        value.snowOpacity = doubleAttribute(environment, QStringLiteral("snowOpacity"), defaults.snowOpacity);
        value.cloudColor = readColor(QStringLiteral("cloudColor"), defaults.cloudColor);
        value.sunColor = readColor(QStringLiteral("sunColor"), defaults.sunColor);
        value.sunSize = doubleAttribute(environment, QStringLiteral("sunSize"), defaults.sunSize);
        value.sunOpacity = doubleAttribute(environment, QStringLiteral("sunOpacity"), defaults.sunOpacity);
        value.moonColor = readColor(QStringLiteral("moonColor"), defaults.moonColor);
        value.moonSize = doubleAttribute(environment, QStringLiteral("moonSize"), defaults.moonSize);
        value.moonOpacity = doubleAttribute(environment, QStringLiteral("moonOpacity"), defaults.moonOpacity);
        value.starColor = readColor(QStringLiteral("starColor"), defaults.starColor);
        value.starSize = doubleAttribute(environment, QStringLiteral("starSize"), defaults.starSize);
        value.starOpacity = doubleAttribute(environment, QStringLiteral("starOpacity"), defaults.starOpacity);
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("viewpoints")).firstChildElement(QStringLiteral("viewpoint"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("viewpoint"))) {
        data.viewpoints.append(readViewpoint(element));
    }
    if (QFile::exists(customLayerFilePath())) {
        data.layers.erase(std::remove_if(data.layers.begin(), data.layers.end(),
            [](const DeploymentLayer &layer) {
                return !layer.runtimeLayer && !layer.systemLayer;
            }), data.layers.end());
        if (!loadLegacyCustomLayers(customLayerFilePath(), data, errorMessage))
            return false;
    }
    if (QFile::exists(viewpointFilePath())) {
        data.viewpoints.clear();
        if (!loadLegacyViewpoints(viewpointFilePath(), data, errorMessage))
            return false;
    }
    return true;
}

bool DeploymentXmlStore::loadFixedAnnotations(DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document;
    if (!readDocument(fixedAnnotationsFilePath(), document, errorMessage))
        return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("normalAnnotations")
        || root.attribute(QStringLiteral("schemaVersion")) != QStringLiteral("1")) {
        errorMessage = QStringLiteral("normal-annotations.xml 的根节点或版本不受支持");
        return false;
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("markers")).firstChildElement(QStringLiteral("marker"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("marker"))) {
        DeploymentMarker marker = readMarker(element);
        if (marker.id.isEmpty())
            marker.id = newId(QStringLiteral("marker"));
        data.markers.append(marker);
    }
    for (QDomElement element = root.firstChildElement(QStringLiteral("imsBindingRules")).firstChildElement(QStringLiteral("rule"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("rule"))) {
        DeploymentBindingRule rule;
        rule.entityType = element.attribute(QStringLiteral("entityType"));
        rule.label = element.attribute(QStringLiteral("label"), rule.entityType);
        rule.color = QColor(element.attribute(QStringLiteral("color"), QStringLiteral("#37d6ff")));
        if (!rule.color.isValid())
            rule.color = QColor(QStringLiteral("#37d6ff"));
        rule.visible = boolAttribute(element, QStringLiteral("visible"), true);
        if (!rule.entityType.isEmpty())
            data.bindingRules.append(rule);
    }
    if (QFile::exists(marksFilePath()) || QFile::exists(plotsFilePath())) {
        data.markers.clear();
        data.sceneFolders.clear();
        if (QFile::exists(marksFilePath())
            && !loadLegacyMarkers(marksFilePath(), false, data, errorMessage))
            return false;
        if (QFile::exists(plotsFilePath())
            && !loadLegacyMarkers(plotsFilePath(), true, data, errorMessage))
            return false;
    }
    return true;
}

bool DeploymentXmlStore::loadNormal(DeploymentData &data, QString &errorMessage) const
{
    // Do not import the old shared markers: they belong to the fixed scene
    // list. Only preserve the existing IMS display rules on first creation.
    if (!QFile::exists(normalFilePath())) return saveNormal(data, errorMessage);
    QDomDocument document;
    if (!readDocument(normalFilePath(), document, errorMessage)) return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("squadAnnotations")
        || root.attribute(QStringLiteral("schemaVersion")) != QStringLiteral("1")) {
        errorMessage = QStringLiteral("squad-annotations.xml 的根节点或版本不受支持");
        return false;
    }
    data.normalLocalVisible = boolAttribute(root, QStringLiteral("visible"), true);
    data.normalFolders.clear();
    data.normalMarkers.clear();
    QSet<QString> folderIds;
    for (QDomElement element = root.firstChildElement(QStringLiteral("folders")).firstChildElement(QStringLiteral("folder"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("folder"))) {
        DeploymentLocalFolder folder;
        folder.id = element.attribute(QStringLiteral("id"));
        folder.parentId = element.attribute(QStringLiteral("parentId"));
        folder.name = element.attribute(QStringLiteral("name"));
        folder.description = element.attribute(QStringLiteral("description"));
        folder.visible = boolAttribute(element, QStringLiteral("visible"), true);
        if (folder.id.isEmpty() || folderIds.contains(folder.id) || folder.name.trimmed().isEmpty()) {
            errorMessage = QStringLiteral("中队标注文件夹 ID 重复或名称为空"); return false;
        }
        folderIds.insert(folder.id);
        data.normalFolders.append(folder);
    }
    for (const auto &folder : data.normalFolders) {
        QSet<QString> visited{folder.id};
        QString parent = folder.parentId;
        while (!parent.isEmpty()) {
            const auto found = std::find_if(data.normalFolders.cbegin(), data.normalFolders.cend(),
                [&parent](const DeploymentLocalFolder &value) { return value.id == parent; });
            if (found == data.normalFolders.cend() || visited.contains(parent)) {
                errorMessage = QStringLiteral("中队标注文件夹的父级不存在或形成循环"); return false;
            }
            visited.insert(parent);
            parent = found->parentId;
        }
    }
    QSet<QString> markerIds;
    for (QDomElement element = root.firstChildElement(QStringLiteral("markers")).firstChildElement(QStringLiteral("marker"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("marker"))) {
        DeploymentMarker marker = readMarker(element);
        if (marker.id.isEmpty()) marker.id = newId(QStringLiteral("squad-marker"));
        if (!folderIds.contains(marker.folderId) || markerIds.contains(marker.id)) {
            errorMessage = QStringLiteral("中队标注的所属文件夹不存在或标注 ID 重复"); return false;
        }
        markerIds.insert(marker.id);
        data.normalMarkers.append(marker);
    }
    const auto ims = root.firstChildElement(QStringLiteral("imsLiveData"));
    data.imsLiveVisible = boolAttribute(ims, QStringLiteral("visible"), true);
    if (!ims.isNull()) {
        data.bindingRules.clear();
        for (QDomElement element = ims.firstChildElement(QStringLiteral("rule"));
             !element.isNull(); element = element.nextSiblingElement(QStringLiteral("rule"))) {
            DeploymentBindingRule rule;
            rule.entityType = element.attribute(QStringLiteral("entityType"));
            rule.label = element.attribute(QStringLiteral("label"), rule.entityType);
            rule.color = QColor(element.attribute(QStringLiteral("color"), QStringLiteral("#37d6ff")));
            if (!rule.color.isValid()) rule.color = QColor(QStringLiteral("#37d6ff"));
            rule.visible = boolAttribute(element, QStringLiteral("visible"), true);
            if (!rule.entityType.isEmpty()) data.bindingRules.append(rule);
        }
    }
    return true;
}

bool DeploymentXmlStore::saveNormal(const DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document(QStringLiteral("zqxxxt-squad-annotations"));
    QDomElement root = document.createElement(QStringLiteral("squadAnnotations"));
    root.setAttribute(QStringLiteral("schemaVersion"), QStringLiteral("1"));
    root.setAttribute(QStringLiteral("coordinateSystem"), QStringLiteral("EPSG:4326"));
    root.setAttribute(QStringLiteral("visible"), boolText(data.normalLocalVisible));
    document.appendChild(root);
    auto folders = document.createElement(QStringLiteral("folders"));
    for (const auto &folder : data.normalFolders) {
        auto element = document.createElement(QStringLiteral("folder"));
        element.setAttribute(QStringLiteral("id"), folder.id);
        element.setAttribute(QStringLiteral("parentId"), folder.parentId);
        element.setAttribute(QStringLiteral("name"), folder.name);
        element.setAttribute(QStringLiteral("description"), folder.description);
        element.setAttribute(QStringLiteral("visible"), boolText(folder.visible));
        folders.appendChild(element);
    }
    root.appendChild(folders);
    auto markers = document.createElement(QStringLiteral("markers"));
    for (const auto &marker : data.normalMarkers) writeMarker(document, markers, marker);
    root.appendChild(markers);
    auto ims = document.createElement(QStringLiteral("imsLiveData"));
    ims.setAttribute(QStringLiteral("visible"), boolText(data.imsLiveVisible));
    for (const auto &rule : data.bindingRules) {
        auto element = document.createElement(QStringLiteral("rule"));
        element.setAttribute(QStringLiteral("entityType"), rule.entityType);
        element.setAttribute(QStringLiteral("label"), rule.label);
        element.setAttribute(QStringLiteral("color"), rule.color.name(QColor::HexRgb));
        element.setAttribute(QStringLiteral("visible"), boolText(rule.visible));
        ims.appendChild(element);
    }
    root.appendChild(ims);
    return writeDocument(normalFilePath(), document, errorMessage);
}

bool DeploymentXmlStore::loadDynamic(DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document;
    if (!readDocument(dynamicFilePath(), document, errorMessage))
        return false;
    const QDomElement root = document.documentElement();
    if (root.tagName() != QStringLiteral("dynamicPlans")
        || root.attribute(QStringLiteral("schemaVersion")) != QStringLiteral("1")) {
        errorMessage = QStringLiteral("dynamic-plans.xml 的根节点或版本不受支持");
        return false;
    }
    for (QDomElement planElement = root.firstChildElement(QStringLiteral("plan"));
         !planElement.isNull(); planElement = planElement.nextSiblingElement(QStringLiteral("plan"))) {
        DeploymentPlan plan;
        plan.id = planElement.attribute(QStringLiteral("id"), newId(QStringLiteral("plan")));
        plan.name = planElement.attribute(QStringLiteral("name"), QStringLiteral("未命名预案"));
        plan.description = planElement.attribute(QStringLiteral("description"));
        plan.loop = boolAttribute(planElement, QStringLiteral("loop"), false);
        for (QDomElement stepElement = planElement.firstChildElement(QStringLiteral("step"));
             !stepElement.isNull(); stepElement = stepElement.nextSiblingElement(QStringLiteral("step"))) {
            DeploymentPlanStep step;
            step.id = stepElement.attribute(QStringLiteral("id"), newId(QStringLiteral("step")));
            step.name = stepElement.attribute(QStringLiteral("name"), QStringLiteral("未命名步骤"));
            step.description = stepElement.attribute(QStringLiteral("description"));
            step.durationSeconds = qMax(1, intAttribute(stepElement, QStringLiteral("durationSeconds"), 8));
            step.markerDurationSeconds = qMax(1, intAttribute(stepElement, QStringLiteral("markerDurationSeconds"), 4));
            step.sequentialDisplay = stepElement.attribute(QStringLiteral("markerDisplay"))
                == QStringLiteral("sequential");
            const QDomElement viewpoint = stepElement.firstChildElement(QStringLiteral("viewpoint"));
            step.viewpoint = viewpoint.isNull() ? DeploymentViewpoint{} : readViewpoint(viewpoint);
            for (QDomElement marker = stepElement.firstChildElement(QStringLiteral("markers")).firstChildElement(QStringLiteral("marker"));
                 !marker.isNull(); marker = marker.nextSiblingElement(QStringLiteral("marker"))) {
                DeploymentMarker value = readMarker(marker);
                if (value.id.isEmpty())
                    value.id = newId(QStringLiteral("dynamic-marker"));
                step.markers.append(value);
            }
            plan.steps.append(step);
        }
        data.plans.append(plan);
    }
    return true;
}

bool DeploymentXmlStore::saveFixed(const DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document(QStringLiteral("zqxxxt-fixed-spatial"));
    QDomElement root = document.createElement(QStringLiteral("fixedSpatial"));
    root.setAttribute(QStringLiteral("schemaVersion"), QStringLiteral("1"));
    root.setAttribute(QStringLiteral("coordinateSystem"), QStringLiteral("EPSG:4326"));
    document.appendChild(root);
    QDomElement layers = document.createElement(QStringLiteral("layers"));
    for (const DeploymentLayer &layer : data.layers) {
        QDomElement element = document.createElement(QStringLiteral("layer"));
        element.setAttribute(QStringLiteral("id"), layer.id);
        element.setAttribute(QStringLiteral("name"), layer.name);
        element.setAttribute(QStringLiteral("kind"), layer.kind);
        element.setAttribute(QStringLiteral("driver"), layer.driver);
        element.setAttribute(QStringLiteral("source"), layer.source);
        element.setAttribute(QStringLiteral("color"), layer.color.name(QColor::HexRgb));
        element.setAttribute(QStringLiteral("visible"), boolText(layer.visible));
        element.setAttribute(QStringLiteral("opacity"), QString::number(layer.opacity, 'f', 2));
        element.setAttribute(QStringLiteral("lineWidth"), QString::number(layer.lineWidth, 'f', 2));
        element.setAttribute(QStringLiteral("runtimeLayer"), boolText(layer.runtimeLayer));
        element.setAttribute(QStringLiteral("systemLayer"), boolText(layer.systemLayer));
        writeLayerDialogOptions(document, element, layer, false);
        if (layer.systemLayer) {
            element.setAttribute(QStringLiteral("styleName"), layer.styleName);
            element.setAttribute(QStringLiteral("minRange"),
                                 QString::number(layer.minRange, 'f', 2));
            element.setAttribute(QStringLiteral("maxRange"),
                                 QString::number(layer.maxRange, 'f', 2));
            element.setAttribute(QStringLiteral("tileSizeFactor"),
                                 QString::number(layer.tileSizeFactor, 'f', 2));
            element.setAttribute(QStringLiteral("cropFeatures"),
                                 boolText(layer.cropFeatures));
        }
        layers.appendChild(element);
    }
    root.appendChild(layers);
    QDomElement measurements = document.createElement(QStringLiteral("measurements"));
    for (const DeploymentMeasurement &measurement : data.measurements) {
        QDomElement element = document.createElement(QStringLiteral("measurement"));
        element.setAttribute(QStringLiteral("id"), measurement.id);
        element.setAttribute(QStringLiteral("name"), measurement.name);
        element.setAttribute(QStringLiteral("kind"), measurement.kind);
        element.setAttribute(QStringLiteral("value"), QString::number(measurement.value, 'f', 3));
        element.setAttribute(QStringLiteral("parameter"), QString::number(measurement.parameter, 'f', 3));
        element.setAttribute(QStringLiteral("secondaryParameter"),
                             QString::number(measurement.secondaryParameter, 'f', 3));
        element.setAttribute(QStringLiteral("showAnalysisBorder"),
                             boolText(measurement.showAnalysisBorder));
        element.setAttribute(QStringLiteral("analysisSurfaceVisible"),
                             boolText(measurement.analysisSurfaceVisible));
        element.setAttribute(QStringLiteral("resultText"), measurement.resultText);
        element.setAttribute(QStringLiteral("terrainBacked"), boolText(measurement.terrainBacked));
        element.setAttribute(QStringLiteral("visible"), boolText(measurement.visible));
        element.setAttribute(QStringLiteral("lineColor"),
                             measurement.lineColor.name(QColor::HexRgb));
        element.setAttribute(QStringLiteral("lineWidth"),
                             QString::number(measurement.lineWidth, 'f', 2));
        element.setAttribute(QStringLiteral("lineOpacity"),
                             QString::number(measurement.lineOpacity, 'f', 2));
        element.setAttribute(QStringLiteral("fillColor"),
                             measurement.fillColor.name(QColor::HexRgb));
        element.setAttribute(QStringLiteral("fillOpacity"),
                             QString::number(measurement.fillOpacity, 'f', 2));
        element.setAttribute(QStringLiteral("analysisColor"),
                             measurement.analysisColor.name(QColor::HexRgb));
        element.setAttribute(QStringLiteral("analysisLineWidth"),
                             QString::number(measurement.analysisLineWidth, 'f', 2));
        element.setAttribute(QStringLiteral("analysisOpacity"),
                              QString::number(measurement.analysisOpacity, 'f', 2));
        QStringList segmentStates;
        segmentStates.reserve(measurement.analysisSegmentStates.size());
        for (int state : measurement.analysisSegmentStates)
            segmentStates.append(QString::number(state));
        if (!segmentStates.isEmpty())
            element.setAttribute(QStringLiteral("analysisSegmentStates"),
                                 segmentStates.join(QLatin1Char(',')));
        for (const GeoCoordinate &point : measurement.points)
            writePoint(document, element, point);
        for (const GeoCoordinate &point : measurement.analysisPoints)
            writePoint(document, element, point, QStringLiteral("analysisPoint"));
        for (const GeoCoordinate &point : measurement.analysisSegments)
            writePoint(document, element, point, QStringLiteral("analysisSegmentPoint"));
        measurements.appendChild(element);
    }
    root.appendChild(measurements);
    QDomElement environment = document.createElement(QStringLiteral("environment"));
    environment.setAttribute(QStringLiteral("timeOfDay"), data.environment.timeOfDay);
    environment.setAttribute(QStringLiteral("minuteOfHour"), data.environment.minuteOfHour);
    environment.setAttribute(QStringLiteral("calendarDate"), data.environment.calendarDate);
    environment.setAttribute(QStringLiteral("secondOfMinute"), data.environment.secondOfMinute);
    environment.setAttribute(QStringLiteral("syncSystemTime"), boolText(data.environment.syncSystemTime));
    environment.setAttribute(QStringLiteral("animateTime"), boolText(data.environment.animateTime));
    environment.setAttribute(QStringLiteral("timeSpeed"), data.environment.timeSpeed);
    environment.setAttribute(QStringLiteral("fog"), data.environment.fog);
    environment.setAttribute(QStringLiteral("rain"), data.environment.rain);
    environment.setAttribute(QStringLiteral("snow"), data.environment.snow);
    environment.setAttribute(QStringLiteral("brightness"), data.environment.brightness);
    environment.setAttribute(QStringLiteral("fogVisible"), boolText(data.environment.fogVisible));
    environment.setAttribute(QStringLiteral("rainVisible"), boolText(data.environment.rainVisible));
    environment.setAttribute(QStringLiteral("snowVisible"), boolText(data.environment.snowVisible));
    environment.setAttribute(QStringLiteral("sunVisible"), boolText(data.environment.sunVisible));
    environment.setAttribute(QStringLiteral("atmosphereVisible"), boolText(data.environment.atmosphereVisible));
    environment.setAttribute(QStringLiteral("moonVisible"), boolText(data.environment.moonVisible));
    environment.setAttribute(QStringLiteral("starsVisible"), boolText(data.environment.starsVisible));
    environment.setAttribute(QStringLiteral("nebulaVisible"), boolText(data.environment.nebulaVisible));
    environment.setAttribute(QStringLiteral("ambientIntensity"), data.environment.ambientIntensity);
    environment.setAttribute(QStringLiteral("rainGlobal"), boolText(data.environment.rainGlobal));
    environment.setAttribute(QStringLiteral("rainLongitude"), QString::number(data.environment.rainLongitude, 'f', 8));
    environment.setAttribute(QStringLiteral("rainLatitude"), QString::number(data.environment.rainLatitude, 'f', 8));
    environment.setAttribute(QStringLiteral("rainRadiusMeters"), QString::number(data.environment.rainRadiusMeters, 'f', 1));
    environment.setAttribute(QStringLiteral("rainHeightMeters"), QString::number(data.environment.rainHeightMeters, 'f', 1));
    environment.setAttribute(QStringLiteral("snowGlobal"), boolText(data.environment.snowGlobal));
    environment.setAttribute(QStringLiteral("snowLongitude"), QString::number(data.environment.snowLongitude, 'f', 8));
    environment.setAttribute(QStringLiteral("snowLatitude"), QString::number(data.environment.snowLatitude, 'f', 8));
    environment.setAttribute(QStringLiteral("snowRadiusMeters"), QString::number(data.environment.snowRadiusMeters, 'f', 1));
    environment.setAttribute(QStringLiteral("snowHeightMeters"), QString::number(data.environment.snowHeightMeters, 'f', 1));
    environment.setAttribute(QStringLiteral("cloudVisible"), boolText(data.environment.cloudVisible));
    environment.setAttribute(QStringLiteral("cloudAltitudeMeters"), QString::number(data.environment.cloudAltitudeMeters, 'f', 1));
    environment.setAttribute(QStringLiteral("cloudThicknessMeters"), QString::number(data.environment.cloudThicknessMeters, 'f', 1));
    environment.setAttribute(QStringLiteral("cloudDensity"), data.environment.cloudDensity);
    environment.setAttribute(QStringLiteral("cloudOpacity"), data.environment.cloudOpacity);
    environment.setAttribute(QStringLiteral("cloudSpeed"), QString::number(data.environment.cloudSpeed, 'f', 1));
    environment.setAttribute(QStringLiteral("cloudDirectionDegrees"), data.environment.cloudDirectionDegrees);
    environment.setAttribute(QStringLiteral("cloudType"), data.environment.cloudType);
    environment.setAttribute(QStringLiteral("atmosphereColor"), data.environment.atmosphereColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("atmosphereOpacity"), QString::number(data.environment.atmosphereOpacity, 'f', 2));
    environment.setAttribute(QStringLiteral("fogColor"), data.environment.fogColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("fogOpacity"), QString::number(data.environment.fogOpacity, 'f', 2));
    environment.setAttribute(QStringLiteral("rainColor"), data.environment.rainColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("rainLineWidth"), QString::number(data.environment.rainLineWidth, 'f', 2));
    environment.setAttribute(QStringLiteral("rainOpacity"), QString::number(data.environment.rainOpacity, 'f', 2));
    environment.setAttribute(QStringLiteral("snowColor"), data.environment.snowColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("snowParticleSize"), QString::number(data.environment.snowParticleSize, 'f', 2));
    environment.setAttribute(QStringLiteral("snowOpacity"), QString::number(data.environment.snowOpacity, 'f', 2));
    environment.setAttribute(QStringLiteral("cloudColor"), data.environment.cloudColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("sunColor"), data.environment.sunColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("sunSize"), QString::number(data.environment.sunSize, 'f', 2));
    environment.setAttribute(QStringLiteral("sunOpacity"), QString::number(data.environment.sunOpacity, 'f', 2));
    environment.setAttribute(QStringLiteral("moonColor"), data.environment.moonColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("moonSize"), QString::number(data.environment.moonSize, 'f', 2));
    environment.setAttribute(QStringLiteral("moonOpacity"), QString::number(data.environment.moonOpacity, 'f', 2));
    environment.setAttribute(QStringLiteral("starColor"), data.environment.starColor.name(QColor::HexRgb));
    environment.setAttribute(QStringLiteral("starSize"), QString::number(data.environment.starSize, 'f', 2));
    environment.setAttribute(QStringLiteral("starOpacity"), QString::number(data.environment.starOpacity, 'f', 2));
    root.appendChild(environment);
    QDomElement viewpoints = document.createElement(QStringLiteral("viewpoints"));
    for (const DeploymentViewpoint &view : data.viewpoints) {
        QDomElement element = document.createElement(QStringLiteral("viewpoint"));
        writeViewpointAttributes(element, view);
        viewpoints.appendChild(element);
    }
    root.appendChild(viewpoints);
    if (!writeDocument(fixedFilePath(), document, errorMessage))
        return false;
    return saveLegacyCustomLayers(customLayerFilePath(), data, errorMessage)
        && saveLegacyViewpoints(viewpointFilePath(), data, errorMessage);
}

bool DeploymentXmlStore::saveFixedAnnotations(const DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document(QStringLiteral("zqxxxt-normal-annotations"));
    QDomElement root = document.createElement(QStringLiteral("normalAnnotations"));
    root.setAttribute(QStringLiteral("schemaVersion"), QStringLiteral("1"));
    root.setAttribute(QStringLiteral("coordinateSystem"), QStringLiteral("EPSG:4326"));
    document.appendChild(root);
    QDomElement markers = document.createElement(QStringLiteral("markers"));
    for (const DeploymentMarker &marker : data.markers)
        writeMarker(document, markers, marker);
    root.appendChild(markers);
    QDomElement rules = document.createElement(QStringLiteral("imsBindingRules"));
    for (const DeploymentBindingRule &rule : data.bindingRules) {
        QDomElement element = document.createElement(QStringLiteral("rule"));
        element.setAttribute(QStringLiteral("entityType"), rule.entityType);
        element.setAttribute(QStringLiteral("label"), rule.label);
        element.setAttribute(QStringLiteral("color"), rule.color.name(QColor::HexRgb));
        element.setAttribute(QStringLiteral("visible"), boolText(rule.visible));
        rules.appendChild(element);
    }
    root.appendChild(rules);
    if (!writeDocument(fixedAnnotationsFilePath(), document, errorMessage))
        return false;
    return saveLegacyMarkers(marksFilePath(), false, data, errorMessage)
        && saveLegacyMarkers(plotsFilePath(), true, data, errorMessage);
}

bool DeploymentXmlStore::saveDynamic(const DeploymentData &data, QString &errorMessage) const
{
    QDomDocument document(QStringLiteral("zqxxxt-dynamic-plans"));
    QDomElement root = document.createElement(QStringLiteral("dynamicPlans"));
    root.setAttribute(QStringLiteral("schemaVersion"), QStringLiteral("1"));
    root.setAttribute(QStringLiteral("coordinateSystem"), QStringLiteral("EPSG:4326"));
    document.appendChild(root);
    for (const DeploymentPlan &plan : data.plans) {
        QDomElement planElement = document.createElement(QStringLiteral("plan"));
        planElement.setAttribute(QStringLiteral("id"), plan.id);
        planElement.setAttribute(QStringLiteral("name"), plan.name);
        planElement.setAttribute(QStringLiteral("description"), plan.description);
        planElement.setAttribute(QStringLiteral("loop"), boolText(plan.loop));
        for (const DeploymentPlanStep &step : plan.steps) {
            QDomElement stepElement = document.createElement(QStringLiteral("step"));
            stepElement.setAttribute(QStringLiteral("id"), step.id);
            stepElement.setAttribute(QStringLiteral("name"), step.name);
            stepElement.setAttribute(QStringLiteral("description"), step.description);
            stepElement.setAttribute(QStringLiteral("durationSeconds"), step.durationSeconds);
            stepElement.setAttribute(QStringLiteral("markerDurationSeconds"), step.markerDurationSeconds);
            stepElement.setAttribute(QStringLiteral("markerDisplay"), step.sequentialDisplay
                ? QStringLiteral("sequential") : QStringLiteral("simultaneous"));
            QDomElement viewpoint = document.createElement(QStringLiteral("viewpoint"));
            writeViewpointAttributes(viewpoint, step.viewpoint);
            stepElement.appendChild(viewpoint);
            QDomElement markers = document.createElement(QStringLiteral("markers"));
            for (const DeploymentMarker &marker : step.markers)
                writeMarker(document, markers, marker);
            stepElement.appendChild(markers);
            planElement.appendChild(stepElement);
        }
        root.appendChild(planElement);
    }
    return writeDocument(dynamicFilePath(), document, errorMessage);
}

bool DeploymentXmlStore::saveAll(const DeploymentData &data, QString &errorMessage) const
{
    return saveFixed(data, errorMessage) && saveFixedAnnotations(data, errorMessage)
        && saveNormal(data, errorMessage) && saveDynamic(data, errorMessage);
}

} // namespace Zqxxxt
