#include "connection_widget.hpp"

#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QVBoxLayout>

ConnectionWidget::ConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    // Fields
    localHostEdit_  = new QLineEdit(this);
    localPortSpin_  = new QSpinBox(this);
    remoteHostEdit_ = new QLineEdit(this);
    remotePortSpin_ = new QSpinBox(this);
    logStdoutCheck_ = new QCheckBox("Log to stdout", this);
    startBtn_       = new QPushButton("Start", this);
    stopBtn_        = new QPushButton("Stop", this);

    // Reasonable defaults
    localHostEdit_->setPlaceholderText("127.0.0.1");
    remoteHostEdit_->setPlaceholderText("example.com");
    localPortSpin_->setRange(0, 65535);
    remotePortSpin_->setRange(0, 65535);
    localPortSpin_->setValue(9000);
    remotePortSpin_->setValue(9001);
    stopBtn_->setEnabled(false); // initially not running

    // Layouts
    auto* form = new QFormLayout;
    form->addRow("Local host:",  localHostEdit_);
    form->addRow("Local port:",  localPortSpin_);
    form->addRow("Remote host:", remoteHostEdit_);
    form->addRow("Remote port:", remotePortSpin_);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(startBtn_);
    buttons->addWidget(stopBtn_);
    buttons->addStretch();

    auto* root = new QVBoxLayout;
    root->addLayout(form);
    root->addWidget(logStdoutCheck_);
    root->addLayout(buttons);
    root->addStretch();
    setLayout(root);

    wireSignals();
}

void ConnectionWidget::wireSignals() {
    // Start: read current config and invoke callback if set
    connect(startBtn_, &QPushButton::clicked, this, [this](){
        if (running_) return;
        if (onStart) onStart(config());
        setRunning(true);
    });

    // Stop: invoke callback if set
    connect(stopBtn_, &QPushButton::clicked, this, [this](){
        if (!running_) return;
        if (onStop) onStop();
        setRunning(false);
    });
}

ConnectionConfig ConnectionWidget::config() const {
    ConnectionConfig c;
    c.localHost   = localHostEdit_->text().trimmed();
    c.localPort   = localPortSpin_->value();
    c.remoteHost  = remoteHostEdit_->text().trimmed();
    c.remotePort  = remotePortSpin_->value();
    c.logToStdout = logStdoutCheck_->isChecked();
    return c;
}

void ConnectionWidget::setConfig(const ConnectionConfig& c) {
    localHostEdit_->setText(c.localHost);
    localPortSpin_->setValue(c.localPort);
    remoteHostEdit_->setText(c.remoteHost);
    remotePortSpin_->setValue(c.remotePort);
    logStdoutCheck_->setChecked(c.logToStdout);
}

void ConnectionWidget::setInputsEnabled(bool on) {
    localHostEdit_->setEnabled(on);
    localPortSpin_->setEnabled(on);
    remoteHostEdit_->setEnabled(on);
    remotePortSpin_->setEnabled(on);
    logStdoutCheck_->setEnabled(on);
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

