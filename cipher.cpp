//
// Created by bill on 6/6/20.
//

#include "cipher.h"
#include <random>
#include <vector>
#include <climits>
#include <iostream>

void cipher::calc_to(std::vector<unsigned char> &from, size_t len, unsigned char* to)
{
    for(unsigned int i=0; i<len; i++){
        to[i] = from[i] ^ dist(generator);
    }
}

void cipher::calc(std::vector<unsigned char> &arr, size_t len)
{
    for(unsigned int i=0; i<len; i++){
        arr[i] = arr[i] ^ dist(generator);
    }
}

void cipher::calc(std::string& str, size_t len)
{
    for(unsigned int i=0; i<len; i++){
        str[i] = str[i] ^ dist(generator);
    }
}

void cipher::calc(beast::flat_buffer& data)
{
    auto str = (unsigned char*)data.data().data();
    for(unsigned int i=0; i<data.data().size(); i++){
        str[i] = str[i] ^ dist(generator);
    }
}

//bool cipher::time_test(const std::string& str, const time_t& now, const int& time_diff)
//{
//    time_t time=0;
//
//    for(unsigned int i=0; i<str.size();i++){
//        if(i < str.size()-4){
//            generator();
//        }
//        else{
//            time <<=8;
//            time += (unsigned char)str[i]^dist(generator);
//        }
//    }
//
//    return std::abs(time - now) == time_diff;
//}

cipher::cipher(unsigned long long passwd)
{
    generator.seed(passwd);
    dist = std::uniform_int_distribution<unsigned char>(0,255);
}

void cipher::set_seed(unsigned long long seed)
{
    generator.seed(seed);
}

unsigned char cipher::next_num()
{
    return dist(generator);
}

xorshift128p::result_type xorshift128p::operator() ()
{
    unsigned long long t = a;
    unsigned long long const s = b;
    a  = s;
    t ^= t<<23;
    t ^= t>>17;
    t ^= s ^ (s>>26);
    b  = t;
    return t+s;
}

xorshift128p::result_type xorshift128p::max()
{
    return ULLONG_MAX;
}

xorshift128p::result_type xorshift128p::min()
{
    return (xorshift128p::result_type)0;
}

void xorshift128p::seed(unsigned long long seed)
{
    a=seed;
    b=~seed;

    xorshift128p();
}
