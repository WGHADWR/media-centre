//
// Created by cyy on 2023/1/10.
//

#include "tcp_server.h"

void start_tcp_server(net::TcpServer* server) {
    try {
        std::cout << "Start tcp server on:" << server->port << std::endl;
        asio::io_context io_context;
        net::Server s(io_context, (short)server->port);
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

void net::TcpServer::start() {
    std::thread server_thread(start_tcp_server, this);
    server_thread.detach();
}
