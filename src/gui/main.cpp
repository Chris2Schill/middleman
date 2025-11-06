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

#include "connection_widget.hpp"
#include "rules_editor_widget.hpp"
#include "schema_editor.hpp"
#include "udp_viewer_widget.hpp"


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
            .multicastEnabled = false,
            .multicastGroup = "224.10.10.19",
            .multicastTTL = 64,
        };
        ui.connectionEditor->setConfig(defaultConn);

        ui.connectionEditor->onStart = [this](const ConnectionConfig& c) {

            auto rulesJsonStr = ui.rulesEditor->schemaJson().toStdString();

            bool to_big_endian = true;
            mm::network::middleman_proxy::settings settings = {
                .local_host  = c.localHost.toStdString(),
                .local_port  = (unsigned short)c.localPort,
                .remote_host = c.remoteHost.toStdString(),
                .remote_port = (unsigned short)c.remotePort,
                .multicast_group = c.multicastGroup.toStdString(),
                .mutator = mm::mutators::json_rule_based_mutator::fromJsonString("dis_pdus_scaffold.json", rulesJsonStr, to_big_endian),
                .log_to_stdout = ui.connectionEditor->logStdoutChecked(),
            };

            proxy_server = std::make_shared<mm::network::middleman_proxy>(&asio_ctx, settings);

            // Cache the dstHost here for performance
            QHostAddress dstHost(QString::fromStdString((proxy_server->getSink().address().to_string())));

            proxy_server->on_recv = [this,&dstHost](auto socket, auto readBuf, auto sender, auto ec, auto bytes){
                QByteArray readBufClone((const char*)readBuf->data(), bytes);
                QHostAddress srcHost(QString::fromStdString(sender->address().to_string()));
                QMetaObject::invokeMethod(this, [=, &dstHost]{
                        int bytesClone = bytes;

                        ui.packetViewer->addPacket(
                                readBufClone, srcHost, 3000, dstHost, 3000
                            );
                    },Qt::QueuedConnection) ;
            };
            ui.rulesEditor->setEnabled(false);
        };
        ui.connectionEditor->onStop = [this]() {
            proxy_server = nullptr;
            ui.rulesEditor->setEnabled(true);
        };

        layout->addWidget(ui.tabWidget);
        ui.tabWidget->setContentsMargins(0,0,0,0);
        ui.tabWidget->addTab(ui.packetViewer, "Packets");
        ui.tabWidget->addTab(ui.rulesEditor, "Rules");
        ui.tabWidget->addTab(ui.schemaEditor, "Types");

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
        UdpViewerWidget* packetViewer = new UdpViewerWidget;

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
    app.setStyle("Fusion");

    MainWindow window;
    window.resize(500, 400);      // Set the initial size of the window
    window.setWindowTitle("Middle Man"); // Set the window title
    window.show();                // Show the window
    return app.exec();            // Start the event loop
}
