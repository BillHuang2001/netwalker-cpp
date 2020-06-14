#include <iostream>
#include <thread>
#include "common.hpp"
#include <unistd.h>
#include "server.hpp"
#include "client.hpp"

using namespace boost;

int main(int argc, char* argv[]) {
    std::srand(std::time(0));
    std::cout <<"Welcome to netwalker"<<std::endl;
    std::cout <<"Version 1.0.0"<<std::endl;
    if(argc < 3){
        std::cout <<"Usage: NetWalker {server/client} [password] [options]\n";
        std::cout <<"-v verbose level           default: warning(0)\n";
        std::cout <<"-l local listen port       default: 6666\n";
        std::cout <<"-s server address          default: none\n";
        std::cout <<"-p path                    default: /\n";
        std::cout <<"-x server port             default: 443"<<std::endl;
        return 0;
    }

    int opt;
    std::string address,path;
    unsigned short listen_port, verbose_level, server_port=443;
    u64 passwd=0;
    for(unsigned int i=0;i<strlen(argv[2]);i++){
        passwd *= 107;
        passwd += argv[2][i];
    }
    std::cout <<"GOOD"<<std::endl;
    while((opt=getopt(argc-2,argv+2,"v:l:s:p:x:"))!=-1){
        switch(opt)
        {
            case 'v'://verbose
                verbose_level = (unsigned short) std::strtoul(optarg, nullptr, 0);
                if(verbose_level > 3) verbose_level = 3;
                break;
            case 'l'://listen port
                listen_port = (unsigned short) std::strtoul(optarg, nullptr, 0);
                break;
            case 's'://server address
                address = optarg;
                break;
            case 'p'://path
                path = optarg;
                break;
            case 'x':
                server_port = (unsigned short) std::strtoul(optarg, nullptr, 0);
                break;
            default:
                //do nothing
                break;
        }
    }

    logger::set_output_level(static_cast<const char>(verbose_level));

#ifdef SINGLE_THREAD
    asio::io_context ioc(1);
    ioc.run();
#else
    asio::io_context ioc;
    try {
        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const std::error_code& error, int) {
            // Stop the io_context. This will cause run()
            // to return immediately, eventually destroying the
            // io_context and any remaining handlers in it.
            if(error){
                logger::print_log(error,0,__POSITION__);
            }
            std::cout <<"Stop signal."<<std::endl;
            ioc.stop();
        });

        if(!strcmp(argv[1], "server")) {
            std::cout <<"Server! "<<listen_port<<" "<<passwd;
            new netwalker_server(ioc, listen_port, passwd);
        }
        else if(!strcmp(argv[1], "client")) {
            new netwalker_client(ioc, address, path, listen_port, server_port, passwd);
        }
        else{
            logger::print_log("Unknown args",LOG_LEVEL::ERROR);
        }

        // Run the I/O service on the requested number of threads
        std::vector<std::thread> threads_list;
        u32 num_threads = 1;
        threads_list.reserve(num_threads - 1);
        for (auto i = num_threads - 1; i > 0; i--) {
            threads_list.emplace_back( [&ioc]() {
                ioc.run();
            });
        }
        ioc.run();

        for (auto &walker : threads_list)
            walker.join();
    }
    catch(std::exception& e){
        e.what();
    }
#endif
    return 0;
}
