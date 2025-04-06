# Find required packages
find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Find nngpp package
find_package(nngpp CONFIG REQUIRED)



# Add test executables
add_executable(transport_tests transport_tests.cpp "ngg.cpp")
target_link_libraries(transport_tests PRIVATE 
  ${PROJECT_NAME}
  gtest
  gtest_main
)

# Add include directories
target_include_directories(transport_tests PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Register test
add_test(NAME transport_tests COMMAND transport_tests)