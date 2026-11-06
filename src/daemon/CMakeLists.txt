orange_library(orange_settings settings)

orange_library(orange_fps fps CONSOLE "^sources/")
target_link_libraries(orange_fps PUBLIC orange_platform)

if(NOT ORANGEHUD_BUILD_TESTS)
	# The stager in injector.c is machine code copied into the target: these flags decide its bytes.
	add_library(orange_remote STATIC inject/remote/injector.c ${ORANGE_THIRD_PARTY}/NineS/pt.c ${ORANGE_THIRD_PARTY}/NineS/elfldr.c)
	set_target_properties(orange_remote PROPERTIES C_STANDARD 17)
	target_include_directories(orange_remote PRIVATE ${ORANGE_THIRD_PARTY}/NineS)
	set(POSIX_DEFINES -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112 -D__BSD_VISIBLE=1 -D__XSI_VISIBLE=500)
	target_compile_options(orange_remote PRIVATE ${ORANGE_TARGET_FLAGS} -march=znver2 ${POSIX_DEFINES} -fno-builtin -nostdlib -fPIC -gfull -O3 -Wall -Werror)
	target_link_libraries(orange_remote PRIVATE orange_platform orange_proc)

	file(GLOB_RECURSE DAEMON_SOURCES CONFIGURE_DEPENDS *.c *.cpp *.h *.hpp)
	list(FILTER DAEMON_SOURCES EXCLUDE REGEX "/daemon/(fps|settings)/|/daemon/inject/remote/")
	orange_elf(daemon "OrangeHUD.elf" "-Wno-keyword-macro" ${DAEMON_SOURCES})
	target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(daemon PUBLIC orange_settings orange_fps orange_proc orange_platform SceSystemService SceUserService orange_remote)
	# .incbin in payloads.c is invisible to the dependency scanner.
	set_source_files_properties(inject/payloads.c PROPERTIES OBJECT_DEPENDS "${ORANGE_BIN_DIR}/shellui.elf")
	target_compile_definitions(daemon PRIVATE ORANGEHUD_SHELLUI_ELF="${ORANGE_BIN_DIR}/shellui.elf")
	add_dependencies(daemon shellui)
endif()
