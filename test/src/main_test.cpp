/// @file
///
/// @brief Loads up catch main.
///

// Let Catch provide main():
#define CATCH_CONFIG_MAIN

#include <iostream>
#include <iomanip>

#include "../lib/catch.hpp"

TEST_CASE("1: All test cases reside in other .cpp files (empty)", 
	  "[multi-file:1]") {
    std::cout << std::scientific << std::setprecision(12) << std::endl;
}
