add_executable(client client.cpp)
target_link_libraries(client PRIVATE ${PROJECT_NAME})

# Set output directory
set_target_properties(client PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE ${PROJECT_NAME})

set_target_properties(server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Find and link NNG library
find_package(nng CONFIG REQUIRED)
target_link_libraries(client PRIVATE nng::nng)
target_link_libraries(server PRIVATE nng::nng)

# Protocol Buffers integration
find_package(Protobuf REQUIRED)
set(PROTO_FILES
    proto/messages.proto
)

# Make sure output directories exist for protobuf-generated files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)

# Generate protobuf files - pass the PROTO_FILES argument
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Display protobuf output locations for debugging
message(STATUS "Proto sources: ${PROTO_SRCS}")
message(STATUS "Proto headers: ${PROTO_HDRS}")

# Add the generated files to the executables
add_library(proto_messages STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(proto_messages PUBLIC protobuf::libprotobuf)
target_include_directories(proto_messages PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Link the proto library to client and server
target_link_libraries(client PRIVATE proto_messages)
target_link_libraries(server PRIVATE proto_messages)

# Add the current binary directory to the include path of both executables
target_include_directories(server PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Install the example if requested
if(INSTALL_EXAMPLES)
  install(TARGETS client
    RUNTIME DESTINATION bin/examples
  )
endif()