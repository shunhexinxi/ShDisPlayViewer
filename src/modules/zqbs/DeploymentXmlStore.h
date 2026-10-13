#pragma once

#include "modules/zqbs/DeploymentTypes.h"

#include <QString>

namespace Zqxxxt {

class DeploymentXmlStore final
{
public:
    explicit DeploymentXmlStore(QString directory = {});

    [[nodiscard]] QString directory() const;
    [[nodiscard]] QString fixedFilePath() const;
    [[nodiscard]] QString fixedAnnotationsFilePath() const;
    [[nodiscard]] QString normalFilePath() const;
    [[nodiscard]] QString dynamicFilePath() const;
    [[nodiscard]] QString customLayerFilePath() const;
    [[nodiscard]] QString marksFilePath() const;
    [[nodiscard]] QString plotsFilePath() const;
    [[nodiscard]] QString viewpointFilePath() const;
    [[nodiscard]] QString systemConfigFilePath() const;
    [[nodiscard]] QString featureConfigFilePath() const;

    bool load(DeploymentData &data, QString &errorMessage);
    bool saveFixed(const DeploymentData &data, QString &errorMessage) const;
    bool saveFixedAnnotations(const DeploymentData &data, QString &errorMessage) const;
    bool saveNormal(const DeploymentData &data, QString &errorMessage) const;
    bool saveDynamic(const DeploymentData &data, QString &errorMessage) const;
    bool saveAll(const DeploymentData &data, QString &errorMessage) const;

private:
    bool ensureFiles(QString &errorMessage) const;
    bool loadFixed(DeploymentData &data, QString &errorMessage) const;
    bool loadFixedAnnotations(DeploymentData &data, QString &errorMessage) const;
    bool loadNormal(DeploymentData &data, QString &errorMessage) const;
    bool loadDynamic(DeploymentData &data, QString &errorMessage) const;

    QString m_directory;
};

} // namespace Zqxxxt
