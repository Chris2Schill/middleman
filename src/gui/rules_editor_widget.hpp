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

    explicit RulesEditorWidget(const QString& schemaFilePath, QWidget* parent = nullptr);

    bool setSchemaJson(const QByteArray &json);
    QByteArray schemaJson(bool pretty = true);

    bool setSchemaFromFile(const QString& filePath, QString* errorOut = nullptr);
    bool saveSchemaToFile(const QString& filePath, bool pretty = true, QString* errorOut = nullptr);

signals:
    void schemaChanged(const QByteArray &json);

private:
    enum class ValueType { Int, Double, Bool, String };

    struct RulePage {
        QWidget*       page       = nullptr;
        QTableWidget*  conditions = nullptr;
        QTableWidget*  mutations  = nullptr;
        QToolButton*   addCond    = nullptr;
        QToolButton*   delCond    = nullptr;
        QToolButton*   addMut     = nullptr;
        QToolButton*   delMut     = nullptr;
    };

    QTabWidget*   tabs_       = nullptr;
    QToolButton*  addRuleBtn_ = nullptr;
    QToolButton*  delRuleBtn_ = nullptr;
    QToolButton*  importBtn_   = nullptr;
    QToolButton*  exportBtn_   = nullptr;

    QIcon stdIcon(QStyle::StandardPixmap sp) const;
    QToolButton* makeIconToolButton(QStyle::StandardPixmap sp, const QString& tip);
    void applyModernStyle();
    void polishTable(QTableWidget* t);

    // UI helpers
    QComboBox*  makeOperatorCombo(const QString& current = QString());
    QComboBox*  makeTypeCombo(ValueType initial);
    void        setupConditionsTable(QTableWidget* t);
    void        setupMutationsTable(QTableWidget* t);
    RulePage    createRulePage();

    // Row ops
    void addConditionRow(RulePage& rp, const QJsonObject& cond = {});
    void addMutationRow(RulePage& rp, const QJsonObject& mut = {});

    // Data helpers
    QJsonObject readConditionRow(const QTableWidget* t, int row);
    QJsonObject readMutationRow(const QTableWidget* t, int row);
    QJsonObject readRuleFrom(const RulePage& rp);

    void writeConditionTo(const QJsonObject& cond, QTableWidget* t, int row);
    void writeMutationTo(const QJsonObject& mut, QTableWidget* t, int row);
    void writeRuleTo(const QJsonObject& rule, RulePage& rp);

    // Type/editor helpers
    static ValueType inferTypeFromJson(const QJsonValue& v);
    static ValueType inferTypeFromText(const QString& s);
    QWidget* createValueEditor(ValueType type, const QJsonValue& initial);
    QJsonValue readValueFromEditor(QWidget* editor, ValueType type) const;

    void installEditorSignals(QWidget* editor); // emit on edits
    void emitCurrentSchemaChanged();

private slots:
    bool loadSchemaFromDialog();
    bool saveSchemaToDialog(bool pretty = true);

    void onAddRule();
    void onDelRule();
};

