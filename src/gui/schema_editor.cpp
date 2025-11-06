#include "schema_editor.hpp"

// Readonly delegate for columns we want locked
class NoEditDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override {
        return nullptr; // never create an editor
    }
};

// ---------------- ValueDelegate -----------------
QWidget* ValueDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
    Q_UNUSED(option);
    if (index.column() != 3) return nullptr;
    const QModelIndex metaIdx = index.sibling(index.row(), 0);
    const auto nodeType = metaIdx.data(Roles::NodeType).toString();
    const auto typeName = metaIdx.data(Roles::TypeName).toString();
    if (SIGNED_INT_TYPES.contains(typeName)) {
                // Use QIntValidator for <=32-bit; 64-bit signed gets regex
                auto* le = new QLineEdit(parent);
                if (typeName == "int8")   { le->setValidator(new QIntValidator(-(1<<7),  (1<<7)-1, le)); }
                else if (typeName == "int16") { le->setValidator(new QIntValidator(-(1<<15), (1<<15)-1, le)); }
                else if (typeName == "int32") { le->setValidator(new QIntValidator(INT_MIN, INT_MAX, le)); }
                else { // int64
                    // Accept long decimal or hex; range will be enforced on commit
                    le->setValidator(new QRegularExpressionValidator(QRegularExpression("^-?[0-9]+$|^0x[0-9A-Fa-f]+$"), le));
                }
                le->setPlaceholderText("signed integer (dec or 0x..)");
                return le;
            }
            if (UNSIGNED_INT_TYPES.contains(typeName)) {
                auto* le = new QLineEdit(parent);
                // Accept large decimal or hex without clamping in the validator
                le->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]+$|^0x[0-9A-Fa-f]+$"), le));
                le->setPlaceholderText("unsigned (dec or 0x..)");
                return le;
            }
    if (FLOAT_TYPES.contains(typeName)) {
        auto* le = new QLineEdit(parent);
        auto* v = new QDoubleValidator(le);
        v->setNotation(QDoubleValidator::StandardNotation);
        le->setValidator(v);
        le->setPlaceholderText("floating-point");
        return le;
    }
    if (nodeType == "bits") {
        auto* le = new QLineEdit(parent);
        le->setPlaceholderText("hex (0x..) or binary (e.g. 101010)");
        return le;
    }
    return nullptr;
}

void ValueDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    if (auto* le = qobject_cast<QLineEdit*>(editor)) {
        model->setData(index, le->text());
    } else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

// ---------------- SchemaEditor ------------------
SchemaEditor::SchemaEditor(const QString& schemaFile, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Packet Schema Editor");
    resize(1100, 700);
    buildUi();
    connectSignals();
    loadSchemaFromFile(schemaFile);
}

