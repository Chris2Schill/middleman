#pragma once

#include "udp_transport.hpp"
#include <iomanip>
#include <functional>

#include <mm/mutators/packet_mutator.hpp>

namespace mm::network{

static std::string buffer_to_hex(const char* buffer, size_t length) {
    std::stringstream ss;
    for (size_t i = 0; i < length; ++i) {
        // Cast to unsigned char to avoid sign extension issues
        // Then cast to int to allow std::hex to format it
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(static_cast<unsigned char>(buffer[i]));
        if ((i+1) % 2 == 0) { ss << " "; }
    }
    return ss.str();
}

class middleman_proxy {
public:
    struct settings {
        std::string local_host;
        unsigned short local_port;
        std::string remote_host;
        unsigned short remote_port;
        std::string multicast_group;
        std::shared_ptr<mutators::packet_mutator> mutator;
        bool log_to_stdout = false;
    };
private:
    mm::network::UDPTransportPtr socket;
    settings cfg;
    Endpoint src_ep;
    Endpoint sink_ep;

public:
    ~middleman_proxy() {
        socket->cancel();
    }
    middleman_proxy(boost::asio::io_context* ctx, const settings& cfg)
        :socket(std::make_shared<UDPTransport>(ctx))
        ,cfg(cfg){

        spdlog::info("middleman_proxy starting with settings:  {}:{} -> {}:{}",
                cfg.local_host,
                cfg.local_port,
                cfg.remote_host,
                cfg.remote_port);

        socket->setReadCallback([this]<typename ...Ts>(Ts&& ...ts) {
                recv_callback(std::forward<Ts>(ts)...);
            });

        src_ep  = {boost::asio::ip::make_address(cfg.local_host), cfg.local_port};
        sink_ep = {boost::asio::ip::make_address(cfg.remote_host), cfg.remote_port};

        bool reuse = true;
        auto rc = socket->startListening(src_ep, reuse);
        if (rc != UDPTransport::SUCCESS) {
            spdlog::error("Failed to start middleman proxy socket: errcode {}", (int)rc);
            exit(-1);
        }
        if (!cfg.multicast_group.empty()) {
            socket->setMulticastOutboundInterface(cfg.local_host);
            bool loopback = false;
            socket->joinGroup(cfg.multicast_group, loopback);
            socket->setTTL(64);
        }
    }

    // For UI to be notified of packets
    std::function<void(mm::network::UDPTransportPtr,
                       mm::network::BufferPtr,
                       mm::network::EndpointPtr,
                       const boost::system::error_code&,
                       std::size_t)> on_recv;

    void recv_callback(mm::network::UDPTransportPtr socket,
                       mm::network::BufferPtr readBuf,
                       mm::network::EndpointPtr sender,
                       const boost::system::error_code& ec,
                       std::size_t bytes) {
        spdlog::info("Received {} bytes", bytes);
        // if (sender->address() == src_ep.address() && sender->port() != cfg.local_port) { return; }

        if (cfg.log_to_stdout) {
            spdlog::info(buffer_to_hex((const char*)readBuf->data(), bytes));
        }

        bool mutated = cfg.mutator->mutate_packet(readBuf,sender,bytes);
        if (mutated && cfg.log_to_stdout) {
            spdlog::info(buffer_to_hex((const char*)readBuf->data(), bytes) + " (mutated)");
        }


        auto rc = socket->send_to(readBuf->data(), bytes, sink_ep);
        if (rc != UDPTransport::SUCCESS) {
            spdlog::warn("Failed to forward packet to remote host: errcode {}", (int)rc);
        }

        on_recv(socket,readBuf,sender,ec,bytes);
    }

};

}
