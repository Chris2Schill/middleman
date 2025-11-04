// =========================================================================
// RESTRICTED RIGHTS
// Contract No.: AVTLS-FSTSSBUILD-23-01 FSTSS
// Contractor Name: Applied Visual Technology, Inc. (d/b/a AVT Simulation)
// Contractor Address: 4715 Data Court, Suite 100, Orlando, Florida 32817
// Expiration Date: None
//
// For any delivery to the U.S. Government:
// The Government's rights to use, modify, reproduce, release, perform,
// display, or disclose this software are restricted by paragraph (c)(3) of
// the DFARS 252.227-7014, Rights in Other Than Commercial Computer Software
// and Other Than Commercial Computer Software Documentation, clause
// contained in the above identified contract. Any reproduction of computer
// software or portions thereof marked with this legend must also reproduce
// the markings. Any person, other than the Government, who has been provided
// access to such software must promptly notify the above named Contractor
//
// For any delivery to a non-U.S. Government entity:
// This work is copyright, and no rights are conferred unless a superseding
// license agreement accompanies the delivery.
//
// SECURITY CLASSIFICATION: UNCLASSIFIED
//
// WARNING - This document contains technical data whose export is restricted
// by the Arms Export Control Act (Title 22, U.S.C., Sec 2751, et seq.) or
// the Export Control Reform Act of 2018(Title 50, U.S.C., Chapter 58,
// Sec. 4801-4852). Violations of these export laws are subject to severe
// criminal penalties. Disseminate in accordance with provisions of DoD
// Directive 5230.25.
//
// Copyright © 2025, Applied Visual Technology, Inc. (d/b/a AVT Simulation)
// Unpublished Work - All Rights Reserved
//
// =========================================================================
#pragma once

#include <boost/asio/error.hpp>
#include <cassert>
#include <sstream>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

namespace mm::network {

using Endpoint = boost::asio::ip::udp::endpoint;
using EndpointPtr = std::shared_ptr<Endpoint>;

using Socket = boost::asio::ip::udp::socket;
using SocketPtr = std::shared_ptr<Socket>;

using Buffer = std::vector<unsigned char>;
using BufferPtr = std::shared_ptr<Buffer>;

using BufferSequence = std::vector<boost::asio::const_buffer>;

using Seconds = std::chrono::duration<double>;

static const Seconds NO_TIMEOUT = std::chrono::seconds(0);

class UDPTransport;
using UDPTransportPtr = std::shared_ptr<UDPTransport>;

#define ASSERT_AND_LOG_FAILURE(condition) \
    if (!(condition)) {                   \
        std::stringstream ss;             \
        ss << "Failed assertion: ["       \
           << #condition << "]";          \
        spdlog::error(ss.str());          \
    }                                     \
    assert(condition);

class UDPTransport : public std::enable_shared_from_this<UDPTransport> {
public:
    using ReadCallback = std::function<void(UDPTransportPtr socket,
                                            BufferPtr readBuf,
                                            EndpointPtr sender,
                                            const boost::system::error_code& ec,
                                            std::size_t bytes)>;

    enum RetCode
    {
        SUCCESS = 0,
        INVALID_ADDRESS,
        INVALID_PORT,
        ALREADY_STARTED,
        MESSAGE_TOO_LARGE,
        SEND_FAILURE,
        BIND_ERROR,
    };

    UDPTransport(boost::asio::io_context* ctx);
    ~UDPTransport();

    RetCode startListening(const Endpoint& endpoint, bool reuse = false);
    RetCode stopListening();

    RetCode send_to(const void* data, size_t size, const Endpoint& endpoint);
    RetCode send_to(const std::string& data, const Endpoint& endpoint);
    RetCode send_to(const std::vector<boost::asio::const_buffer>& bufs, const Endpoint& endpoint);

    void setReadCallback(ReadCallback cb);

    bool isListening() const;

    void setBroadcast(bool bcast);

    void cancel() { socket->cancel(); }

private:
    void startRead();

