#ifndef CPPPLUMBERD_CALCULATOR_HPP
#define CPPPLUMBERD_CALCULATOR_HPP

#include <concepts>

namespace cppplumberd {

    // Example class with C++20 concepts
    class Calculator {
    public:
        // Platform-independent calculation methods with concepts
        template<typename T>
            requires std::is_arithmetic_v<T>
        static T multiply(T a, T b) {
            return a * b;
        }

        template<typename T>
            requires std::is_arithmetic_v<T>
        static T divide(T a, T b) {
            return a / b;
        }

        // C++23 feature - if consteval
        constexpr static int compileTimeSquare(int x) {
            if consteval {
                return x * x; // Compile-time execution
            }
            else {
                return x * x; // Runtime execution
            }
        }
    };

} // namespace cppplumberd

#endif // CPPPLUMBERD_CALCULATOR_HPP