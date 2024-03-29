//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_CLIENT_HPP
#define NETWALKER_CLIENT_HPP

#include "common.hpp"
#include "cipher.h"

#define MOD 32

class client_session : public std::enable_shared_from_this<client_session>
{
public:
    explicit client_session(asio::io_context& ioc, asio::ssl::context& ctx) :
    buffer_(2048),encrypt_(passwd_),decrypt_(passwd_),ws_(ioc,ctx), socket_in_(ioc),is_ready_(false)
    {
    }

    static void set_password(const u64& passwd){
        passwd_ = passwd;
    }

    static void set_remote_host(tcp::endpoint host){
        remote_host_ = std::move(host);
    }

    static void set_address_and_path(const std::string& address, const std::string& path){
        client_session::address = address;
        client_session::path = path;
    }

    void start(){
        //The magic code that will enable TLS-SNI
        SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),address.data());
        beast::get_lowest_layer(ws_).async_connect(remote_host_, [self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->do_ssl_handshake();
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });

        read_socks5_header();
    }

    tcp::socket& get_socket_in(){
        return socket_in_;
    }



private:
    void close_all(){
        if(!is_ready_) {
            is_ready_ = true;
            ws_.async_close(beast::websocket::close_code::normal,
                            [self = shared_from_this()](const std::error_code &error) {
                                try {
                                    self->socket_in_.close();
                                }
                                catch (std::exception &) {}
                                logger::print_log("server session closed", LOG_LEVEL::INFO);
                            });
        }
    }

    void read_socks5_header(){
        asio::async_read(socket_in_, asio::buffer(buffer_,3),
            [self=shared_from_this()](const std::error_code& error, size_t length)
            {
                if(!error){
                    if(length ==3 && self->buffer_[0]==5 && self->buffer_[2]==0){
                        self->send_socks5_reply();
                    }
                    else{
                        logger::print_log("Unknown protocol",LOG_LEVEL::WARNING);
                    }
                }
                else {
                    logger::print_log(error,0,STRINGIZE(__LINE__));
                }
            }
        );
    }

    void send_socks5_reply(){
        buffer_[1]=0;
        asio::async_write(socket_in_, asio::buffer(buffer_,2),
            [self=shared_from_this()](const std::error_code& error, size_t)
            {
              if(!error){
                  self->read_socks5_request();
              }
              else{
                  logger::print_log(error,0,STRINGIZE(__LINE__));
              }
            }
        );
    }

    void read_socks5_request(){
        //
        //        +----+-----+-------+------+----------+----------+
        //        |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
        //        +----+-----+-------+------+----------+----------+
        //        | 1  |  1  | X'00' |  1   | Variable |    2     |
        //        +----+-----+-------+------+----------+----------+
        //
        //                              Where:
        //
        //        o  VER    protocol version: X'05'
        //        o  CMD
        //        o  CONNECT X'01'
        //        o  BIND X'02'
        //        o  UDP ASSOCIATE X'03'
        //        o  RSV    RESERVED
        //        o  ATYP   address type of following address
        //        o  IP V4 address: X'01'
        //        o  DOMAINNAME: X'03'
        //        o  IP V6 address: X'04'
        //        o  DST.ADDR       desired destination address
        //        o  DST.PORT desired destination port in network octet
        //        order

        socket_in_.async_read_some(asio::buffer(buffer_),
                              [self=shared_from_this()](const std::error_code& error, size_t length)
                              {
                                  if(!error){
                                      if(self->buffer_[3]==3 && (char)length!=self->buffer_[4]+7) logger::print_log("read incomplete",0);
                                      if(self->buffer_[0]==5 && self->buffer_[1]==1) {
                                          self->handle_successful_connection();
                                      }
                                      else if(self->buffer_[1] == 3){
                                          logger::print_log("UDP protocol not supported",1);
                                      }
                                  }
                                  else{
                                      logger::print_log(error,0,STRINGIZE(__LINE__));
                                  }
                              }
        );
    }

    void socks_request_forward()
    {
        //
        //        +----+-----+-------+------+----------+----------+------------+-------------+------+
        //        |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT | RAND BYTES | System Time | Rand |
        //        +----+-----+-------+------+----------+----------+------------+-------------+------+
        //        | 1  |  1  | X'00' |  1   | Variable |    2     |     Var    |      4      |  1   |
        //        +----+-----+-------+------+----------+----------+------------+-------------+------+
        //
        //        encrypt from [0..-2);
        //

        u32 length = buffer_[4]+7;

        auto random_bytes = static_cast<unsigned char>(rand());
        u32 total_length = length + 5 + random_bytes%MOD;

        for(auto i = length; i < total_length-5; i++){
            buffer_[i] = static_cast<unsigned char>(rand());
        }

        time_t now = time(nullptr);
        *(time_t*)(buffer_.data()+total_length-5) = now;
        buffer_[total_length-1] = random_bytes;
        decrypt_.set_seed(passwd_ + buffer_[total_length-2] + buffer_[total_length-1]);
        encrypt_.set_seed(passwd_ + buffer_[total_length-2] + buffer_[total_length-1]);

        auto *message = new unsigned char[total_length];
        encrypt_.calc_to(buffer_, total_length-2, message);
        message[total_length-2] = buffer_[total_length-2];
        message[total_length-1] = buffer_[total_length-1];

        ws_.binary(true);
        ws_.async_write(asio::buffer(message, total_length),
                [self=shared_from_this(), message](const std::error_code& error, size_t){
                if(!error) {
                    self->send_socks5_reply_successful();
                }
                else{
                    logger::print_log(error,0, STRINGIZE(__LINE__));
                    self->handle_unsuccessful_connection();
                }
                delete[] message;
            }
        );
    }

    void send_socks5_reply_successful(){
        buffer_[1] = 0;
        asio::async_write(socket_in_, asio::buffer(buffer_, buffer_[4]+7), [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                self->flat_buffer_.clear();
                self->read_from_socket_in();
                self->read_from_socket_out();
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });
    }

    void do_ssl_handshake(){
        //ws_.next_layer().set_verify_callback(asio::ssl::rfc2818_verification(address));

        ws_.next_layer().async_handshake(asio::ssl::stream_base::client,[self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->do_ws_handshake();
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });
    }

    void do_ws_handshake(){
        ws_.async_handshake(address,path,[self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->handle_successful_connection();
            }
            else{
                logger::print_log(error,0,STRINGIZE(__LINE__));
            }
        });
    }

    void handle_unsuccessful_connection(){
        buffer_[1] = 4;
        asio::async_write(socket_in_, asio::buffer(buffer_, buffer_[4]+7), [self=shared_from_this()](const std::error_code& error, size_t){
            self->close_all();
        });
    }

    void handle_successful_connection(){
        if(is_ready_){
            socks_request_forward();
            is_ready_ = false;
        }
        else{
            is_ready_ = true;
        }
    }

    void read_from_socket_in(){
        socket_in_.async_read_some(asio::buffer(buffer_), [self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error){
                self->encrypt_.calc(self->buffer_, length);
                self->write_to_socket_out(length);
            }
            else{
                self->close_all();
            }
        });
    }

    void write_to_socket_out(const size_t length){
        ws_.async_write(asio::buffer(buffer_,length), [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                self->read_from_socket_in();
            }
            else{
                self->close_all();
            }
        });
    }

    void read_from_socket_out(){
        ws_.async_read(flat_buffer_, [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                //std::cout <<"decrypting data of length" <<length<<"\n";
                self->decrypt_.calc(self->flat_buffer_);
                self->write_to_socket_in();
            }
            else{
                self->close_all();
            }
        });
    }

    void write_to_socket_in(){
        asio::async_write(socket_in_, flat_buffer_.data(), [self=shared_from_this()](const std::error_code& error, size_t){
            if(!error){
                self->flat_buffer_.consume(self->flat_buffer_.size());
                self->read_from_socket_out();
            }
            else{
                self->close_all();
            }
        });
    }

    beast:: flat_buffer flat_buffer_;
    std::vector<unsigned char> buffer_;
    cipher encrypt_,decrypt_;
    beast::websocket::stream<asio::ssl::stream<tcp::socket> > ws_;
    tcp::socket socket_in_;
    static tcp::endpoint remote_host_;
    static u64 passwd_;
    static std::string address, path;
    std::atomic<bool> is_ready_{};
};

