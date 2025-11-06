#include <QtWidgets>
#include <QtCore>
#include <QtGui>
#include <QtNetwork/QUdpSocket>

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

        QAction *actOpen{};
        QAction *actSavePacket{};
        QAction *actSaveValues{};
        QAction *actExpandAll{};
        QAction *actCollapseAll{};
        QAction *actSendUdp{};

        void buildUi();
        void connectSignals();

        // Value persistence helpers
        QHash<QString, QHash<QString, QString>> storedValues;
        QString packetKey(const QJsonObject& obj) const {
            const QString name = obj.value("name").toString();
            const QString opcode = obj.value("opcode").toVariant().toString();
            return name + "#" + opcode; // stable key per packet type
        }

        void loadSchemaFromFile(const QString& filePath);
        void populatePacketCombo();
        void saveCurrentPacketValues();
        void rebuildTree();
        void addNodeRecursive(QTreeWidgetItem* parent, const QJsonValue& nodeVal, QStringList path);

        // UDP serialization helpers
        QByteArray serializeCurrentPacket() const;
        static void writeField(QDataStream& ds, const QString& typeName, const QString& valueText);
        static void writeBits(QDataStream& ds, int sizeBits, const QString& valueText);

        private slots:
            void onSendUdp();

    private:
};
