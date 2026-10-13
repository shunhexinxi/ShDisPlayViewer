#pragma once
#include "modules/zqbs/DeploymentTypes.h"
#include "ui/SkinnedDialog.h"
#include <QDateTime>
#include <functional>
#include <memory>

namespace Zqxxxt {
class EnvironmentSettingsDialog final : public SkinnedDialog
{
public:
    using Preview = std::function<void(const EnvironmentSettings &)>;
    using Locate = std::function<void(double, double, double)>;
    using TimeProvider = std::function<QDateTime()>;
    EnvironmentSettingsDialog(const EnvironmentSettings &initial, QWidget *parent,
        Preview preview, Locate locate, TimeProvider clock);
    ~EnvironmentSettingsDialog();
    EnvironmentSettings settings() const;
    EnvironmentSettings snapshot() const;
    void selectPage(int index);
private:
    void populate(const EnvironmentSettings &value);
    void preview(bool resetClock = false);
    void updateEnabled();
    void updateTimeLabel();
    class Impl;
    std::unique_ptr<Impl> m;
};
}
