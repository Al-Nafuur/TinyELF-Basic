/*
 * Linker script for the relocatable TinyELF link (gcc -r ... -Wl,-T,tinyelf.ld).
 *
 * Puts every zero-initialised variable into .data, so the ELF has no .bss at
 * all. The UCA ELF loader places .bss in the CCM; without it the whole 64 KiB
 * CCM stays free and ballocmem() in main.c uses it as BASIC memory. It also
 * leaves the CCM to the loader for the relocation/symbol tail of a file that
 * is larger than its 96 KiB buffer.
 *
 * Only .data is placed here; all other sections keep their default (orphan)
 * placement. The explicit address 0 keeps .data section-relative like every
 * other section of a relocatable file.
 */
SECTIONS
{
  .data 0 : { *(.data .data.*) *(.bss .bss.* COMMON) }
}