    boost::asio::io_context* ioCtx = nullptr;
    SocketPtr   socket = nullptr;
    int         listeningPort = 0;
    EndpointPtr senderEndpoint = nullptr;
    BufferPtr   readBuffer = nullptr;
    ReadCallback readCb = nullptr;

};

///////////////////// IMPL ///////////////////////
inline UDPTransport::UDPTransport(boost::asio::io_context* ctx)
    : ioCtx(ctx)
{
    ASSERT_AND_LOG_FAILURE(ctx != nullptr);
}

inline UDPTransport::~UDPTransport()
{
    stopListening();
    spdlog::info("~UDPTransport()");
}

inline UDPTransport::RetCode UDPTransport::startListening(const Endpoint& endpoint, bool reuse)
{
    ASSERT_AND_LOG_FAILURE(readCb != nullptr);

    if (listeningPort != 0) {
        return ALREADY_STARTED;
    }

    if (endpoint.port() == 0) {
        return INVALID_PORT;
    }

    stopListening();

    socket = std::make_shared<Socket>(*ioCtx);
    readBuffer = std::make_shared<Buffer>(0xffff);
    senderEndpoint = std::make_shared<Endpoint>();

    boost::system::error_code ec;
    socket->open(endpoint.protocol(), ec);
    if (ec)
    {
        return INVALID_ADDRESS;
    }

    socket->set_option(boost::asio::ip::udp::socket::reuse_address(reuse));

    socket->bind(endpoint, ec);
    if (ec)
    {
        return BIND_ERROR;
    }

    listeningPort = socket->local_endpoint().port();

    startRead();

    return SUCCESS;
}

inline UDPTransport::RetCode UDPTransport::stopListening()
{
    if (socket)
    {
        socket->cancel();
        socket->close();
        socket = nullptr;
    }
    listeningPort = 0;
    return SUCCESS;
}

inline UDPTransport::RetCode UDPTransport::send_to(const void* data, size_t size, const Endpoint& endpoint)
{
    if (!socket)
    {
        socket = std::make_shared<Socket>(*ioCtx);
        readBuffer = std::make_shared<Buffer>(0xffff);
        senderEndpoint = std::make_shared<Endpoint>();

        boost::system::error_code ec;
        socket->open(endpoint.protocol(), ec);
        if (ec)
        {
            socket = nullptr;
            return INVALID_ADDRESS;
        }
    }

    static const int MAX_SEND_SIZE = 67108864;
    if (size > MAX_SEND_SIZE)
    {
        return MESSAGE_TOO_LARGE;
    }

    
    boost::system::error_code ec;
    socket->send_to(boost::asio::buffer(data, size), endpoint, 0, ec);
    if (ec) {
        return SEND_FAILURE;
    }

    return SUCCESS;
}

inline UDPTransport::RetCode UDPTransport::send_to(const std::string& data, const Endpoint& endpoint)
{
    return send_to(data.c_str(), data.size(), endpoint);
}

inline UDPTransport::RetCode UDPTransport::send_to(const std::vector<boost::asio::const_buffer>& bufs, const Endpoint& endpoint) {
    boost::system::error_code ec;
    socket->send_to(bufs, endpoint, 0, ec);
    if (ec) {
        return SEND_FAILURE;
    }
    return SUCCESS;
}

inline void UDPTransport::setReadCallback(ReadCallback cb)
{
    readCb = cb;
}

inline bool UDPTransport::isListening() const
{
    return listeningPort != 0;
}

inline void UDPTransport::startRead()
{
    ASSERT_AND_LOG_FAILURE(socket->is_open());
    ASSERT_AND_LOG_FAILURE(readCb != nullptr);

    auto self = shared_from_this();
    socket->async_receive_from(
            boost::asio::buffer(*readBuffer),
            *senderEndpoint,
            [self](const boost::system::error_code& ec, std::size_t bytes_transferred){
                if (ec == boost::asio::error::operation_aborted) {
                    return;
                }
                if (!ec) {
                    self->readCb(self, self->readBuffer, self->senderEndpoint, ec, bytes_transferred);
                }
                self->startRead();
            });
}

inline void UDPTransport::setBroadcast(bool bcast)
{
    socket->set_option(boost::asio::socket_base::broadcast(bcast));
}

}
