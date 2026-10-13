#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QVBoxLayout;
class QShowEvent;

namespace Zqxxxt {

class SkinnedDialog : public QDialog
{
public:
    explicit SkinnedDialog(const QString &title, QWidget *parent = nullptr);

    void setSkinTitle(const QString &title);
    [[nodiscard]] QVBoxLayout *contentLayout() const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    QLabel *m_titleLabel = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    bool m_initialGeometryApplied = false;
    bool m_unifiedStyleApplied = false;
};

}
