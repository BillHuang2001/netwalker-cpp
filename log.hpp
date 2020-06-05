//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_LOG_HPP
#define NETWALKER_LOG_HPP
#include <iostream>

enum LOG_LEVEL{
    ERROR=0,
    WARNING=1,
    INFO=2,
    DEBUG=3
};

class logger {
public:
    static void print_log(const std::error_code& error, const char& level){
        if(level == 0){
            std::cerr << error << " " << error.message() << std::endl;
        }
        else if (level <= output_level_){
            std::cout << error << " " << error.message() << std::endl;
        }
    }

    static void print_log(const std::string &message, const char& level) //0-error 1-warning 2-info debug
    {
        if(level == 0){
            std::cerr << message << std::endl;
        }
        else if (level <= output_level_){
            std::cout << message << "\n";
        }
    }

    static void set_output_level(const char& level){
        output_level_ = level;
    }

    static char output_level_;
};

char logger::output_level_ = LOG_LEVEL::DEBUG;

#endif //NETWALKER_LOG_HPP
