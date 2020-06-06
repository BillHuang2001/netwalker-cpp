#include <iostream>
#include <thread>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include "common.hpp"

#include "server.hpp"
#include "client.hpp"

using namespace boost;

int main() {
    std::cout << "Welcome to netwalker" << std::endl;

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
            std::cout <<"unexpected stop"<<std::endl;
            ioc.stop();
        });

        netwalker_server server(ioc, 1234);
        netwalker_client client(ioc, "localhost");
        std::cout << "test!" <<std::endl;
        // Run the I/O service on the requested number of threads
        std::vector<std::thread> threads_list;
        int num_threads = 1;
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
