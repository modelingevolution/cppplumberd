# cppplumberd

A modern, cross-platform, header-only C++ library designed to work on WSL, Linux, x64, and arm64 architectures.

## Features

- Header-only library (no compilation required)
- Cross-platform compatibility (WSL, Linux, x64, arm64)
- Modern C++23 implementation
- CMake integration
- Example applications
- Unit tests with Google Test

## Requirements

- CMake 3.14 or higher
- C++23 compatible compiler (GCC 12+, Clang 14+, MSVC 19.34+)
- For testing: Google Test (automatically fetched if not found)

## Project Structure

```
cppplumberd/
├── CMakeLists.txt              # Main CMake configuration
├── cmake/
│   └── cppplumberdConfig.cmake.in  # CMake package configuration
├── include/
│   ├── plumberd.hpp           # Main include header
│   └── cppplumberd/           # Library header files
│       ├── utils.hpp
│       └── calculator.hpp
├── examples/
|   ├── CMakeLists.txt         # Example build configuration
│   └── helloword
│       ├── CMakeLists.txt         # Example build configuration
│       └── main.cpp               # Example application
└── tests/
    ├── CMakeLists.txt         # Tests build configuration
    └── test_cppplumberd.cpp   # Unit tests
```

## Building with Visual Studio

1. Open Visual Studio
2. Select "Open a local folder" and navigate to the project directory
3. Visual Studio should automatically detect the CMake project
4. Configure the CMake project by going to Project > CMake Settings
5. Choose your preferred configuration (Debug/Release) and platform target
6. Build the project from the Build menu

## Building on the Command Line

```bash
# Create a build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build the project
cmake --build .

# Run tests
ctest

# Install the library (optional)
cmake --install . --prefix /path/to/install
```

## Using the Library

To use the library in your own project with CMake:

```cmake
find_package(cppplumberd REQUIRED)
target_link_libraries(your_target PRIVATE cppplumberd::cppplumberd)
```

Or simply include the headers in your project:

```cpp
#include <plumberd.hpp>

int main() {
    auto result = cppplumberd::add(1, 2);
    // ...
}
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.