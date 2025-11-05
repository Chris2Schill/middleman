#include "rules_editor_widget.hpp"

#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QSaveFile>
#include <QShortcut>

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
    root->setContentsMargins(2,2,2,2);
    root->setSpacing(2);

    /* --- Top bar / toolbar-like container --- */
    auto* topBarWrap = new QWidget;              // give it an objectName so CSS applies
    topBarWrap->setObjectName("TopBar");
    auto* ruleBar = new QHBoxLayout(topBarWrap);
    ruleBar->setContentsMargins(1,1,1,1);
    ruleBar->setSpacing(2);

    auto* rulesLbl = new QLabel("Rules");
    rulesLbl->setProperty("role", "title");

    importBtn_ = makeIconToolButton(QStyle::SP_DialogOpenButton,  "Import schema (Ctrl+O)");
    exportBtn_ = makeIconToolButton(QStyle::SP_DialogSaveButton,  "Export schema (Ctrl+S)");
    addRuleBtn_ = makeIconToolButton(QStyle::SP_FileDialogNewFolder, "Add Rule");
    delRuleBtn_ = makeIconToolButton(QStyle::SP_TrashIcon, "Delete current Rule");

    // left title
    ruleBar->addWidget(rulesLbl);
    ruleBar->addSpacing(8);
    ruleBar->addStretch();

    // center/right controls
    // group import/export together
    ruleBar->addWidget(importBtn_);
    ruleBar->addWidget(exportBtn_);

    // a tiny separator
    auto addSep = [&](){
        auto* sep = new QWidget; sep->setFixedWidth(8); return sep;
    };
    ruleBar->addWidget(addSep());

    // rule controls
    ruleBar->addWidget(addRuleBtn_);
    ruleBar->addWidget(delRuleBtn_);

    root->addWidget(topBarWrap);

    /* --- Tabs --- */
    tabs_ = new QTabWidget(this);
    tabs_->setContentsMargins(2,2,2,2);
    tabs_->setTabsClosable(false);
    root->addWidget(tabs_);

    /* --- Global style --- */
    applyModernStyle();
    QFont f = font();
    f.setPointSizeF(std::max(7.5, f.pointSizeF() - 1.0));   // guard against too small
    setFont(f);

    /* --- Initial rule and wiring --- */
    onAddRule();
    connect(addRuleBtn_, &QToolButton::clicked, this, &RulesEditorWidget::onAddRule);
    connect(delRuleBtn_, &QToolButton::clicked, this, &RulesEditorWidget::onDelRule);
    connect(importBtn_, &QToolButton::clicked, this, [this]{ loadSchemaFromDialog(); });
    connect(exportBtn_, &QToolButton::clicked, this, [this]{ saveSchemaToDialog(true); });

    /* --- Shortcuts --- */
    new QShortcut(QKeySequence::Open,  this, [this]{ loadSchemaFromDialog(); });
    new QShortcut(QKeySequence::Save,  this, [this]{ saveSchemaToDialog(true); });
}

RulesEditorWidget::RulesEditorWidget(const QString& schemaFilePath, QWidget* parent)
    : RulesEditorWidget(parent)  // delegate to primary ctor to build UI
{
    QString err;
    if (!setSchemaFromFile(schemaFilePath, &err)) {
        qWarning() << "RulesEditorWidget: failed to load schema from"
                   << schemaFilePath << ":" << err;
    }
}

bool RulesEditorWidget::setSchemaFromFile(const QString& filePath, QString* errorOut) {
    QFile f(filePath);
    if (!f.exists()) {
        if (errorOut) *errorOut = QStringLiteral("File does not exist");
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Unable to open file for reading");
        return false;
    }

    const QByteArray data = f.readAll();
    f.close();

    if (!setSchemaJson(data)) {
        if (errorOut) *errorOut = QStringLiteral("Invalid JSON or schema format");
        return false;
    }
    return true;
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
    t->setColumnCount(4);
    t->setHorizontalHeaderLabels({"field", "operator", "type", "value"});
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::AllEditTriggers);
}