void SchemaEditor::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);
    auto* vbox = new QVBoxLayout(central);

    // --- Toolbar for fast UDP send ---
    topBar = addToolBar("Packet");
    topBar->setMovable(false);
    topBar->setIconSize(QSize(16,16));

    hostEdit = new QLineEdit("127.0.0.1");
    hostEdit->setMaximumWidth(160);
    hostEdit->setPlaceholderText("Host/IP");
    portSpin = new QSpinBox; portSpin->setRange(1, 65535); portSpin->setValue(3000); portSpin->setMaximumWidth(100);
    leCheck = new QCheckBox("LE"); leCheck->setToolTip("Little Endian");
    intervalSpin = new QSpinBox; intervalSpin->setRange(10, 60*60*1000); intervalSpin->setValue(1000); intervalSpin->setSuffix(" ms"); intervalSpin->setMaximumWidth(120);
    sendBtn = new QToolButton; sendBtn->setText("Send"); sendBtn->setToolTip("Send UDP now"); sendBtn->setAutoRaise(true);
    autoBtn = new QToolButton; autoBtn->setText("Auto OFF"); autoBtn->setCheckable(true); autoBtn->setToolTip("Toggle periodic send"); autoBtn->setAutoRaise(true);

    topBar->addWidget(new QLabel("Host:")); topBar->addWidget(hostEdit);
    topBar->addSeparator();
    topBar->addWidget(new QLabel("Port:")); topBar->addWidget(portSpin);
    topBar->addSeparator();
    topBar->addWidget(new QLabel("Interval:")); topBar->addWidget(intervalSpin);
    topBar->addSeparator();
    topBar->addWidget(leCheck);
    topBar->addSeparator();
    topBar->addWidget(sendBtn);
    topBar->addWidget(autoBtn);

    auto* top = new QHBoxLayout; vbox->addLayout(top);
    packetCombo = new QComboBox; packetCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    searchEdit = new QLineEdit; searchEdit->setPlaceholderText("Search fields…");
    top->addWidget(new QLabel("Packet:")); top->addWidget(packetCombo, 2);
    top->addSpacing(12);
    top->addWidget(new QLabel("Filter:")); top->addWidget(searchEdit, 3);

    // Tree
    tree = new QTreeWidget; vbox->addWidget(tree, 1);
    tree->setColumnCount(4);
    tree->setHeaderLabels({"Name","Kind","Type/Size","Value"});
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(true);
    tree->setAlternatingRowColors(true);
    // Install delegates: lock columns 0..2, editor only on column 3
    tree->setItemDelegateForColumn(0, new NoEditDelegate(tree));
    tree->setItemDelegateForColumn(1, new NoEditDelegate(tree));
    tree->setItemDelegateForColumn(2, new NoEditDelegate(tree));
    tree->setItemDelegateForColumn(3, new ValueDelegate(tree));
    tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    // Status bar
    status = new QStatusBar; setStatusBar(status);

    // Timer for auto-send
    autoTimer = new QTimer(this); autoTimer->setTimerType(Qt::CoarseTimer);

    // Menus
    auto* fileMenu = menuBar()->addMenu("&File");
    actOpen = fileMenu->addAction("Open Schema…"); actOpen->setShortcut(QKeySequence::Open);
    actSavePacket = fileMenu->addAction("Save Edited Packet…");
    actSaveValues = fileMenu->addAction("Save Values Only…");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QWidget::close, QKeySequence::Quit);
    actLoadValues    = fileMenu->addAction("Load Values for Current…");
    actSaveAllValues = fileMenu->addAction("Save All Values…");
    actLoadAllValues = fileMenu->addAction("Load All Values…");

    auto* viewMenu = menuBar()->addMenu("&View");
    actExpandAll = viewMenu->addAction("Expand All");
    actCollapseAll = viewMenu->addAction("Collapse All");

    viewMenu->addSeparator();
    actLittleEndian = viewMenu->addAction("Little Endian");
    actLittleEndian->setCheckable(true);
    actLittleEndian->setChecked(false); // default Big Endian

    auto* actionsMenu = menuBar()->addMenu("&Actions");
    actSendUdp = actionsMenu->addAction("Send UDP…");
}

void SchemaEditor::connectSignals() {
    // Values I/O
    connect(actLoadValues, &QAction::triggered, this, [this]{
        loadValuesForKeyFromFile(packetKey(currentPacket));
    });
    connect(actSaveValues, &QAction::triggered, this, [this]{
        saveValuesForKeyToFile(packetKey(currentPacket));
    });
    connect(actLoadAllValues, &QAction::triggered, this, [this]{
        loadAllValuesFromFile();
    });
    connect(actSaveAllValues, &QAction::triggered, this, [this]{
        saveAllValuesToFile();
    });
    // Rebuild tree when the selected packet changes
    connect(packetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
        saveCurrentPacketValues();
        currentPacket = packetCombo->currentData().toJsonObject();
        rebuildTree();
    });
    // Store edits live
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column){
        if (column != 3) return; // Value column only
        if (item->text(1) != "field") return;
        const auto pathList = item->data(0, Roles::PathList).toStringList();
        if (pathList.isEmpty()) return;
        storedValues[packetKey(currentPacket)][pathList.join("/")] = item->text(3);
    });

    // Menu actions
    connect(actSendUdp, &QAction::triggered, this, &SchemaEditor::onSendUdp);
    connect(actExpandAll, &QAction::triggered, tree, &QTreeWidget::expandAll);
    connect(actCollapseAll, &QAction::triggered, tree, &QTreeWidget::collapseAll);

    // Endianness: keep menu action and toolbar checkbox in sync
    connect(actLittleEndian, &QAction::toggled, this, [this](bool on){
        if (leCheck->isChecked() != on) leCheck->setChecked(on);
        littleEndian = on;
        status->showMessage(on ? "Little endian" : "Big endian", 1500);
    });
    connect(leCheck, &QCheckBox::toggled, this, [this](bool on){
        if (actLittleEndian->isChecked() != on) actLittleEndian->setChecked(on);
        littleEndian = on;
    });

    // Toolbar buttons
    connect(sendBtn, &QToolButton::clicked, this, [this]{
        const QByteArray d = serializeCurrentPacket();
        QUdpSocket sock; sock.writeDatagram(d, QHostAddress(hostEdit->text()), quint16(portSpin->value()));
        status->showMessage(QString("UDP sent %1 bytes to %2:%3").arg(d.size()).arg(hostEdit->text()).arg(portSpin->value()), 2000);
    });
    connect(autoBtn, &QToolButton::toggled, this, [this](bool on){
        autoBtn->setText(on ? "Auto ON" : "Auto OFF");
        autoTimer->stop();
        if (on) {
            autoTimer->start(intervalSpin->value());
        }
    });
    connect(autoTimer, &QTimer::timeout, this, [this]{
        const QByteArray d = serializeCurrentPacket();
        QUdpSocket sock; sock.writeDatagram(d, QHostAddress(hostEdit->text()), quint16(portSpin->value()));
    });
}

