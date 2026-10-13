#include "DynamicPlanEditor.h"
#include "DynamicStepPlayback.h"
#include "ui/AppTheme.h"
#include "ui/SkinnedDialog.h"
#include <QCheckBox>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Zqxxxt {
namespace {
QVBoxLayout *bodyLayout(SkinnedDialog &dialog)
{
    auto *body = new QWidget(&dialog);
    body->setStyleSheet(AppTheme::deploymentPanelStyleSheet());
    dialog.contentLayout()->addWidget(body);
    return new QVBoxLayout(body);
}
void addButtons(SkinnedDialog &dialog, QVBoxLayout *layout, QLineEdit *name)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->setObjectName(QStringLiteral("dynamicEditorButtons"));
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    auto validate = [buttons, name] { buttons->button(QDialogButtonBox::Ok)->setEnabled(!name->text().trimmed().isEmpty()); };
    QObject::connect(name, &QLineEdit::textChanged, &dialog, validate);
    validate();
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
}
}
bool editDynamicPlanProperties(DeploymentPlan &plan, bool creating, QWidget *parent)
{
    SkinnedDialog dialog(creating ? QStringLiteral("新建执勤预案") : QStringLiteral("执勤预案属性"), parent);
    dialog.setObjectName(QStringLiteral("dynamicPlanProperties"));
    dialog.resize(560, 360);
    auto *layout = bodyLayout(dialog);
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(plan.name, &dialog);
    name->setObjectName(QStringLiteral("dynamicName"));
    auto *description = new QPlainTextEdit(plan.description, &dialog);
    description->setObjectName(QStringLiteral("dynamicDescription"));
    auto *loop = new QCheckBox(QStringLiteral("完成最后一步后从第一步继续"), &dialog);
    loop->setObjectName(QStringLiteral("dynamicLoop"));
    loop->setChecked(plan.loop);
    form->addRow(QStringLiteral("预案名称"), name);
    form->addRow(QStringLiteral("预案说明"), description);
    form->addRow(QStringLiteral("循环演练"), loop);
    layout->addLayout(form);
    addButtons(dialog, layout, name);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) return false;
    plan.name = name->text().trimmed();
    plan.description = description->toPlainText();
    plan.loop = loop->isChecked();
    return true;
}

