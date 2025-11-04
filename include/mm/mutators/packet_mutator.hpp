#pragma once

#include <mm/network/udp_transport.hpp>

namespace mm::mutators{

struct packet_mutator {
    virtual ~packet_mutator() = default;

    virtual bool should_mutate(mm::network::BufferPtr readBuf,
                               mm::network::EndpointPtr sender,
                               std::size_t bytes) = 0;

    virtual bool mutate_packet(mm::network::BufferPtr readBuf,
                               mm::network::EndpointPtr sender,
                               std::size_t bytes) = 0;
};

}