void RulesEditorWidget::setupMutationsTable(QTableWidget* t) {
    t->setColumnCount(3);
    t->setHorizontalHeaderLabels({"field", "type", "new_value"});
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
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Vertical, rp.page);

    /* --- Conditions card --- */
    auto* condCard = new QFrame;
    condCard->setProperty("card", true);
    auto* condWrap = new QWidget(condCard);
    auto* condCardLayout = new QVBoxLayout(condCard);
    condCardLayout->setContentsMargins(3,3,3,3);
    condCardLayout->addWidget(condWrap);

    auto* condLayout = new QVBoxLayout(condWrap);
    auto* condBar = new QHBoxLayout;
    auto* condLbl = new QLabel("Conditions");
    condLbl->setProperty("role", "title");
    rp.addCond = makeIconToolButton(QStyle::SP_FileDialogNewFolder, "Add condition");
    rp.delCond = makeIconToolButton(QStyle::SP_TrashIcon, "Delete selected condition");
    condBar->addWidget(condLbl);
    condBar->addStretch();
    condBar->addWidget(rp.addCond);
    condBar->addWidget(rp.delCond);
    condLayout->addLayout(condBar);

    rp.conditions = new QTableWidget;
    setupConditionsTable(rp.conditions);
    polishTable(rp.conditions);
    condLayout->addWidget(rp.conditions);

    /* --- Mutations card --- */
    auto* mutCard = new QFrame;
    mutCard->setProperty("card", true);
    auto* mutWrap = new QWidget(mutCard);
    auto* mutCardLayout = new QVBoxLayout(mutCard);
    mutCardLayout->setContentsMargins(3,3,3,3);
    mutCardLayout->addWidget(mutWrap);

    auto* mutLayout = new QVBoxLayout(mutWrap);
    auto* mutBar = new QHBoxLayout;
    auto* mutLbl = new QLabel("Mutations");
    mutLbl->setProperty("role", "title");
    rp.addMut = makeIconToolButton(QStyle::SP_FileDialogNewFolder, "Add mutation");
    rp.delMut = makeIconToolButton(QStyle::SP_TrashIcon, "Delete selected mutation");
    mutBar->addWidget(mutLbl);
    mutBar->addStretch();
    mutBar->addWidget(rp.addMut);
    mutBar->addWidget(rp.delMut);
    mutLayout->addLayout(mutBar);

    rp.mutations = new QTableWidget;
    setupMutationsTable(rp.mutations);
    polishTable(rp.mutations);
    mutLayout->addWidget(rp.mutations);

    /* --- Splitter --- */
    splitter->addWidget(condCard);
    splitter->addWidget(mutCard);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setHandleWidth(2);

    v->addWidget(splitter);

    /* wiring stays the same ... */


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

    // field
    auto* fieldItem = new QTableWidgetItem(cond.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    // operator
    auto* opCombo = makeOperatorCombo(cond.value("operator").toString());
    t->setCellWidget(row, 1, opCombo);

    // type + value
    QJsonValue v = cond.value("value");
    ValueType vt = v.isUndefined() || v.isNull()
                   ? ValueType::String
                   : inferTypeFromJson(v);
    auto* typeCombo = makeTypeCombo(vt);
    t->setCellWidget(row, 2, typeCombo);

    QWidget* editor = createValueEditor(vt, v.isUndefined() ? QJsonValue("") : v);
    t->setCellWidget(row, 3, editor);

    emitCurrentSchemaChanged();
}

