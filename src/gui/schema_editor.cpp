#include "schema_editor.hpp"

void SchemaEditor::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);
    auto* vbox = new QVBoxLayout(central);


    // Top controls
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
    tree->setItemDelegateForColumn(3, new ValueDelegate(tree));
    tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);


    // Status bar
    status = new QStatusBar; setStatusBar(status);


    // Menus
    auto* fileMenu = menuBar()->addMenu("&File");
    actOpen = fileMenu->addAction("Open Schema…"); actOpen->setShortcut(QKeySequence::Open);
    actSavePacket = fileMenu->addAction("Save Edited Packet…");
    actSaveValues = fileMenu->addAction("Save Values Only…");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, &QWidget::close, QKeySequence::Quit);


    auto* viewMenu = menuBar()->addMenu("&View");
    actExpandAll = viewMenu->addAction("Expand All");
    actCollapseAll = viewMenu->addAction("Collapse All");


    auto* actionsMenu = menuBar()->addMenu("&Actions");
    actSendUdp = actionsMenu->addAction("Send UDP…");
}

void SchemaEditor::connectSignals() {
    // Rebuild tree when the selected packet changes
    connect(packetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
            saveCurrentPacketValues();
            currentPacket = packetCombo->currentData().toJsonObject();
            rebuildTree();
        });
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column){
            if (column != 3) return; // Value column only
            const QString kind = item->text(1);
            if (kind != "field") return;
            const auto pathList = item->data(0, Roles::PathList).toStringList();
            if (pathList.isEmpty()) return;
            const QString pathKey = pathList.join("/");
            storedValues[packetKey(currentPacket)][pathKey] = item->text(3);
            });

    // Placeholder for future change tracking on values
    connect(actSendUdp, &QAction::triggered, this, &SchemaEditor::onSendUdp);
}

// -------------------------- UDP send -------------------------------
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
    ds.setByteOrder(QDataStream::BigEndian);
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
                if (!typeName.isEmpty()) {
                    writeField(ds, typeName, valueText);
                } else if (sizeBits > 0) {
                    writeBits(ds, sizeBits, valueText);
                }
            }
        }
    };
    walk(root);
    return buf;
}

void SchemaEditor::writeField(QDataStream& ds, const QString& typeName, const QString& valueText) {
    auto toLL = [&](const QString& s)->long long{
        bool ok=false; long long v=0; if (s.startsWith("0x", Qt::CaseInsensitive)) v = s.toLongLong(&ok, 16); else v = s.toLongLong(&ok, 10); return ok ? v : 0; };
    auto toD = [&](const QString& s)->double{ bool ok=false; double v=s.toDouble(&ok); return ok? v:0.0; };
    if (typeName == "int8")   { ds << qint8(toLL(valueText)); return; }
    if (typeName == "uint8")  { ds << quint8(toLL(valueText)); return; }
    if (typeName == "int16")  { ds << qint16(toLL(valueText)); return; }
    if (typeName == "uint16") { ds << quint16(toLL(valueText)); return; }
    if (typeName == "int32")  { ds << qint32(toLL(valueText)); return; }
    if (typeName == "uint32") { ds << quint32(toLL(valueText)); return; }
    if (typeName == "int64")  { ds << qint64(toLL(valueText)); return; }
    if (typeName == "uint64") { ds << quint64(toLL(valueText)); return; }
    if (typeName == "float")  { ds << float(toD(valueText));  return; }
    if (typeName == "double") { ds << double(toD(valueText)); return; }
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
            quint64 acc = 0; for (QChar ch : s) { acc = (acc<<1) | (ch == '1'); }
            for (int i = 0; i < bytes; ++i) { raw[bytes-1-i] = char(acc & 0xFF); acc >>= 8; }
        }
    }
    ds.writeRawData(raw.constData(), raw.size());
}


// -------------------------- existing methods -----------------------
void SchemaEditor::loadSchemaFromFile(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Load Schema", f.errorString());
        return;
    }
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    QJsonParseError err; auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, "Schema", QString("JSON parse error: %1").arg(err.errorString()));
        return;
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
        for (int i = 0; i < item->childCount(); ++i) {
            auto* c = item->child(i);
            const QString kind = c->text(1);
            if (kind == "struct") { walk(c); }
            else if (kind == "field") {
                const auto pathList = c->data(0, Roles::PathList).toStringList();
                if (!pathList.isEmpty()) {
                    map[pathList.join("/")] = c->text(3);
                }
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
    if (obj.contains("type")) {
        typeOrSize = obj.value("type").toString();
        nodeType = typeOrSize;
    } else if (obj.contains("size")) {
        typeOrSize = QString::number(obj.value("size").toInt()) + " bits";
        nodeType = "bits";
    }
    auto* it = new QTreeWidgetItem(parent, QStringList{valueName, "field", typeOrSize, ""});
    it->setFlags(it->flags() | Qt::ItemIsEditable);
    it->setData(0, Roles::NodeType, nodeType);
    it->setData(0, Roles::TypeName, obj.value("type").toString());
    it->setData(0, Roles::SizeBits, obj.value("size").toInt());
    path.push_back(valueName);
    it->setData(0, Roles::PathList, path);
    // Restore saved value if present
    const QString pathKey = path.join("/");
    const QString key = packetKey(currentPacket);
    if (storedValues.contains(key)) {
        const auto& map = storedValues[key];
        auto itVal = map.find(pathKey);
        if (itVal != map.end()) it->setText(3, itVal.value());
    }
}
