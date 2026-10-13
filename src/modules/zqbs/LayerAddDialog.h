#pragma once

#include "ui/SkinnedDialog.h"
#include "modules/zqbs/DeploymentTypes.h"
#include <memory>

class QLabel;
namespace Ui { class CImageLayerDialog; class CVectorLayerDialog; }
namespace Zqxxxt {

class LayerAddDialog final : public SkinnedDialog
{
public:
    LayerAddDialog(const QString &kind, const QString &dataDirectory, QWidget *parent = nullptr);
    ~LayerAddDialog() override;
    DeploymentLayer layer() const;
    QString validationError() const;
    void accept() override;

private:
    void updateRasterDriver();
    void updateRasterSourceHint();
    QString resolveSource(const QString &path) const;
    QString m_kind, m_dataDirectory;
    std::unique_ptr<Ui::CImageLayerDialog> m_raster;
    std::unique_ptr<Ui::CVectorLayerDialog> m_vector;
    QLabel *m_error = nullptr;
};
}
