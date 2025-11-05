#pragma once

#include <QtWidgets>
#include <QtNetwork>

struct UdpPacketRow {
    QDateTime ts;
    QHostAddress src;
    quint16 srcPort = 0;
    QHostAddress dst;
    quint16 dstPort = 0;
    QByteArray payload;
};

static inline QString udpShortPreview(const QByteArray& ba, int max = 32) {
    QString text = QString::fromLatin1(ba.constData(), qMin(ba.size(), max));
    text.replace('\n', ' ').replace('\r', ' ');
    if (ba.size() > max) text += "…";
    return text;
}

static inline QString udpHexDump(const QByteArray& data) {
    QString out; out.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); i += 16) {
        out += QString("%1  ").arg(i, 6, 16, QLatin1Char('0'));
        QString ascii;
        for (int j = 0; j < 16; ++j) {
            if (i + j < data.size()) {
                const unsigned char c = static_cast<unsigned char>(data[i + j]);
                out += QString("%1 ").arg(c, 2, 16, QLatin1Char('0'));
                ascii += (c >= 32 && c < 127) ? QChar(c) : QChar('.');
            } else {
                out += "   ";
                ascii += ' ';
            }
        }
        out += " |" + ascii + "|\n";
    }
    return out;
}

class UdpTableModel : public QAbstractTableModel {
public:
    enum Col { COL_TIME, COL_SRC, COL_DST, COL_LEN, COL_PREVIEW, COL__COUNT };
    explicit UdpTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : rows_.size();
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : COL__COUNT;
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() < 0 || idx.row() >= rows_.size()) return {};
        const auto& r = rows_[idx.row()];
        if (role == Qt::DisplayRole) {
            switch (idx.column()) {
                case COL_TIME:    return r.ts.toString("HH:mm:ss.zzz");
                case COL_SRC:     return QString("%1:%2").arg(r.src.toString()).arg(r.srcPort);
                case COL_DST:     return QString("%1:%2").arg(r.dst.toString()).arg(r.dstPort);
                case COL_LEN:     return r.payload.size();
                case COL_PREVIEW: return udpShortPreview(r.payload);
            }
        } else if (role == Qt::UserRole) {
            return r.payload; // raw payload
        }
        return {};
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (o == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
                case COL_TIME:    return "Time";
                case COL_SRC:     return "Source";
                case COL_DST:     return "Destination";
                case COL_LEN:     return "Length";
                case COL_PREVIEW: return "Preview";
            }
        }
        return QAbstractTableModel::headerData(section, o, role);
    }

    void add(UdpPacketRow row) {
        beginInsertRows({}, rows_.size(), rows_.size());
        rows_.push_back(std::move(row));
        endInsertRows();
    }

    const UdpPacketRow& at(int r) const { return rows_[r]; }
    void clear() {
        beginResetModel();
        rows_.clear();
        endResetModel();
    }

private:
    QVector<UdpPacketRow> rows_;
};

class UdpHexDialog : public QDialog {
public:
    explicit UdpHexDialog(const QByteArray& payload, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("UDP Payload (hex)");
        resize(700, 500);
        auto* lay = new QVBoxLayout(this);
        auto* text = new QTextEdit(this);
        text->setReadOnly(true);
        text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        text->setPlainText(udpHexDump(payload));
        lay->addWidget(text);
        auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        lay->addWidget(btns);
    }
};

class UdpViewerWidget : public QWidget {
    // No Q_OBJECT: only lambda connects; public API to push packets in.
public:
    explicit UdpViewerWidget(QWidget* parent = nullptr)
        : QWidget(parent),
          model_(new UdpTableModel(this)),
          proxy_(new QSortFilterProxyModel(this)) {

        // Top bar (Filter + Clear + Count)
        auto* top = new QHBoxLayout;
        auto* filterLbl = new QLabel("Filter:", this);
        filterEdit_ = new QLineEdit(this);
        filterEdit_->setPlaceholderText("Type to filter any column…");
        clearBtn_ = new QToolButton(this);
        clearBtn_->setText("Clear");
        countLbl_ = new QLabel("0 packets", this);

        top->addWidget(filterLbl);
        top->addWidget(filterEdit_, 1);
        top->addSpacing(8);
        top->addWidget(clearBtn_);
        top->addSpacing(12);
        top->addWidget(countLbl_);

        // Table
        proxy_->setSourceModel(model_);
        proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxy_->setFilterKeyColumn(-1); // all columns

        view_ = new QTableView(this);
        view_->setModel(proxy_);
        view_->setSelectionBehavior(QAbstractItemView::SelectRows);
        view_->setSelectionMode(QAbstractItemView::SingleSelection);
        view_->setAlternatingRowColors(true);
        view_->horizontalHeader()->setStretchLastSection(true);
        view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        view_->setEditTriggers(QAbstractItemView::NoEditTriggers);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(6,6,6,6);
        root->addLayout(top);
        root->addWidget(view_);

        // Signals
        connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& s){
            proxy_->setFilterFixedString(s);
        });
        connect(clearBtn_, &QToolButton::clicked, model_, &UdpTableModel::clear);
        connect(model_, &QAbstractItemModel::modelReset, this, [this]{ updateCount(); });
        connect(model_, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int, int){ updateCount(); });

        connect(view_, &QTableView::doubleClicked, this, [this](const QModelIndex& ix){
            const auto src = proxy_->mapToSource(ix);
            const auto& row = model_->at(src.row());
            UdpHexDialog(row.payload, this).exec();
        });

        updateCount();
    }

    // ---- Public API: feed packets you already captured ----

    // 1) Convenient entry point when you have raw buffer + sizes/addresses
    void addPacket(const void* data, int length,
                   const QHostAddress& srcIp, quint16 srcPort,
                   const QHostAddress& dstIp, quint16 dstPort,
                   const QDateTime& ts = QDateTime::currentDateTime())
    {
        UdpPacketRow r;
        r.ts = ts;
        r.src = srcIp; r.srcPort = srcPort;
        r.dst = dstIp; r.dstPort = dstPort;
        r.payload = QByteArray(static_cast<const char*>(data), length);
        model_->add(std::move(r));
    }

    // 2) Overload if you already store the payload as QByteArray
    void addPacket(const QByteArray& payload,
                   const QHostAddress& srcIp, quint16 srcPort,
                   const QHostAddress& dstIp, quint16 dstPort,
                   const QDateTime& ts = QDateTime::currentDateTime())
    {
        UdpPacketRow r{ts, srcIp, srcPort, dstIp, dstPort, payload};
        model_->add(std::move(r));
    }

    void clear() { model_->clear(); }

    // Optional helper to fetch currently selected packet payload
    QByteArray selectedPayload() const {
        const auto ix = view_->currentIndex();
        if (!ix.isValid()) return {};
        const auto src = proxy_->mapToSource(ix);
        return model_->data(model_->index(src.row(), 0), Qt::UserRole).toByteArray();
    }

private:
    void updateCount() {
        countLbl_->setText(QString::number(model_->rowCount()) + " packets");
    }

private:
    UdpTableModel* model_;
    QSortFilterProxyModel* proxy_;
    QTableView* view_;
    QLineEdit* filterEdit_;
    QToolButton* clearBtn_;
    QLabel* countLbl_;
};

