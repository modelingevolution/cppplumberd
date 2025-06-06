# Find required packages
set(gtest_force_shared_crt OFF)  # Make sure GTest uses static runtime
find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Find dependencies with static linking
set(nngpp_STATIC ON)
set(Protobuf_USE_STATIC_LIBS ON)
find_package(nngpp CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# Set up directories for generated protobuf files
set(TESTS_PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
file(MAKE_DIRECTORY ${TESTS_PROTO_OUTPUT_DIR})

# Test-specific proto files
set(TEST_PROTO_FILES
    proto/test_msgs.proto
)

# Generate protobuf files for tests
protobuf_generate_cpp(TEST_PROTO_SRCS TEST_PROTO_HDRS ${TEST_PROTO_FILES})

# Collect all test source files
set(TEST_SOURCES
    transport_tests.cpp
    "nng.cpp"
    proto_pub_sub_handlers_tests.cpp
    proto_req_rsp_handlers_tests.cpp
    "plumberd_command_flow_e2e.cpp" 
    "plumberd_event_flow_e2e.cpp")

# Single test executable - static linking
add_executable(cppplumberd_tests ${TEST_SOURCES} ${TEST_PROTO_SRCS} ${TEST_PROTO_HDRS})

# Set up library dependencies - all static
target_link_libraries(cppplumberd_tests PRIVATE
  ${PROJECT_NAME}
  cppplumberd-messages
  protobuf::libprotobuf
  gtest
  gtest_main
  gmock
)

if(MSVC)
  target_compile_options(cppplumberd_tests PRIVATE /MT$<$<CONFIG:Debug>:d>)
endif()

# Set up include directories
target_include_directories(cppplumberd_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
)

# Register the test
add_test(NAME cppplumberd_tests COMMAND cppplumberd_tests)

# Set output directory
set_target_properties(cppplumberd_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LINK_FLAGS_RELEASE "/INCREMENTAL:NO /OPT:REF /OPT:ICF"
)

# Visual feedback on test setup
message(STATUS "Test proto headers: ${TEST_PROTO_HDRS}")
message(STATUS "All tests consolidated into single executable: cppplumberd_tests")