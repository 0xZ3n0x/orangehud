set(ORANGE_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(ORANGE_LIB_WARNINGS -Wall -Wextra -Werror)
set(ORANGE_ELF_WARNINGS -Wall -Werror)
set(ORANGE_ELF_CXX_WARNINGS -pedantic -pedantic-errors)

# orange_library(<name> <dir> [CONSOLE <regex>]): every .c/.cpp under <dir>; files whose path relative to <dir> matches CONSOLE are left out of the host build.
function(orange_library name dir)
	cmake_parse_arguments(ARG "" "CONSOLE" "" ${ARGN})
	cmake_path(ABSOLUTE_PATH dir NORMALIZE OUTPUT_VARIABLE _dir)
	cmake_path(GET _dir PARENT_PATH _root)
	file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${_dir}/*.c" "${_dir}/*.cpp")
	if(ORANGEHUD_BUILD_TESTS AND ARG_CONSOLE)
		foreach(_file ${_sources})
			file(RELATIVE_PATH _rel "${_dir}" "${_file}")
			if(_rel MATCHES "${ARG_CONSOLE}")
				list(REMOVE_ITEM _sources "${_file}")
			endif()
		endforeach()
	endif()
	add_library(${name} STATIC ${_sources})
	target_include_directories(${name} PUBLIC "${_root}")
	if(ORANGEHUD_BUILD_TESTS)
		target_compile_options(${name} PRIVATE -Wall -Wextra -g -O0)
	else()
		target_compile_options(${name} PRIVATE ${ORANGE_TARGET_FLAGS} -march=znver2 -fPIC -fstack-protector-all "$<$<COMPILE_LANGUAGE:C>:-g0>" -O3 "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>" ${ORANGE_LIB_WARNINGS})
	endif()
endfunction()

# orange_elf(<target> <output-name> <extra-c-flags> <sources...>)
function(orange_elf name output c_flags)
	add_executable(${name} ${ARGN})
	set_target_properties(${name} PROPERTIES OUTPUT_NAME "${output}" RUNTIME_OUTPUT_DIRECTORY "${ORANGE_BIN_DIR}")
	target_compile_options(${name} PRIVATE ${ORANGE_TARGET_FLAGS} ${c_flags} -fPIC -fPIE -march=znver2 ${ORANGE_ELF_WARNINGS} -fstack-protector-all -ffunction-sections -fdata-sections "$<$<COMPILE_LANGUAGE:CXX>:-gfull;-gdwarf-2;${ORANGE_ELF_CXX_WARNINGS}>")
	target_link_options(${name} PRIVATE -Wl,--gc-sections -Wl,--icf=safe)
	add_custom_command(TARGET ${name} POST_BUILD
		COMMAND ${CMAKE_OBJCOPY}
			--remove-section .debug_info --remove-section .debug_abbrev
			--remove-section .debug_line --remove-section .debug_str
			--remove-section .debug_loc --remove-section .debug_aranges
			--remove-section .debug_ranges --remove-section .debug_pubnames
			--remove-section .debug_pubtypes --remove-section .debug_frame
			--strip-unneeded $<TARGET_FILE:${name}>
		COMMENT "Stripping debugging information from ${name}")
endfunction()
