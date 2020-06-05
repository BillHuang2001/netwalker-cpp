//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_SERVER_HPP
#define NETWALKER_SERVER_HPP

#include <iostream>
#include <functional>
#include "common.hpp"

class server_session : public std::enable_shared_from_this<server_session>
{
public:
    typedef std::shared_ptr<server_session> pointer;
    explicit server_session(asio::io_context& ioc) : ws_(ioc)
    {

    }

    void start(){
        ws_.async_accept([self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->read_client_hand_shake();
            }
            else{
                logger::print_log(error,0);
            }
        });
    }

    tcp::socket& get_socket(){
        return beast::get_lowest_layer(ws_).socket();
    }

private:
    void read_client_hand_shake(){
        ws_.async_read(buffer_,[self=shared_from_this()](const std::error_code& error, size_t length){
            std::cout << beast::buffers_to_string(self->buffer_.data()) << std::endl;
        });
    }

    beast::websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
};

class netwalker_server
{
public:
    netwalker_server(asio::io_context& ioc, u16 listen_port) : acceptor_(ioc, tcp::endpoint(tcp::v4(), listen_port))
    {
        start();
    }

private:
    void start()
    {
        auto session = server_session::pointer();

        acceptor_.async_accept(session->get_socket(), [session](const std::error_code& error){
            if(!error){
                session->start();
            }
            else{
                logger::print_log(error,0);
            }
        });
    }

    asio::ip::tcp::acceptor acceptor_;
};


#endif //NETWALKER_SERVER_HPP
