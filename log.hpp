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

const char* log_type[4] = {"\033[31m[ERROR]\033[0m:", "\033[33m[WARN]\033[0m :", "[INFO]: ", "\033[34m[DEGUB]\033[0m:"};

class logger {
public:
    static void print_log(const std::error_code& error, const char& level, const std::string& additional_message=""){
        if(level <= output_level_){
            std::cout << log_type[level] << error << " " << error.message() << additional_message << std::endl;
        }
    }

    static void print_log(const std::string &message, const char& level, const std::string& additional_message="") //0-error 1-warning 2-info debug
    {
        if(level <= output_level_){
            std::cout << log_type[level] << message << additional_message << std::endl;
        }
    }

    static void set_output_level(const char& level){
        output_level_ = level;
    }

    static char output_level_;
};

char logger::output_level_ = LOG_LEVEL::WARNING;

#endif //NETWALKER_LOG_HPP
