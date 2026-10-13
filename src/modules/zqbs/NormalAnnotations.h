#pragma once

#include "modules/zqbs/DeploymentTypes.h"
#include <QSet>
#include <algorithm>

namespace Zqxxxt {
inline const DeploymentLocalFolder *normalFolder(const DeploymentData &data, const QString &id)
{
    const auto found = std::find_if(data.normalFolders.cbegin(), data.normalFolders.cend(),
        [&id](const DeploymentLocalFolder &folder) { return folder.id == id; });
    return found == data.normalFolders.cend() ? nullptr : &*found;
}

inline bool normalFolderVisible(const DeploymentData &data, QString id)
{
    if (!data.normalLocalVisible) return false;
    QSet<QString> visited;
    while (!id.isEmpty()) {
        if (visited.contains(id)) return false;
        visited.insert(id);
        const auto *folder = normalFolder(data, id);
        if (!folder || !folder->visible) return false;
        id = folder->parentId;
    }
    return true;
}

// A newly placed visible marker should not disappear behind a previously
// hidden root or ancestor folder. Existing child visibility is unchanged.
inline bool revealNormalMarkerAncestors(DeploymentData &data, QString folderId)
{
    bool changed = !data.normalLocalVisible;
    data.normalLocalVisible = true;
    QSet<QString> visited;
    while (!folderId.isEmpty() && !visited.contains(folderId)) {
        visited.insert(folderId);
        auto folder = std::find_if(data.normalFolders.begin(), data.normalFolders.end(),
            [&folderId](const DeploymentLocalFolder &value) { return value.id == folderId; });
        if (folder == data.normalFolders.end())
            break;
        changed = changed || !folder->visible;
        folder->visible = true;
        folderId = folder->parentId;
    }
    return changed;
}

inline QSet<QString> normalFolderSubtree(const DeploymentData &data, const QString &id)
{
    QSet<QString> ids{id};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &folder : data.normalFolders) {
            if (ids.contains(folder.parentId) && !ids.contains(folder.id)) {
                ids.insert(folder.id);
                changed = true;
            }
        }
    }
    return ids;
}

inline void removeNormalFolder(DeploymentData &data, const QString &id)
{
    const auto ids = normalFolderSubtree(data, id);
    data.normalMarkers.erase(std::remove_if(data.normalMarkers.begin(), data.normalMarkers.end(),
        [&ids](const DeploymentMarker &marker) { return ids.contains(marker.folderId); }), data.normalMarkers.end());
    data.normalFolders.erase(std::remove_if(data.normalFolders.begin(), data.normalFolders.end(),
        [&ids](const DeploymentLocalFolder &folder) { return ids.contains(folder.id); }), data.normalFolders.end());
}

// Shared scene composition keeps checkbox behavior independent of the page
// currently selected, of IMS refreshes, and of the saved child checkboxes.
inline QVector<DeploymentMarker> composeDeploymentMarkers(const DeploymentData &data,
    const QVector<DeploymentMarker> &liveMarkers, const QVector<DeploymentMarker> &dynamicMarkers)
{
    QVector<DeploymentMarker> result;
    QSet<QString> consumedLive;
    const auto append = [&](DeploymentMarker marker, bool normal) {
        if (normal) marker.visible = marker.visible && normalFolderVisible(data, marker.folderId);
        if (!marker.bindingType.isEmpty() && !marker.bindingId.isEmpty()) {
            const auto live = std::find_if(liveMarkers.cbegin(), liveMarkers.cend(),
                [&marker](const DeploymentMarker &value) {
                    return value.bindingType == marker.bindingType && value.bindingId == marker.bindingId;
                });
            if (live != liveMarkers.cend()) {
                marker.position = live->position;
                marker.liveState = live->liveState;
                marker.live = true;
                if (marker.visible) consumedLive.insert(live->id);
            }
        }
        result.append(marker);
    };
    for (const auto &marker : data.markers) append(marker, false);
    for (const auto &marker : data.normalMarkers) append(marker, true);
    for (auto marker : liveMarkers) {
        if (consumedLive.contains(marker.id)) continue;
        const auto rule = std::find_if(data.bindingRules.cbegin(), data.bindingRules.cend(),
            [&marker](const DeploymentBindingRule &value) { return value.entityType == marker.bindingType; });
        marker.visible = marker.visible && data.imsLiveVisible
            && (rule == data.bindingRules.cend() || rule->visible);
        result.append(marker);
    }
    result += dynamicMarkers;
    return result;
}
} // namespace Zqxxxt
