//
// Created by bill on 6/6/20.
//

#ifndef NETWALKER_CIPHER_H
#define NETWALKER_CIPHER_H

#include <vector>
#include <random>
#include <boost/beast.hpp>
using namespace boost;

struct xorshift128p
{
    typedef unsigned long long result_type;

    result_type a,b;
    result_type operator () ();

    void seed(unsigned long long seed);

    static result_type max();

    static result_type min();
};


class cipher
{
public:
    void calc(std::vector<unsigned char> &arr, size_t len);

    void calc(std::string& str, size_t len);

    void calc(beast::flat_buffer& data);

    void calc_to(std::vector<unsigned char> &from, size_t len, unsigned char* to);

    //bool time_test(const std::string& str, const time_t& now, const int& time_diff);

    cipher(unsigned long long passwd);

    void set_seed(unsigned long long seed);

    unsigned char next_num();

private:
    //std::default_random_engine generator;
    xorshift128p generator;
    std::uniform_int_distribution<unsigned char> dist;
};

#endif //NETWALKER_CIPHER_H