void RulesEditorWidget::addMutationRow(RulePage& rp, const QJsonObject& mut) {
    auto* t = rp.mutations;
    int row = t->rowCount();
    t->insertRow(row);

    // field
    auto* fieldItem = new QTableWidgetItem(mut.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    // type + value
    QJsonValue v = mut.value("new_value");
    ValueType vt = v.isUndefined() || v.isNull()
                   ? ValueType::String
                   : inferTypeFromJson(v);
    auto* typeCombo = makeTypeCombo(vt);
    t->setCellWidget(row, 1, typeCombo);

    QWidget* editor = createValueEditor(vt, v.isUndefined() ? QJsonValue("") : v);
    t->setCellWidget(row, 2, editor);

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
    auto* opCombo   = qobject_cast<QComboBox*>(t->cellWidget(row, 1));
    auto* typeCombo = qobject_cast<QComboBox*>(t->cellWidget(row, 2));
    QWidget* editor = t->cellWidget(row, 3);

    cond["field"] = fieldItem ? fieldItem->text() : "";
    cond["operator"] = opCombo ? opCombo->currentText() : "==";

    ValueType vt = typeCombo ? static_cast<ValueType>(typeCombo->currentIndex()) : ValueType::String;
    cond["value"] = readValueFromEditor(editor, vt);

    return cond;
}

QJsonObject RulesEditorWidget::readMutationRow(const QTableWidget* t, int row) {
    QJsonObject mut;
    auto* fieldItem = t->item(row, 0);
    auto* typeCombo = qobject_cast<QComboBox*>(t->cellWidget(row, 1));
    QWidget* editor = t->cellWidget(row, 2);

    mut["field"] = fieldItem ? fieldItem->text() : "";

    ValueType vt = typeCombo ? static_cast<ValueType>(typeCombo->currentIndex()) : ValueType::String;
    mut["new_value"] = readValueFromEditor(editor, vt);

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

    // field
    auto* fieldItem = new QTableWidgetItem(cond.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    // operator
    auto* opCombo = new QComboBox;
    opCombo->addItems({"==", "!=", "<", ">", "<=", ">="});
    opCombo->setCurrentText(cond.value("operator").toString("=="));
    t->setCellWidget(row, 1, opCombo);
    connect(opCombo, &QComboBox::currentTextChanged, this, [this](const QString&){ emitCurrentSchemaChanged(); });

    // type + value
    QJsonValue v = cond.value("value");
    ValueType vt = inferTypeFromJson(v);
    auto* typeCombo = makeTypeCombo(vt);
    t->setCellWidget(row, 2, typeCombo);

    QWidget* editor = createValueEditor(vt, v);
    t->setCellWidget(row, 3, editor);
}

void RulesEditorWidget::writeMutationTo(const QJsonObject& mut, QTableWidget* t, int row) {
    if (row == t->rowCount()) t->insertRow(row);

    // field
    auto* fieldItem = new QTableWidgetItem(mut.value("field").toString());
    fieldItem->setFlags(fieldItem->flags() | Qt::ItemIsEditable);
    t->setItem(row, 0, fieldItem);

    // type + value
    QJsonValue v = mut.value("new_value");
    ValueType vt = inferTypeFromJson(v);
    auto* typeCombo = makeTypeCombo(vt);
    t->setCellWidget(row, 1, typeCombo);

    QWidget* editor = createValueEditor(vt, v);
    t->setCellWidget(row, 2, editor);
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

        RulePage rp;
        rp.page       = page;
        rp.conditions = page->findChild<QTableWidget*>("conditionsTable");
        rp.mutations  = page->findChild<QTableWidget*>("mutationsTable");

        if (!rp.conditions || !rp.mutations) {
            qWarning() << "RulesEditorWidget: could not find tables on tab" << i;
            continue;
        }

        rules.push_back(readRuleFrom(rp));
    }

    root["rules"] = rules;
    QJsonDocument doc(root);
    return pretty ? doc.toJson(QJsonDocument::Indented)
                  : doc.toJson(QJsonDocument::Compact);
}

void RulesEditorWidget::emitCurrentSchemaChanged() {
    emit schemaChanged(schemaJson(true));
}

RulesEditorWidget::ValueType RulesEditorWidget::inferTypeFromJson(const QJsonValue& v) {
    if (v.isBool()) return ValueType::Bool;
    if (v.isDouble()) {
        const double d = v.toDouble();
        const double i = std::llround(d);
        return (std::fabs(d - i) < 1e-9) ? ValueType::Int : ValueType::Double;
    }
    // strings or null -> decide by text later; default to string
    return ValueType::String;
}

RulesEditorWidget::ValueType RulesEditorWidget::inferTypeFromText(const QString& s) {
    if (s.compare("true", Qt::CaseInsensitive) == 0 ||
        s.compare("false", Qt::CaseInsensitive) == 0)
        return ValueType::Bool;

    bool okInt=false; s.toLongLong(&okInt);
    if (okInt) return ValueType::Int;

    bool okD=false; s.toDouble(&okD);
    if (okD) return ValueType::Double;

    return ValueType::String;
}

QComboBox* RulesEditorWidget::makeTypeCombo(ValueType initial) {
    auto* cb = new QComboBox;
    cb->addItems({"int", "double", "bool", "string"});
    cb->setCurrentIndex(static_cast<int>(initial));
    connect(cb, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, cb](int){
        // when type changes, rebuild the value editor in the same row
        auto* t = qobject_cast<QTableWidget*>(cb->parentWidget()->parentWidget());
        if (!t) return;
        int row = t->indexAt(cb->parentWidget()->pos()).row();
        if (row < 0) return;

        const bool isCond = (t->columnCount() == 4);
        int valueCol = isCond ? 3 : 2;

        // grab current text to preserve if possible
        QWidget* oldEditor = t->cellWidget(row, valueCol);
        QString existingText;
        if (auto* le = qobject_cast<QLineEdit*>(oldEditor)) existingText = le->text();
        else if (auto* sb = qobject_cast<QSpinBox*>(oldEditor)) existingText = QString::number(sb->value());
        else if (auto* dsb = qobject_cast<QDoubleSpinBox*>(oldEditor)) existingText = QString::number(dsb->value());
        else if (auto* chk = qobject_cast<QCheckBox*>(oldEditor)) existingText = chk->isChecked() ? "true" : "false";
        else if (auto* itm = t->item(row, valueCol)) existingText = itm->text();

        ValueType vt = static_cast<ValueType>(cb->currentIndex());
        QJsonValue init;
        switch (vt) {
            case ValueType::Bool:   init = (existingText.compare("true", Qt::CaseInsensitive)==0); break;
            case ValueType::Int:    init = existingText.toLongLong(); break;
            case ValueType::Double: init = existingText.toDouble(); break;
            case ValueType::String: init = existingText; break;
        }

        QWidget* editor = createValueEditor(vt, init);
        t->setCellWidget(row, valueCol, editor);
        if (t->item(row, valueCol)) { delete t->item(row, valueCol); t->setItem(row, valueCol, nullptr); }
        emitCurrentSchemaChanged();
    });
    return cb;
}

