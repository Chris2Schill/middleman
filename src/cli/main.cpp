#include <mm/network/udp_transport.hpp>
#include <mm/network/middleman_proxy.hpp>
#include <mm/mutators/packet_mutator.hpp>
#include <mm/mutators/test_mutator.hpp>
#include <mm/mutators/json_rule_based_mutator.hpp>
#include <mm/config_reader.hpp>

int main() {
    std::string config_file = "mm_config.json";
    spdlog::info("Reading configuration file: " + config_file + "...");
    json config = read_configuration(config_file);
    spdlog::info("Config: " + config.dump(2));

    spdlog::info("Opening Socket");
    boost::asio::io_context ctx;

    std::string    local_host = config["local_host"].get<std::string>();
    unsigned short local_port = config["local_port"].get<unsigned short>();

    std::string    remote_host = config["remote_host"].get<std::string>();
    unsigned short remote_port = config["remote_port"].get<unsigned short>();

    
    std::thread thrd = std::thread([&ctx](){
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(boost::asio::make_work_guard(ctx));;
            ctx.run();
        });

    bool to_big_endian = true;
    mm::network::middleman_proxy::settings settings = {
        .local_host  = local_host,
        .local_port  = local_port,
        .remote_host = remote_host,
        .remote_port = remote_port,
        .mutator = std::make_shared<mm::mutators::json_rule_based_mutator>("dis_types.json", "test_rules2.json", to_big_endian),
        .log_to_stdout = true
    };

    mm::network::middleman_proxy proxy_server(&ctx, settings);

    sleep(5000);
}
