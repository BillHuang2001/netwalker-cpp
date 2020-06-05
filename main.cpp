#include <iostream>
#include <thread>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include "common.hpp"

#include "server.hpp"
#include "client.hpp"

using namespace boost;

int main() {
    std::cout << "Hello, World!" << std::endl;

#ifdef SINGLE_THREAD
    asio::io_context ioc(1);
    ioc.run();
#else
    asio::io_context ioc;

    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](boost::system::error_code const&, int){
        // Stop the io_context. This will cause run()
        // to return immediately, eventually destroying the
        // io_context and any remaining handlers in it.
        ioc.stop();
    });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> threads_list;
    int num_threads = 4;
    threads_list.reserve(num_threads - 1);
    for(auto i = num_threads - 1; i > 0; i--) {
        threads_list.emplace_back(
            [&ioc]() {
                ioc.run();
            }
        );
    }
    ioc.run();

    for(auto& walker : threads_list)
        walker.join();
#endif


    return 0;
}
