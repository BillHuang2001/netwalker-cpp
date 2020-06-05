#include <utility>

//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_CLIENT_HPP
#define NETWALKER_CLIENT_HPP

class client_session : public std::enable_shared_from_this<client_session>
{
public:
    explicit client_session(asio::io_context& ioc, tcp::endpoint& host) : ws_(ioc),remote_host_(host)
    {

    }

    void start(){
        beast::get_lowest_layer(ws_).socket().async_connect(remote_host_, [self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->do_ws_handshake();
            }
            else{
                logger::print_log(error,0);
            }
        });
    }

private:
    void do_ws_handshake(){
        ws_.async_handshake("localhost","/",[self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->send_handshake();
            }
            else{
                logger::print_log(error,0);
            }
        });
    }

    void send_handshake(){
        ws_.async_write(buffer_.data(), [self=shared_from_this()](const std::error_code& error, size_t length){
            std::cout << "message sent" <<std::endl;
        });
    }

    beast::flat_buffer buffer_;
    beast::websocket::stream<beast::tcp_stream> ws_;
    tcp::endpoint remote_host_;
};

class netwalker_client
{
    netwalker_client(asio::io_context& ioc, std::string& domain_name) : ioc_(ioc)
    {
        tcp::resolver resolver(ioc);
        remote_host_ = *resolver.resolve(domain_name);
        start();
    }

    void start()
    {
        auto session = std::make_shared<client_session>(ioc_, remote_host_);

//        acceptor_.async_accept(session->get_socket(), [session](const std::error_code& error){
//            if(!error){
//                session->start();
//            }
//            else{
//                logger::print_log(error,0);
//            }
//        });
        session->start();
    }

    asio::io_context& ioc_;
    tcp::endpoint remote_host_;
};

#endif //NETWALKER_CLIENT_HPP
