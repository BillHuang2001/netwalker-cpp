#include <utility>


//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_CLIENT_HPP
#define NETWALKER_CLIENT_HPP

class client_session : public std::enable_shared_from_this<client_session>
{
public:
    explicit client_session(asio::io_context& ioc) : ws_(ioc)
    {

    }

    static void set_remote_host(tcp::endpoint host){
        remote_host_ = std::move(host);
    }

    void start(){
        beast::get_lowest_layer(ws_).socket().async_connect(remote_host_, [self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->do_ws_handshake();
            }
            else{
                logger::print_log(error,0,__POSITION__);
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
                logger::print_log(error,0,__POSITION__);
            }
        });
    }

    void send_handshake(){
        ws_.async_write(asio::buffer("hello world, using websocket!"), [self=shared_from_this()](const std::error_code& error, size_t length){
            std::cout << "message sent" <<std::endl;
        });
    }

    //beast::flat_buffer buffer_;
    beast::websocket::stream<beast::tcp_stream> ws_;
    static tcp::endpoint remote_host_;
};

tcp::endpoint client_session::remote_host_ = tcp::endpoint();

class netwalker_client
{
public:
    netwalker_client(asio::io_context& ioc, const std::string& domain_name) : ioc_(ioc),resolver_(ioc)
    {
        resolver_.async_resolve(domain_name, "", [this](const std::error_code& error, tcp::resolver::results_type results){
            if(!error) {
                for(auto temp = results.begin();temp!=results.end();temp++){
                    std::cout << tcp::endpoint(*temp) <<"\n";
                }

                tcp::endpoint host(*results);
                host.port(1234);
                client_session::set_remote_host(host);
                start();
            }
            else{
                logger::print_log(error,0, __POSITION__);
            }
        });
    }

private:

    void start()
    {
        auto session = std::make_shared<client_session>(ioc_);

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
    asio::ip::tcp::resolver resolver_;
};

#endif //NETWALKER_CLIENT_HPP