QWidget* RulesEditorWidget::createValueEditor(ValueType type, const QJsonValue& initial) {
    switch (type) {
        case ValueType::Int: {
            auto* sb = new QSpinBox;
            sb->setMinimum(std::numeric_limits<int>::min());
            sb->setMaximum(std::numeric_limits<int>::max());
            sb->setValue(initial.isDouble() ? static_cast<int>(std::llround(initial.toDouble()))
                                            : initial.toVariant().toInt());
            installEditorSignals(sb);
            return sb;
        }
        case ValueType::Double: {
            auto* dsb = new QDoubleSpinBox;
            dsb->setDecimals(6);
            dsb->setMinimum(std::numeric_limits<double>::lowest()/2);
            dsb->setMaximum(std::numeric_limits<double>::max()/2);
            dsb->setValue(initial.toDouble());
            installEditorSignals(dsb);
            return dsb;
        }
        case ValueType::Bool: {
            auto* chk = new QCheckBox;
            chk->setChecked(initial.toBool());
            // center it
            auto* w = new QWidget;
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(0,0,0,0);
            l->addStretch();
            l->addWidget(chk);
            l->addStretch();
            installEditorSignals(chk);
            return w; // return wrapper; we'll special-case reading
        }
        case ValueType::String:
        default: {
            auto* le = new QLineEdit;
            le->setText(initial.isString() ? initial.toString()
                                           : initial.toVariant().toString());
            installEditorSignals(le);
            return le;
        }
    }
}

QJsonValue RulesEditorWidget::readValueFromEditor(QWidget* editor, ValueType type) const {
    // bool editor is wrapped in a QWidget with QCheckBox child
    if (type == ValueType::Bool) {
        QCheckBox* chk = editor->findChild<QCheckBox*>();
        return chk ? QJsonValue(chk->isChecked()) : QJsonValue(false);
    }
    if (auto* sb = qobject_cast<QSpinBox*>(editor)) return QJsonValue(sb->value());
    if (auto* dsb = qobject_cast<QDoubleSpinBox*>(editor)) return QJsonValue(dsb->value());
    if (auto* le = qobject_cast<QLineEdit*>(editor)) return QJsonValue(le->text());
    // Fallback: if it's an item cell (shouldn't happen after upgrade)
    return QJsonValue();
}

