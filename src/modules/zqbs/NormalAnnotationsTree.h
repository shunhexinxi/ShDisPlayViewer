#pragma once

#include "modules/zqbs/NormalAnnotations.h"
#include <QIcon>
#include <QTreeWidget>
#include <functional>

namespace Zqxxxt {
inline void buildNormalAnnotationsTree(QTreeWidget *tree, const DeploymentData &data,
    const QString &filter,
    const std::function<QString(const QString &)> &markerIcon)
{
    const auto matches = [&filter](const QString &name) {
        return filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive);
    };
    const QIcon folderIcon(QStringLiteral(":/deployment/icons/folder.png"));
    const auto item = [&](QTreeWidgetItem *parent, const QString &name, const QString &type,
                          const QString &id, bool visible, bool checkable = true) {
        auto *value = new QTreeWidgetItem;
        value->setText(0, name);
        value->setData(0, Qt::UserRole, type);
        value->setData(0, Qt::UserRole + 1, id);
        value->setIcon(0, folderIcon);
        if (checkable) {
            value->setFlags(value->flags() | Qt::ItemIsUserCheckable);
            value->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
        }
        if (parent) parent->addChild(value); else tree->addTopLevelItem(value);
        return value;
    };
    auto *local = item(nullptr, QStringLiteral("本地标注"), QStringLiteral("normal-local-root"), {}, data.normalLocalVisible);
    local->setToolTip(0, QStringLiteral("右键添加文件夹；文件夹内可添加标记和标绘，独立保存在 squad-annotations.xml"));
    std::function<void(QTreeWidgetItem *, const QString &, bool)> appendFolders;
    appendFolders = [&](QTreeWidgetItem *parent, const QString &parentId, bool parentMatched) {
        for (const auto &folder : data.normalFolders) {
            if (folder.parentId != parentId) continue;
            const bool showContents = parentMatched || matches(folder.name);
            auto *branch = item(parent, folder.name, QStringLiteral("normal-folder"), folder.id, folder.visible);
            branch->setToolTip(0, folder.description);
            appendFolders(branch, folder.id, showContents);
            for (const auto &marker : data.normalMarkers) {
                if (marker.folderId != folder.id || (!showContents && !matches(marker.name))) continue;
                auto *leaf = item(branch, marker.name, QStringLiteral("normal-marker"), marker.id, marker.visible);
                leaf->setData(0, Qt::UserRole + 2, folder.id);
                leaf->setIcon(0, QIcon(markerIcon(marker.geometryType)));
            }
            if (!showContents && branch->childCount() == 0) delete branch;
        }
    };
    appendFolders(local, {}, filter.isEmpty());
}
} // namespace Zqxxxt