void SchemaEditor::loadSchemaFromFile(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Load Schema", f.errorString()); return; }
    const auto text = QString::fromUtf8(f.readAll()); f.close();
    QJsonParseError err; auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, "Schema", QString("JSON parse error: %1").arg(err.errorString())); return;
    }
    schemaRoot = doc.object();
    populatePacketCombo();
}

void SchemaEditor::populatePacketCombo() {
    packetCombo->clear();
    const auto packets = schemaRoot.value("packets").toArray();
    for (const auto& p : packets) {
        const auto obj = p.toObject();
        packetCombo->addItem(obj.value("name").toString(), obj);
    }
    if (packetCombo->count() > 0) {
        currentPacket = packetCombo->currentData().toJsonObject();
        rebuildTree();
    }
}

void SchemaEditor::saveCurrentPacketValues() {
    if (tree->topLevelItemCount() == 0 || currentPacket.isEmpty()) return;
    const QString key = packetKey(currentPacket);
    QHash<QString, QString> map = storedValues.value(key);
    auto* root = tree->topLevelItem(0);
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item){
        for (int i=0;i<item->childCount();++i) {
            auto* c = item->child(i);
            const QString kind = c->text(1);
            if (kind == "struct") { walk(c); }
            else if (kind == "field") {
                const auto pathList = c->data(0, Roles::PathList).toStringList();
                if (!pathList.isEmpty()) map[pathList.join("/")] = c->text(3);
            }
        }
    };
    walk(root);
    storedValues.insert(key, map);
}

void SchemaEditor::rebuildTree() {
    tree->clear();
    if (currentPacket.isEmpty()) return;
    auto* root = new QTreeWidgetItem(tree, QStringList{currentPacket.value("name").toString(), "packet", "", ""});
    root->setData(0, Roles::NodeType, "packet");
    root->setData(0, Roles::PathList, QStringList{currentPacket.value("name").toString()});
    const auto data = currentPacket.value("data").toArray();
    for (const auto& n : data) addNodeRecursive(root, n, QStringList{currentPacket.value("name").toString()});
    tree->expandToDepth(1);
}

void SchemaEditor::addNodeRecursive(QTreeWidgetItem* parent, const QJsonValue& nodeVal, QStringList path) {
    if (!nodeVal.isObject()) return;
    const auto obj = nodeVal.toObject();
    if (obj.contains("struct")) {
        const auto structName = obj.value("struct").toString();
        auto* it = new QTreeWidgetItem(parent, QStringList{structName, "struct", "", ""});
        it->setData(0, Roles::NodeType, "struct");
        path.push_back(structName);
        it->setData(0, Roles::PathList, path);
        const auto arr = obj.value("data").toArray();
        for (const auto& child : arr) addNodeRecursive(it, child, path);
        return;
    }
    const auto valueName = obj.value("value").toString("<value>");
    QString typeOrSize, nodeType;
    if (obj.contains("type")) { typeOrSize = obj.value("type").toString(); nodeType = typeOrSize; }
    else if (obj.contains("size")) { typeOrSize = QString::number(obj.value("size").toInt()) + " bits"; nodeType = "bits"; }

    QString defaultValue = "0";
    auto* it = new QTreeWidgetItem(parent, QStringList{valueName, "field", typeOrSize, defaultValue});
    it->setFlags(it->flags() | Qt::ItemIsEditable); // allow editing at item-level
    it->setData(0, Roles::NodeType, nodeType);
    it->setData(0, Roles::TypeName, obj.value("type").toString());
    it->setData(0, Roles::SizeBits, obj.value("size").toInt());
    path.push_back(valueName);
    it->setData(0, Roles::PathList, path);


    // Restore saved value if present
    const QString pathKey = path.join("/");
    const QString key = packetKey(currentPacket);

    auto& mapRef = storedValues[key];
    if (!mapRef.contains(pathKey)) {
        mapRef.insert(pathKey, QStringLiteral("0"));
    }
    it->setText(3, mapRef.value(pathKey));
}

