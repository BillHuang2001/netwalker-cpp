//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_SERVER_HPP
#define NETWALKER_SERVER_HPP

#include <iostream>
#include <functional>
#include <memory>
#include "cipher.h"
#include "common.hpp"

#define TIME_DIFF 15

class server_session : public std::enable_shared_from_this<server_session>
{
public:
    typedef std::shared_ptr<server_session> pointer;
    explicit server_session(asio::io_context& ioc) : ws_(ioc),encrypt_(passwd_),decrypt_(passwd_),socket_out_(ioc)
    {

    }

    static void set_password(const u64& passwd){
        passwd_ = passwd;
    }


    void start(){
        ws_.async_accept([self=shared_from_this()](const std::error_code& error){
            if(!error){
                logger::print_log("ws handshake complete",LOG_LEVEL::DEBUG);
                self->read_handshake();
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
    void read_handshake(){
        ws_.async_read(buffer_,[self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error) {
                std::cout << beast::buffers_to_string(self->buffer_.data()) << std::endl;
                self->handle_client_request(length);
            }
            else{
                logger::print_log(error,0,__POSITION__);
            }
        });
    }

    void handle_client_request(size_t length) {
        //
        //        +----+-----+-------+------+----------+----------+------------+-------------+------+
        //        |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT | RAND BYTES | System Time | Rand |
        //        +----+-----+-------+------+----------+----------+------------+-------------+------+
        //        | 1  |  1  | X'00' |  1   | Variable |    2     |     Var    |      4      |  1   |
        //        +----+-----+-------+------+----------+----------+------------+-------------+------+
        //
        //        encrypt from [0..-2);
        //
        time_t now = std::time(0);
        std::string req = beast::buffers_to_string(buffer_.data());
        //in most of the cases: now > client_time
        encrypt_.set_seed(passwd_ + (unsigned char)req[req.size()-1] + (unsigned char)req[req.size()-2]);
        decrypt_.set_seed(passwd_ + (unsigned char)req[req.size()-1] + (unsigned char)req[req.size()-2]);

        decrypt_.calc(req,req.size()-2);

        logger::print_log(req,LOG_LEVEL::DEBUG);
    }

//    void analyse_socks5_request(size_t length)
//    {
//        /*
//                C->S : TOTAL_LENGTH = len -3 +4 +NUM_RANDOM_DIGITS +1
//                SystemTime  Time_Salt   LEN_AFTER  |->  ATYP    DST.ADDR  DST.PORT
//                4           1           1          |->   1       Var       2
//
//                S-C :
//                SystemTime    STATUS
//                4             1
//
//                STATUS: 1-good 0-bad
//        */
//        decrypt_.calc(buffer_in_, length);
//
//        unsigned short port_num = buffer_in_[length-2];
//        port_num = (port_num<<8) +buffer_in_[length-1];
//
//
//        switch(buffer_in_[0])
//        {
//            case 1://ipv4
//            {
//                asio::ip::address address;
//                std::array<unsigned char, 4> ipv4_address;
//                int i = 1, j = 0;
//                while (j < 4) {
//                    ipv4_address[j++] = buffer_in_[i++];
//                }
//                address = asio::ip::address_v4(ipv4_address);
//                server_endpoint_ = tcp::endpoint(address,port_num);
//                try_connect();
//                break;
//            }
//            case 3://domain_name
//            {
//                unsigned short domain_len = buffer_in_[1];
//                if (domain_len != length - 4) {
//                    std::cout << "Wrong domain name length\n";
//                    return;
//                }
//                std::string domain_name(domain_len,' ');
//                int i = 2, j = 0;
//                while (j < domain_len) {
//                    domain_name[j++] = buffer_in_[i++];
//                }
//                if(verbose_) std::cout<<domain_name<<std::endl;
//
//                pointer self(shared_from_this());
//                resolver_.async_resolve(domain_name,"",
//                                        [port_num, self](const std::error_code& error, tcp::resolver::results_type results){
//                                            if(!error){
//                                                self->server_endpoint_ = results->endpoint();
//                                                self->server_endpoint_.port(port_num);
//
//                                                self->try_connect();
//                                            }
//                                            else{
//                                                std::cout <<"Cannot resolve domain name"<<std::endl;
//                                            }
//                                        }
//                );
//                break;
//            }
//            case 4://ipv6
//            {
//                asio::ip::address address;
//                std::array<unsigned char, 16> ipv6_address;
//                int i = 1, j = 0;
//                while (j < 16) {
//                    ipv6_address[j++] = buffer_in_[i++];
//                }
//                address = asio::ip::address_v6(ipv6_address);
//                server_endpoint_ = tcp::endpoint(address,port_num);
//
//                try_connect();
//                break;
//            }
//            default:
//                std::cout <<"Unsupported protocol\n";
//                return;
//        }
//
//    }

    beast::websocket::stream<beast::tcp_stream> ws_;
    tcp::socket socket_out_;
    beast::flat_buffer buffer_;
    static u64 passwd_;
    cipher encrypt_,decrypt_;
};

u64 server_session::passwd_ = 0;

class netwalker_server
{
public:
    netwalker_server(asio::io_context& ioc, u16 listen_port) : ioc_(ioc),acceptor_(ioc, tcp::endpoint(tcp::v6(), listen_port))
    {
        start();
    }

private:
    void start()
    {
        auto session = std::make_shared<server_session>(ioc_);

        acceptor_.async_accept(session->get_socket(), [session](const std::error_code& error){
            if(!error){
                std::cout <<"Accepted"<<std::endl;
                session->start();
            }
            else{
                logger::print_log(error,0);
            }
        });
    }

    asio::io_context& ioc_;
    asio::ip::tcp::acceptor acceptor_;
};


#endif //NETWALKER_SERVER_HPP
