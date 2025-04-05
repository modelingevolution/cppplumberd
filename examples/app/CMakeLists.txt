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



# Protocol Buffers integration
find_package(Protobuf REQUIRED)
set(PROTO_FILES
    proto/messages.proto
)



# Make sure output directories exist for protobuf-generated files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)

# Generate protobuf files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Display protobuf output locations for debugging
message(STATUS "Proto sources: ${PROTO_SRCS}")
message(STATUS "Proto headers: ${PROTO_HDRS}")
# Install the example if requested
if(INSTALL_EXAMPLES)
  install(TARGETS client
    RUNTIME DESTINATION bin/examples
  )
endif()