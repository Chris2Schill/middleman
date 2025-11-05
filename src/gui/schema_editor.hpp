// Qt 6 C++ front end to dynamically view/edit a nested packet schema like the one
// you provided. Single-file main.cpp for simplicity + a CMakeLists.txt at the end.
//
// Features
// - Load schema JSON (File ▸ Open Schema…) or start with the built-in sample below.
// - Packet selector (by name/opcode).
// - Expandable tree for structs/fields with columns: Name, Kind, Type/Size, Value.
// - Inline editing of leaf values with validators (int ranges, float/double, raw bits).
// - Search box to filter tree items by name.
// - Save Edited Packet… : exports current packet schema including a "current" field for each
//   editable leaf (non-destructive to original keys; keeps struct/value/type/size).
// - Save Values Only… : exports compact JSON mirroring the schema but only names + entered values.
//
// Build (Qt 6):
//   mkdir build && cd build
//   cmake -S .. -B . -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x/gcc_64"  // adjust for your Qt
//   cmake --build .
//
// Run:
//   ./QtSchemaEditor

#include <QtWidgets>
#include <QtCore>
#include <QtGui>

// ---------------------------- Sample schema ----------------------------
static const char* SAMPLE_SCHEMA_JSON = R"JSON(
{
    "packets": [
        {
            "name": "entity_state",
            "opcode_field": "pdu_header.pdu_type",
            "opcode": 1,
            "data": [ 
                {
                    "struct": "pdu_header",
                    "data": [
                        { "value": "protocol_version", "type": "uint8" },
                        { "value": "exercise_id",      "type": "uint8" },
                        { "value": "pdu_type",         "type": "uint8" },
                        { "value": "protocol_family",  "type": "uint8" },
                        { "value": "timestamp",        "type": "int32" },
                        { "value": "pdu_length",       "type": "uint16" },
                        { "value": "padding",          "type": "uint16" }
                    ]
                },
                {
                    "struct": "entity_id",
                    "data": [
                        { "value": "site",          "type": "uint16" },
                        { "value": "app",           "type": "uint16" },
                        { "value": "entity",        "type": "uint16" }
                    ]
                },
                { "value": "force",            "type": "uint8" },
                { "value": "num_articulated_parts", "type": "uint8" },
                {
                    "struct": "entity_type",
                    "data": [
                        { "value": "kind",        "type": "uint8" },
                        { "value": "domain",      "type": "uint8" },
                        { "value": "country",     "type": "uint16" },
                        { "value": "category",    "type": "uint8" },
                        { "value": "subcategory", "type": "uint8" },
                        { "value": "specific",    "type": "uint8" },
                        { "value": "extra",       "type": "uint8" }
                    ]
                },
                {
                    "struct": "alternative_entity_type",
                    "data": [
                        { "value": "kind",        "type": "uint8" },
                        { "value": "domain",      "type": "uint8" },
                        { "value": "country",     "type": "uint16" },
                        { "value": "category",    "type": "uint8" },
                        { "value": "subcategory", "type": "uint8" },
                        { "value": "specific",    "type": "uint8" },
                        { "value": "extra",       "type": "uint8" }
                    ]
                },
                {
                    "struct": "entity_linear_velocity",
                    "data": [
                        { "value": "x",       "type": "float" },
                        { "value": "y",       "type": "float" },
                        { "value": "z",       "type": "float" }
                    ]
                },
                {
                    "struct": "entity_location",
                    "data": [
                        { "value": "x",       "type": "double" },
                        { "value": "y",       "type": "double" },
                        { "value": "z",       "type": "double" }
                    ]
                },
                {
                    "struct": "entity_orientation",
                    "data": [
                        { "value": "psi",       "type": "float" },
                        { "value": "theta",       "type": "float" },
                        { "value": "phi",       "type": "float" }
                    ]
                },
                {
                    "struct": "entity_appearance",
                    "data": [
                        { "value": "general",       "type": "float" },
                        {
                            "struct": "specific",
                            "data": [
                                { "value": "land_platforms",       "type": "uint16" },
                                { "value": "air_platforms",        "type": "uint16" },
                                { "value": "surface_platforms",    "type": "uint16" },
                                { "value": "subsurface_platforms", "type": "uint16" },
                                { "value": "space_platforms",      "type": "uint16" },
                                { "value": "guided_munitions",     "type": "uint16" },
                                { "value": "life_forms",           "type": "uint16" },
                                { "value": "environmentals",       "type": "uint16" }
                            ]
                        }
                    ]
                },
                {
                    "struct": "dead_reckoning",
                    "data": [
                        { "value": "algorithm",       "type": "uint8" },
                        {
                            "struct": "other",
                            "data": [
                                { "value": "padding", "size": 120 }
                            ]
                        },
                        {
                            "struct": "entity_linear_acceleration",
                            "data": [
                                { "value": "x",       "type": "float" },
                                { "value": "y",       "type": "float" },
                                { "value": "z",       "type": "float" }
                            ]
                        },
                        {
                            "struct": "entity_angular_velocity",
                            "data": [
                                { "value": "x",       "type": "float" },
                                { "value": "y",       "type": "float" },
                                { "value": "z",       "type": "float" }
                            ]
                        }
                    ]
                },
                {
                    "struct": "entity_marking",
                    "data": [
                        { "value": "char_set",  "type": "int8" },
                        { "value": "char_set",  "size": 88 }
                    ]
                },
                { "value": "entity_capabilities",  "type": "uint32" },
                {
                    "struct": "articulation_parameter",
                    "data": [
                        { "value": "parameter_type_designator",  "type": "uint8" },
                        { "value": "parameter_change_indicator",  "type": "uint8" },
                        { "value": "articulation_attachment_id",  "type": "uint16" },
                        {
                            "struct": "paramter_type_varient",
                            "data": [
                                { "value": "attached_parts",  "type": "uint32" },
                                { "value": "articulated_parts",  "type": "uint32" }
                            ]
                        }
                    ]
                },
                { "value": "articulation_parameter_value",  "type": "uint64" }
            ]
        }
    ]
}
)JSON";

