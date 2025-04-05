#ifndef CPPPLUMBERD_UTILS_HPP
#define CPPPLUMBERD_UTILS_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <concepts>
#include <ranges>

namespace cppplumberd {

    // Example class with C++20 features
    class Utils {
    public:
        // Example function that works on all target platforms
        static std::string getUpperCase(const std::string& input) {
            std::string result = input;
            std::ranges::transform(result, result.begin(),
                [](unsigned char c) { return std::toupper(c); });
            return result;
        }

        // Example vector utility with C++20 ranges
        template<typename T>
        static std::vector<T> reverseVector(const std::vector<T>& input) {
            std::vector<T> result(input.size());
            std::ranges::copy(input | std::views::reverse, result.begin());
            return result;
        }

        // C++20 concepts example
        template<std::integral T>
        static T add(T a, T b) {
            return a + b;
        }

        // C++20 constexpr improvements
        static constexpr auto square(auto x) {
            return x * x;
        }
    };

} // namespace cppplumberd

#endif // CPPPLUMBERD_UTILS_HPP