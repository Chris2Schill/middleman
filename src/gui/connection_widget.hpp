#pragma once
#include <QWidget>
#include <functional>

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

struct ConnectionConfig {
    QString localHost;
    int     localPort = 0;
    QString remoteHost;
    int     remotePort = 0;
    bool    logToStdout = false;
};

class ConnectionWidget : public QWidget {
public:
    explicit ConnectionWidget(QWidget* parent = nullptr);

    // Read/Write the form as a single struct
    ConnectionConfig config() const;
    void setConfig(const ConnectionConfig& c);

    // Enable/disable fields (e.g., while running)
    void setInputsEnabled(bool on);

    // Optional callbacks (set these from outside)
    std::function<void(const ConnectionConfig&)> onStart;
    std::function<void()> onStop;

    // Convenience: set the running state (updates buttons/enables)
    void setRunning(bool running);
    bool isRunning() const { return running_; }

private:
    // UI elements
    QLineEdit*  localHostEdit_;
    QSpinBox*   localPortSpin_;
    QLineEdit*  remoteHostEdit_;
    QSpinBox*   remotePortSpin_;
    QCheckBox*  logStdoutCheck_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;

    bool running_ = false;

    void wireSignals();
    void updateButtons();
};