void RulesEditorWidget::installEditorSignals(QWidget* editor) {
    if (auto* le = qobject_cast<QLineEdit*>(editor))
        connect(le, &QLineEdit::textChanged, this, [this](const QString&){ emitCurrentSchemaChanged(); });
    if (auto* sb = qobject_cast<QSpinBox*>(editor))
        connect(sb, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){ emitCurrentSchemaChanged(); });
    if (auto* dsb = qobject_cast<QDoubleSpinBox*>(editor))
        connect(dsb, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double){ emitCurrentSchemaChanged(); });
    if (auto* chk = qobject_cast<QCheckBox*>(editor))
        connect(chk, &QCheckBox::stateChanged, this, [this](int){ emitCurrentSchemaChanged(); });
}

bool RulesEditorWidget::loadSchemaFromDialog() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Rules Schema"),
        QString(),
        tr("JSON files (*.json);;All files (*)")
    );
    if (path.isEmpty()) return false;

    QString err;
    const bool ok = setSchemaFromFile(path, &err);
    if (!ok) {
        qWarning() << "RulesEditorWidget: could not load schema from dialog:" << err;
    }
    return ok;
}

bool RulesEditorWidget::saveSchemaToFile(const QString& filePath, bool pretty, QString* errorOut) {
    if (filePath.trimmed().isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Empty file path");
        return false;
    }

    QSaveFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Unable to open file for writing");
        return false;
    }

    const QByteArray data = schemaJson(pretty);
    if (f.write(data) != data.size()) {
        if (errorOut) *errorOut = QStringLiteral("Failed to write all data");
        f.cancelWriting();
        return false;
    }

    if (!f.commit()) {
        if (errorOut) *errorOut = QStringLiteral("Failed to finalize (commit) save");
        return false;
    }
    return true;
}

bool RulesEditorWidget::saveSchemaToDialog(bool pretty) {
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Rules Schema As"),
        QStringLiteral("rules.json"),
        tr("JSON files (*.json);;All files (*)")
    );
    if (path.isEmpty())
        return false;

    QString err;
    const bool ok = saveSchemaToFile(path, pretty, &err);
    if (!ok) {
        qWarning() << "RulesEditorWidget: could not save schema:" << err;
    }
    return ok;
}

QIcon RulesEditorWidget::stdIcon(QStyle::StandardPixmap sp) const {
    return style()->standardIcon(sp, nullptr, this);
}

QToolButton* RulesEditorWidget::makeIconToolButton(QStyle::StandardPixmap sp, const QString& tip) {
    auto* b = new QToolButton;
    b->setIcon(stdIcon(sp));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setIconSize(QSize(12,12));
    b->setToolButtonStyle(Qt::ToolButtonIconOnly);
    return b;
}

void RulesEditorWidget::applyModernStyle() {
    // soft neutral palette + cards for sections
    setStyleSheet(R"CSS(
        RulesEditorWidget {
            background: palette(base);
        }
        QLabel[role="title"] {
            font-weight: 600;
            color: palette(windowText);
        }
        /* top toolbar row */
        QWidget#TopBar {
            background: palette(window);
            border: 1px solid rgba(0,0,0,25);
            border-radius: 2px;
            padding: 3px;
        }
        QToolButton {
            padding: 1px;
            border-radius: 4px;
        }
        QToolButton:hover {
            background: rgba(0,0,0,6%);
        }
        QToolButton:pressed {
            background: rgba(0,0,0,12%);
        }

        /* 'cards' around conditions/mutations */
        QFrame[card="true"] {
            background: palette(window);
            border: 1px solid rgba(0,0,0,25);
            border-radius: 2;
        }

        /* table visuals */
        QHeaderView::section {
            background: rgba(0,0,0,4%);
            border: none;
            padding: 1px 4px;
            font-weight: 500;
        }
        QTableView {
            gridline-color: rgba(0,0,0,18%);
            selection-background-color: palette(highlight);
            selection-color: palette(highlightedText);
            alternate-background-color: rgba(0,0,0,3%);
        }
    )CSS");
}

void RulesEditorWidget::polishTable(QTableWidget* t) {
    t->setAlternatingRowColors(true);
    t->setShowGrid(true);
    t->setWordWrap(false);

    t->verticalHeader()->setDefaultSectionSize(14);      // was 26/20
    t->horizontalHeader()->setDefaultSectionSize(16);     // header row height
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // zero padding around the view
    t->setStyleSheet(R"CSS(
        QTableView { margin: 0px; padding: 0px; }
    )CSS");
}

