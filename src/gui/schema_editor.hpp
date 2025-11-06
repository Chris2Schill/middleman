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

// Numeric categories
static const QSet<QString> SIGNED_INT_TYPES = {"int8","int16","int32","int64"};
static const QSet<QString> UNSIGNED_INT_TYPES = {"uint8","uint16","uint32","uint64"};
static const QSet<QString> FLOAT_TYPES = {"float", "double"};


class ValueDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                const QModelIndex& index) const override;
        void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};

class SchemaEditor : public QMainWindow {
    public:
        SchemaEditor(const QString& schemaFile, QWidget* parent=nullptr);

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
        QAction *actLittleEndian{};
        QAction *actLoadValues{};
        QAction *actLoadAllValues{};
        QAction *actSaveAllValues{};

        // Toolbar widgets
        QToolBar* topBar{};
        QLineEdit* hostEdit{};
        QSpinBox* portSpin{};
        QToolButton* sendBtn{};
        QToolButton* autoBtn{}; // start/stop auto-send
        QSpinBox* intervalSpin{}; // ms interval
        QCheckBox* leCheck{}; // little-endian quick toggle

        // Auto-send timer
        QTimer* autoTimer{};

        bool littleEndian = false;

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
        bool loadValuesForKeyFromFile(const QString& key);
        bool saveValuesForKeyToFile(const QString& key) const;
        bool loadAllValuesFromFile();
        bool saveAllValuesToFile() const;

        // UDP serialization helpers
        QByteArray serializeCurrentPacket() const;
        static void writeField(QDataStream& ds, const QString& typeName, const QString& valueText);
        static void writeBits(QDataStream& ds, int sizeBits, const QString& valueText);

        private slots:
            void onSendUdp();

    private:
};
