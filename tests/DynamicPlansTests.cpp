#include "app/AppPaths.h"
#include "modules/zqbs/DynamicPlanEditor.h"
#include "modules/zqbs/DynamicStepPlayback.h"
#include "modules/zqbs/DynamicTimelineButton.h"
#include <QHBoxLayout>
#include "modules/zqbs/DeploymentXmlStore.h"
#include "ui/AppTheme.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLineEdit>
#include <QMap>
#include <QSet>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

using namespace Zqxxxt;
namespace {
int failures = 0;
void check(bool ok, const char *message) { if (!ok) { ++failures; std::cerr << message << '\n'; } }
QByteArray bytes(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
void inDialog(const std::function<void(QDialog *)> &action)
{
    QTimer::singleShot(30, [action] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        check(dialog != nullptr, "property dialog missing");
        if (dialog) action(dialog);
    });
}
void save(QDialog *dialog) { dialog->findChild<QDialogButtonBox *>("dynamicEditorButtons")->button(QDialogButtonBox::Ok)->click(); }
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    AppPaths::initialize(QCoreApplication::applicationDirPath());
    AppTheme::apply(app);
    DeploymentPlanStep timing;
    check(timing.durationSeconds == 8 && timing.markerDurationSeconds == 4
        && !timing.sequentialDisplay, "mode defaults must be 8s / 4s");
    timing.markers.resize(3);
    check(dynamicStepDurationMs(timing) == 8000 && dynamicStepVisibleCount(timing, 0) == 3
        && dynamicStepNextDelayMs(timing, 0) == 8000, "simultaneous total/view semantics changed");
    timing.sequentialDisplay = true;
    check(dynamicStepDurationMs(timing) == 12000, "sequential total must be N times per-marker duration");
    check(dynamicStepFocusIndex(timing, 0) == 0 && dynamicStepVisibleCount(timing, 0) == 0,
        "flight must start immediately while first mark remains hidden");
    check(dynamicStepRevealMs(timing, 0) == 1000 && dynamicStepRevealMs(timing, 1) == 5000
        && dynamicStepRevealMs(timing, 2) == 9000, "arrival times incorrect");
    check(dynamicStepVisibleCount(timing, 999) == 0 && dynamicStepVisibleCount(timing, 1000) == 1
        && dynamicStepVisibleCount(timing, 4000) == 1 && dynamicStepVisibleCount(timing, 4999) == 1
        && dynamicStepVisibleCount(timing, 5000) == 2 && dynamicStepVisibleCount(timing, 9000) == 3,
        "reveal must follow flight, not divide total by N+1");
    for (int phase=0;phase<5;++phase)
        check(dynamicStepMarkerVisible(timing, 0, 1000 + phase*125) == (phase%2==0),
            "blink must show/hide/show/hide/show in 0.5 seconds");
    check(dynamicStepMarkerVisible(timing, 0, 1500) && dynamicStepMarkerVisible(timing, 0, 3999)
        && dynamicStepMarkerVisible(timing, 0, 5125) && !dynamicStepMarkerVisible(timing, 1, 5125),
        "hold/prior marks changed while the new mark blinked");
    check(dynamicStepFocusIndex(timing, 3999) == 0 && dynamicStepFocusIndex(timing, 4000) == 1
        && dynamicStepFocusIndex(timing, 8000) == 2, "next flight must start only at the next slot");
    check(dynamicStepNextDelayMs(timing, 0) == 1000 && dynamicStepNextDelayMs(timing, 999) == 1
        && dynamicStepNextDelayMs(timing, 1000) == 125
        && dynamicStepNextDelayMs(timing, 1499) == 1 && dynamicStepNextDelayMs(timing, 1500) == 2500
        && dynamicStepNextDelayMs(timing, 3999) == 1, "flight/blink/hold event deadlines drift");
    check(dynamicStepProgress(timing, -1) == 0 && dynamicStepProgress(timing, 3000) == 0.25
        && dynamicStepProgress(timing, 13000) == 1, "derived duration not used for timeline progress");
    timing.markers[1].viewFlyTimeSeconds = 2.25;
    timing.markers[1].viewpoint.flyTimeSeconds = 99;
    timing.markers[1].viewpoint.longitude = 120.5; timing.markers[1].viewpoint.latitude = 36.1;
    timing.markers[1].viewpoint.altitude = 125; timing.markers[1].viewpoint.heading = 150;
    timing.markers[1].viewpoint.pitch = -42; timing.markers[1].viewpoint.range = 6500;
    check(dynamicStepRevealMs(timing, 1) == 6250 && dynamicStepFlightSeconds(timing, 4000) == 2.25,
        "saved marker flyTime field ignored / confused with viewpoint default");
    const auto view = dynamicStepViewpointAt(timing, 4000);
    check(view.longitude == 120.5 && view.latitude == 36.1 && view.altitude == 125
        && view.heading == 150 && view.pitch == -42 && view.range == 6500,
        "full saved marker viewpoint must remain intact");
    check(dynamicStepFlightSeconds(timing, 4000, true) == 0, "scrubbing must remain instantaneous");
    timing.markers[0].viewFlyTimeSeconds = 0;
    check(dynamicStepRevealMs(timing, 0) == 0 && dynamicStepMarkerVisible(timing, 0, 0),
        "explicit instant marker flight must remain valid");
    for(double invalid:{-1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        timing.markers[0].viewFlyTimeSeconds = invalid;
        check(dynamicMarkerFlightSeconds(timing.markers[0]) == 1
            && dynamicStepRevealMs(timing, 0) == 1000, "invalid/unset flight must default to one second");
    }
    timing.markers[0].viewFlyTimeSeconds = .1;
    check(dynamicStepRevealMs(timing, 0) == 100, "fractional flight introduced an extra millisecond");
    timing.markers[0].viewFlyTimeSeconds = 1;
    check(dynamicStepTimingError(timing).isEmpty(), "valid 4-second slots rejected");
    timing.markers[1].name = QStringLiteral("冲突标记"); timing.markers[1].viewFlyTimeSeconds = 3.51;
    check(dynamicStepTimingError(timing).contains(QStringLiteral("冲突标记")), "over-budget marker not identified");
    timing.markers[1].viewFlyTimeSeconds = 3.5;
    check(dynamicStepTimingError(timing).isEmpty(), "zero hold time at exact budget must be valid");
    timing.sequentialDisplay = false;
    check(dynamicStepTimingError(timing).isEmpty()
        && dynamicStepViewpointAt(timing, 0).longitude == timing.viewpoint.longitude,
        "simultaneous mode must use recommended view, not per-marker flight budget");
    timing.sequentialDisplay = true;
    timing.markers[0].visible = false;
    check(!dynamicStepMarkerVisible(timing, 0, 1600) && dynamicStepFocusIndex(timing, 0) == -1
        && !dynamicViewpointValid(dynamicStepViewpointAt(timing, 0)),
        "hidden marker / sequential mode must not activate the recommended view");
    timing.markers.clear();
    check(dynamicStepDurationMs(timing) == 0 && dynamicStepVisibleCount(timing, 999) == 0
        && dynamicStepNextDelayMs(timing, 0) == 1 && dynamicStepProgress(timing, 0) == 0,
        "empty sequential step must have zero duration and safe timer/progress");
    timing.markerDurationSeconds = std::numeric_limits<int>::max(); timing.markers.resize(3);
    check(dynamicStepDurationMs(timing) == qint64(std::numeric_limits<int>::max()) * 3000,
        "large slot duration overflows total");
    timing.markers[0].viewFlyTimeSeconds = 0;
    check(dynamicStepNextDelayMs(timing, 1000) == std::numeric_limits<int>::max(),
        "long hold overflows QTimer");
    timing.markerDurationSeconds = 0;
    check(dynamicStepDurationMs(timing) == 3000, "invalid XML slot duration not clamped");
    {
        DeploymentPlan plan;
        DeploymentPlanStep a, b; a.sequentialDisplay = b.sequentialDisplay = true;
        a.markers.resize(2); b.markers.resize(1); a.markers[0].id="a"; a.markers[1].id="b"; b.markers[0].id="c";
        plan.steps = {a,b};
        check(dynamicPlanMarkersAt(plan,0,0).isEmpty(), "replay must begin hidden during first flight");
        check(dynamicPlanMarkersAt(plan,1,0).size() == 2, "earlier step marks not retained during next flight");
        const auto complete = dynamicPlanMarkersAt(plan,1,4000);
        check(complete.size()==3 && complete[0].visible && complete[1].visible && complete[2].visible,
            "natural completion must retain every marker without residual blink");
        check(dynamicPlanMarkersAt(plan,0,4500).size()==1 && dynamicStepFocusIndex(a,4500)==1,
            "seek into next flight must restore prior mark and focus upcoming mark");
        check(dynamicPlanMarkersAt(plan,0,1125).size()==1
            && !dynamicPlanMarkersAt(plan,0,1125)[0].visible, "seek did not restore blink phase");
    }
    {
        QWidget preview; auto *layout = new QHBoxLayout(&preview);
        layout->setContentsMargins(6, 6, 6, 6); layout->setSpacing(4);
        auto *first = new DynamicTimelineButton(QStringLiteral("01\n第一阶段"), &preview);
        auto *second = new DynamicTimelineButton(QStringLiteral("02\n第2阶段"), &preview);
        auto *third = new DynamicTimelineButton(QStringLiteral("03\n紧急出动"), &preview);
        first->setPlaybackProgress(1, 8000, 8000, false);
        second->setPlaybackProgress(0.35, 2800, 8000, true);
        third->setPlaybackProgress(0, 0, 12000, false);
        layout->addWidget(first); layout->addWidget(second); layout->addWidget(third);
        preview.resize(1200, 64); preview.show(); QApplication::processEvents();
        const auto painted = second->grab().toImage();
        // grab() pixels and widget logical coordinates differ at Windows DPI scaling.
        check(painted.pixelColor(painted.width() / 10, painted.height() - 3) == QColor("#126d94")
            && painted.pixelColor(painted.width() * 9 / 10, painted.height() - 3) == QColor("#0a355d"),
            "timeline fill must cover elapsed fraction, not entire selected block");
        if (app.arguments().size() > 2) preview.grab().save(app.arguments()[2]);
    }
    const QStringList types{QString(), "plan", "step", "dynamic-marker"};
    const QList<int> counts{1, 4, 36, 35};
    for (int i = 0; i < types.size(); ++i) {
        QMenu menu; QList<DynamicAction> invoked; QStringList kinds;
        populateDynamicPlanMenu(menu, types[i], [&](auto action, const QString &kind) {
            invoked.append(action);
            if (action == DynamicAction::AddMarker) kinds.append(kind);
        }, i == 3);
        std::function<void(QMenu *)> triggerLeaves = [&](QMenu *branch) {
            for (auto *action : branch->actions()) {
                if (action->isSeparator()) continue;
                check(!action->icon().isNull(), "context menu icon missing");
                if (action->menu()) triggerLeaves(action->menu()); else action->trigger();
            }
        };
        triggerLeaves(&menu);
        check(invoked.size() == counts[i], "CRUD menu action count wrong");
        check(invoked.first() == (i == 3 ? DynamicAction::AddMarker : DynamicAction::AddPlan),
              "context menu starts with the wrong action");
        check(invoked.contains(DynamicAction::AddPlan) == (i != 3), "marker menu must hide plan creation");
        check(invoked.contains(DynamicAction::AddStep) == (i == 1 || i == 2),
              "step creation must be limited to plan and step menus");
        check(invoked.contains(DynamicAction::AddMarker) == (i >= 2), "marker creation must require a step");
        check(invoked.contains(DynamicAction::MoveUp) == (i == 3), "move-up must only appear for markers");
        check(invoked.contains(DynamicAction::Properties) == (i > 0), "properties missing/wrong target");
        check(invoked.contains(DynamicAction::Remove) == (i > 0), "delete missing/wrong target");
        if (i == 3) {
            const auto actions = menu.actions();
            const auto move = std::find_if(actions.cbegin(), actions.cend(), [](const QAction *action) {
                return action->text() == QStringLiteral("上移");
            });
            check(move != actions.cend() && (*move)->isEnabled(), "marker move-up action missing or disabled");
        }
        if (i >= 2) {
            QMenu *add = nullptr;
            for (auto *action : menu.actions()) if (action->text() == QStringLiteral("添加动态标记")) add = action->menu();
            check(add && add->actions().size() == 2, "expected exactly two annotation submenus");
            if (!add) return 1;
            check(add->actions()[0]->text() == QStringLiteral("添加标记")
                && add->actions()[1]->text() == QStringLiteral("添加标绘"), "wrong submenu titles/order");
            check(add->actions()[0]->menu()->actions().size() == 15
                && add->actions()[1]->menu()->actions().size() == 17, "mark/plot types are incomplete");
            for (int group = 0; group < 2; ++group) {
                QMenu reference;
                addAnnotationTypeActions(reference, group == 1, [](const QString &) {});
                const auto actual = add->actions()[group]->menu()->actions();
                for (int n = 0; n < actual.size(); ++n) {
                    check(actual[n]->text() != QStringLiteral("文件夹") && !actual[n]->menu(), "folder action must be excluded");
                    check(actual[n]->data() == reference.actions()[n]->data()
                        && actual[n]->text() == reference.actions()[n]->text(), "dynamic types differ from fixed types");
                }
            }
            check(QSet<QString>(kinds.cbegin(), kinds.cend()).size() == 32
                && kinds.contains("plot-cardinal-arrow") && kinds.contains("tilt-model"), "missing/duplicate geometry IDs");
        }
    }
    {
        QMenu firstMarkerMenu;
        populateDynamicPlanMenu(firstMarkerMenu, QStringLiteral("dynamic-marker"),
            [](DynamicAction, const QString &) {}, false);
        const auto actions = firstMarkerMenu.actions();
        const auto move = std::find_if(actions.cbegin(), actions.cend(),
            [](const QAction *action) { return action->text() == QStringLiteral("上移"); });
        check(move != actions.cend() && !(*move)->isEnabled(),
              "first marker must not be movable upward");
    }

    DeploymentPlan plan; plan.id = "plan-a";
    inDialog([&](QDialog *dialog) {
        auto *buttons = dialog->findChild<QDialogButtonBox *>("dynamicEditorButtons");
        check(!buttons->button(QDialogButtonBox::Ok)->isEnabled(), "blank plan name accepted");
        dialog->findChild<QLineEdit *>("dynamicName")->setText(QStringLiteral(" 中文应急预案 "));
        dialog->findChild<QPlainTextEdit *>("dynamicDescription")->setPlainText(QStringLiteral("第一行\n第二行"));
        dialog->findChild<QCheckBox *>("dynamicLoop")->setChecked(true);
        save(dialog);
    });
    check(editDynamicPlanProperties(plan, true, nullptr), "create plan failed");
    check(plan.name == QStringLiteral("中文应急预案") && plan.loop && plan.id == "plan-a", "plan metadata or ID wrong");
    DeploymentPlanStep step; step.id = "step-a";
    step.viewpoint.id = "view-a"; step.viewpoint.heading = 45; step.viewpoint.pitch = -36; step.viewpoint.range = 1234;
    const auto currentView = [] { DeploymentViewpoint v; v.longitude = 119.93; v.latitude = 36.22;
        v.altitude = 58; v.heading = 127.5; v.pitch = -52; v.range = 2345.6; return v; };
    inDialog([&](QDialog *dialog) {
        check(dialog->findChild<QDoubleSpinBox *>("dynamicHeading")->value() == 45, "saved view not populated");
        dialog->findChild<QLineEdit *>("dynamicName")->setText(QStringLiteral("集结阶段"));
        dialog->findChild<QSpinBox *>("dynamicDuration")->setValue(17);
        check(dialog->findChild<QSpinBox *>("dynamicDuration")->isEnabled()
            && !dialog->findChild<QSpinBox *>("dynamicMarkerDuration")->isEnabled(), "simultaneous controls not enabled correctly");
        dialog->findChild<QPushButton *>("dynamicCaptureView")->click();
        dialog->findChild<QDoubleSpinBox *>("dynamicFlyTime")->setValue(2.5);
        auto *simultaneous = dialog->findChild<QCheckBox *>("dynamicSimultaneousDisplay");
        auto *sequential = dialog->findChild<QCheckBox *>("dynamicSequentialDisplay");
        check(simultaneous && sequential, "step display mode controls missing");
        if (simultaneous && sequential) {
            check(simultaneous->isChecked() && !sequential->isChecked(), "new step must default to simultaneous");
            sequential->click();
            check(sequential->isChecked() && !simultaneous->isChecked(), "display modes not exclusive");
            sequential->click();
            check(sequential->isChecked(), "at least one display mode must remain selected");
            check(!dialog->findChild<QSpinBox *>("dynamicDuration")->isEnabled()
                && dialog->findChild<QSpinBox *>("dynamicMarkerDuration")->isEnabled()
                && !dialog->findChild<QDoubleSpinBox *>("dynamicHeading")->isEnabled(),
                "sequential controls must disable total/recommended view");
            check(dialog->findChild<QSpinBox *>("dynamicMarkerDuration")->value()==4, "new slot default not four seconds");
            dialog->findChild<QSpinBox *>("dynamicMarkerDuration")->setValue(6);
            simultaneous->click();
            check(dialog->findChild<QSpinBox *>("dynamicDuration")->value()==17
                && dialog->findChild<QSpinBox *>("dynamicDuration")->isEnabled()
                && dialog->findChild<QDoubleSpinBox *>("dynamicHeading")->isEnabled()
                && !dialog->findChild<QSpinBox *>("dynamicMarkerDuration")->isEnabled(),
                "switching back must restore simultaneous duration/view controls");
            QApplication::processEvents();
            if (app.arguments().size()>3) dialog->grab().save(app.arguments()[3]);
            sequential->click();
            check(dialog->findChild<QSpinBox *>("dynamicMarkerDuration")->value()==6,
                "switching display modes discarded per-marker duration");
        }
        dialog->findChild<QPlainTextEdit *>("dynamicDescription")->setPlainText(QStringLiteral("人员集结\n观察周界"));
        QApplication::processEvents();
        if (app.arguments().size() > 1) dialog->grab().save(app.arguments()[1]);
        save(dialog);
    });
    check(editDynamicStepProperties(step, true, nullptr, currentView), "create step failed");
    check(step.durationSeconds == 17 && step.markerDurationSeconds == 6 && step.sequentialDisplay && step.viewpoint.heading == 127.5 && step.viewpoint.pitch == -52
        && step.viewpoint.range == 2345.6 && step.viewpoint.flyTimeSeconds == 2.5
        && step.viewpoint.id == "view-a", "step fields/view capture wrong");
    DeploymentMarker marker; marker.id = "marker-a"; marker.name = QStringLiteral("集结点");
    marker.geometryType = "point"; marker.points = {{119.93, 36.22, 58}};
    marker.position = marker.points.first(); marker.viewpoint = step.viewpoint; marker.lineWidth = 4.5;
    marker.viewFlyTimeSeconds = 2.25;
    step.markers.append(marker); plan.steps.append(step);
    inDialog([](QDialog *dialog) { dialog->findChild<QLineEdit *>("dynamicName")->setText("discard"); dialog->reject(); });
    check(!editDynamicPlanProperties(plan, false, nullptr) && plan.name == QStringLiteral("中文应急预案")
        && plan.steps[0].markers.size() == 1, "cancel changed plan or children");
    inDialog([](QDialog *dialog) {
        check(dialog->findChild<QCheckBox *>("dynamicSequentialDisplay")->isChecked(), "saved mode not loaded into editor");
        dialog->findChild<QCheckBox *>("dynamicSimultaneousDisplay")->click();
        dialog->findChild<QSpinBox *>("dynamicDuration")->setValue(99); dialog->reject();
    });
    check(!editDynamicStepProperties(step, false, nullptr, currentView) && step.durationSeconds == 17 && step.sequentialDisplay,
        "cancel changed step");
    inDialog([](QDialog *dialog) { dialog->findChild<QLineEdit *>("dynamicName")->setText(QStringLiteral("修改后的步骤")); save(dialog); });
    check(editDynamicStepProperties(step, false, nullptr, currentView) && step.markers.size() == 1
        && step.markers[0].id == "marker-a", "step editing lost markers");
    plan.steps[0] = step;

    QTemporaryDir dir; DeploymentXmlStore store(dir.path()); DeploymentData data; QString error;
    check(store.load(data, error), "initial XML load failed");
    const QStringList protectedFiles{store.fixedFilePath(), store.fixedAnnotationsFilePath(),
        store.normalFilePath(), store.marksFilePath(), store.plotsFilePath(), store.featureConfigFilePath()};
    QMap<QString, QByteArray> before;
    for (const auto &path : protectedFiles) before[path] = bytes(path);
    auto other = plan; other.id = "plan-b"; other.name = QStringLiteral("其他预案");
    other.steps[0].sequentialDisplay = false;
    // Same child IDs in another plan must never make an operation cross plan boundaries.
    data.plans = {plan, other};
    check(store.saveDynamic(data, error), "dynamic save failed");
    DeploymentData loaded;
    check(store.load(loaded, error), "dynamic reload failed");
    auto *loadedStep = findDynamicStep(loaded, "plan-a", "step-a");
    check(loadedStep && loadedStep->durationSeconds == 17 && loadedStep->markerDurationSeconds == 6 && loadedStep->sequentialDisplay && loadedStep->viewpoint.heading == 127.5
        && loadedStep->viewpoint.pitch == -52 && loadedStep->viewpoint.range == 2345.6
        && loadedStep->markers.size() == 1, "step/markers/view did not round trip");
    check(!findDynamicStep(loaded, "plan-b", "step-a")->sequentialDisplay, "simultaneous XML mode lost");
    check(loadedStep && dynamicStepFlightSeconds(*loadedStep, 0) == 2.25, "marker saved flight did not round-trip to playback");
    const auto savedXml = bytes(store.dynamicFilePath());
    {
        auto legacy = savedXml; legacy.replace(" markerDurationSeconds=\"6\"", "");
        QFile file(store.dynamicFilePath()); check(file.open(QIODevice::WriteOnly|QIODevice::Truncate), "legacy timing fixture open");
        file.write(legacy); file.close();
        DeploymentData legacyData; check(store.load(legacyData,error), "legacy timing fixture load");
        const auto *legacyStep=findDynamicStep(legacyData,"plan-a","step-a");
        check(legacyStep && legacyStep->markerDurationSeconds==4 && legacyStep->durationSeconds==17
            && legacyStep->markers.size()==1, "legacy XML defaults lost saved total or markers");
    }
    for (const auto &replacement : {QByteArray(), QByteArray("markerDisplay=\"unknown\"")}) {
        auto legacy = savedXml;
        legacy.replace("markerDisplay=\"sequential\"", replacement);
        QFile file(store.dynamicFilePath());
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "legacy fixture open failed");
        file.write(legacy); file.close();
        DeploymentData compatibility;
        check(store.load(compatibility, error) && !findDynamicStep(compatibility, "plan-a", "step-a")->sequentialDisplay,
            "missing/unknown XML mode must remain backward compatible");
    }
    check(store.saveDynamic(loaded, error), "restore mode fixture failed");
    auto *loadedMarker = findDynamicMarker(loaded, "plan-a", "step-a", "marker-a");
    if (!loadedMarker) return 1;
    loadedMarker->name = QStringLiteral("编辑后的动态标记"); loadedMarker->position.altitude = 67;
    loadedMarker->points[0] = loadedMarker->position;
    check(store.saveDynamic(loaded, error) && store.load(data, error), "marker edit did not persist");
    check(findDynamicMarker(data, "plan-a", "step-a", "marker-a")->position.altitude == 67, "marker position lost");
    {
        auto *ordered = findDynamicStep(data, "plan-a", "step-a");
        auto second = ordered->markers[0]; second.id = "marker-b"; second.name = QStringLiteral("第二标记");
        second.viewpoint.longitude = 120.2;
        auto third = ordered->markers[0]; third.id = "marker-c"; third.name = QStringLiteral("第三标记");
        third.viewpoint.longitude = 121.3;
        ordered->markers.append(second); ordered->markers.append(third);
        check(!canMoveDynamicMarkerUp(*ordered, "marker-a") && canMoveDynamicMarkerUp(*ordered, "marker-c"),
              "move-up boundary was not identified");
        check(!moveDynamicMarkerUp(data, "plan-a", "step-a", "marker-a")
            && !moveDynamicMarkerUp(data, "plan-a", "missing", "marker-c")
            && !moveDynamicMarkerUp(data, "plan-a", "step-a", "missing"),
            "move-up crossed a boundary or accepted a missing marker");
        check(moveDynamicMarkerUp(data, "plan-a", "step-a", "marker-c")
            && ordered->markers[0].id == "marker-a"
            && ordered->markers[1].id == "marker-c"
            && ordered->markers[2].id == "marker-b"
            && dynamicStepViewpointAt(*ordered, ordered->markerDurationSeconds * 1000).longitude == 121.3,
            "move-up did not swap playback order with the previous marker");
        check(findDynamicStep(data, "plan-b", "step-a")->markers.size() == 1,
              "move-up changed another plan");
        check(store.saveDynamic(data, error) && store.load(loaded, error), "move-up save/reload failed");
        const auto *persisted = findDynamicStep(loaded, "plan-a", "step-a");
        check(persisted && persisted->markers.size() == 3
            && persisted->markers[0].id == "marker-a"
            && persisted->markers[1].id == "marker-c"
            && persisted->markers[2].id == "marker-b"
            && persisted->markers[1].viewpoint.longitude == 121.3,
            "move-up order or marker attributes did not persist");
    }
    check(!removeDynamicData(data, "dynamic-marker", "marker-a", "missing|step-a"), "missing owner must not fall back");
    check(removeDynamicData(data, "dynamic-marker", "marker-a", "plan-a|step-a"), "marker delete failed");
    check(findDynamicMarker(data, "plan-b", "step-a", "marker-a") != nullptr, "delete affected another plan");
    check(removeDynamicData(data, "step", "step-a", "plan-a"), "step delete failed");
    check(findDynamicPlan(data, "plan-a")->steps.isEmpty(), "step left ghost children");
    check(removeDynamicData(data, "plan", "plan-a", {}), "plan delete failed");
    check(data.plans.size() == 1 && data.plans[0].id == "plan-b", "plan delete affected sibling");
    check(removeDynamicData(data, "plan", "plan-b", {}), "last plan delete failed");
    check(store.saveDynamic(data, error) && store.load(loaded, error) && loaded.plans.isEmpty(), "deleted last plan returns after reload");
    for (const auto &path : protectedFiles) check(before[path] == bytes(path), "dynamic CRUD changed fixed/normal XML");
    std::cout << "Dynamic plan CRUD/menu/dialog/XML checks: " << failures << " failures\n";
    return failures ? 1 : 0;
}
