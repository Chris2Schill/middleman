#include <QtWidgets>
#include <QtCore>
#include <QtGui>

namespace Roles {
    enum : int {
        NodeType = Qt::UserRole + 1,
        TypeName,
        SizeBits,
        PathList
    };
}

static const QHash<QString, QPair<long long,long long>> INT_LIMITS = {
    {"int8",  {-(1LL<<7),  (1LL<<7)-1}},
    {"uint8", {0,          (1LL<<8)-1}},
    {"int16", {-(1LL<<15), (1LL<<15)-1}},
    {"uint16",{0,          (1LL<<16)-1}},
    {"int32", {-(1LL<<31), (1LL<<31)-1}},
    {"uint32",{0,          (1LL<<32)-1}},
    {"int64", {LLONG_MIN,  LLONG_MAX}},
    {"uint64",{0,          (long long)ULLONG_MAX}}
};

static const QSet<QString> FLOAT_TYPES = {"float", "double"};

class ValueDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        Q_UNUSED(option);
        if (index.column() != 3) return nullptr;
        const QModelIndex metaIdx = index.sibling(index.row(), 0);
        const auto nodeType = metaIdx.data(Roles::NodeType).toString();
        const auto typeName = metaIdx.data(Roles::TypeName).toString();
        if (INT_LIMITS.contains(typeName)) {
            auto* le = new QLineEdit(parent);
            const auto lim = INT_LIMITS.value(typeName);
            auto* v = new QIntValidator((int)lim.first, (int)lim.second, le);
            le->setValidator(v);
            le->setPlaceholderText(QString::number(lim.first)+".."+QString::number(lim.second));
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

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        if (auto* le = qobject_cast<QLineEdit*>(editor)) {
            model->setData(index, le->text());
        } else {
            QStyledItemDelegate::setModelData(editor, model, index);
        }
    }
};

class SchemaEditor : public QMainWindow {
public:
    SchemaEditor(const QString& schemaFile, QWidget* parent=nullptr) : QMainWindow(parent) {
        setWindowTitle("Packet Schema Editor");
        resize(1100, 700);
        buildUi();
        connectSignals();
        loadSchemaFromFile(schemaFile);
    }

private:
    QJsonObject schemaRoot;
    QJsonObject currentPacket;
    QComboBox* packetCombo{};
    QLineEdit* searchEdit{};
    QTreeWidget* tree{};
    QStatusBar* status{};

    QAction *actOpen{}, *actSavePacket{}, *actSaveValues{}, *actExpandAll{}, *actCollapseAll{};

    void buildUi() {
        auto* central = new QWidget; setCentralWidget(central);
        auto* vbox = new QVBoxLayout(central);

        auto* top = new QHBoxLayout; vbox->addLayout(top);
        packetCombo = new QComboBox; packetCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        searchEdit = new QLineEdit; searchEdit->setPlaceholderText("Search fields…");
        top->addWidget(new QLabel("Packet:")); top->addWidget(packetCombo, 2);
        top->addSpacing(12);
        top->addWidget(new QLabel("Filter:")); top->addWidget(searchEdit, 3);

        tree = new QTreeWidget; vbox->addWidget(tree, 1);
        tree->setColumnCount(4);
        tree->setHeaderLabels({"Name","Kind","Type/Size","Value"});
        tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        tree->header()->setStretchLastSection(true);
        tree->setAlternatingRowColors(true);
        tree->setItemDelegateForColumn(3, new ValueDelegate(tree));
        tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

        status = new QStatusBar; setStatusBar(status);
    }

    void connectSignals() {
        // Rebuild tree when the selected packet changes
        connect(packetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){
            currentPacket = packetCombo->currentData().toJsonObject();
            rebuildTree();
        });
        // Placeholder for future change tracking on values
        connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem*, int){ /* no-op */ });
    }

    void loadSchemaFromFile(const QString& filePath) {
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

    void populatePacketCombo() {
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

    void rebuildTree() {
        tree->clear();
        if (currentPacket.isEmpty()) return;
        auto* root = new QTreeWidgetItem(tree, QStringList{currentPacket.value("name").toString(), "packet", "", ""});
        root->setData(0, Roles::NodeType, "packet");
        root->setData(0, Roles::PathList, QStringList{currentPacket.value("name").toString()});
        const auto data = currentPacket.value("data").toArray();
        for (const auto& n : data) addNodeRecursive(root, n, QStringList{currentPacket.value("name").toString()});
        tree->expandToDepth(1);
    }

    void addNodeRecursive(QTreeWidgetItem* parent, const QJsonValue& nodeVal, QStringList path) {
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
    }
};

