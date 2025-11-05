#pragma once

#include <mm/network/udp_transport.hpp>
#include "packet_mutator.hpp"

namespace mm::mutators {

struct tester : public mm::mutators::packet_mutator{

    virtual bool mutate_packet(mm::network::BufferPtr readBuf,
                               mm::network::EndpointPtr sender,
                               std::size_t bytes) {
        if (readBuf->data()[3] == 'z') {
            spdlog::info("Mutating packet...");
            readBuf->data()[3] = 'g';
            return true;
        }
        return false;
    }
};

}
