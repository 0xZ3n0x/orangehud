/* Embeds the ShellUI module (shellui.elf) into the daemon image. The CMake
 * target passes the real path in ORANGEHUD_SHELLUI_ELF. */
#ifndef ORANGEHUD_SHELLUI_ELF
#error "ORANGEHUD_SHELLUI_ELF is set by the CMake daemon target"
#endif

__asm__(".global shellui_elf_start\n"
        ".type   shellui_elf_start, @object\n"
        ".align  16\n"
        "shellui_elf_start:\n"
        ".incbin \"" ORANGEHUD_SHELLUI_ELF "\"\n"
        "shellui_elf_end:\n"
        ".global shellui_elf_size\n"
        ".type   shellui_elf_size, @object\n"
        ".align  4\n"
        "shellui_elf_size:\n"
        ".int    shellui_elf_end - shellui_elf_start\n");
