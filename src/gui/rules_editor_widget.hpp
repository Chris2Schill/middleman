#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSplitter>
#include <QtGui/QIcon>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QVariant>
#include <QtCore/QDebug>

class RulesEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit RulesEditorWidget(QWidget *parent = nullptr);

    // Load/save JSON in the schema shape you provided
    bool setSchemaJson(const QByteArray &json);
    QByteArray schemaJson(bool pretty = true);

signals:
    // Emitted whenever tables change (row add/remove or edit)
    void schemaChanged(const QByteArray &json);

private:
    struct RulePage {
        QWidget* page = nullptr;
        QTableWidget* conditions = nullptr;
        QTableWidget* mutations = nullptr;
        QToolButton* addCond = nullptr;
        QToolButton* delCond = nullptr;
        QToolButton* addMut = nullptr;
        QToolButton* delMut = nullptr;
    };

    QTabWidget* tabs_ = nullptr;
    QToolButton* addRuleBtn_ = nullptr;
    QToolButton* delRuleBtn_ = nullptr;

    // UI helpers
    RulePage createRulePage();
    void setupConditionsTable(QTableWidget* t);
    void setupMutationsTable(QTableWidget* t);
    QComboBox* makeOperatorCombo(const QString& current = QString());

    // Data helpers
    QJsonObject readRuleFrom(const RulePage& rp);
    QJsonObject readConditionRow(const QTableWidget* t, int row);
    QJsonObject readMutationRow(const QTableWidget* t, int row);

    void writeRuleTo(const QJsonObject& rule, RulePage& rp);
    void writeConditionTo(const QJsonObject& cond, QTableWidget* t, int row);
    void writeMutationTo(const QJsonObject& mut, QTableWidget* t, int row);

    void emitCurrentSchemaChanged();

    // Row ops
    void addConditionRow(RulePage& rp, const QJsonObject& cond = {});
    void addMutationRow(RulePage& rp, const QJsonObject& mut = {});

private slots:
    void onAddRule();
    void onDelRule();
};

