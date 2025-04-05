/**
 * @file plumberd.hpp
 * @author ModelingEvolution
 * @brief Main include file for the cppplumberd library
 * @version 0.1.0
 * @date 2025-04-05
 *
 * @copyright Copyright (c) 2025 ModelingEvolution
 * MIT License
 */

#ifndef PLUMBERD_HPP
#define PLUMBERD_HPP

 // Include all components
#include "cppplumberd/utils.hpp"
#include "cppplumberd/calculator.hpp"

/**
 * @namespace cppplumberd
 * @brief Main namespace for the cppplumberd library
 */
namespace cppplumberd {

    /**
     * @brief Library version information
     */
    struct Version {
        static constexpr int major = 0;
        static constexpr int minor = 1;
        static constexpr int patch = 0;

        static constexpr const char* toString() {
            return "0.1.0";
        }
    };

    // Global utility functions

    /**
     * @brief Template function to add two values
     * @tparam T Type of the values
     * @param a First value
     * @param b Second value
     * @return Sum of the values
     */
    template<typename T>
    constexpr T add(const T& a, const T& b) {
        return a + b;
    }

    /**
     * @brief Template function to subtract two values
     * @tparam T Type of the values
     * @param a First value
     * @param b Second value
     * @return Difference of the values
     */
    template<typename T>
    constexpr T subtract(const T& a, const T& b) {
        return a - b;
    }

} // namespace cppplumberd

#endif // PLUMBERD_HPP