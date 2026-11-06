find_package(Threads REQUIRED)

file(GLOB_RECURSE ORANGE_TEST_SOURCES CONFIGURE_DEPENDS *.c *.cpp)
add_executable(orange-host-tests ${ORANGE_SRC}/shellui/boot/hook_lifecycle.cpp ${ORANGE_TEST_SOURCES})
set(HOST_LIBS orange_platform orange_settings orange_detour orange_fps)
target_link_libraries(orange-host-tests PRIVATE ${HOST_LIBS} Threads::Threads)

foreach(_t orange-host-tests hde64 ${HOST_LIBS})
	target_compile_definitions(${_t} PRIVATE ORANGE_HOST_TEST=1 _DEFAULT_SOURCE ORANGE_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}" ORANGE_DATA_ROOT="${CMAKE_CURRENT_BINARY_DIR}/runtime")
endforeach()

target_compile_options(orange-host-tests PRIVATE -Wall -Wextra -g -O0)

target_include_directories(orange-host-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${ORANGE_SRC}/daemon ${ORANGE_SRC}/shellui)

add_test(NAME host-tests COMMAND orange-host-tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties(host-tests PROPERTIES ENVIRONMENT "ORANGE_TEST_ROOT=${CMAKE_CURRENT_SOURCE_DIR}")

