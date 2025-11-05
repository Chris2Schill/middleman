#include "rules_editor_widget.hpp"

static inline QToolButton* makeToolButton(const QString& text, const QString& tt) {
    auto* b = new QToolButton;
    b->setText(text);
    b->setToolTip(tt);
    b->setAutoRaise(true);
    return b;
}

RulesEditorWidget::RulesEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);

    // Top bar for rules add/remove
    auto* ruleBar = new QHBoxLayout;
    auto* rulesLbl = new QLabel("Rules");
    rulesLbl->setStyleSheet("font-weight: 600;");
    addRuleBtn_ = makeToolButton("+ Rule", "Add a new rule");
    delRuleBtn_ = makeToolButton("− Rule", "Delete current rule");
    ruleBar->addWidget(rulesLbl);
    ruleBar->addStretch();
    ruleBar->addWidget(addRuleBtn_);
    ruleBar->addWidget(delRuleBtn_);
    root->addLayout(ruleBar);

    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(false);
    root->addWidget(tabs_);

    // Keep compact
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    // Create an initial empty rule
    onAddRule();

    connect(addRuleBtn_, &QToolButton::clicked, this, &RulesEditorWidget::onAddRule);
    connect(delRuleBtn_, &QToolButton::clicked, this, &RulesEditorWidget::onDelRule);
}

QComboBox* RulesEditorWidget::makeOperatorCombo(const QString& current) {
    static const QStringList ops = {"==", "!=", "<", ">", "<=", ">="};
    auto* c = new QComboBox;
    c->addItems(ops);
    int idx = current.isEmpty() ? 0 : ops.indexOf(current);
    c->setCurrentIndex(idx < 0 ? 0 : idx);
    connect(c, &QComboBox::currentTextChanged, this, [this](const QString&){
        emitCurrentSchemaChanged();
    });
    return c;
}

void RulesEditorWidget::setupConditionsTable(QTableWidget* t) {
    t->setColumnCount(3);
    t->setHorizontalHeaderLabels({"field", "operator", "value"});
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::AllEditTriggers);
}

void RulesEditorWidget::setupMutationsTable(QTableWidget* t) {
    t->setColumnCount(2);
    t->setHorizontalHeaderLabels({"field", "new_value"});
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::AllEditTriggers);
}

RulesEditorWidget::RulePage RulesEditorWidget::createRulePage() {
    RulePage rp;

    rp.page = new QWidget;
    auto* v = new QVBoxLayout(rp.page);

    auto* splitter = new QSplitter(Qt::Vertical, rp.page);

    // Conditions area
    auto* condWrap = new QWidget;
    auto* condLayout = new QVBoxLayout(condWrap);
    auto* condBar = new QHBoxLayout;
    auto* condLbl = new QLabel("Conditions");
    condLbl->setStyleSheet("font-weight: 600;");
    rp.addCond = makeToolButton("+", "Add condition");
    rp.delCond = makeToolButton("−", "Delete selected condition");
    condBar->addWidget(condLbl);
    condBar->addStretch();
    condBar->addWidget(rp.addCond);
    condBar->addWidget(rp.delCond);
    condLayout->addLayout(condBar);

    rp.conditions = new QTableWidget;
    setupConditionsTable(rp.conditions);
    condLayout->addWidget(rp.conditions);

    // Mutations area
    auto* mutWrap = new QWidget;
    auto* mutLayout = new QVBoxLayout(mutWrap);
    auto* mutBar = new QHBoxLayout;
    auto* mutLbl = new QLabel("Mutations");
    mutLbl->setStyleSheet("font-weight: 600;");
    rp.addMut = makeToolButton("+", "Add mutation");
    rp.delMut = makeToolButton("−", "Delete selected mutation");
    mutBar->addWidget(mutLbl);
    mutBar->addStretch();
    mutBar->addWidget(rp.addMut);
    mutBar->addWidget(rp.delMut);
    mutLayout->addLayout(mutBar);

    rp.mutations = new QTableWidget;
    setupMutationsTable(rp.mutations);
    mutLayout->addWidget(rp.mutations);

    splitter->addWidget(condWrap);
    splitter->addWidget(mutWrap);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    v->addWidget(splitter);

    // Wiring for row add/remove
    connect(rp.addCond, &QToolButton::clicked, this, [this, rp]() mutable { addConditionRow(rp); });
    connect(rp.delCond, &QToolButton::clicked, this, [this, rp]() mutable {
        auto* t = rp.conditions;
        int r = t->currentRow();
        if (r >= 0) {
            t->removeRow(r);
            emitCurrentSchemaChanged();
        }
    });

    connect(rp.addMut, &QToolButton::clicked, this, [this, rp]() mutable { addMutationRow(rp); });
    connect(rp.delMut, &QToolButton::clicked, this, [this, rp]() mutable {
        auto* t = rp.mutations;
        int r = t->currentRow();
        if (r >= 0) {
            t->removeRow(r);
            emitCurrentSchemaChanged();
        }
    });

    // Emit schemaChanged on item edits
    auto wireEdits = [this](QTableWidget* t){
        connect(t, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) { emitCurrentSchemaChanged(); });
    };
    wireEdits(rp.conditions);
    wireEdits(rp.mutations);

    return rp;
}

