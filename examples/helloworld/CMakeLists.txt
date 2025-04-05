add_executable(helloworld main.cpp)
target_link_libraries(helloworld PRIVATE ${PROJECT_NAME})

# Set output directory
set_target_properties(helloworld PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install the example if requested
if(INSTALL_EXAMPLES)
  install(TARGETS helloworld
    RUNTIME DESTINATION bin/examples
  )
endif()