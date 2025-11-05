#include <QApplication>

#include <QWidget>
#include <QMainWindow>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QThread>

#include <memory>
#include <thread>
#include <mm/network/udp_transport.hpp>
#include <mm/network/middleman_proxy.hpp>
#include <mm/mutators/packet_mutator.hpp>
#include <mm/mutators/test_mutator.hpp>
#include <mm/mutators/json_rule_based_mutator.hpp>
#include <mm/config_reader.hpp>
#include "rules_editor_widget.hpp"
#include "schema_editor.hpp"
#include "connection_widget.hpp"


class AsioThread : public QThread {
    public:
        AsioThread(boost::asio::io_context* ctx) : ctx(ctx){

        }

        void run() override {
            spdlog::info("RUN");
            ctx->run();
        }

        boost::asio::io_context* ctx;
};

class MainWindow : public QMainWindow{
public:

    MainWindow() {
        setCentralWidget(new QWidget);

        QVBoxLayout* layout = new QVBoxLayout;
        centralWidget()->setLayout(layout);

        layout->setContentsMargins(0,0,0,0);

        layout->addWidget(ui.connectionEditor);

        // // Default config
        ConnectionConfig defaultConn = {
            .localHost = "0.0.0.0",
            .localPort = 3000,
            .remoteHost = "172.28.208.1",
            .remotePort = 3000,
            .logToStdout = true,
        };
        ui.connectionEditor->setConfig(defaultConn);

        ui.connectionEditor->onStart = [this](const ConnectionConfig& c) {
            bool to_big_endian = true;
            mm::network::middleman_proxy::settings settings = {
                .local_host  = c.localHost.toStdString(),
                .local_port  = (unsigned short)c.localPort,
                .remote_host = c.remoteHost.toStdString(),
                .remote_port = (unsigned short)c.remotePort,
                .mutator = std::make_shared<mm::mutators::json_rule_based_mutator>("dis_types.json", "test_rules2.json", to_big_endian),
                .log_to_stdout = true
            };

            proxy_server = std::make_shared<mm::network::middleman_proxy>(&asio_ctx, settings);
        };
        ui.connectionEditor->onStop = [this]() {
            proxy_server = nullptr;
        };

        layout->addWidget(ui.tabWidget);
        ui.tabWidget->addTab(ui.schemaEditor, "Packets");
        ui.tabWidget->addTab(ui.rulesEditor, "Rules");

        asio_thread = std::thread([this](){
                boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(boost::asio::make_work_guard(asio_ctx));
                asio_ctx.run();
        });
    }

    struct ui_container {
        QTabWidget* tabWidget = new QTabWidget;
        SchemaEditor* schemaEditor = new SchemaEditor("dis_pdus_scaffold.json");
        ConnectionWidget* connectionEditor = new ConnectionWidget;
        RulesEditorWidget* rulesEditor = new RulesEditorWidget("test_rules2.json");

        QWidget* configurationSection = new QWidget;
        QGridLayout* configurationSectionLayout = new QGridLayout;
        QLineEdit* localHostEdit = new QLineEdit;
        QLineEdit* localPortEdit = new QLineEdit;
        QLineEdit* remoteHostEdit = new QLineEdit;
        QLineEdit* remotePortEdit = new QLineEdit;
        QCheckBox* logToStdoutCheckbox = new QCheckBox;

        QPushButton* startButton = new QPushButton("Start");
    }ui;


    boost::asio::io_context asio_ctx;
    std::shared_ptr<mm::network::middleman_proxy> proxy_server;
    std::thread asio_thread;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv); // Create an application object

    MainWindow window;
    window.resize(500, 400);      // Set the initial size of the window
    window.setWindowTitle("Middle Man"); // Set the window title
    window.show();                // Show the window
    return app.exec();            // Start the event loop
}