tcp::endpoint client_session::remote_host_ = tcp::endpoint();
u64 client_session::passwd_ = 0;
std::string client_session::address, client_session::path;

class netwalker_client
{
public:
    netwalker_client(asio::io_context& ioc, asio::ssl::context& ctx, const std::string& domain_name, const std::string& path, u16 listen_port, u16 server_port, u64 passwd) : ctx_(ctx), ioc_(ioc), acceptor_(ioc,tcp::endpoint(tcp::v6(),listen_port))
    {
        client_session::set_password(passwd);
        auto resolver = make_shared<tcp::resolver>(ioc);
        client_session::set_address_and_path(domain_name,path);
        logger::print_log(domain_name+path,LOG_LEVEL::INFO);
        resolver->async_resolve(domain_name, "", [this, resolver, server_port](const std::error_code& error, tcp::resolver::results_type results){
            if(!error) {
                tcp::endpoint host(*results);
                host.port(server_port);
                client_session::set_remote_host(host);
                start();
            }
            else{
                logger::print_log(error,0, STRINGIZE(__LINE__));
            }
        });
        logger::print_log("client started",LOG_LEVEL::INFO);
    }

private:

    void start()
    {
        auto session = std::make_shared<client_session>(ioc_, ctx_);

        acceptor_.async_accept(session->get_socket_in(), [this,session](const std::error_code& error){
            if(!error){
                session->start();
            }
            else{
                logger::print_log(error,0, STRINGIZE(__LINE__));
            }
            start();
        });
    }

    asio::ssl::context& ctx_;
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
};

#endif //NETWALKER_CLIENT_HPP