// ---------------- UDP -------------------------
void SchemaEditor::onSendUdp() {
    if (tree->topLevelItemCount() == 0) { QMessageBox::warning(this, "Send UDP", "No packet loaded."); return; }
    bool ok = false;
    const QString host = QInputDialog::getText(this, "Send UDP", "Destination host/IP:", QLineEdit::Normal, "127.0.0.1", &ok);
    if (!ok || host.isEmpty()) return;
    int port = QInputDialog::getInt(this, "Send UDP", "Destination port:", 3000, 1, 65535, 1, &ok);
    if (!ok) return;

    const QByteArray datagram = serializeCurrentPacket();
    QUdpSocket sock;
    const qint64 sent = sock.writeDatagram(datagram, QHostAddress(host), static_cast<quint16>(port));
    if (sent != datagram.size()) {
        QMessageBox::warning(this, "Send UDP", QString("Sent %1 of %2 bytes").arg(sent).arg(datagram.size()));
    } else {
        status->showMessage(QString("UDP sent %1 bytes to %2:%3").arg(sent).arg(host).arg(port), 3000);
    }
}

QByteArray SchemaEditor::serializeCurrentPacket() const {
    QByteArray buf;
    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian); // network order
    if (tree->topLevelItemCount() == 0) return buf;
    auto* root = tree->topLevelItem(0);
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item){
        for (int i = 0; i < item->childCount(); ++i) {
            auto* c = item->child(i);
            const QString kind = c->text(1);
            if (kind == "struct") {
                walk(c);
            } else if (kind == "field") {
                const QString typeName = c->data(0, Roles::TypeName).toString();
                const int sizeBits = c->data(0, Roles::SizeBits).toInt();
                const QString valueText = c->text(3).trimmed();
                if (!typeName.isEmpty())      writeField(ds, typeName, valueText);
                else if (sizeBits > 0)        writeBits(ds, sizeBits, valueText);
            }
        }
    };
    walk(root);
    return buf;
}

