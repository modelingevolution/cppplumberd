cmake_minimum_required(VERSION 3.14)

# Find Protocol Buffers
find_package(Protobuf REQUIRED)

# Set output directories for library
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Define the proto files
set(PROTO_FILES
    cqrs.proto
)

# Create the directory where we will copy generated headers
set(PROTO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/cppplumberd/proto")
file(MAKE_DIRECTORY ${PROTO_INCLUDE_DIR})

# Generate protobuf files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create shared library
add_library(cppplumberd-messages SHARED ${PROTO_SRCS} ${PROTO_HDRS})

# Set the library properties
set_target_properties(cppplumberd-messages PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# Link against protobuf
target_link_libraries(cppplumberd-messages PUBLIC protobuf::libprotobuf)

# Add include directories
target_include_directories(cppplumberd-messages
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Copy generated headers to include directory
foreach(HEADER ${PROTO_HDRS})
    get_filename_component(HEADER_FILENAME ${HEADER} NAME)
    add_custom_command(
        TARGET cppplumberd-messages
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HEADER}
                "${PROTO_INCLUDE_DIR}/${HEADER_FILENAME}"
        COMMENT "Copying ${HEADER_FILENAME} to include directory"
    )
endforeach()

# Install the library
install(TARGETS cppplumberd-messages
    EXPORT cppplumberd-messagesTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install the generated headers
install(FILES ${PROTO_HDRS}
    DESTINATION include/cppplumberd/proto
)

# Export the targets
install(EXPORT cppplumberd-messagesTargets
    FILE cppplumberd-messagesTargets.cmake
    NAMESPACE cppplumberd::
    DESTINATION lib/cmake/cppplumberd-messages
)