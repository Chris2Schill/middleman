#include "connection_widget.hpp"

#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QPainter>
#include <QPixmap>

// Helper: tint a standard icon with a given color
static QIcon tintedIcon(const QIcon& baseIcon, const QColor& color, const QSize& size = QSize(24, 24)) {
    QPixmap pix = baseIcon.pixmap(size);
    QPixmap tinted(pix.size());
    tinted.fill(Qt::transparent);

    QPainter p(&tinted);
    p.drawPixmap(0, 0, pix);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(tinted.rect(), color);
    p.end();

    return QIcon(tinted);
}

ConnectionWidget::ConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    // Base fields
    localHostEdit_  = new QLineEdit(this);
    localPortSpin_  = new QSpinBox(this);
    remoteHostEdit_ = new QLineEdit(this);
    remotePortSpin_ = new QSpinBox(this);
    logStdoutCheck_ = new QCheckBox("Log to stdout", this);
    startBtn_       = new QPushButton(this);
    stopBtn_        = new QPushButton(this);

    // Defaults
    localHostEdit_->setPlaceholderText("127.0.0.1");
    remoteHostEdit_->setPlaceholderText("example.com");
    localPortSpin_->setRange(0, 65535);
    remotePortSpin_->setRange(0, 65535);
    localPortSpin_->setValue(9000);
    remotePortSpin_->setValue(9001);

    // Icons (green play, red stop)
    auto playIcon = style()->standardIcon(QStyle::SP_MediaPlay);
    auto stopIcon = style()->standardIcon(QStyle::SP_MediaStop);
    startBtn_->setIcon(tintedIcon(playIcon, QColor(0, 180, 0)));
    stopBtn_->setIcon(tintedIcon(stopIcon, QColor(200, 0, 0)));
    startBtn_->setToolTip("Start");
    stopBtn_->setToolTip("Stop");
    stopBtn_->setEnabled(false);

    // --- Multicast controls ---
    multicastCheck_      = new QCheckBox("Multicast", this);
    multicastLabel_      = new QLabel("Group:", this);
    multicastGroupEdit_  = new QLineEdit(this);
    multicastTTLLabel_   = new QLabel("TTL:", this);
    multicastTTLSpin_    = new QSpinBox(this);

    multicastGroupEdit_->setPlaceholderText("239.0.0.1");
    multicastTTLSpin_->setRange(0, 255);
    multicastTTLSpin_->setValue(1);

    // --- Single-line horizontal layout with labels ---
    auto* row = new QHBoxLayout;

    // Local
    row->addWidget(new QLabel("Local:", this));
    row->addWidget(localHostEdit_);
    row->addWidget(localPortSpin_);
    row->addSpacing(12);

    // Remote
    row->addWidget(new QLabel("Remote:", this));
    row->addWidget(remoteHostEdit_);
    row->addWidget(remotePortSpin_);
    row->addSpacing(12);

    // Log
    row->addWidget(logStdoutCheck_);
    row->addSpacing(12);

    // Multicast
    row->addWidget(multicastCheck_);
    row->addWidget(multicastLabel_);
    row->addWidget(multicastGroupEdit_);
    row->addWidget(multicastTTLLabel_);
    row->addWidget(multicastTTLSpin_);
    row->addSpacing(10);

    // Start / Stop
    row->addWidget(startBtn_);
    row->addWidget(stopBtn_);

    // Tight spacing and margins for compact strip
    row->setContentsMargins(0,0,0,0);
    row->setSpacing(6);
    setLayout(row);

    // Keep it compact vertically; allow reasonable horizontal use
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout()->setSizeConstraint(QLayout::SetFixedSize);

    // Initial multicast enabled state
    updateMulticastEnabled();

    wireSignals();
    adjustSize();
}

void ConnectionWidget::wireSignals() {
    // Start
    connect(startBtn_, &QPushButton::clicked, this, [this]() {
        if (running_) return;
        if (onStart) onStart(config());
        setRunning(true);
    });

    // Stop
    connect(stopBtn_, &QPushButton::clicked, this, [this]() {
        if (!running_) return;
        if (onStop) onStop();
        setRunning(false);
    });

    // Multicast enable/disable
    connect(multicastCheck_, &QCheckBox::toggled, this, [this](bool){
        updateMulticastEnabled();
        adjustSize();
    });
}

ConnectionConfig ConnectionWidget::config() const {
    ConnectionConfig c;
    c.localHost        = localHostEdit_->text().trimmed();
    c.localPort        = localPortSpin_->value();
    c.remoteHost       = remoteHostEdit_->text().trimmed();
    c.remotePort       = remotePortSpin_->value();
    c.logToStdout      = logStdoutCheck_->isChecked();
    c.multicastEnabled = multicastCheck_->isChecked();
    c.multicastGroup   = multicastGroupEdit_->text().trimmed();
    c.multicastTTL     = multicastTTLSpin_->value();
    return c;
}

void ConnectionWidget::setConfig(const ConnectionConfig& c) {
    localHostEdit_->setText(c.localHost);
    localPortSpin_->setValue(c.localPort);
    remoteHostEdit_->setText(c.remoteHost);
    remotePortSpin_->setValue(c.remotePort);
    logStdoutCheck_->setChecked(c.logToStdout);

    multicastCheck_->setChecked(c.multicastEnabled);
    multicastGroupEdit_->setText(c.multicastGroup);
    multicastTTLSpin_->setValue(c.multicastTTL);
    updateMulticastEnabled();
}

void ConnectionWidget::setInputsEnabled(bool on) {
    localHostEdit_->setEnabled(on);
    localPortSpin_->setEnabled(on);
    remoteHostEdit_->setEnabled(on);
    remotePortSpin_->setEnabled(on);
    logStdoutCheck_->setEnabled(on);

    // multicast checkbox can always be toggled while not running
    multicastCheck_->setEnabled(on);

    // multicast fields themselves depend on both 'on' and checkbox state
    bool mcFieldsOn = on && multicastCheck_->isChecked();
    multicastGroupEdit_->setEnabled(mcFieldsOn);
    multicastTTLSpin_->setEnabled(mcFieldsOn);
    multicastLabel_->setEnabled(mcFieldsOn);
    multicastTTLLabel_->setEnabled(mcFieldsOn);
}

void ConnectionWidget::setRunning(bool running) {
    running_ = running;
    setInputsEnabled(!running_);
    updateButtons();
}

void ConnectionWidget::updateButtons() {
    startBtn_->setEnabled(!running_);
    stopBtn_->setEnabled(running_);
}

void ConnectionWidget::updateMulticastEnabled() {
    bool mcOn = multicastCheck_->isChecked() && !running_;

    multicastGroupEdit_->setEnabled(mcOn);
    multicastTTLSpin_->setEnabled(mcOn);

    // Labels also reflect enabled state for visual clarity
    multicastLabel_->setEnabled(mcOn);
    multicastTTLLabel_->setEnabled(mcOn);
}

bool ConnectionWidget::logStdoutChecked() {
    return logStdoutCheck_->isChecked();
}