void SchemaEditor::writeField(QDataStream& ds, const QString& typeName, const QString& valueText) {
    QString val = valueText.trimmed();

    // Default empty values to 0
    if (val.isEmpty()) val = "0";

    // Treat "\\0" as a literal null byte
    if (val == "\\0") { ds.writeRawData("\0", 1); return; }

    auto parseUnsigned = [](const QString& s, qulonglong& out)->bool {
        bool ok = false;
        qulonglong v = 0;
        if (s.startsWith("0x", Qt::CaseInsensitive))
            v = s.mid(2).toULongLong(&ok, 16);
        else
            v = s.toULongLong(&ok, 10);
        if (ok) out = v;
        return ok;
    };

    auto parseSigned = [](const QString& s, qlonglong& out)->bool {
        bool ok = false;
        qlonglong v = 0;
        if (s.startsWith("0x", Qt::CaseInsensitive))
            v = s.mid(2).toLongLong(&ok, 16);
        else
            v = s.toLongLong(&ok, 10);
        if (ok) out = v;
        return ok;
    };

    // ---- Signed integer types ----
    if (typeName == "int8" || typeName == "int16" || typeName == "int32" || typeName == "int64") {
        qlonglong v = 0;
        parseSigned(val, v);
        if (typeName == "int8")   { v = std::clamp<qlonglong>(v, -(1ll<<7),  (1ll<<7)-1);  ds << qint8(v);  return; }
        if (typeName == "int16")  { v = std::clamp<qlonglong>(v, -(1ll<<15), (1ll<<15)-1); ds << qint16(v); return; }
        if (typeName == "int32")  { v = std::clamp<qlonglong>(v, INT_MIN, INT_MAX);         ds << qint32(v); return; }
        /* int64 */                 { ds << qint64(v); return; }
    }

    // ---- Unsigned integer types ----
    if (typeName == "uint8" || typeName == "uint16" || typeName == "uint32" || typeName == "uint64") {
        qulonglong u = 0;
        parseUnsigned(val, u);
        if (typeName == "uint8")  { u = std::min<qulonglong>(u, (1ull<<8)-1);   ds << quint8(u);  return; }
        if (typeName == "uint16") { u = std::min<qulonglong>(u, (1ull<<16)-1);  ds << quint16(u); return; }
        if (typeName == "uint32") { u = std::min<qulonglong>(u, 0xFFFFFFFFull); ds << quint32(u); return; }
        if (typeName == "uint64") { ds << quint64(u); return; }
    }

    // ---- Floating point ----
    if (typeName == "float") {
        bool ok=false; double d = QLocale::c().toDouble(valueText.trimmed(), &ok);
        float f = ok ? float(d) : 0.0f;

        const auto old = ds.floatingPointPrecision();
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision); // 4 bytes
        ds << f;
        ds.setFloatingPointPrecision(old);
        return;
    }

    if (typeName == "double") {
        bool ok=false; double d = QLocale::c().toDouble(valueText.trimmed(), &ok);

        const auto old = ds.floatingPointPrecision();
        ds.setFloatingPointPrecision(QDataStream::DoublePrecision); // 8 bytes
        ds << (ok ? d : 0.0);
        ds.setFloatingPointPrecision(old);
        return;
    }
}

void SchemaEditor::writeBits(QDataStream& ds, int sizeBits, const QString& valueText) {
    const int bytes = (sizeBits + 7)/8;
    QByteArray raw(bytes, '\0');
    QString s = valueText.trimmed();
    if (!s.isEmpty()) {
        if (s.startsWith("0x", Qt::CaseInsensitive)) {
            QByteArray hex = s.mid(2).toLatin1();
            QByteArray parsed = QByteArray::fromHex(hex);
            if (!parsed.isEmpty()) {
                int copy = qMin(parsed.size(), bytes);
                memcpy(raw.data() + (bytes - copy), parsed.constData() + (parsed.size() - copy), copy);
            }
        } else {
            quint64 acc = 0; for (QChar ch : s) { if (ch == '0' || ch == '1') { acc = (acc<<1) | (ch == '1'); } }
            for (int i = 0; i < bytes; ++i) { raw[bytes-1-i] = char(acc & 0xFF); acc >>= 8; }
        }
    }
    ds.writeRawData(raw.constData(), raw.size());
}

bool SchemaEditor::loadValuesForKeyFromFile(const QString& key) {
    const QString fn = QFileDialog::getOpenFileName(this, "Load Values (current packet)", {}, "JSON (*.json)");
    if (fn.isEmpty()) return false;

    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Load Values", f.errorString());
        return false;
    }
    QJsonParseError e; const auto doc = QJsonDocument::fromJson(f.readAll(), &e); f.close();
    if (e.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, "Load Values", QString("JSON parse error: %1").arg(e.errorString()));
        return false;
    }

    const auto obj = doc.object();
    QHash<QString, QString> map;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        map.insert(it.key(), it.value().toString());

    storedValues[key] = map;
    rebuildTree(); // refresh values into the Value column
    return true;
}

