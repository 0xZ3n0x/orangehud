orange_library(orange_platform platform)

orange_library(hde64 ${ORANGE_THIRD_PARTY}/hde64)

orange_library(orange_detour detour CONSOLE "^detour\\.cpp$")
target_link_libraries(orange_detour PUBLIC orange_platform hde64)

if(NOT ORANGEHUD_BUILD_TESTS)
	orange_library(orange_proc proc)
	target_link_libraries(orange_proc PUBLIC orange_platform)
endif()
