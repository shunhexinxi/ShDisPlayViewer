#include "modules/zqbs/PlotGeometry.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double MetersPerDegree = 111320.0;

struct LocalTransform
{
    Zqxxxt::GeoCoordinate origin;
    double longitudeScale = MetersPerDegree;

    explicit LocalTransform(const Zqxxxt::GeoCoordinate &value)
        : origin(value)
    {
        longitudeScale = std::max(1000.0,
            MetersPerDegree * std::cos(value.latitude * Pi / 180.0));
    }

    QPointF local(const Zqxxxt::GeoCoordinate &point) const
    {
        return {(point.longitude - origin.longitude) * longitudeScale,
                (point.latitude - origin.latitude) * MetersPerDegree};
    }

    Zqxxxt::GeoCoordinate geographic(const QPointF &point, double altitude) const
    {
        return {origin.longitude + point.x() / longitudeScale,
                origin.latitude + point.y() / MetersPerDegree, altitude};
    }
};

QPointF normalized(const QPointF &point)
{
    const double length = std::hypot(point.x(), point.y());
    return length < 1e-8 ? QPointF(1.0, 0.0) : point / length;
}

QPointF normal(const QPointF &direction)
{
    return {-direction.y(), direction.x()};
}

QVector<QPointF> toLocal(const QVector<Zqxxxt::GeoCoordinate> &points,
                         const LocalTransform &transform)
{
    QVector<QPointF> result;
    result.reserve(points.size());
    for (const auto &point : points)
        result.append(transform.local(point));
    return result;
}

QVector<Zqxxxt::GeoCoordinate> toGeographic(
    const QVector<QPointF> &points, const LocalTransform &transform,
    double altitude)
{
    QVector<Zqxxxt::GeoCoordinate> result;
    result.reserve(points.size());
    for (const QPointF &point : points)
        result.append(transform.geographic(point, altitude));
    return result;
}