bool SchemaEditor::saveValuesForKeyToFile(const QString& key) const {
    const QString fn = QFileDialog::getSaveFileName(nullptr, "Save Values (current packet)", key + ".values.json", "JSON (*.json)");
    if (fn.isEmpty()) return false;

    // Values map
    const auto it = storedValues.find(key);
    const QHash<QString, QString> map = (it != storedValues.end()) ? it.value() : QHash<QString, QString>{};

    // Decide order
    QStringList order;
    const QString curKey = packetKey(currentPacket);
    if (!currentPacket.isEmpty() && key == curKey && tree && tree->topLevelItemCount() > 0) {
        // Exact visual order from the tree for the current packet
        order = currentPacketTreeOrder();
    } else {
        // Fallback: schema order if you have it; else just whatever keys exist
        if (auto pkt = findPacketByKey(key); !pkt.isEmpty()) {
            order = orderedFieldPaths(pkt);
        } else {
            order = map.keys(); // as a last resort
        }
    }

    // Build JSON in that order
    QJsonObject obj;
    for (const auto& path : order)
        obj.insert(path, map.value(path, QStringLiteral("0")));

    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(nullptr, "Save Values", f.errorString());
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool SchemaEditor::loadAllValuesFromFile() {
    const QString fn = QFileDialog::getOpenFileName(this, "Load All Values", {}, "JSON (*.json)");
    if (fn.isEmpty()) return false;

    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Load Values", f.errorString());
        return false;
    }
    QJsonParseError e; const auto doc = QJsonDocument::fromJson(f.readAll(), &e); f.close();
    if (e.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, "Load Values", QString("JSON parse error: %1").arg(e.errorString()));
        return false;
    }

    const auto root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it->isObject()) continue;
        const QString pktKey = it.key();
        const auto valObj = it->toObject();
        QHash<QString, QString> map;
        for (auto vIt = valObj.begin(); vIt != valObj.end(); ++vIt)
            map.insert(vIt.key(), vIt.value().toString());
        storedValues.insert(pktKey, map);
    }
    rebuildTree(); // refresh currently selected packet from store
    return true;
}

bool SchemaEditor::saveAllValuesToFile() const {
    const QString fn = QFileDialog::getSaveFileName(nullptr, "Save All Values", "all.values.json", "JSON (*.json)");
    if (fn.isEmpty()) return false;

    QJsonObject root;
    for (auto it = storedValues.begin(); it != storedValues.end(); ++it) {
        QJsonObject obj;
        for (auto vIt = it.value().begin(); vIt != it.value().end(); ++vIt)
            obj.insert(vIt.key(), vIt.value());
        root.insert(it.key(), obj);
    }

    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        QMessageBox::warning(nullptr, "Save All Values", f.errorString());
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

// Put near your other helpers in schema_editor.cpp
QStringList SchemaEditor::currentPacketTreeOrder() const {
    QStringList order;
    if (!tree || tree->topLevelItemCount() == 0) return order;
    auto* root = tree->topLevelItem(0);

    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* it){
        for (int i = 0; i < it->childCount(); ++i) {
            auto* c = it->child(i);
            if (c->text(1) == QLatin1String("struct")) {
                walk(c);
            } else if (c->text(1) == QLatin1String("field")) {
                const auto pathList = c->data(0, Roles::PathList).toStringList();
                if (!pathList.isEmpty()) order << pathList.join("/");
            }
        }
    };
    walk(root);
    return order;
}

QJsonObject SchemaEditor::findPacketByKey(const QString& key) const {
    // key is "name#opcode"
    const auto parts = key.split('#');
    if (parts.size() != 2) return {};
    const QString name = parts[0], opcode = parts[1];
    const auto arr = schemaRoot.value("packets").toArray();
    for (const auto& p : arr) {
        const auto obj = p.toObject();
        if (obj.value("name").toString() == name &&
            obj.value("opcode").toVariant().toString() == opcode)
            return obj;
    }
    return {};
}

static void collectPathsRec(const QJsonValue& node, QStringList& prefix, QStringList& out) {
    if (!node.isObject()) return;
    const auto o = node.toObject();
    if (o.contains("struct")) {
        prefix.push_back(o.value("struct").toString());
        const auto arr = o.value("data").toArray();
        for (const auto& ch : arr) collectPathsRec(ch, prefix, out);
        prefix.removeLast();
        return;
    }
    // leaf "field"
    const QString name = o.value("value").toString("<value>");
    prefix.push_back(name);
    out.push_back(prefix.join("/"));
    prefix.removeLast();
}

QStringList SchemaEditor::orderedFieldPaths(const QJsonObject& packet) const {
    QStringList out, prefix;
    const QString pktName = packet.value("name").toString();
    if (pktName.isEmpty()) return out;
    prefix << pktName;
    const auto arr = packet.value("data").toArray();
    for (const auto& n : arr) collectPathsRec(n, prefix, out);
    return out;
}