bool editDynamicStepProperties(DeploymentPlanStep &step, bool creating, QWidget *parent,
                               const std::function<DeploymentViewpoint()> &currentView)
{
    SkinnedDialog dialog(creating ? QStringLiteral("添加演练步骤") : QStringLiteral("演练步骤属性"), parent);
    dialog.setObjectName(QStringLiteral("dynamicStepProperties"));
    dialog.resize(580, 660);
    auto *layout = bodyLayout(dialog);
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(step.name, &dialog);
    name->setObjectName(QStringLiteral("dynamicName"));
    auto *duration = new QSpinBox(&dialog);
    duration->setObjectName(QStringLiteral("dynamicDuration"));
    duration->setRange(1, qMax(3600, step.durationSeconds));
    duration->setValue(step.durationSeconds);
    duration->setSuffix(QStringLiteral(" 秒"));
    auto *markerDuration = new QSpinBox(&dialog);
    markerDuration->setObjectName(QStringLiteral("dynamicMarkerDuration"));
    markerDuration->setRange(1, qMax(3600, step.markerDurationSeconds));
    markerDuration->setValue(step.markerDurationSeconds);
    markerDuration->setSuffix(QStringLiteral(" 秒 / 标记"));
    auto *description = new QPlainTextEdit(step.description, &dialog);
    description->setObjectName(QStringLiteral("dynamicDescription"));
    description->setMaximumHeight(100);
    form->addRow(QStringLiteral("步骤名称"), name);
    form->addRow(QStringLiteral("总持续时间"), duration);
    form->addRow(QStringLiteral("每个标记持续时间"), markerDuration);
    auto *displayMode = new QWidget(&dialog);
    auto *modeLayout = new QHBoxLayout(displayMode);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    auto *simultaneous = new QCheckBox(QStringLiteral("同时显示"), displayMode);
    simultaneous->setObjectName(QStringLiteral("dynamicSimultaneousDisplay"));
    auto *sequential = new QCheckBox(QStringLiteral("顺序显示"), displayMode);
    sequential->setObjectName(QStringLiteral("dynamicSequentialDisplay"));
    auto *modeGroup = new QButtonGroup(displayMode);
    modeGroup->setExclusive(true);
    modeGroup->addButton(simultaneous);
    modeGroup->addButton(sequential);
    simultaneous->setChecked(!step.sequentialDisplay);
    sequential->setChecked(step.sequentialDisplay);
    modeLayout->addWidget(simultaneous);
    modeLayout->addWidget(sequential);
    modeLayout->addStretch();
    form->addRow(QStringLiteral("标记显示"), displayMode);
    auto *modeHint = new QLabel(&dialog);
    modeHint->setObjectName(QStringLiteral("dynamicTimingHint"));
    modeHint->setWordWrap(true);
    form->addRow(QString(), modeHint);
    form->addRow(QStringLiteral("演练内容"), description);
    layout->addLayout(form);
    auto *viewGroup = new QGroupBox(QStringLiteral("推荐视角（同时显示时生效）"), &dialog);
    viewGroup->setObjectName(QStringLiteral("dynamicRecommendedView"));
    auto *viewForm = new QFormLayout(viewGroup);
    const auto spin = [&](const QString &key, const QString &label, double minimum, double maximum, int decimals) {
        auto *value = new QDoubleSpinBox(viewGroup);
        value->setObjectName(key); value->setRange(minimum, maximum); value->setDecimals(decimals);
        viewForm->addRow(label, value); return value;
    };
    auto *lon = spin(QStringLiteral("dynamicLongitude"), QStringLiteral("经度（°）"), -180, 180, 7);
    auto *lat = spin(QStringLiteral("dynamicLatitude"), QStringLiteral("纬度（°）"), -90, 90, 7);
    auto *alt = spin(QStringLiteral("dynamicAltitude"), QStringLiteral("高度（米）"), -100000, 100000000, 3);
    auto *heading = spin(QStringLiteral("dynamicHeading"), QStringLiteral("方位角（°）"), -360, 360, 3);
    auto *pitch = spin(QStringLiteral("dynamicPitch"), QStringLiteral("俯仰角（°）"), -90, 90, 3);
    auto *range = spin(QStringLiteral("dynamicRange"), QStringLiteral("视距（米）"), 0.01, 1000000000, 3);
    auto *flyTime = spin(QStringLiteral("dynamicFlyTime"), QStringLiteral("飞行时间（秒）"), 0, 3600, 2);
    auto setView = [&](const DeploymentViewpoint &view) {
        lon->setValue(view.longitude); lat->setValue(view.latitude); alt->setValue(view.altitude);
        heading->setValue(view.heading); pitch->setValue(view.pitch); range->setValue(view.range);
    };
    setView(step.viewpoint); flyTime->setValue(step.viewpoint.flyTimeSeconds);
    auto *capture = new QPushButton(QStringLiteral("使用当前地图视角"), viewGroup);
    capture->setObjectName(QStringLiteral("dynamicCaptureView"));
    QObject::connect(capture, &QPushButton::clicked, &dialog, [&] { if (currentView) setView(currentView()); });
    viewForm->addRow(capture);
    layout->addWidget(viewGroup);
    const auto setHint = [&](const QString &text) {
        modeHint->setText(text);
        // Mode changes add the computed total and possibly a conflict message.
        // Reserve wrapped text height explicitly (QFormLayout otherwise keeps
        // the shorter simultaneous-mode row height on Windows).
        const int width = qMax(240, dialog.width() - 140);
        modeHint->setMinimumHeight(modeHint->fontMetrics().boundingRect(
            QRect(0, 0, width, 10000), Qt::TextWordWrap, text).height() + 8);
    };
    const auto updateMode = [&] {
        const bool ordered = sequential->isChecked();
        duration->setEnabled(!ordered);
        form->labelForField(duration)->setEnabled(!ordered);
        markerDuration->setEnabled(ordered);
        form->labelForField(markerDuration)->setEnabled(ordered);
        viewGroup->setEnabled(!ordered);
        if (!ordered) {
            setHint(QStringLiteral("全部标记立即显示，使用推荐视角；步骤总持续时间由上方设置。"));
            return;
        }
        DeploymentPlanStep preview = step;
        preview.sequentialDisplay = true; preview.markerDurationSeconds = markerDuration->value();
        QString hint = QStringLiteral("按标记保存顺序逐个累加。每个标记：飞行（使用标记属性，默认 1 秒）→ 闪烁两次（共 0.5 秒）→ 保持至本标记时段结束。\n%1 个标记 × %2 秒 = 步骤总持续 %3 秒；空步骤播放时直接跳过。")
            .arg(step.markers.size()).arg(markerDuration->value()).arg(dynamicStepDurationMs(preview) / 1000);
        const auto error = dynamicStepTimingError(preview);
        if (!error.isEmpty()) hint += QLatin1Char('\n') + error;
        setHint(hint);
    };
    QObject::connect(sequential, &QCheckBox::toggled, &dialog, updateMode);
    QObject::connect(markerDuration, &QSpinBox::valueChanged, &dialog, updateMode);
    updateMode();
    addButtons(dialog, layout, name);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) return false;
    step.name = name->text().trimmed(); step.description = description->toPlainText();
    step.durationSeconds = duration->value();
    step.markerDurationSeconds = markerDuration->value();
    step.sequentialDisplay = sequential->isChecked();
    step.viewpoint.longitude = lon->value(); step.viewpoint.latitude = lat->value(); step.viewpoint.altitude = alt->value();
    step.viewpoint.heading = heading->value(); step.viewpoint.pitch = pitch->value(); step.viewpoint.range = range->value();
    step.viewpoint.flyTimeSeconds = flyTime->value();
    return true;
}
} // namespace Zqxxxt
