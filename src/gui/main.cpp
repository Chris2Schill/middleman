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
#include "schema_editor.hpp"


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

        layout->addWidget(ui.configurationSection);
        ui.configurationSection->setLayout(ui.configurationSectionLayout);
        ui.configurationSectionLayout->addWidget(ui.localHostEdit, 0, 0);
        ui.configurationSectionLayout->addWidget(new QLabel("local host"), 0, 1);
        ui.configurationSectionLayout->addWidget(ui.localPortEdit, 1, 0);
        ui.configurationSectionLayout->addWidget(new QLabel("local port"), 1, 1);
        ui.configurationSectionLayout->addWidget(ui.remoteHostEdit, 0, 2);
        ui.configurationSectionLayout->addWidget(new QLabel("remote host"), 0, 3);
        ui.configurationSectionLayout->addWidget(ui.remotePortEdit, 1, 2);
        ui.configurationSectionLayout->addWidget(new QLabel("remote port"), 1, 3);
        ui.configurationSectionLayout->addWidget(ui.logToStdoutCheckbox, 2, 0);
        ui.configurationSectionLayout->addWidget(new QLabel("log to stdout"), 2, 1);
        // Default config
        ui.localHostEdit->setText("0.0.0.0");
        ui.localPortEdit->setText("3000");
        ui.remoteHostEdit->setText("172.28.208.1");
        ui.remotePortEdit->setText("3000");


        layout->addWidget(ui.startButton);
        connect(ui.startButton, &QPushButton::released, this, [this](){
            if (ui.configurationSection->isEnabled()) {

                bool to_big_endian = true;
                mm::network::middleman_proxy::settings settings = {
                    .local_host  = ui.localHostEdit->text().toStdString(),
                    .local_port  = ui.localPortEdit->text().toUShort(),
                    .remote_host = ui.remoteHostEdit->text().toStdString(),
                    .remote_port = ui.remotePortEdit->text().toUShort(),
                    .mutator = std::make_shared<mm::mutators::json_rule_based_mutator>("dis_types.json", "test_rules2.json", to_big_endian),
                    .log_to_stdout = true
                };

                proxy_server = std::make_shared<mm::network::middleman_proxy>(&asio_ctx, settings);
                disableConfig();
            }
            else {
                proxy_server = nullptr;
                enableConfig();
            }
        });

        layout->addWidget(ui.tabWidget);
        ui.tabWidget->addTab(ui.schemaEditor, "Packets");

        asio_thread = std::thread([this](){
                boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(boost::asio::make_work_guard(asio_ctx));
                asio_ctx.run();
        });
    }

    void disableConfig() {
        ui.configurationSection->setDisabled(true);
        ui.startButton->setText("Stop");
    }
    void enableConfig() {
        ui.configurationSection->setDisabled(false);
        ui.startButton->setText("Start");
    }

    struct ui_container {
        QTabWidget* tabWidget = new QTabWidget;
        SchemaEditor* schemaEditor = new SchemaEditor;

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
