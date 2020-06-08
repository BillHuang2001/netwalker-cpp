//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_CLIENT_HPP
#define NETWALKER_CLIENT_HPP

#include "common.hpp"
#include "cipher.h"

class client_session : public std::enable_shared_from_this<client_session>
{
public:
    explicit client_session(asio::io_context& ioc) :
    buffer_(256),encrypt_(passwd_),decrypt_(passwd_),ws_(ioc), socket_in_(ioc),is_ready_(false)
    {
    }

    static void set_password(const u64& passwd){
        passwd_ = passwd;
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

        read_socks5_header();
    }

    tcp::socket& get_socket_in(){
        return socket_in_;
    }

private:
    void close_all(){
        socket_in_.close();
        ws_.async_close(beast::websocket::close_code::none,[self=shared_from_this()](const std::error_code&){});
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
                        std::cout <<"Unknown protocol or authentication method not supported, header is"<<(int)self->buffer_[0]<<" "<<(int)self->buffer_[1]<<" "<<(int)self->buffer_[2]<<std::endl;
                    }
                }
                else {
                    logger::print_log(error,0,__POSITION__);
                }
            }
        );
    }

    void send_socks5_reply(){
        buffer_[1]=0;
        asio::async_write(socket_in_, asio::buffer(buffer_,2),
            [self=shared_from_this()](const std::error_code& error, size_t length)
            {
              if(!error){
                  self->read_socks5_request();
              }
              else{
                  std::cout <<"In client -> send_reply: "<< error <<' '<< error.message() <<std::endl;
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
                                      logger::print_log(error,0,__POSITION__);
                                  }
                              }
        );
    }

    void socks_request_forward()
    {
        logger::print_log("forwarding socks5 request",LOG_LEVEL::DEBUG);
//            asio::socket_base::keep_alive option;
//            socket_out_.get_option(option);
//            bool is_set = option.value();
//            std::cout <<"keep alive is "<< (is_set ? "true":"false") <<std::endl;
        /*
                C->S : TOTAL_LENGTH = len -3 +4 +NUM_RANDOM_DIGITS +1
                SystemTime  Time_Salt   LEN_AFTER     ATYP    DST.ADDR  DST.PORT
                4           1           1             1       Var       2

                S-C :
                SystemTime    STATUS
                4             1

                STATUS: 1-good 0-bad
        */
        u32 length = buffer_[4]+7;
        auto time_salt = static_cast<unsigned char>(rand() % 256);
        u32 total_length = length +3;
        std::vector<unsigned char> message(total_length);

        time_t now = std::time(nullptr);
        time_t now_bak = now;
        //filling system time
        for(int i=3; i>=0 ;i--){
            //std::cout <<"CHECK POINT 1.5 i:"<<i<<std::endl;
            message[i] = (unsigned char)(now & 255);
            now >>=8;
        }

        //filling len_after
        message[5] = static_cast<unsigned char>(total_length - 6);

        //filling socks5 request
        for(u32 i=6; i < total_length; i++){
            message[i] = static_cast<unsigned char>(buffer_[i - 3]);
        }

        encrypt_.set_seed((u64)now_bak + passwd_ ^ (u64)time_salt << 8u);
        decrypt_.set_seed((u64)now_bak + passwd_ ^ (u64)time_salt << 8u);

        encrypt_.calc(message, total_length);

        //filling time salt
        message[4] = time_salt; //time_salt is plaintext
        logger::print_log("sending request",LOG_LEVEL::DEBUG);
        //ws_.async_write(asio::buffer("Hello World from client"),
        ws_.async_write(asio::buffer(message, total_length),
                [self=shared_from_this()](const std::error_code& error, size_t length){
                if(!error) {
                    self->read_from_socket_in();
                    self->read_from_socket_out();
                }
                else{
                    logger::print_log(error,0,__POSITION__);
                    self->handle_unsuccessful_connection();
                }
            }
        );
    }

    void do_ws_handshake(){
        ws_.async_handshake("localhost","/",[self=shared_from_this()](const std::error_code& error){
            if(!error){
                self->handle_successful_connection();
            }
            else{
                logger::print_log(error,0,__POSITION__);
            }
        });
    }

    void handle_unsuccessful_connection(){
        close_all();
    }

    void handle_successful_connection(){
        if(is_ready_){
            socks_request_forward();
        }
        else{
            is_ready_ = true;
        }
    }

    void read_from_socket_in(){
        socket_in_.async_read_some(asio::buffer(buffer_), [self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error){
                self->write_to_socket_out();
            }
            else{
                self->close_all();
            }
        });
    }

    void write_to_socket_out(){
        ws_.async_write(asio::buffer(buffer_), [self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error){
                self->read_from_socket_in();
            }
            else{
                self->close_all();
            }
        });
    }

    void read_from_socket_out(){
        ws_.async_read(flat_buffer_, [self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error){
                self->write_to_socket_in();
            }
            else{
                self->close_all();
            }
        });
    }

    void write_to_socket_in(){
        asio::async_write(socket_in_, flat_buffer_.data(), [self=shared_from_this()](const std::error_code& error, size_t length){
            if(!error){
                self->read_from_socket_out();
            }
            else{
                self->close_all();
            }
        });
    }


    void send_handshake(){
        ws_.async_write(asio::buffer("hello world, using websocket!"), [self=shared_from_this()](const std::error_code& error, size_t length){
            std::cout << "message sent" <<std::endl;
        });
    }

    beast:: flat_buffer flat_buffer_;
    std::vector<unsigned char> buffer_;
    cipher encrypt_,decrypt_;
    beast::websocket::stream<beast::tcp_stream> ws_;
    tcp::socket socket_in_;
    static tcp::endpoint remote_host_;
    static u64 passwd_;
    size_t sock5_request_length;
    std::atomic<bool> is_ready_{};
};

tcp::endpoint client_session::remote_host_ = tcp::endpoint();
u64 client_session::passwd_ = 0;

class netwalker_client
{
public:
    netwalker_client(asio::io_context& ioc, const std::string& domain_name, u16 listen_port) : ioc_(ioc), acceptor_(ioc,tcp::endpoint(tcp::v6(),listen_port))
    {
        auto resolver = make_shared<tcp::resolver>(ioc);
        resolver->async_resolve(domain_name, "", [this, resolver](const std::error_code& error, tcp::resolver::results_type results){
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

        acceptor_.async_accept(session->get_socket_in(), [session](const std::error_code& error){
            if(!error){
                session->start();
            }
            else{
                logger::print_log(error,0);
            }
        });
    }

    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
};

#endif //NETWALKER_CLIENT_HPP
