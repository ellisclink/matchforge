// BoostRedisSource.cpp: compiles the Boost.Redis implementation into this library.
//
// Boost.Redis is "separately compiled": most of its code lives in .cpp-style source
// that must be built in exactly one translation unit. This file is that unit. Without
// it, the linker reports undefined references to boost::redis::connection and similar.
#include <boost/redis/src.hpp>
