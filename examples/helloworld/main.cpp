#include <iostream>
#include <vector>
#include <array>
#include <string_view>
#include "plumberd.hpp"

int main() {
    // Use template function
    std::cout << "Using add<int>(3, 4): " << cppplumberd::add<int>(3, 4) << std::endl;
    std::cout << "Using add<double>(3.5, 4.2): " << cppplumberd::add<double>(3.5, 4.2) << std::endl;

    // Using C++20 concepts
    std::cout << "Using Utils::add(10, 20): " << cppplumberd::Utils::add(10, 20) << std::endl;

    // Use constexpr function
    std::cout << "Using Utils::square(5): " << cppplumberd::Utils::square(5) << std::endl;

    // Use Calculator class with concepts
    std::cout << "Using Calculator::multiply(6, 7): " << cppplumberd::Calculator::multiply(6, 7) << std::endl;
    std::cout << "Using Calculator::divide(10.0, 2.0): " << cppplumberd::Calculator::divide(10.0, 2.0) << std::endl;

    // Use C++23 feature
    constexpr auto compiletime_result = cppplumberd::Calculator::compileTimeSquare(4);
    std::cout << "Compile-time square: " << compiletime_result << std::endl;

    // Use Utils class
    std::string text = "Hello, World!";
    std::cout << "Original: " << text << std::endl;
    std::cout << "Uppercase: " << cppplumberd::Utils::getUpperCase(text) << std::endl;

    // Use vector utility with C++20 ranges
    std::vector<int> numbers = { 1, 2, 3, 4, 5 };
    std::cout << "Original vector: ";
    for (const auto& num : numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    auto reversed = cppplumberd::Utils::reverseVector(numbers);
    std::cout << "Reversed vector: ";
    for (const auto& num : reversed) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    // Library version
    std::cout << "cppplumberd version: " << cppplumberd::Version::toString() << std::endl;

    return 0;
}