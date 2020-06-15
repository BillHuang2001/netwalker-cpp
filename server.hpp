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

#define MOD 32
#define TIME_DIFF 15

class server_session : public std::enable_shared_from_this<server_session>
{
public:
    typedef std::shared_ptr<server_session> pointer;
    explicit server_session(asio::io_context& ioc) : ws_(ioc),socket_out_(ioc),encrypt_(passwd_),decrypt_(passwd_),is_ready_(false)
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
    void close_all(){
        if(!is_ready_) {
            is_ready_ = true;
            ws_.async_close(beast::websocket::close_code::normal,
                            [self = shared_from_this()](const std::error_code &error) {
                                try {
                                    self->socket_out_.close();
                                }
                                catch (std::exception &) {}
                                logger::print_log("server session closed", LOG_LEVEL::INFO);
                            });
        }
    }

    void read_handshake(){
        ws_.async_read(buffer_,[self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error) {
                self->handle_client_request();
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });
    }

    void handle_client_request() {
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
        buffer_.consume(buffer_.size());
        //in most of the cases: now > client_time
        encrypt_.set_seed(passwd_ + (unsigned char)req[req.size()-1] + (unsigned char)req[req.size()-2]);
        decrypt_.set_seed(passwd_ + (unsigned char)req[req.size()-1] + (unsigned char)req[req.size()-2]);

        decrypt_.calc(req,req.size()-2);

        analyse_socks5_request(req, req.size() - 5 - (unsigned char)(req[req.size() - 1])%MOD);
    }

    void analyse_socks5_request(std::string& req, size_t length)
    {

        auto port_num = static_cast<unsigned short>(req[length - 2]);
        port_num = (unsigned short)(port_num << 8u) + (unsigned char)req[length - 1];


        switch(req[3])
        {
            case 1://ipv4
            {
                asio::ip::address address;
                std::array<unsigned char, 4> ipv4_address{};
                int i = 4, j = 0;
                while (j < 4) {
                    ipv4_address[j++] = static_cast<unsigned char>(req[i++]);
                }
                address = asio::ip::address_v4(ipv4_address);
                tcp::endpoint server_endpoint = tcp::endpoint(address,port_num);
                try_connect(server_endpoint);
                break;
            }
            case 3://domain_name
            {
                auto domain_len = static_cast<unsigned short>(req[4]);
                if (domain_len != length - 7) {
                    logger::print_log("Wrong domain name length",LOG_LEVEL::ERROR);
                    return;
                }
                std::string domain_name = req.substr(5,domain_len);

                logger::print_log(domain_name,LOG_LEVEL::INFO);
                tcp::resolver resolver(socket_out_.get_executor().context());
                resolver.async_resolve(domain_name,"https",
                                        [port_num, self=shared_from_this()](const std::error_code& error, tcp::resolver::results_type results){
                                            if(!error){
                                                if(port_num==443){
                                                    self->try_connect_range(std::move(results));
                                                }
                                                else {
                                                    tcp::endpoint server_endpoint = results->endpoint();
                                                    server_endpoint.port(port_num);
                                                    self->try_connect(server_endpoint);
                                                }
                                            }
                                            else{
                                                logger::print_log(error,0,STRINGIZE(__LINE__));
                                            }
                                        }
                );
                break;
            }
            case 4://ipv6
            {
                asio::ip::address address;
                std::array<unsigned char, 16> ipv6_address{};
                int i = 1, j = 0;
                while (j < 16) {
                    ipv6_address[j++] = req[i++];
                }
                address = asio::ip::address_v6(ipv6_address);
                tcp::endpoint server_endpoint = tcp::endpoint(address,port_num);

                try_connect(server_endpoint);
                break;
            }
            default:
                std::cout <<"Unsupported protocol\n";
                return;
        }

    }

    void try_connect(tcp::endpoint& server_endpoint){
        socket_out_.async_connect(server_endpoint, [self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->ws_.binary(true);
                self->read_from_socket_in();
                self->read_from_socket_out();
                logger::print_log("Connect successful",LOG_LEVEL::DEBUG);
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });
    }

    void try_connect_range(tcp::resolver::results_type results){
        asio::async_connect(socket_out_, results, [self=shared_from_this()](const std::error_code& error, const tcp::endpoint& endpoint){
            if(!error){
                self->ws_.binary(true);
                self->read_from_socket_in();
                self->read_from_socket_out();
                logger::print_log("Connect successful",LOG_LEVEL::DEBUG);
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });
    }

    void read_from_socket_in(){
        ws_.async_read(buffer_, [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                self->decrypt_.calc(self->buffer_);
                self->write_to_socket_out();
            }
            else{
                self->close_all();
            }
        });
    }

    void write_to_socket_out(){
        asio::async_write(socket_out_, buffer_.data(), [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                self->buffer_.consume(self->buffer_.size());
                self->read_from_socket_in();
            }
            else{
                self->close_all();
            }
        });
    }

    void read_from_socket_out(){
        socket_out_.async_read_some(buffer_out_.prepare(2048), [self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error){
                self->buffer_out_.commit(length);
                self->encrypt_.calc(self->buffer_out_);
                self->write_to_socket_in();
            }
            else{
                self->close_all();
            }
        });
    }

    void write_to_socket_in(){
        ws_.async_write(buffer_out_.data(), [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                self->buffer_out_.consume(self->buffer_out_.size());
                self->read_from_socket_out();
            }
            else{
                self->close_all();
            }
        });
    }


    beast::websocket::stream<beast::tcp_stream> ws_;
    tcp::socket socket_out_;
    beast::flat_buffer buffer_, buffer_out_;
    static u64 passwd_;
    cipher encrypt_,decrypt_;
    std::atomic<bool> is_ready_{};
};

u64 server_session::passwd_ = 0;

class netwalker_server
{
public:
    netwalker_server(asio::io_context& ioc, u16 listen_port, u64 passwd) : ioc_(ioc),acceptor_(ioc, tcp::endpoint(tcp::v6(), listen_port))
    {
        server_session::set_password(passwd);
        logger::print_log("server started",LOG_LEVEL::INFO);
        start();
    }

private:
    void start()
    {
        auto session = std::make_shared<server_session>(ioc_);

        acceptor_.async_accept(session->get_socket(), [this,session](const std::error_code& error){
            if(!error){
                session->start();
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
            start();
        });
    }

    asio::io_context& ioc_;
    asio::ip::tcp::acceptor acceptor_;
};


#endif //NETWALKER_SERVER_HPP
