cmake_minimum_required(VERSION 3.14)

# Ensure static linking
set(Protobuf_USE_STATIC_LIBS ON)
set(protobuf_BUILD_SHARED_LIBS OFF)

# Find Protocol Buffers
find_package(Protobuf REQUIRED)
message(STATUS "Found Protobuf: ${Protobuf_VERSION}")
message(STATUS "Protobuf include directory: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}")
message(STATUS "Protobuf is static: ${Protobuf_USE_STATIC_LIBS}")

# Define the proto files
set(PROTO_FILES
    cqrs.proto
)

# Create the directory where we will copy generated headers
set(PROTO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/cppplumberd/proto")
file(MAKE_DIRECTORY ${PROTO_INCLUDE_DIR})

# Generate protobuf files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Print the generated files for debugging
message(STATUS "Proto source files: ${PROTO_SRCS}")
message(STATUS "Proto header files: ${PROTO_HDRS}")

# Create static library
add_library(cppplumberd-messages STATIC ${PROTO_SRCS} ${PROTO_HDRS})

# Set the library properties - ensure static compilation
set_target_properties(cppplumberd-messages PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    STATIC_LIBRARY TRUE
    # Make sure the output name matches what's expected
    OUTPUT_NAME "cppplumberd-messages"
)

# Link against protobuf static libraries
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
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HEADER}
                "${PROTO_INCLUDE_DIR}/${HEADER_FILENAME}"
        COMMENT "Copying ${HEADER_FILENAME} to include directory"
    )
endforeach()

# Add include directory for generated files 
target_include_directories(cppplumberd-messages
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

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