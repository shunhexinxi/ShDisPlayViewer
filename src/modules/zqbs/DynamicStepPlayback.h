#pragma once

#include "DeploymentTypes.h"
#include <limits>
#include <cmath>

namespace Zqxxxt {

inline qint64 dynamicStepSlotMs(const DeploymentPlanStep &step)
{
    return qint64(qMax(1, step.markerDurationSeconds)) * 1000;
}

inline qint64 dynamicStepDurationMs(const DeploymentPlanStep &step)
{
    if (!step.sequentialDisplay) return qint64(qMax(1, step.durationSeconds)) * 1000;
    const qint64 slot = dynamicStepSlotMs(step);
    const auto count = qint64(step.markers.size());
    return count > std::numeric_limits<qint64>::max() / slot
        ? std::numeric_limits<qint64>::max() : count * slot;
}

inline double dynamicMarkerFlightSeconds(const DeploymentMarker &marker)
{
    // The property dialog and XML use viewFlyTimeSeconds, not the separate
    // DeploymentViewpoint default (3s). Explicit zero remains valid.
    const double seconds = marker.viewFlyTimeSeconds;
    return std::isfinite(seconds) && seconds >= 0.0 ? seconds : 1.0;
}

inline qint64 dynamicMarkerFlightMs(const DeploymentMarker &marker)
{
    const long double ms = std::round(static_cast<long double>(dynamicMarkerFlightSeconds(marker)) * 1000);
    return ms >= std::numeric_limits<qint64>::max() ? std::numeric_limits<qint64>::max() : qint64(ms);
}

inline qint64 dynamicStepFlightStartMs(const DeploymentPlanStep &step, qsizetype index)
{
    const qint64 slot = dynamicStepSlotMs(step);
    return index <= 0 ? 0 : index > std::numeric_limits<qint64>::max() / slot
        ? std::numeric_limits<qint64>::max() : qint64(index) * slot;
}

// The camera starts at the slot boundary; the marker appears only on arrival.
inline qint64 dynamicStepRevealMs(const DeploymentPlanStep &step, qsizetype index)
{
    if (index < 0 || index >= step.markers.size()) return dynamicStepDurationMs(step);
    const qint64 start = dynamicStepFlightStartMs(step, index);
    const qint64 flight = dynamicMarkerFlightMs(step.markers[index]);
    return flight > std::numeric_limits<qint64>::max() - start
        ? std::numeric_limits<qint64>::max() : start + flight;
}

// Revealed prefix size, including a temporarily hidden marker during its blink.
inline qsizetype dynamicStepVisibleCount(const DeploymentPlanStep &step, qint64 elapsedMs)
{
    const qsizetype count = step.markers.size();
    if (!step.sequentialDisplay || count == 0) return count;
    const qint64 elapsed = qMax(qint64(0), elapsedMs);
    if (elapsed >= dynamicStepDurationMs(step)) return count;
    const auto completed = qsizetype(elapsed / dynamicStepSlotMs(step));
    return completed + (elapsed >= dynamicStepRevealMs(step, completed) ? 1 : 0);
}

inline qint64 dynamicStepBlinkPhaseMs(const DeploymentPlanStep &, qsizetype)
{
    // show / hide / show / hide / show: four transitions, 0.5s total.
    return 125;
}

inline bool dynamicStepMarkerVisible(const DeploymentPlanStep &step, qsizetype index, qint64 elapsedMs)
{
    if (index < 0 || index >= step.markers.size() || !step.markers[index].visible) return false;
    if (!step.sequentialDisplay) return true;
    if (elapsedMs >= dynamicStepDurationMs(step)) return true;
    const qint64 age = elapsedMs - dynamicStepRevealMs(step, index);
    if (age < 0) return false;
    const qint64 phase = age / dynamicStepBlinkPhaseMs(step, index);
    return phase != 1 && phase != 3;
}

inline int dynamicStepNextDelayMs(const DeploymentPlanStep &step, qint64 elapsedMs)
{
    const qint64 duration = dynamicStepDurationMs(step);
    const qint64 elapsed = qBound(qint64(0), elapsedMs, duration);
    qint64 deadline = duration;
    if (step.sequentialDisplay && elapsed < duration) {
        const auto index = qsizetype(elapsed / dynamicStepSlotMs(step));
        deadline = dynamicStepFlightStartMs(step, index + 1);
        const auto reveal = dynamicStepRevealMs(step, index);
        if (elapsed < reveal) deadline = qMin(deadline, reveal);
        else {
            const auto phaseMs = dynamicStepBlinkPhaseMs(step, index);
            const auto phase = (elapsed - reveal) / phaseMs;
            if (phase < 4) deadline = qMin(deadline, reveal + (phase + 1) * phaseMs);
        }
    }
    return int(qBound(qint64(1), deadline - elapsed, qint64(std::numeric_limits<int>::max())));
}

inline double dynamicStepProgress(const DeploymentPlanStep &step, qint64 elapsedMs)
{
    const auto duration = dynamicStepDurationMs(step);
    return duration > 0 ? qBound(0.0, double(elapsedMs) / double(duration), 1.0) : 0.0;
}

inline QVector<DeploymentMarker> dynamicPlanMarkersAt(
    const DeploymentPlan &plan, int stepIndex, qint64 elapsedMs)
{
    QVector<DeploymentMarker> result;
    if (stepIndex < 0 || stepIndex >= plan.steps.size()) return result;
    for (int index = 0; index < stepIndex; ++index) result += plan.steps[index].markers;
    const auto &step = plan.steps[stepIndex];
    const auto count = dynamicStepVisibleCount(step, elapsedMs);
    for (qsizetype index = 0; index < count; ++index) {
        auto marker = step.markers[index];
        marker.visible = dynamicStepMarkerVisible(step, index, elapsedMs);
        result.append(marker);
    }
    return result;
}

inline qsizetype dynamicStepFocusIndex(const DeploymentPlanStep &step, qint64 elapsedMs)
{
    if (!step.sequentialDisplay || step.markers.isEmpty()) return -1;
    const auto started = qMin(qint64(step.markers.size() - 1),
        qMax(qint64(0), elapsedMs) / dynamicStepSlotMs(step));
    for (qsizetype index = qsizetype(started) + 1; index > 0; --index)
        if (step.markers[index - 1].visible) return index - 1;
    return -1;
}

inline bool dynamicViewpointValid(const DeploymentViewpoint &view)
{
    return std::isfinite(view.longitude) && std::abs(view.longitude) <= 180
        && std::isfinite(view.latitude) && std::abs(view.latitude) <= 90
        && std::isfinite(view.altitude) && std::isfinite(view.heading)
        && std::isfinite(view.pitch) && std::isfinite(view.range) && view.range > 0;
}

inline DeploymentViewpoint dynamicStepViewpointAt(const DeploymentPlanStep &step, qint64 elapsedMs)
{
    if (!step.sequentialDisplay) return step.viewpoint;
    const auto index = dynamicStepFocusIndex(step, elapsedMs);
    if (index >= 0) {
        auto view = step.markers[index].viewpoint;
        view.flyTimeSeconds = dynamicMarkerFlightSeconds(step.markers[index]);
        return view;
    }
    // Sequential mode never activates the step's recommended viewpoint.
    DeploymentViewpoint none; none.range = 0.0; return none;
}

inline double dynamicStepFlightSeconds(const DeploymentPlanStep &step, qint64 elapsedMs,
                                       bool instant = false)
{
    if (instant) return 0.0;
    const double seconds = dynamicStepViewpointAt(step, elapsedMs).flyTimeSeconds;
    return std::isfinite(seconds) ? qMax(0.0, seconds) : 1.0;
}

inline QString dynamicStepTimingError(const DeploymentPlanStep &step)
{
    if (!step.sequentialDisplay) return {};
    for (const auto &marker : step.markers) {
        if (dynamicMarkerFlightSeconds(marker) + 0.5 > qMax(1, step.markerDurationSeconds))
            return QStringLiteral("步骤“%1”的标记“%2”：飞行 %3 秒＋闪烁 0.5 秒，超过每标记 %4 秒。请调大步骤的每标记时长，或调短该标记属性中的飞行时间。")
                .arg(step.name, marker.name).arg(dynamicMarkerFlightSeconds(marker), 0, 'f', 2)
                .arg(step.markerDurationSeconds);
    }
    return {};
}

} // namespace Zqxxxt
