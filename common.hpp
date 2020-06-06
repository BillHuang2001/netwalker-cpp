//
// Created by bill on 6/5/20.
//

#ifndef NETWALKER_COMMON_HPP
#define NETWALKER_COMMON_HPP


#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include "log.hpp"

using namespace boost;
using tcp = asio::ip::tcp;

typedef unsigned short u16;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)
#define __POSITION__ (" in: " __FILE__ " line: " STRINGIZE(__LINE__) )

#endif //NETWALKER_COMMON_HPP
