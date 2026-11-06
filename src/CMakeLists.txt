# The daemon embeds the other two ELFs, so it comes last.
add_subdirectory(shared)
add_subdirectory(shellui)
add_subdirectory(daemon)