// ----------------------------- Roles/consts ----------------------------
namespace Roles {
    enum : int {
        NodeType = Qt::UserRole + 1,   // "struct" | int type | float type | "bits"
        TypeName,                      // e.g. "uint16", "float"
        SizeBits,                      // for raw size-only fields
        PathList                       // QStringList human path
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

// ----------------------------- Delegate --------------------------------
class ValueDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        Q_UNUSED(option);
        if (index.column() != 3) return nullptr; // Value column only
        // IMPORTANT: roles are stored on column 0; read them from the sibling
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
        return nullptr; // structs are not directly editable
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        // Ensure we commit text into the Value column
        if (auto* le = qobject_cast<QLineEdit*>(editor)) {
            model->setData(index, le->text());
        } else {
            QStyledItemDelegate::setModelData(editor, model, index);
        }
    }
};

// ------------------------------ Main UI --------------------------------
class SchemaEditor : public QMainWindow {
public:
    SchemaEditor(QWidget* parent=nullptr) : QMainWindow(parent) {
        setWindowTitle("Packet Schema Editor");
        resize(1100, 700);
        buildUi();
        connectSignals();
        loadSchemaFromText(SAMPLE_SCHEMA_JSON);
    }

private:
    QJsonObject schemaRoot;                 // whole schema { packets: [...] }
    QJsonObject currentPacket;              // selected packet object

    QComboBox* packetCombo{};
    QLineEdit* searchEdit{};
    QTreeWidget* tree{};
    QStatusBar* status{};

    QAction *actOpen{}, *actSavePacket{}, *actSaveValues{}, *actExpandAll{}, *actCollapseAll{};

private slots:
    void onPacketChanged(int idx) {
        Q_UNUSED(idx);
        currentPacket = packetCombo->currentData().toJsonObject();
        rebuildTree();
    }

    void onSearchChanged(const QString& s) { applyFilter(s); }

