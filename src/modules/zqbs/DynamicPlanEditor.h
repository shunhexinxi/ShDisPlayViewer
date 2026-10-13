#pragma once

#include "modules/zqbs/DeploymentTypes.h"
#include "modules/zqbs/AnnotationMenu.h"
#include <QAction>
#include <QMenu>
#include <QStyle>
#include <algorithm>
#include <functional>
#include <utility>

namespace Zqxxxt {
inline DeploymentPlan *findDynamicPlan(DeploymentData &data, const QString &id)
{
    for (auto &plan : data.plans) if (plan.id == id) return &plan;
    return nullptr;
}
inline DeploymentPlanStep *findDynamicStep(DeploymentData &data, const QString &planId, const QString &stepId)
{
    if (auto *plan = findDynamicPlan(data, planId))
        for (auto &step : plan->steps) if (step.id == stepId) return &step;
    return nullptr;
}
inline DeploymentMarker *findDynamicMarker(DeploymentData &data, const QString &planId,
                                           const QString &stepId, const QString &id)
{
    if (auto *step = findDynamicStep(data, planId, stepId))
        for (auto &marker : step->markers) if (marker.id == id) return &marker;
    return nullptr;
}

inline bool canMoveDynamicMarkerUp(const DeploymentPlanStep &step, const QString &markerId)
{
    for (qsizetype index = 1; index < step.markers.size(); ++index)
        if (step.markers[index].id == markerId) return true;
    return false;
}

inline bool moveDynamicMarkerUp(DeploymentData &data, const QString &planId,
                                const QString &stepId, const QString &markerId)
{
    auto *step = findDynamicStep(data, planId, stepId);
    if (!step) return false;
    for (qsizetype index = 1; index < step->markers.size(); ++index) {
        if (step->markers[index].id != markerId) continue;
        std::swap(step->markers[index - 1], step->markers[index]);
        return true;
    }
    return false;
}

inline bool removeDynamicData(DeploymentData &data, const QString &type, const QString &id, const QString &aux)
{
    if (type == QStringLiteral("plan")) {
        const auto count = data.plans.size();
        data.plans.erase(std::remove_if(data.plans.begin(), data.plans.end(),
            [&](const auto &plan) { return plan.id == id; }), data.plans.end());
        return data.plans.size() != count;
    }
    if (type == QStringLiteral("step")) {
        if (auto *plan = findDynamicPlan(data, aux)) {
            const auto count = plan->steps.size();
            plan->steps.erase(std::remove_if(plan->steps.begin(), plan->steps.end(),
                [&](const auto &step) { return step.id == id; }), plan->steps.end());
            return plan->steps.size() != count;
        }
    } else if (type == QStringLiteral("dynamic-marker")) {
        if (auto *step = findDynamicStep(data, aux.section(QLatin1Char('|'), 0, 0), aux.section(QLatin1Char('|'), 1, 1))) {
            const auto count = step->markers.size();
            step->markers.erase(std::remove_if(step->markers.begin(), step->markers.end(),
                [&](const auto &marker) { return marker.id == id; }), step->markers.end());
            return step->markers.size() != count;
        }
    }
    return false;
}

// Pure menu construction also covers a completely empty/filtered tree.
enum class DynamicAction { AddPlan, AddStep, AddMarker, MoveUp, Properties, Remove };
inline void populateDynamicPlanMenu(QMenu &menu, const QString &type,
                                    const std::function<void(DynamicAction, const QString &)> &dispatch,
                                    bool canMoveUp = false)
{
    const auto add = [&](DynamicAction operation, const QString &text, const QString &icon) {
        auto *action = menu.addAction(QIcon(QStringLiteral(":/deployment/icons/") + icon), text);
        action->setData(static_cast<int>(operation));
        QObject::connect(action, &QAction::triggered, &menu, [dispatch, operation] { dispatch(operation, {}); });
    };
    const bool plan = type == QStringLiteral("plan");
    const bool step = type == QStringLiteral("step");
    const bool marker = type == QStringLiteral("dynamic-marker");
    if (!marker) add(DynamicAction::AddPlan, QStringLiteral("新建预案"), QStringLiteral("folder.png"));
    if (plan || step)
        add(DynamicAction::AddStep, QStringLiteral("添加演练步骤"), QStringLiteral("folder.png"));
    if (step || marker) {
        auto *annotations = menu.addMenu(QIcon(QStringLiteral(":/deployment/icons/point.png")),
            QStringLiteral("添加动态标记"));
        annotations->menuAction()->setData(static_cast<int>(DynamicAction::AddMarker));
        for (bool plot : {false, true}) {
            auto *types = annotations->addMenu(QIcon(plot
                ? QStringLiteral(":/legacy/images/toolbar/Plot.png")
                : QStringLiteral(":/legacy/images/toolbar/mark.png")),
                plot ? QStringLiteral("添加标绘") : QStringLiteral("添加标记"));
            addAnnotationTypeActions(*types, plot, [dispatch](const QString &kind) {
                dispatch(DynamicAction::AddMarker, kind);
            });
        }
    }
    if (plan || step || marker) {
        menu.addSeparator();
        if (marker) {
            auto *move = menu.addAction(menu.style()->standardIcon(QStyle::SP_ArrowUp), QStringLiteral("上移"));
            move->setData(static_cast<int>(DynamicAction::MoveUp));
            move->setEnabled(canMoveUp);
            QObject::connect(move, &QAction::triggered, &menu,
                             [dispatch] { dispatch(DynamicAction::MoveUp, {}); });
        }
        add(DynamicAction::Properties, QStringLiteral("属性"), QStringLiteral("setttings.png"));
        add(DynamicAction::Remove, QStringLiteral("删除"), QStringLiteral("delete_normal.png"));
    }
}

// Work on copies: cancelling either dialog leaves data/IDs/children unchanged.
bool editDynamicPlanProperties(DeploymentPlan &plan, bool creating, QWidget *parent);
bool editDynamicStepProperties(DeploymentPlanStep &step, bool creating, QWidget *parent,
                               const std::function<DeploymentViewpoint()> &currentView);
} // namespace Zqxxxt
