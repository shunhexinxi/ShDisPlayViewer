#pragma once

#include <QString>

class QApplication;
class QPushButton;

namespace Zqxxxt::AppTheme {

void apply(QApplication &application);

[[nodiscard]] QString applicationStyleSheet();
[[nodiscard]] QString managementModuleStyleSheet();
[[nodiscard]] QString managementContentStyleSheet();
[[nodiscard]] QString deploymentPanelStyleSheet();
void configureSideNavigationButton(QPushButton &button);

}
