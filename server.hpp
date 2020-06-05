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
    server_session()
    {

    }

    void start(){

    }

    tcp::socket& get_socket(){

    }

};

class netwalker_server
{
public:
    netwalker_server(asio::io_context& ioc) : acceptor_(ioc)
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
