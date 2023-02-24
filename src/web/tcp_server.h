//
// Created by cyy on 2023/1/10.
//

#ifndef MEDIACENTRE_TCP_SERVER_H
#define MEDIACENTRE_TCP_SERVER_H

#include <asio.hpp>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <memory>
#include <utility>

namespace net {

    using asio::ip::tcp;

    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        Session(tcp::socket socket) : socket_(std::move(socket)) {}

        void start() {
            do_read();
        }

    private:
        void do_read() {
            auto self(shared_from_this());
            socket_.async_read_some(asio::buffer(data_, max_length),
                                    [this, self](std::error_code ec, std::size_t length)
                                    {
                                        if (!ec)
                                        {
                                            std::string buffer(data_, 0, length);
                                            std::cout << buffer << std::endl;
                                            do_write(length);
                                        }
                                    });
        }

        void do_write(std::size_t length) {
            auto self(shared_from_this());
            asio::async_write(socket_, asio::buffer(data_, length),
                              [this, self](std::error_code ec, std::size_t /*length*/)
                              {
                                  if (!ec)
                                  {
                                      do_read();
                                  }
                              });
        }

        tcp::socket socket_;
        enum { max_length = 1024 };
        char data_[max_length];
    };

    class Server
    {
    public:
        Server(asio::io_context& io_context, short port)
                : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
            do_accept();
        }

    private:
        void do_accept() {
            acceptor_.async_accept(
                    [this](std::error_code ec, tcp::socket socket)
                    {
                        if (!ec)
                        {
                            std::make_shared<Session>(std::move(socket))->start();
                        }

                        do_accept();
                    });
        }

        tcp::acceptor acceptor_;
    };

    class TcpServer {
    public:
        int port;

    public:
        explicit TcpServer(int port) : port(port) {};

        void start();

    };

}
#endif //MEDIACENTRE_TCP_SERVER_H
