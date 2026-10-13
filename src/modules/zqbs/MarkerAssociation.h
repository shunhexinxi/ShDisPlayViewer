#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>

namespace Zqxxxt::MarkerAssociation {

inline QString customRtspTarget()
{
    return QStringLiteral("__custom_rtsp__");
}

inline bool validRtspUrl(const QString &address)
{
    const QUrl url(address.trimmed(), QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    return url.isValid() && !url.host().isEmpty()
        && (scheme == QStringLiteral("rtsp") || scheme == QStringLiteral("rtsps"));
}

inline QJsonValue decodedValue(const QJsonObject &point)
{
    const QByteArray json = point.value(QStringLiteral("latestValueJson")).toString().toUtf8();
    if (json.isEmpty()) return {};
    const QJsonArray wrapped = QJsonDocument::fromJson(QByteArrayLiteral("[") + json
        + QByteArrayLiteral("]")).array();
    return wrapped.size() == 1 ? wrapped.first() : QJsonValue{};
}

inline bool fresh(const QJsonObject &point)
{
    if (point.isEmpty() || !point.value(QStringLiteral("enabled")).toBool()) return false;
    const QDateTime sampled = QDateTime::fromString(
        point.value(QStringLiteral("sampledAt")).toString(), Qt::ISODate);
    if (!sampled.isValid()) return false;
    const int staleSeconds = qMax(1, point.value(QStringLiteral("staleAfterSeconds")).toInt(60));
    const qint64 age = sampled.secsTo(QDateTime::currentDateTimeUtc());
    return age >= -30 && age <= staleSeconds;
}

inline QString formattedValue(const QJsonObject &point, int decimals = -1)
{
    if (!fresh(point)) return QStringLiteral("--");
    const QJsonValue value = decodedValue(point);
    if (value.isDouble()) {
        const double number = value.toDouble();
        return decimals >= 0 ? QString::number(number, 'f', decimals)
                             : QString::number(number, 'g', 10);
    }
    if (value.isBool()) return value.toBool() ? QStringLiteral("是") : QStringLiteral("否");
    if (value.isString()) return value.toString();
    if (value.isObject()) return QString::fromUtf8(
        QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray()) return QString::fromUtf8(
        QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return QStringLiteral("--");
}

inline QStringList pointIds(const QJsonObject &association)
{
    QSet<QString> seen;
    QStringList result;
    const auto add = [&seen, &result](const QString &id) {
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            result.append(id);
        }
    };
    for (const QJsonValue &value : association.value(QStringLiteral("fields")).toArray())
        add(value.toObject().value(QStringLiteral("pointId")).toString());
    for (const QJsonValue &value : association.value(QStringLiteral("alarmRules")).toArray())
        add(value.toObject().value(QStringLiteral("pointId")).toString());
    add(association.value(QStringLiteral("alarmPointId")).toString());
    add(association.value(QStringLiteral("thresholdPointId")).toString());
    return result;
}

inline QString label(const QString &markerName, const QJsonObject &association,
                     const QHash<QString, QJsonObject> &points)
{
    if (association.value(QStringLiteral("type")).toString() == QStringLiteral("ALARM"))
        return markerName;
    QString result = association.value(QStringLiteral("template")).toString().trimmed();
    const bool custom = !result.isEmpty();
    if (!custom) result = markerName;
    result.replace(QStringLiteral("{name}"), markerName);
    for (const QJsonValue &value : association.value(QStringLiteral("fields")).toArray()) {
        const QJsonObject field = value.toObject();
        if (!field.value(QStringLiteral("visible")).toBool(true)) continue;
        const QString name = field.value(QStringLiteral("label")).toString().trimmed();
        if (name.isEmpty()) continue;
        const QJsonObject point = points.value(field.value(QStringLiteral("pointId")).toString());
        const QString unit = field.value(QStringLiteral("unit")).toString().trimmed().isEmpty()
            ? point.value(QStringLiteral("unit")).toString()
            : field.value(QStringLiteral("unit")).toString().trimmed();
        const QString text = formattedValue(point, field.value(QStringLiteral("decimals")).toInt(-1));
        if (custom) {
            result.replace(QStringLiteral("{%1}").arg(name), text);
            result.replace(QStringLiteral("{%1单位}").arg(name), unit);
        } else {
            result += QStringLiteral("\n%1  %2%3").arg(name, text,
                unit.isEmpty() ? QString() : QStringLiteral(" ") + unit);
        }
    }
    return result;
}

inline QString nodeText(const QString &content, const QString &markerName,
                        const QJsonObject &association,
                        const QHash<QString, QJsonObject> &points)
{
    QHash<QString, QString> replacements;
    replacements.insert(QStringLiteral("name"), markerName);
    for (const QJsonValue &value : association.value(QStringLiteral("fields")).toArray()) {
        const QJsonObject field = value.toObject();
        if (!field.value(QStringLiteral("visible")).toBool(true)) continue;
        const QString name = field.value(QStringLiteral("label")).toString().trimmed();
        if (name.isEmpty() || name == QStringLiteral("name")) continue;
        const QJsonObject point = points.value(field.value(QStringLiteral("pointId")).toString());
        const QString unit = field.value(QStringLiteral("unit")).toString().trimmed();
        replacements.insert(name, formattedValue(point,
            field.value(QStringLiteral("decimals")).toInt(-1)));
        replacements.insert(name + QStringLiteral("单位"), unit.isEmpty()
            ? point.value(QStringLiteral("unit")).toString() : unit);
    }
    // Only parse the saved template. Values returned by a point can themselves
    // contain braces and must never be interpreted as another placeholder.
    static const QRegularExpression placeholder(QStringLiteral("\\{([^{}]+)\\}"));
    auto matches = placeholder.globalMatch(content);
    QString result;
    qsizetype offset = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += content.mid(offset, match.capturedStart() - offset);
        const auto value = replacements.constFind(match.captured(1));
        result += value == replacements.cend() ? match.captured() : value.value();
        offset = match.capturedEnd();
    }
    return result + content.mid(offset);
}

inline QStringList labelNodes(const DeploymentMarker &marker,
                              const QHash<QString, QJsonObject> &points)
{
    QStringList texts{marker.text.isEmpty() ? marker.name : marker.text};
    for (const auto &item : marker.labelItems) texts.append(item.text);
    if (marker.association.value(QStringLiteral("type")).toString() != QStringLiteral("LABEL")
        || !marker.association.value(QStringLiteral("imsBound")).toBool(true)) return texts;
    const QString titleTemplate = marker.association.value(QStringLiteral("template")).toString();
    if (!titleTemplate.trimmed().isEmpty()) texts[0] = titleTemplate;
    for (QString &text : texts) text = nodeText(text, marker.name, marker.association, points);
    return texts;
}

// Apply to the transient scene copy only. Saved parent/child contents remain
// templates, so the next data sample and reopening the editor can reuse them.
inline void applyLabelNodes(DeploymentMarker &renderMarker,
                            const QHash<QString, QJsonObject> &points)
{
    const QStringList texts = labelNodes(renderMarker, points);
    renderMarker.associationText = texts.first();
    for (qsizetype index = 0; index < renderMarker.labelItems.size(); ++index)
        renderMarker.labelItems[index].text = texts.at(index + 1);
}

inline QJsonValue ruleValue(const QString &text)
{
    const QByteArray json = QByteArrayLiteral("[") + text.trimmed().toUtf8() + QByteArrayLiteral("]");
    const QJsonArray parsed = QJsonDocument::fromJson(json).array();
    return parsed.size() == 1 ? parsed.first() : QJsonValue(text.trimmed());
}

inline QJsonObject alarmRule(const QJsonObject &association,
                             const QHash<QString, QJsonObject> &points)
{
    QJsonObject selected;
    for (const QJsonValue &value : association.value(QStringLiteral("alarmRules")).toArray()) {
        const QJsonObject rule = value.toObject();
        const QJsonObject point = points.value(rule.value(QStringLiteral("pointId")).toString());
        if (!fresh(point)) continue;
        const QJsonValue actual = decodedValue(point);
        const QJsonValue expected = ruleValue(rule.value(QStringLiteral("value")).toString());
        if (actual == expected || (actual.isString() && actual.toString() == expected.toVariant().toString()))
            selected = rule; // Later matching rows take precedence.
    }
    return selected;
}

inline bool alert(const QJsonObject &association,
                  const QHash<QString, QJsonObject> &points)
{
    if (!association.value(QStringLiteral("alarmRules")).toArray().isEmpty())
        return alarmRule(association, points).value(QStringLiteral("action")).toString()
            == QStringLiteral("FLASH");
    const QString alarmId = association.value(QStringLiteral("alarmPointId")).toString();
    if (!alarmId.isEmpty()) {
        const QJsonObject point = points.value(alarmId);
        if (fresh(point)) {
            const QJsonValue actual = decodedValue(point);
            const QJsonValue trigger = decodedValue(QJsonObject{
                {QStringLiteral("latestValueJson"), point.value(QStringLiteral("alarmTriggerValueJson"))}});
            if (!trigger.isUndefined() && !trigger.isNull()) {
                if (actual == trigger) return true;
            } else if (actual.toBool() || actual.toDouble() != 0.0
                       || actual.toString().compare(QStringLiteral("alarm"), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    const QString thresholdId = association.value(QStringLiteral("thresholdPointId")).toString();
    const QJsonObject point = points.value(thresholdId);
    if (thresholdId.isEmpty() || !fresh(point) || !decodedValue(point).isDouble()) return false;
    const double value = decodedValue(point).toDouble();
    const double threshold = association.value(QStringLiteral("threshold")).toDouble();
    const QString comparison = association.value(QStringLiteral("comparison")).toString();
    if (comparison == QStringLiteral("LT")) return value < threshold;
    if (comparison == QStringLiteral("GT")) return value > threshold;
    return false;
}

} // namespace Zqxxxt::MarkerAssociation