QVector<QPointF> rectangleRing(const QPointF &a, const QPointF &b)
{
    const QRectF bounds(a, b);
    const QRectF rect = bounds.normalized();
    return {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
}

QVector<QPointF> ellipseRing(const QPointF &majorPoint,
                             const QPointF &minorPoint,
                             int samples = 72)
{
    const double majorRadius = std::max(0.1,
        std::hypot(majorPoint.x(), majorPoint.y()));
    const double minorRadius = std::max(0.1,
        std::hypot(minorPoint.x(), minorPoint.y()));
    const double rotation = std::atan2(majorPoint.y(), majorPoint.x());
    QVector<QPointF> result;
    result.reserve(samples);
    for (int index = 0; index < samples; ++index) {
        const double angle = 2.0 * Pi * index / samples;
        const double x = majorRadius * std::cos(angle);
        const double y = minorRadius * std::sin(angle);
        result.append({x * std::cos(rotation) - y * std::sin(rotation),
                       x * std::sin(rotation) + y * std::cos(rotation)});
    }
    return result;
}

QVector<QPointF> circularArc(const QPointF &startPoint,
                             const QPointF &endPoint,
                             bool includeCenter)
{
    const double radius = std::max(0.1,
        std::hypot(startPoint.x(), startPoint.y()));
    double startAngle = std::atan2(startPoint.y(), startPoint.x());
    double endAngle = std::atan2(endPoint.y(), endPoint.x());
    double sweep = endAngle - startAngle;
    while (sweep <= 0.0)
        sweep += 2.0 * Pi;
    // CircleNode in the reference client always uses the minor arc between
    // the two handles. If the click order describes the long way round,
    // reverse the endpoints instead of drawing a 180..360 degree sector.
    if (sweep > Pi) {
        startAngle = endAngle;
        sweep = 2.0 * Pi - sweep;
    }
    const int segments = std::max(12,
        static_cast<int>(std::ceil(72.0 * sweep / (2.0 * Pi))));
    QVector<QPointF> result;
    result.reserve(segments + (includeCenter ? 2 : 1));
    if (includeCenter)
        result.append(QPointF());
    for (int index = 0; index <= segments; ++index) {
        const double angle = startAngle + sweep * index / segments;
        result.append({radius * std::cos(angle), radius * std::sin(angle)});
    }
    return result;
}

QVector<QPointF> twoPointArrow(const QPointF &tail, const QPointF &tip,
                               double widthScale, bool dovetail)
{
    const QPointF axis = tip - tail;
    const double length = std::max(1.0, std::hypot(axis.x(), axis.y()));
    const QPointF direction = normalized(axis);
    const QPointF side = normal(direction);
    const double shaftHalf = length * widthScale;
    const double headHalf = length * std::max(0.14, widthScale * 2.15);
    const double headLength = length * 0.27;
    const QPointF neck = tip - direction * headLength;
    QVector<QPointF> polygon{
        tail + side * shaftHalf,
        neck + side * shaftHalf,
        neck + side * headHalf,
        tip,
        neck - side * headHalf,
        neck - side * shaftHalf,
        tail - side * shaftHalf
    };
    if (dovetail)
        polygon.append(tail + direction * std::min(headLength * 0.45, length * 0.12));
    return polygon;
}

QVector<QPointF> straightLineArrow(const QPointF &tail, const QPointF &tip)
{
    const QPointF direction = normalized(tip - tail);
    const QPointF side = normal(direction);
    const double length = std::max(1.0, std::hypot((tip - tail).x(), (tip - tail).y()));
    const QPointF head = tip - direction * length * 0.22;
    return {tail, tip, head + side * length * 0.10, tip,
            head - side * length * 0.10};
}

QVector<QPointF> smoothOpen(const QVector<QPointF> &input, int samples = 10)
{
    if (input.size() < 3)
        return input;
    QVector<QPointF> result;
    for (qsizetype i = 0; i + 1 < input.size(); ++i) {
        const QPointF p0 = input.at(std::max<qsizetype>(0, i - 1));
        const QPointF p1 = input.at(i);
        const QPointF p2 = input.at(i + 1);
        const QPointF p3 = input.at(std::min<qsizetype>(input.size() - 1, i + 2));
        for (int step = 0; step < samples; ++step) {
            const double t = double(step) / samples;
            const double t2 = t * t;
            const double t3 = t2 * t;
            result.append((p1 * 2.0
                + (p2 - p0) * t
                + (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * t2
                + (-p0 + p1 * 3.0 - p2 * 3.0 + p3) * t3) * 0.5);
        }
    }
    result.append(input.constLast());
    return result;
}

QVector<QPointF> smoothClosed(const QVector<QPointF> &input, int samples = 10)
{
    if (input.size() < 3)
        return input;
    QVector<QPointF> extended = input;
    extended.prepend(input.constLast());
    extended.append(input.constFirst());
    extended.append(input.at(1));
    QVector<QPointF> result;
    for (qsizetype i = 1; i + 2 < extended.size(); ++i) {
        const QPointF p0 = extended.at(i - 1);
        const QPointF p1 = extended.at(i);
        const QPointF p2 = extended.at(i + 1);
        const QPointF p3 = extended.at(i + 2);
        for (int step = 0; step < samples; ++step) {
            const double t = double(step) / samples;
            const double t2 = t * t;
            const double t3 = t2 * t;
            result.append((p1 * 2.0 + (p2 - p0) * t
                + (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * t2
                + (-p0 + p1 * 3.0 - p2 * 3.0 + p3) * t3) * 0.5);
        }
    }
    return result;
}

QVector<QPointF> corridorArrow(QVector<QPointF> centerLine,
                               bool smooth, bool dovetail, double widthScale)
{
    if (centerLine.size() < 2)
        return centerLine;
    if (smooth)
        centerLine = smoothOpen(centerLine);
    double length = 0.0;
    for (qsizetype i = 1; i < centerLine.size(); ++i)
        length += std::hypot((centerLine.at(i) - centerLine.at(i - 1)).x(),
                            (centerLine.at(i) - centerLine.at(i - 1)).y());
    const double halfWidth = std::max(0.5, length * widthScale);
    const QPointF tip = centerLine.constLast();
    const QPointF lastDirection = normalized(tip - centerLine.at(centerLine.size() - 2));
    const double headLength = std::max(halfWidth * 4.0, length * 0.16);
    const QPointF neck = tip - lastDirection * headLength;

    while (centerLine.size() > 2) {
        const QPointF before = centerLine.at(centerLine.size() - 2);
        if (std::hypot((tip - before).x(), (tip - before).y()) >= headLength)
            break;
        centerLine.removeAt(centerLine.size() - 2);
    }
    centerLine.last() = neck;

    QVector<QPointF> left;
    QVector<QPointF> right;
    for (qsizetype i = 0; i < centerLine.size(); ++i) {
        QPointF direction;
        if (i == 0)
            direction = normalized(centerLine.at(1) - centerLine.at(0));
        else if (i + 1 == centerLine.size())
            direction = normalized(centerLine.at(i) - centerLine.at(i - 1));
        else
            direction = normalized(normalized(centerLine.at(i) - centerLine.at(i - 1))
                                   + normalized(centerLine.at(i + 1) - centerLine.at(i)));
        const QPointF side = normal(direction);
        left.append(centerLine.at(i) + side * halfWidth);
        right.append(centerLine.at(i) - side * halfWidth);
    }
    const QPointF headSide = normal(lastDirection);
    QVector<QPointF> result = left;
    result.append(neck + headSide * halfWidth * 2.5);
    result.append(tip);
    result.append(neck - headSide * halfWidth * 2.5);
    for (auto it = right.crbegin(); it != right.crend(); ++it)
        result.append(*it);
    if (dovetail)
        result.append(centerLine.constFirst() + normalized(centerLine.at(1)
            - centerLine.constFirst()) * halfWidth * 1.5);
    return result;
}

QVector<QPointF> roundedRectangle(const QPointF &a, const QPointF &b)
{
    const QRectF bounds(a, b);
    const QRectF rect = bounds.normalized();
    const double radius = std::min(rect.width(), rect.height()) * 0.18;
    if (radius < 0.1)
        return {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
    QVector<QPointF> result;
    const QPointF centers[]{
        {rect.right() - radius, rect.top() + radius},
        {rect.right() - radius, rect.bottom() - radius},
        {rect.left() + radius, rect.bottom() - radius},
        {rect.left() + radius, rect.top() + radius}
    };
    const double starts[]{-Pi / 2.0, 0.0, Pi / 2.0, Pi};
    for (int corner = 0; corner < 4; ++corner)
        for (int step = 0; step <= 8; ++step) {
            const double angle = starts[corner] + (Pi / 2.0) * step / 8.0;
            result.append(centers[corner]
                + QPointF(std::cos(angle), std::sin(angle)) * radius);
        }
    return result;
}

QVector<QPointF> gatheringPlace(const QPointF &a, const QPointF &b)
{
    const QPointF center = (a + b) * 0.5;
    const QPointF axis = b - a;
    const double radiusX = std::max(1.0, std::hypot(axis.x(), axis.y()) * 0.5);
    const double radiusY = radiusX * 0.62;
    const double rotation = std::atan2(axis.y(), axis.x());
    QVector<QPointF> result;
    for (int index = 0; index < 64; ++index) {
        const double angle = 2.0 * Pi * index / 64.0;
        const double pulse = 1.0 + 0.10 * std::sin(angle * 3.0);
        const QPointF raw(radiusX * std::cos(angle) * pulse,
                          radiusY * std::sin(angle) * pulse);
        result.append(center + QPointF(raw.x() * std::cos(rotation)
            - raw.y() * std::sin(rotation), raw.x() * std::sin(rotation)
            + raw.y() * std::cos(rotation)));
    }
    return result;
}

QVector<QPointF> sectorSearch(const QPointF &center, const QPointF &edge)
{
    const QPointF axis = edge - center;
    const double radius = std::max(1.0, std::hypot(axis.x(), axis.y()));
    const double bearing = std::atan2(axis.y(), axis.x());
    QVector<QPointF> result{center};
    for (int ray = 0; ray < 7; ++ray) {
        const double angle = bearing - Pi / 3.0 + ray * Pi / 9.0;
        const QPointF outer = center + QPointF(std::cos(angle), std::sin(angle)) * radius;
        result.append(outer);
        if (ray != 6)
            result.append(center);
    }
    return result;
}

QVector<QPointF> appendLineArrow(QVector<QPointF> line)
{
    if (line.size() < 2)
        return line;
    const QPointF tip = line.constLast();
    const QPointF direction = normalized(tip - line.at(line.size() - 2));
    double length = 0.0;
    for (qsizetype i = 1; i < line.size(); ++i)
        length += std::hypot((line.at(i) - line.at(i - 1)).x(),
                            (line.at(i) - line.at(i - 1)).y());
    const QPointF head = tip - direction * std::max(1.0, length * 0.10);
    const QPointF side = normal(direction) * std::max(0.5, length * 0.045);
    line.append(head + side);
    line.append(tip);
    line.append(head - side);
    return line;
}

QVector<QPointF> bezierCurve(const QVector<QPointF> &controls,
                             int samples = 72)
{
    if (controls.size() < 2)
        return controls;
    QVector<QPointF> result;
    result.reserve(samples + 1);
    for (int sample = 0; sample <= samples; ++sample) {
        QVector<QPointF> work = controls;
        const double t = double(sample) / samples;
        for (qsizetype level = work.size() - 1; level > 0; --level)
            for (qsizetype index = 0; index < level; ++index)
                work[index] = work.at(index) * (1.0 - t)
                    + work.at(index + 1) * t;
        result.append(work.constFirst());
    }
    return result;
}

QVector<QPointF> parallelSearch(const QVector<QPointF> &controls)
{
    if (controls.size() < 3)
        return controls;
    const QPointF along = normalized(controls.at(1) - controls.at(0));
    const QPointF across = normal(along);
    QVector<QPointF> result{controls.constFirst()};
    QPointF previous = controls.constFirst();
    for (qsizetype index = 1; index < controls.size(); ++index) {
        const QPointF direction = index % 2 == 1 ? along : across;
        previous += direction
            * QPointF::dotProduct(controls.at(index) - previous, direction);
        result.append(previous);
    }
    return appendLineArrow(result);
}

} // namespace

namespace Zqxxxt {

bool isPlotAnnotation(const QString &kind)
{
    return kind.startsWith(QStringLiteral("plot-"));
}

bool annotationIsArea(const QString &kind)
{
    static const QSet<QString> lineKinds{
        QStringLiteral("line"), QStringLiteral("arc"),
        QStringLiteral("plot-parallel-search"),
        QStringLiteral("plot-sector-search"),
        QStringLiteral("plot-straight-line-arrow"),
        QStringLiteral("plot-bezier-arrow"),
        QStringLiteral("plot-polyline-arrow"),
        QStringLiteral("plot-cardinal-arrow")
    };
    if (lineKinds.contains(kind))
        return false;
    return kind == QStringLiteral("polygon") || kind == QStringLiteral("rectangle")
        || kind == QStringLiteral("circle") || kind == QStringLiteral("ellipse")
        || kind == QStringLiteral("sector") || isPlotAnnotation(kind);
}

int annotationMinimumPoints(const QString &kind)
{
    if (kind == QStringLiteral("point") || kind == QStringLiteral("text")
        || kind == QStringLiteral("label") || kind == QStringLiteral("legacy-point")
        || kind == QStringLiteral("model") || kind == QStringLiteral("billboard")
        || kind == QStringLiteral("overlay") || kind == QStringLiteral("tilt-model")
        || kind == QStringLiteral("particle") || kind == QStringLiteral("arc")
        || kind == QStringLiteral("ellipse") || kind == QStringLiteral("rectangle")
        || kind == QStringLiteral("sector") || kind == QStringLiteral("circle"))
        return 1;
    if (kind == QStringLiteral("line")
        || kind == QStringLiteral("plot-straight-arrow")
        || kind == QStringLiteral("plot-dovetail-diagonal-arrow")
        || kind == QStringLiteral("plot-dovetail-straight-arrow")
        || kind == QStringLiteral("plot-diagonal-arrow")
        || kind == QStringLiteral("plot-rounded-rect")
        || kind == QStringLiteral("plot-gathering-place")
        || kind == QStringLiteral("plot-sector-search")
        || kind == QStringLiteral("plot-straight-line-arrow")
        || kind == QStringLiteral("plot-polyline-arrow"))
        return 2;
    if (kind == QStringLiteral("plot-double-arrow"))
        return 4;
    return 3;
}

int annotationAutoFinishPoints(const QString &kind)
{
    static const QSet<QString> localizedMarks{
        QStringLiteral("point"), QStringLiteral("text"),
        QStringLiteral("label"), QStringLiteral("legacy-point"),
        QStringLiteral("model"), QStringLiteral("billboard"),
        QStringLiteral("overlay"), QStringLiteral("arc"),
        QStringLiteral("ellipse"), QStringLiteral("rectangle"),
        QStringLiteral("sector"), QStringLiteral("circle"),
        QStringLiteral("tilt-model"), QStringLiteral("particle")
    };
    if (localizedMarks.contains(kind))
        return 1;
    // Feature marks (line/polygon) and FePlots retain their multi-point,
    // right-click/double-click completion flow.
    return 0;
}

QVector<GeoCoordinate> referenceMarkerControlPoints(
    const QString &kind, const QVector<GeoCoordinate> &clickedPoints,
    double viewRangeMeters)
{
    if (clickedPoints.size() != 1)
        return clickedPoints;
    const GeoCoordinate center = clickedPoints.constFirst();
    const LocalTransform transform(center);
    const double markSize = std::max(10.0, std::abs(viewRangeMeters) / 10.0);
    const auto point = [&transform, &center](double east, double north) {
        return transform.geographic(QPointF(east, north), center.altitude);
    };

    if (kind == QStringLiteral("arc")) {
        return {center, point(markSize, 0.0), point(0.0, markSize)};
    }
    if (kind == QStringLiteral("ellipse")) {
        return {center, point(markSize / 2.0, 0.0),
                point(0.0, markSize / 4.0)};
    }
    if (kind == QStringLiteral("sector")) {
        const double radius = markSize / 2.0;
        return {center, point(radius, 0.0), point(0.0, radius)};
    }
    if (kind == QStringLiteral("circle")) {
        return {center, point(markSize / 2.0, 0.0)};
    }
    if (kind == QStringLiteral("rectangle")) {
        const double half = markSize / 2.0;
        return {point(-half, -half), point(half, half)};
    }
    return clickedPoints;
}

QString annotationDisplayName(const QString &kind)
{
    const QHash<QString, QString> names{
        {QStringLiteral("point"), QStringLiteral("点标记")},
        {QStringLiteral("line"), QStringLiteral("线标记")},
        {QStringLiteral("polygon"), QStringLiteral("多边形标记")},
        {QStringLiteral("model"), QStringLiteral("模型标记")},
        {QStringLiteral("billboard"), QStringLiteral("广告牌标记")},
        {QStringLiteral("overlay"), QStringLiteral("遮盖标签")},
        {QStringLiteral("arc"), QStringLiteral("弧形标记")},
        {QStringLiteral("ellipse"), QStringLiteral("椭圆标记")},
        {QStringLiteral("rectangle"), QStringLiteral("矩形标记")},
        {QStringLiteral("sector"), QStringLiteral("扇形标记")},
        {QStringLiteral("circle"), QStringLiteral("圆形标记")},
        {QStringLiteral("text"), QStringLiteral("文本标记")},
        {QStringLiteral("tilt-model"), QStringLiteral("倾斜摄影")},
        {QStringLiteral("particle"), QStringLiteral("粒子标记")},
        {QStringLiteral("label"), QStringLiteral("标牌标记")},
        {QStringLiteral("plot-straight-arrow"), QStringLiteral("直箭头")},
        {QStringLiteral("plot-straight-multi-arrow"), QStringLiteral("多点直箭头")},
        {QStringLiteral("plot-dovetail-diagonal-arrow"), QStringLiteral("燕尾斜箭头")},
        {QStringLiteral("plot-dovetail-diagonal-multi-arrow"), QStringLiteral("多点燕尾斜箭头")},
        {QStringLiteral("plot-dovetail-straight-arrow"), QStringLiteral("燕尾直箭头")},
        {QStringLiteral("plot-dovetail-straight-multi-arrow"), QStringLiteral("多点燕尾直箭头")},
        {QStringLiteral("plot-diagonal-arrow"), QStringLiteral("斜箭头")},
        {QStringLiteral("plot-diagonal-multi-arrow"), QStringLiteral("多点斜箭头")},
        {QStringLiteral("plot-double-arrow"), QStringLiteral("双箭头")},
        {QStringLiteral("plot-closed-curve"), QStringLiteral("闭合曲线")},
        {QStringLiteral("plot-gathering-place"), QStringLiteral("聚集区")},
        {QStringLiteral("plot-rounded-rect"), QStringLiteral("圆角矩形")},
        {QStringLiteral("plot-bezier-arrow"), QStringLiteral("贝塞尔曲线箭头")},
        {QStringLiteral("plot-polyline-arrow"), QStringLiteral("折线箭头")},
        {QStringLiteral("plot-parallel-search"), QStringLiteral("平行搜索区")},
        {QStringLiteral("plot-sector-search"), QStringLiteral("扇形搜索区")},
        {QStringLiteral("plot-cardinal-arrow"), QStringLiteral("基数曲线箭头")},
        {QStringLiteral("plot-straight-line-arrow"), QStringLiteral("直线箭头")}
    };
    return names.value(kind, kind);
}

ExpandedAnnotationGeometry expandAnnotationGeometry(
    const QString &kind, const QVector<GeoCoordinate> &controlPoints)
{
    ExpandedAnnotationGeometry result{kind, controlPoints};
    if (controlPoints.isEmpty())
        return result;
    const LocalTransform transform(controlPoints.constFirst());
    const QVector<QPointF> local = toLocal(controlPoints, transform);
    QVector<QPointF> expanded;

    if (kind == QStringLiteral("point") || kind == QStringLiteral("legacy-point")
        || kind == QStringLiteral("model") || kind == QStringLiteral("billboard")
        || kind == QStringLiteral("overlay") || kind == QStringLiteral("tilt-model")
        || kind == QStringLiteral("particle")) {
        result.geometryType = QStringLiteral("point");
        result.points = {controlPoints.constFirst()};
        return result;
    }
    if (kind == QStringLiteral("text") || kind == QStringLiteral("label")) {
        result.geometryType = kind;
        result.points = {controlPoints.constFirst()};
        return result;
    }
    if (kind == QStringLiteral("line")) {
        result.geometryType = QStringLiteral("line");
        return result;
    }
    if (kind == QStringLiteral("polygon")) {
        result.geometryType = QStringLiteral("polygon");
        return result;
    }
    if (kind == QStringLiteral("rectangle") && local.size() >= 2) {
        result.geometryType = QStringLiteral("polygon");
        expanded = rectangleRing(local.at(0), local.at(1));
    } else if (kind == QStringLiteral("circle") && local.size() >= 2) {
        result.geometryType = QStringLiteral("polygon");
        const double radius = std::max(0.1,
            std::hypot(local.at(1).x(), local.at(1).y()));
        expanded = ellipseRing(QPointF(radius, 0.0), QPointF(0.0, radius));
    } else if (kind == QStringLiteral("ellipse") && local.size() >= 3) {
        result.geometryType = QStringLiteral("polygon");
        expanded = ellipseRing(local.at(1), local.at(2));
    } else if (kind == QStringLiteral("arc") && local.size() >= 3) {
        result.geometryType = QStringLiteral("line");
        expanded = circularArc(local.at(1), local.at(2), false);
    } else if (kind == QStringLiteral("sector") && local.size() >= 3) {
        result.geometryType = QStringLiteral("polygon");
        expanded = circularArc(local.at(1), local.at(2), true);
    } else if (!isPlotAnnotation(kind)) {
        return result;
    }

    if (!isPlotAnnotation(kind)) {
        if (!expanded.isEmpty())
            result.points = toGeographic(expanded, transform,
                                         controlPoints.constFirst().altitude);
        return result;
    }

    result.geometryType = annotationIsArea(kind)
        ? QStringLiteral("polygon") : QStringLiteral("line");

    if (local.size() >= 2) {
        if (kind == QStringLiteral("plot-straight-arrow"))
            expanded = twoPointArrow(local.at(0), local.at(1), 0.065, false);
        else if (kind == QStringLiteral("plot-dovetail-diagonal-arrow"))
            expanded = twoPointArrow(local.at(0), local.at(1), 0.085, true);
        else if (kind == QStringLiteral("plot-dovetail-straight-arrow"))
            expanded = twoPointArrow(local.at(0), local.at(1), 0.060, true);
        else if (kind == QStringLiteral("plot-diagonal-arrow"))
            expanded = twoPointArrow(local.at(0), local.at(1), 0.090, false);
        else if (kind == QStringLiteral("plot-rounded-rect"))
            expanded = roundedRectangle(local.at(0), local.at(1));
        else if (kind == QStringLiteral("plot-gathering-place"))
            expanded = gatheringPlace(local.at(0), local.at(1));
        else if (kind == QStringLiteral("plot-sector-search"))
            expanded = sectorSearch(local.at(0), local.at(1));
        else if (kind == QStringLiteral("plot-straight-line-arrow"))
            expanded = straightLineArrow(local.at(0), local.at(1));
        else if (kind == QStringLiteral("plot-straight-multi-arrow"))
            expanded = corridorArrow(local, false, false, 0.035);
        else if (kind == QStringLiteral("plot-dovetail-diagonal-multi-arrow"))
            expanded = corridorArrow(local, true, true, 0.048);
        else if (kind == QStringLiteral("plot-dovetail-straight-multi-arrow"))
            expanded = corridorArrow(local, false, true, 0.035);
        else if (kind == QStringLiteral("plot-diagonal-multi-arrow"))
            expanded = corridorArrow(local, true, false, 0.048);
        else if (kind == QStringLiteral("plot-bezier-arrow"))
            expanded = appendLineArrow(bezierCurve(local));
        else if (kind == QStringLiteral("plot-polyline-arrow"))
            expanded = appendLineArrow(local);
        else if (kind == QStringLiteral("plot-cardinal-arrow"))
            expanded = appendLineArrow(smoothOpen(local));
        else if (kind == QStringLiteral("plot-parallel-search"))
            expanded = parallelSearch(local);
        else if (kind == QStringLiteral("plot-closed-curve"))
            expanded = smoothClosed(local);
        else if (kind == QStringLiteral("plot-double-arrow") && local.size() >= 4) {
            const QPointF base = (local.at(0) + local.at(1)) * 0.5;
            const QVector<QPointF> left = twoPointArrow(base, local.at(2), 0.045, false);
            const QVector<QPointF> right = twoPointArrow(base, local.at(3), 0.045, false);
            expanded = {local.at(0), left.at(1), left.at(2), left.at(3),
                        left.at(4), base, right.at(2), right.at(3),
                        right.at(4), right.at(5), local.at(1)};
        }
    }
    if (!expanded.isEmpty())
        result.points = toGeographic(expanded, transform,
                                     controlPoints.constFirst().altitude);
    return result;
}

} // namespace Zqxxxt
