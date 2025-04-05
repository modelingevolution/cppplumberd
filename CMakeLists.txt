cmake_minimum_required(VERSION 3.14)
project(cppplumberd VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Boost components
set(BOOST_MIN_VERSION "1.87.0")
# Try to find Boost using BOOST_ROOT environment variable
find_package(Boost ${BOOST_MIN_VERSION} REQUIRED)

if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost ${BOOST_MIN_VERSION} not found. Please set BOOST_ROOT environment variable.")
endif()

message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")

# Define the header-only library target
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>
  $<INSTALL_INTERFACE:include>
)


target_link_libraries(${PROJECT_NAME} INTERFACE
  
)

# Installation
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets)
install(DIRECTORY include/ DESTINATION include)

# Export targets
install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION lib/cmake/${PROJECT_NAME}
)

# Create config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
  cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
)

# Create version file
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION lib/cmake/${PROJECT_NAME}
)

# Add test directory if tests are enabled
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  #add_subdirectory(tests)
endif()

# Create an example executable that uses the library
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()