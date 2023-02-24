//
// Created by cyy on 2023/1/13.
//

#ifndef MEDIACENTRE_TCP_CLIENT_H
#define MEDIACENTRE_TCP_CLIENT_H

#include "asio.hpp"

#define MESSAGE_MAX_LEN 1024 * 10

using asio::ip::tcp;

class TcpClient {

private:
    asio::io_context ioContext;
    tcp::socket *socket;

private:
    void connect(std::string& host, int port) {
        this->socket = new tcp::socket(this->ioContext);
        tcp::resolver resolver(this->ioContext);

        asio::connect(*this->socket, resolver.resolve(host, std::to_string(port)));
    }

    std::string write(std::string& msg) {
        asio::write(*this->socket, asio::buffer(msg));

        char reply[MESSAGE_MAX_LEN];
        std::size_t len = asio::read(*this->socket, asio::buffer(reply));
        return std::string(reply, len);
    }

public:
    std::string send(std::string& host, int port, std::string& msg) {
        this->connect(host, port);
        this->write(msg);
    }
};


#endif //MEDIACENTRE_TCP_CLIENT_H
