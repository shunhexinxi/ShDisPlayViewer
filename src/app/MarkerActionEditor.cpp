#include "MarkerActionEditor.h"
#include "MarkerActions.h"
#include <QComboBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace Zqxxxt {
MarkerActionEditor::MarkerActionEditor(const QJsonObject &association, QWidget *parent)
    : QWidget(parent), m_table(new QTableWidget(0, 6, this))
{
    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(QStringLiteral("鼠标点击：单击标记时执行。自动触发：动态演练中标记出现时执行，演练继续播放。地址和程序路径统一填写；相对程序路径基于项目 Data。恢复从动作启动计时，网页使用系统浏览器，不支持延时关闭。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_table->setObjectName(QStringLiteral("markerActionTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("启用 / 名称"), QStringLiteral("动作"),
        QStringLiteral("地址 / 程序路径"), QStringLiteral("程序参数"),
        QStringLiteral("触发"), QStringLiteral("恢复")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setColumnWidth(0, 130);
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(2, 300);
    m_table->setColumnWidth(3, 140);
    m_table->setColumnWidth(4, 120);
    m_table->setColumnWidth(5, 170);
    layout->addWidget(m_table);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("添加动作"), this);
    auto *remove = new QPushButton(QStringLiteral("删除选中动作"), this);
    buttons->addWidget(add); buttons->addWidget(remove); buttons->addStretch();
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this] { addRow(); });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (m_table->currentRow() >= 0) m_table->removeRow(m_table->currentRow());
    });
    int ignored = 0;
    for (const auto &value : association.value("actions").toArray()) {
        const auto action = MarkerActions::normalize(value.toObject());
        if (action.isEmpty()) ++ignored;
        else addRow(action);
    }
    if (ignored) layout->addWidget(new QLabel(QStringLiteral("旧方案中的 %1 个 IMS 专用动作不会执行，保存时将移除。" ).arg(ignored), this));
}

void MarkerActionEditor::addRow(const QJsonObject &action)
{
    const int row = m_table->rowCount(); m_table->insertRow(row);
    for (int c = 0; c < 6; ++c) m_table->setItem(row, c, new QTableWidgetItem);
    m_table->item(row, 0)->setText(action.value("triggerName").toString());
    m_table->item(row, 0)->setCheckState(action.value("enabled").toBool(true) ? Qt::Checked : Qt::Unchecked);
    auto *type = new QComboBox(m_table);
    type->addItem(QStringLiteral("RTSP 视频"), "video");
    type->addItem(QStringLiteral("打开网页"), "web");
    type->addItem(QStringLiteral("打开程序"), "program");
    const int index = type->findData(action.value("type").toString());
    type->setCurrentIndex(index < 0 ? 0 : index);
    m_table->setCellWidget(row, 1, type);
    auto *destination = m_table->item(row, 2);
    destination->setText(MarkerActions::target(action));
    destination->setData(Qt::UserRole, action);
    if (!action.value("platformTargets").toObject().isEmpty())
        destination->setToolTip(QStringLiteral("旧方案：此处显示当前平台程序路径；保存时保留另一平台路径。新动作统一使用本字段。"));
    m_table->item(row, 3)->setText(action.value("parameter").toString());
    auto *trigger = new QComboBox(m_table);
    trigger->setObjectName(QStringLiteral("markerActionTrigger"));
    trigger->addItem(QStringLiteral("鼠标点击"), "single");
    trigger->addItem(QStringLiteral("自动触发"), "auto");
    trigger->setCurrentIndex(action.value("trigger").toString() == "auto" ? 1 : 0);
    m_table->setCellWidget(row, 4, trigger);
    auto *recovery = new QComboBox(m_table);
    recovery->setObjectName(QStringLiteral("markerActionRecovery"));
    recovery->addItem(QStringLiteral("无动作"), 0);
    recovery->addItem(QStringLiteral("延时10秒关闭"), 10);
    recovery->addItem(QStringLiteral("延时30秒关闭"), 30);
    recovery->addItem(QStringLiteral("延时1分钟关闭"), 60);
    const int recoveryIndex = recovery->findData(action.value("recoverySeconds").toInt());
    recovery->setCurrentIndex(qMax(0, recoveryIndex));
    m_table->setCellWidget(row, 5, recovery);
    const auto updateType = [type, recovery] {
        const bool web = type->currentData().toString() == "web";
        if (web) recovery->setCurrentIndex(0);
        recovery->setEnabled(!web);
        recovery->setToolTip(web ? QStringLiteral("系统浏览器不支持由 Viewer 延时关闭。")
                                : QStringLiteral("从动作启动开始计时，不随演练暂停。程序需独立进程运行。"));
    };
    connect(type, &QComboBox::currentIndexChanged, this, updateType);
    updateType();
}

QJsonObject MarkerActionEditor::value() const
{
    QJsonArray actions;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const auto type = static_cast<QComboBox *>(m_table->cellWidget(r, 1))->currentData().toString();
        const auto *trigger = static_cast<QComboBox *>(m_table->cellWidget(r, 4));
        const auto *recovery = static_cast<QComboBox *>(m_table->cellWidget(r, 5));
        QJsonObject action{{"type", type}, {"trigger", trigger->currentData().toString()},
            {"recoverySeconds", recovery->currentData().toInt()},
            {"enabled", m_table->item(r, 0)->checkState() == Qt::Checked},
            {"triggerName", m_table->item(r, 0)->text()},
            {"target", m_table->item(r, 2)->text().trimmed()},
            {"parameter", m_table->item(r, 3)->text()}};
        const auto original = m_table->item(r, 2)->data(Qt::UserRole).toJsonObject();
        auto paths = original.value("platformTargets").toObject();
        if (type == "program" && original.value("type").toString() == "program"
            && original.value("target").toString().isEmpty() && !paths.isEmpty()) {
            paths.insert(MarkerActions::currentPlatform(), action.value("target"));
            action.insert("target", QString());
            action.insert("platformTargets", paths);
        }
        actions.append(MarkerActions::normalize(action));
    }
    return {{"type", "NONE"}, {"actions", actions}};
}

bool MarkerActionEditor::validate(QString *error) const
{
    for (const auto &value : this->value().value("actions").toArray()) {
        if (value.toObject().value("enabled").toBool() && !MarkerActions::validate(value.toObject(), error)) return false;
    }
    return true;
}
}
