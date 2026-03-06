file(COPY setup.py calc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(python_module
    COMMENT "Building Python wheels for Calculator"
    DEPENDS c_api
    COMMAND ${Python3_EXECUTABLE} setup.py bdist_wheel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
