#pragma once
#include <QWidget>
#include <QJsonObject>
class QTableWidget;
namespace Zqxxxt {
class MarkerActionEditor final : public QWidget {
public:
    explicit MarkerActionEditor(const QJsonObject &association, QWidget *parent = nullptr);
    QJsonObject value() const;
    bool validate(QString *error) const;
    template<class A, class B> void setLabelContents(const A &, const B &) {}
private:
    void addRow(const QJsonObject &action = {});
    QTableWidget *m_table;
};
}
