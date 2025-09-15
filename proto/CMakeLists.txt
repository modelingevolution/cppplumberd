cmake_minimum_required(VERSION 3.14)

# On Linux use static, on Windows use dynamic to match runtime library
if(NOT WIN32)
    set(Protobuf_USE_STATIC_LIBS ON)
    set(protobuf_BUILD_SHARED_LIBS OFF)
else()
    set(Protobuf_USE_STATIC_LIBS OFF)
    set(protobuf_BUILD_SHARED_LIBS ON)
endif()

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
# On Windows, ensure we use the vcpkg protoc
if(WIN32 AND EXISTS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/protobuf/protoc.exe")
    set(PROTOBUF_PROTOC_EXECUTABLE "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/protobuf/protoc.exe")
    message(STATUS "Using vcpkg protoc: ${PROTOBUF_PROTOC_EXECUTABLE}")
endif()

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# On Windows, ensure generated files use dynamic runtime
if(MSVC)
    set_source_files_properties(${PROTO_SRCS} PROPERTIES
        COMPILE_FLAGS "/MDd"
    )
endif()

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

# On Windows, ensure we use the same runtime library as the rest of the project
if(MSVC)
    set_target_properties(cppplumberd-messages PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

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
    # Remove any double slashes from the path
    string(REPLACE "//" "/" HEADER_CLEAN "${HEADER}")
    add_custom_command(
        TARGET cppplumberd-messages
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HEADER_CLEAN}"
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