void RulesEditorWidget::addConditionRow(RulePage& rp, const QJsonObject& cond) {
    auto* t = rp.conditions;
    int row = t->rowCount();
    t->insertRow(row);

    auto* fieldItem = new QTableWidgetItem(cond.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    auto* opCombo = makeOperatorCombo(cond.value("operator").toString());
    t->setCellWidget(row, 1, opCombo);

    auto* valItem = new QTableWidgetItem(cond.value("value").toVariant().toString());
    valItem->setFlags(valItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 2, valItem);

    emitCurrentSchemaChanged();
}

void RulesEditorWidget::addMutationRow(RulePage& rp, const QJsonObject& mut) {
    auto* t = rp.mutations;
    int row = t->rowCount();
    t->insertRow(row);

    auto* fieldItem = new QTableWidgetItem(mut.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    auto* valItem = new QTableWidgetItem(mut.value("new_value").toVariant().toString());
    valItem->setFlags(valItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 1, valItem);

    emitCurrentSchemaChanged();
}

void RulesEditorWidget::onAddRule() {
    RulePage rp = createRulePage();
    int idx = tabs_->addTab(rp.page, QString("Rule %1").arg(tabs_->count() + 1));
    tabs_->setCurrentIndex(idx);
    emitCurrentSchemaChanged();
}

void RulesEditorWidget::onDelRule() {
    int idx = tabs_->currentIndex();
    if (idx >= 0 && tabs_->count() > 0) {
        QWidget* w = tabs_->widget(idx);
        tabs_->removeTab(idx);
        w->deleteLater();
        // Re-label remaining tabs
        for (int i = 0; i < tabs_->count(); ++i) {
            tabs_->setTabText(i, QString("Rule %1").arg(i + 1));
        }
        if (tabs_->count() == 0) {
            onAddRule();
        }
        emitCurrentSchemaChanged();
    }
}

QJsonObject RulesEditorWidget::readConditionRow(const QTableWidget* t, int row) {
    QJsonObject cond;
    auto* fieldItem = t->item(row, 0);
    auto* valItem   = t->item(row, 2);
    auto* opCombo   = qobject_cast<QComboBox*>(t->cellWidget(row, 1));

    cond["field"] = fieldItem ? fieldItem->text() : "";
    cond["operator"] = opCombo ? opCombo->currentText() : "==";

    // Try to coerce to number if it looks like one
    QString v = valItem ? valItem->text() : "";
    bool okInt = false;
    qlonglong asInt = v.toLongLong(&okInt);
    if (okInt) cond["value"] = static_cast<double>(asInt);
    else cond["value"] = v;

    return cond;
}

QJsonObject RulesEditorWidget::readMutationRow(const QTableWidget* t, int row) {
    QJsonObject mut;
    auto* fieldItem = t->item(row, 0);
    auto* valItem   = t->item(row, 1);

    mut["field"] = fieldItem ? fieldItem->text() : "";

    QString v = valItem ? valItem->text() : "";
    bool okInt = false;
    qlonglong asInt = v.toLongLong(&okInt);
    if (okInt) mut["new_value"] = static_cast<double>(asInt);
    else mut["new_value"] = v;

    return mut;
}

QJsonObject RulesEditorWidget::readRuleFrom(const RulePage& rp) {
    QJsonObject rule;
    QJsonArray conditions;
    QJsonArray mutations;

    for (int r = 0; r < rp.conditions->rowCount(); ++r)
        conditions.push_back(readConditionRow(rp.conditions, r));

    for (int r = 0; r < rp.mutations->rowCount(); ++r)
        mutations.push_back(readMutationRow(rp.mutations, r));

    rule["conditions"] = conditions;
    rule["mutations"] = mutations;
    return rule;
}

void RulesEditorWidget::writeConditionTo(const QJsonObject& cond, QTableWidget* t, int row) {
    if (row == t->rowCount()) t->insertRow(row);

    auto* fieldItem = new QTableWidgetItem(cond.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    auto* opCombo = new QComboBox;
    opCombo->addItems({"==", "!=", "<", ">", "<=", ">="});
    opCombo->setCurrentText(cond.value("operator").toString("=="));
    t->setCellWidget(row, 1, opCombo);

    QString v;
    auto val = cond.value("value");
    if (val.isDouble()) v = QString::number(val.toDouble());
    else v = val.toVariant().toString();

    auto* valItem = new QTableWidgetItem(v);
    valItem->setFlags(valItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 2, valItem);

    // react to operator changes
    connect(opCombo, &QComboBox::currentTextChanged, this, [this](const QString&){ emitCurrentSchemaChanged(); });
}

void RulesEditorWidget::writeMutationTo(const QJsonObject& mut, QTableWidget* t, int row) {
    if (row == t->rowCount()) t->insertRow(row);

    auto* fieldItem = new QTableWidgetItem(mut.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    QString v;
    auto val = mut.value("new_value");
    if (val.isDouble()) v = QString::number(val.toDouble());
    else v = val.toVariant().toString();

    auto* valItem = new QTableWidgetItem(v);
    valItem->setFlags(valItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 1, valItem);
}

void RulesEditorWidget::writeRuleTo(const QJsonObject& rule, RulePage& rp) {
    rp.conditions->setRowCount(0);
    rp.mutations->setRowCount(0);

    const auto conds = rule.value("conditions").toArray();
    for (int i = 0; i < conds.size(); ++i)
        writeConditionTo(conds[i].toObject(), rp.conditions, rp.conditions->rowCount());

    const auto muts = rule.value("mutations").toArray();
    for (int i = 0; i < muts.size(); ++i)
        writeMutationTo(muts[i].toObject(), rp.mutations, rp.mutations->rowCount());
}

bool RulesEditorWidget::setSchemaJson(const QByteArray &json) {
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "RulesEditorWidget: JSON parse error:" << err.errorString();
        return false;
    }
    const auto root = doc.object();
    const auto rules = root.value("rules").toArray();

    // Clear all tabs
    while (tabs_->count() > 0) {
        QWidget* w = tabs_->widget(0);
        tabs_->removeTab(0);
        w->deleteLater();
    }

    if (rules.isEmpty()) {
        onAddRule();
        return true;
    }

    for (int i = 0; i < rules.size(); ++i) {
        RulePage rp = createRulePage();
        writeRuleTo(rules[i].toObject(), rp);
        int idx = tabs_->addTab(rp.page, QString("Rule %1").arg(tabs_->count() + 1));
        if (i == rules.size() - 1) tabs_->setCurrentIndex(idx);
    }

    emitCurrentSchemaChanged();
    return true;
}

QByteArray RulesEditorWidget::schemaJson(bool pretty) {
    QJsonObject root;
    QJsonArray rules;

    for (int i = 0; i < tabs_->count(); ++i) {
        auto* page = tabs_->widget(i);
        // Reconstruct a RulePage façade to read widgets
        RulePage rp;
        rp.page = page;
        // Find the tables in this page
        rp.conditions = page->findChild<QTableWidget*>();
        rp.mutations  = page->findChildren<QTableWidget*>().size() > 1
                        ? page->findChildren<QTableWidget*>().at(1)
                        : nullptr;

        // Slightly safer: locate by labels' proximity—here we rely on creation order.
        if (!rp.conditions || !rp.mutations) {
            // fallback: iterate all tables
            auto tables = page->findChildren<QTableWidget*>();
            if (tables.size() >= 2) {
                rp.conditions = tables.at(0);
                rp.mutations = tables.at(1);
            }
        }

        rules.push_back(readRuleFrom(rp));
    }

    root["rules"] = rules;
    QJsonDocument doc(root);
    return pretty ? doc.toJson(QJsonDocument::Indented) : doc.toJson(QJsonDocument::Compact);
}

void RulesEditorWidget::emitCurrentSchemaChanged() {
    emit schemaChanged(schemaJson(true));
}

