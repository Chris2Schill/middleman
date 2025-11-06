#pragma once
#include <QWidget>
#include <functional>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

struct ConnectionConfig {
    QString localHost;
    int     localPort = 0;
    QString remoteHost;
    int     remotePort = 0;
    bool    logToStdout = false;

    // Multicast
    bool    multicastEnabled = false;
    QString multicastGroup;   // e.g. 239.0.0.1
    int     multicastTTL = 1; // typical default
};

class ConnectionWidget : public QWidget {
public:
    explicit ConnectionWidget(QWidget* parent = nullptr);

    ConnectionConfig config() const;
    void setConfig(const ConnectionConfig& c);

    void setInputsEnabled(bool on);
    void setRunning(bool running);
    bool isRunning() const { return running_; }
    bool logStdoutChecked();

    std::function<void(const ConnectionConfig&)> onStart;
    std::function<void()> onStop;

private:
    // Base fields
    QLineEdit*   localHostEdit_;
    QSpinBox*    localPortSpin_;
    QLineEdit*   remoteHostEdit_;
    QSpinBox*    remotePortSpin_;
    QCheckBox*   logStdoutCheck_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;

    // Multicast controls
    QCheckBox*   multicastCheck_;
    QLabel*      multicastLabel_;
    QLineEdit*   multicastGroupEdit_;
    QLabel*      multicastTTLLabel_;
    QSpinBox*    multicastTTLSpin_;

    bool running_ = false;

    void wireSignals();
    void updateButtons();
    void updateMulticastEnabled(); // enable/disable group/TTL based on checkbox and running state
};

