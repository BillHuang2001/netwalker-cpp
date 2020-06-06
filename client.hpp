#include <utility>


//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_CLIENT_HPP
#define NETWALKER_CLIENT_HPP

class client_session : public std::enable_shared_from_this<client_session>
{
public:
    explicit client_session(asio::io_context& ioc) : ws_(ioc), socket_in_(ioc), buffer_(64)
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
        asio::async_read(socket_in_, asio::dynamic_buffer(buffer_,3),
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

        socket_in_.async_receive(asio::buffer(buffer_),
                              [self=shared_from_this()](const std::error_code& error, size_t length)
                              {
                                  if(!error){
                                      if(self->buffer_[3]==3 && length!=self->buffer_[4]+7) std::cout <<"!debug::error::read not complete"<<std::endl;
                                      if(self->buffer_[0]==5 && self->buffer_[1]==1) {
                                          self->request_length_ = length;
                                          if(self->socket_out_status_){
                                              if(self->verbose_) std::cout <<"tcp handshake finish first"<<std::endl;
                                              self->socks_request_forward();
                                          }
                                          self->request_status_ = true;
                                      }
                                      else if(self->buffer_[1] == 3){
                                          std::cout <<"UDP protocol not supported"<<std::endl;
                                      }
                                  }
                                  else{
                                      logger::print_log(error,0,__POSITION__)
                                  }
                              }
        );
    }


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

    std::vector<char> buffer_;
    //beast::flat_buffer buffer_;
    beast::websocket::stream<beast::tcp_stream> ws_;
    tcp::socket socket_in_;
    static tcp::endpoint remote_host_;
};

tcp::endpoint client_session::remote_host_ = tcp::endpoint();

class netwalker_client
{
public:
    typedef std::shared_ptr<client_session> pointer;
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
