//
// Created by cyy on 2023/1/10.
//

#include "tcp_server.h"

void start_tcp_server(net::TcpServer* server) {
    try {
        sm_log("Start tcp server on: {}", server->port);
        asio::io_context io_context;
        net::Server s(io_context, (short)server->port);
        io_context.run();
    } catch (std::exception& e) {
        sm_error("Start tcp server failed. exception: {}", e.what());
    }
}

void net::TcpServer::start() {
    std::thread server_thread(start_tcp_server, this);
    server_thread.detach();
}