    void onOpenSchema() {
        const QString fn = QFileDialog::getOpenFileName(this, "Open Schema JSON", {}, "JSON (*.json)");
        if (fn.isEmpty()) return;
        QFile f(fn);
        if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Open", f.errorString()); return; }
        const auto data = f.readAll();
        loadSchemaFromText(QString::fromUtf8(data));
    }

    void onSavePacket() { // full schema of current packet + current values
        if (currentPacket.isEmpty()) return;
        QJsonObject pkt = currentPacket; // copy
        // add current values into leaves under key "current"
        injectCurrentValuesIntoSchema(pkt, tree->invisibleRootItem());
        const QString fn = QFileDialog::getSaveFileName(this, "Save Edited Packet", {}, "JSON (*.json)");
        if (fn.isEmpty()) return;
        saveJsonToFile(pkt, fn);
    }

    void onSaveValuesOnly() {
        if (currentPacket.isEmpty()) return;
        QJsonObject compact;
        compact.insert("name", currentPacket.value("name"));
        compact.insert("opcode", currentPacket.value("opcode"));
        compact.insert("values", valuesOnly(tree->invisibleRootItem()));
        const QString fn = QFileDialog::getSaveFileName(this, "Save Values Only", {}, "JSON (*.json)");
        if (fn.isEmpty()) return;
        saveJsonToFile(compact, fn);
    }

    void expandAll() { tree->expandAll(); }
    void collapseAll() { tree->collapseAll(); }

private:
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

        auto* fileMenu = menuBar()->addMenu("&File");
        actOpen = fileMenu->addAction("Open Schema…"); actOpen->setShortcut(QKeySequence::Open);
        actSavePacket = fileMenu->addAction("Save Edited Packet…");
        actSaveValues = fileMenu->addAction("Save Values Only…");
        fileMenu->addSeparator();
        fileMenu->addAction("Quit", this, &QWidget::close, QKeySequence::Quit);

