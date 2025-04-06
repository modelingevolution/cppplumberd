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
add_executable(transport_tests 
  transport_tests.cpp 
  "ngg.cpp"
)

target_link_libraries(transport_tests PRIVATE 
  ${PROJECT_NAME}
  cppplumberd-messages
  gtest
  gtest_main
  gmock
)

# ProtoPublishHandler test executable
add_executable(proto_publish_handler_tests
  proto_publish_handler_tests.cpp
)

# Link against app_proto_messages to get access to app-specific messages
target_link_libraries(proto_publish_handler_tests PRIVATE
  ${PROJECT_NAME}
  cppplumberd-messages
  app_proto_messages
  gtest
  gtest_main
  gmock
)

# Add include directories
target_include_directories(transport_tests PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR}/../ # For generated protobuf headers
)

target_include_directories(proto_publish_handler_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_BINARY_DIR}/../ # For generated protobuf headers
  ${CMAKE_SOURCE_DIR} # To access examples/app dir
  ${CMAKE_BINARY_DIR} # For generated app protobuf headers
)

# Register tests
add_test(NAME transport_tests COMMAND transport_tests)
add_test(NAME proto_publish_handler_tests COMMAND proto_publish_handler_tests)