# Find required packages
find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Add test executable
add_executable(library_tests test_cppplumberd.cpp)
target_link_libraries(library_tests PRIVATE ${PROJECT_NAME} gtest gtest_main)

# Register test
add_test(NAME library_tests COMMAND library_tests)