        auto* viewMenu = menuBar()->addMenu("&View");
        actExpandAll = viewMenu->addAction("Expand All");
        actCollapseAll = viewMenu->addAction("Collapse All");
    }

    void connectSignals() {
        connect(packetCombo, &QComboBox::currentIndexChanged, this, &SchemaEditor::onPacketChanged);
        connect(searchEdit, &QLineEdit::textChanged, this, &SchemaEditor::onSearchChanged);
        connect(actOpen, &QAction::triggered, this, &SchemaEditor::onOpenSchema);
        connect(actSavePacket, &QAction::triggered, this, &SchemaEditor::onSavePacket);
        connect(actSaveValues, &QAction::triggered, this, &SchemaEditor::onSaveValuesOnly);
        connect(actExpandAll, &QAction::triggered, this, &SchemaEditor::expandAll);
        connect(actCollapseAll, &QAction::triggered, this, &SchemaEditor::collapseAll);
        connect(tree, &QTreeWidget::itemSelectionChanged, this, [this]{
            auto* it = tree->currentItem();
            if (!it) { status->clearMessage(); return; }
            const auto path = it->data(0, Roles::PathList).toStringList().join(" → ");
            status->showMessage(path);
        });
    }

    void loadSchemaFromText(const QString& text) {
        QJsonParseError err; auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            QMessageBox::critical(this, "Schema", QString("JSON parse error: %1").arg(err.errorString()));
            return;
        }
        schemaRoot = doc.object();
        populatePacketCombo();
    }

    void populatePacketCombo() {
        packetCombo->blockSignals(true);
        packetCombo->clear();
        const auto packets = schemaRoot.value("packets").toArray();
        for (const auto& p : packets) {
            const auto obj = p.toObject();
            const auto name = obj.value("name").toString("<unnamed>");
            const auto opcode = obj.value("opcode");
            packetCombo->addItem(QString("%1 (opcode=%2)").arg(name, opcode.toVariant().toString()), obj);
        }
        packetCombo->blockSignals(false);
        if (packetCombo->count() > 0) {
            packetCombo->setCurrentIndex(0);
            currentPacket = packetCombo->currentData().toJsonObject();
            rebuildTree();
        }
    }

    // -------------------------- Tree building --------------------------
    void rebuildTree() {
        tree->clear();
        if (currentPacket.isEmpty()) return;
        auto* root = new QTreeWidgetItem(tree, QStringList{currentPacket.value("name").toString(), "packet", "", ""});
        root->setData(0, Roles::NodeType, "packet");
        root->setData(0, Roles::PathList, QStringList{currentPacket.value("name").toString()});
        const auto data = currentPacket.value("data").toArray();
        for (const auto& n : data) {
            addNodeRecursive(root, n, QStringList{currentPacket.value("name").toString()});
        }
        tree->expandToDepth(1);
    }

    void addNodeRecursive(QTreeWidgetItem* parent, const QJsonValue& nodeVal, QStringList path) {
        if (nodeVal.isObject()) {
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
            // Leaf with type/size
            const auto valueName = obj.value("value").toString("<value>");
            QString typeOrSize;
            QString nodeType;
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
            return;
        }
        // (Arrays/other forms could be added here later.)
    }

    // -------------------------- Filtering ------------------------------
    void applyFilter(const QString& text) {
        const auto pattern = text.trimmed();
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            filterItem(tree->topLevelItem(i), pattern);
    }

    bool filterItem(QTreeWidgetItem* item, const QString& pattern) {
        bool match = pattern.isEmpty() || item->text(0).contains(pattern, Qt::CaseInsensitive);
        bool anyChild = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyChild |= filterItem(item->child(i), pattern);
        const bool visible = match || anyChild;
        item->setHidden(!visible);
        return visible;
    }

    // -------------------------- Export helpers -------------------------
    void injectCurrentValuesIntoSchema(QJsonObject& packetObj, QTreeWidgetItem* treeRoot) {
        auto arr = packetObj.value("data").toArray();
        int ti = 0; // tree child index
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject obj = arr[i].toObject();
            mirrorInto(obj, treeRoot->child(ti++));
            arr[i] = obj;
        }
        packetObj["data"] = arr;
    }

    void mirrorInto(QJsonObject& schemaNode, QTreeWidgetItem* item) {
        if (!item) return;
        if (schemaNode.contains("struct")) {
            auto arr = schemaNode.value("data").toArray();
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject child = arr[i].toObject();
                mirrorInto(child, item->child(i));
                arr[i] = child;
            }
            schemaNode["data"] = arr;
            return;
        }
        if (schemaNode.contains("value")) {
            const QString cur = item->text(3);
            if (!cur.isEmpty()) schemaNode["current"] = cur;
        }
    }

    QJsonValue valuesOnly(QTreeWidgetItem* root) {
        QJsonObject obj;
        for (int i = 0; i < root->childCount(); ++i) {
            auto* it = root->child(i);
            const QString kind = it->data(0, Roles::NodeType).toString();
            if (kind == "struct") {
                obj.insert(it->text(0), valuesOnly(it));
            } else {
                const QString name = it->text(0);
                if (name.isEmpty()) continue;
                obj.insert(name, it->text(3));
            }
        }
        return obj;
    }

    static void saveJsonToFile(const QJsonObject& obj, const QString& fn) {
        QFile f(fn);
        if (!f.open(QIODevice::WriteOnly)) { QMessageBox::warning(nullptr, "Save", f.errorString()); return; }
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
};

// // ------------------------------ main() ---------------------------------
// int main(int argc, char** argv) {
//     QApplication app(argc, argv);
//     SchemaEditor w; w.show();
//     return app.exec();
// }
//
// --------------------------- CMakeLists.txt ----------------------------
// Save the following as CMakeLists.txt next to main.cpp
/*
cmake_minimum_required(VERSION 3.21)
project(QtSchemaEditor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(QtSchemaEditor main.cpp)

target_link_libraries(QtSchemaEditor PRIVATE Qt6::Widgets)
*/

