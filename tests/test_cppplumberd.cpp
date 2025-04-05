#include <gtest/gtest.h>
#include "plumberd.hpp"
#include <string>
#include <vector>
#include <concepts>

// Test template add function
TEST(CppPlumberdTest, AddFunction) {
    EXPECT_EQ(cppplumberd::add(3, 4), 7);
    EXPECT_FLOAT_EQ(cppplumberd::add(3.5f, 2.5f), 6.0f);
}

// Test Utils constexpr square function
TEST(CppPlumberdTest, SquareFunction) {
    EXPECT_EQ(cppplumberd::Utils::square(5), 25);
    EXPECT_EQ(cppplumberd::Utils::square(-3), 9);

    // Test with floating point
    EXPECT_FLOAT_EQ(cppplumberd::Utils::square(2.5f), 6.25f);
}

// Test Calculator class
TEST(CppPlumberdTest, CalculatorClass) {
    EXPECT_EQ(cppplumberd::Calculator::multiply(6, 7), 42);
    EXPECT_FLOAT_EQ(cppplumberd::Calculator::divide(10.0, 2.0), 5.0);

    // Test compile-time square
    constexpr auto result = cppplumberd::Calculator::compileTimeSquare(3);
    EXPECT_EQ(result, 9);
}

// Test Utils class
TEST(CppPlumberdTest, UtilsClass) {
    // Test uppercase
    std::string input = "Hello, World!";
    EXPECT_EQ(cppplumberd::Utils::getUpperCase(input), "HELLO, WORLD!");

    // Test vector reverse
    std::vector<int> numbers = { 1, 2, 3, 4, 5 };
    std::vector<int> expected = { 5, 4, 3, 2, 1 };
    EXPECT_EQ(cppplumberd::Utils::reverseVector(numbers), expected);

    // Test concepts-enabled add function
    EXPECT_EQ(cppplumberd::Utils::add(10, 20), 30);
}

// Test Version information
TEST(CppPlumberdTest, VersionInfo) {
    EXPECT_EQ(cppplumberd::Version::major, 0);
    EXPECT_EQ(cppplumberd::Version::minor, 1);
    EXPECT_EQ(cppplumberd::Version::patch, 0);
    EXPECT_STREQ(cppplumberd::Version::toString(), "0.1.0");
}