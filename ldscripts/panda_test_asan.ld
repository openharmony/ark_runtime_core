/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
    Panda linker script for placement of sections above 4GB.
*/
ENTRY(_start)
SECTIONS {
    /*
    According to asan_mapping.h mem range lower than ~2G is safe for usage on all platforms.
    So memory map is set up for data in 0..1G and ELF in 1..2G.

    Typical shadow mapping on Linux/x86_64 with SHADOW_OFFSET == 0x00007fff8000:
    || `[0x10007fff8000, 0x7fffffffffff]` || HighMem    ||
    || `[0x02008fff7000, 0x10007fff7fff]` || HighShadow ||
    || `[0x00008fff7000, 0x02008fff6fff]` || ShadowGap  ||
    || `[0x00007fff8000, 0x00008fff6fff]` || LowShadow  ||
    || `[0x000000000000, 0x00007fff7fff]` || LowMem     ||

    When SHADOW_OFFSET is zero (-pie):
    || `[0x100000000000, 0x7fffffffffff]` || HighMem    ||
    || `[0x020000000000, 0x0fffffffffff]` || HighShadow ||
    || `[0x000000040000, 0x01ffffffffff]` || ShadowGap  ||

    Special case when something is already mapped between
    0x003000000000 and 0x005000000000 (e.g. when prelink is installed):
    || `[0x10007fff8000, 0x7fffffffffff]` || HighMem    ||
    || `[0x02008fff7000, 0x10007fff7fff]` || HighShadow ||
    || `[0x005000000000, 0x02008fff6fff]` || ShadowGap3 ||
    || `[0x003000000000, 0x004fffffffff]` || MidMem     ||
    || `[0x000a7fff8000, 0x002fffffffff]` || ShadowGap2 ||
    || `[0x00067fff8000, 0x000a7fff7fff]` || MidShadow  ||
    || `[0x00008fff7000, 0x00067fff7fff]` || ShadowGap  ||
    || `[0x00007fff8000, 0x00008fff6fff]` || LowShadow  ||
    || `[0x000000000000, 0x00007fff7fff]` || LowMem     ||

    Default Linux/i386 mapping on x86_64 machine:
    || `[0x40000000, 0xffffffff]` || HighMem    ||
    || `[0x28000000, 0x3fffffff]` || HighShadow ||
    || `[0x24000000, 0x27ffffff]` || ShadowGap  ||
    || `[0x20000000, 0x23ffffff]` || LowShadow  ||
    || `[0x00000000, 0x1fffffff]` || LowMem     ||

    Default Linux/i386 mapping on i386 machine
    (addresses starting with 0xc0000000 are reserved
    for kernel and thus not sanitized):
    || `[0x38000000, 0xbfffffff]` || HighMem    ||
    || `[0x27000000, 0x37ffffff]` || HighShadow ||
    || `[0x24000000, 0x26ffffff]` || ShadowGap  ||
    || `[0x20000000, 0x23ffffff]` || LowShadow  ||
    || `[0x00000000, 0x1fffffff]` || LowMem     ||

    Default Linux/MIPS32 mapping:
    || `[0x2aaa0000, 0xffffffff]` || HighMem    ||
    || `[0x0fff4000, 0x2aa9ffff]` || HighShadow ||
    || `[0x0bff4000, 0x0fff3fff]` || ShadowGap  ||
    || `[0x0aaa0000, 0x0bff3fff]` || LowShadow  ||
    || `[0x00000000, 0x0aa9ffff]` || LowMem     ||

    Default Linux/MIPS64 mapping:
    || `[0x4000000000, 0xffffffffff]` || HighMem    ||
    || `[0x2800000000, 0x3fffffffff]` || HighShadow ||
    || `[0x2400000000, 0x27ffffffff]` || ShadowGap  ||
    || `[0x2000000000, 0x23ffffffff]` || LowShadow  ||
    || `[0x0000000000, 0x1fffffffff]` || LowMem     ||

    Default Linux/AArch64 (39-bit VMA) mapping:
    || `[0x2000000000, 0x7fffffffff]` || highmem    ||
    || `[0x1400000000, 0x1fffffffff]` || highshadow ||
    || `[0x1200000000, 0x13ffffffff]` || shadowgap  ||
    || `[0x1000000000, 0x11ffffffff]` || lowshadow  ||
    || `[0x0000000000, 0x0fffffffff]` || lowmem     ||

    Default Linux/AArch64 (42-bit VMA) mapping:
    || `[0x10000000000, 0x3ffffffffff]` || highmem    ||
    || `[0x0a000000000, 0x0ffffffffff]` || highshadow ||
    || `[0x09000000000, 0x09fffffffff]` || shadowgap  ||
    || `[0x08000000000, 0x08fffffffff]` || lowshadow  ||
    || `[0x00000000000, 0x07fffffffff]` || lowmem     ||

    Default Linux/S390 mapping:
    || `[0x30000000, 0x7fffffff]` || HighMem    ||
    || `[0x26000000, 0x2fffffff]` || HighShadow ||
    || `[0x24000000, 0x25ffffff]` || ShadowGap  ||
    || `[0x20000000, 0x23ffffff]` || LowShadow  ||
    || `[0x00000000, 0x1fffffff]` || LowMem     ||

    Default Linux/SystemZ mapping:
    || `[0x14000000000000, 0x1fffffffffffff]` || HighMem    ||
    || `[0x12800000000000, 0x13ffffffffffff]` || HighShadow ||
    || `[0x12000000000000, 0x127fffffffffff]` || ShadowGap  ||
    || `[0x10000000000000, 0x11ffffffffffff]` || LowShadow  ||
    || `[0x00000000000000, 0x0fffffffffffff]` || LowMem     ||

    Shadow mapping on FreeBSD/x86-64 with SHADOW_OFFSET == 0x400000000000:
    || `[0x500000000000, 0x7fffffffffff]` || HighMem    ||
    || `[0x4a0000000000, 0x4fffffffffff]` || HighShadow ||
    || `[0x480000000000, 0x49ffffffffff]` || ShadowGap  ||
    || `[0x400000000000, 0x47ffffffffff]` || LowShadow  ||
    || `[0x000000000000, 0x3fffffffffff]` || LowMem     ||

    Shadow mapping on FreeBSD/i386 with SHADOW_OFFSET == 0x40000000:
    || `[0x60000000, 0xffffffff]` || HighMem    ||
    || `[0x4c000000, 0x5fffffff]` || HighShadow ||
    || `[0x48000000, 0x4bffffff]` || ShadowGap  ||
    || `[0x40000000, 0x47ffffff]` || LowShadow  ||
    || `[0x00000000, 0x3fffffff]` || LowMem     ||

    Shadow mapping on NetBSD/x86-64 with SHADOW_OFFSET == 0x400000000000:
    || `[0x4feffffffe01, 0x7f7ffffff000]` || HighMem    ||
    || `[0x49fdffffffc0, 0x4feffffffe00]` || HighShadow ||
    || `[0x480000000000, 0x49fdffffffbf]` || ShadowGap  ||
    || `[0x400000000000, 0x47ffffffffff]` || LowShadow  ||
    || `[0x000000000000, 0x3fffffffffff]` || LowMem     ||

    Default Windows/i386 mapping:
    (the exact location of HighShadow/HighMem may vary depending
     on WoW64, /LARGEADDRESSAWARE, etc).
    || `[0x50000000, 0xffffffff]` || HighMem    ||
    || `[0x3a000000, 0x4fffffff]` || HighShadow ||
    || `[0x36000000, 0x39ffffff]` || ShadowGap  ||
    || `[0x30000000, 0x35ffffff]` || LowShadow  ||
    || `[0x00000000, 0x2fffffff]` || LowMem     ||


    NB: this mapping imposes limit on available memory limit in ~1G for Panda VM.
    */

    . =  0x40000000 + SIZEOF_HEADERS;

  .interp         : { *(.interp) }
  .note.gnu.build-id : { *(.note.gnu.build-id) }
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
      *(.rela.ldata .rela.ldata.* .rela.gnu.linkonce.l.*)
      *(.rela.lbss .rela.lbss.* .rela.gnu.linkonce.lb.*)
      *(.rela.lrodata .rela.lrodata.* .rela.gnu.linkonce.lr.*)
      *(.rela.ifunc)
    }
  .rela.plt       :
    {
      *(.rela.plt)
      PROVIDE_HIDDEN (__rela_iplt_start = .);
      *(.rela.iplt)
      PROVIDE_HIDDEN (__rela_iplt_end = .);
    }
  .init           :
  {
    KEEP (*(SORT_NONE(.init)))
  }
  .plt            : { *(.plt) *(.iplt) }
.plt.got        : { *(.plt.got) }
.plt.sec        : { *(.plt.sec) }
  .text           :
  {
    *(.text.unlikely .text.*_unlikely .text.unlikely.*)
    *(.text.exit .text.exit.*)
    *(.text.startup .text.startup.*)
    *(.text.hot .text.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  }
  .fini           :
  {
    KEEP (*(SORT_NONE(.fini)))
  }
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  .rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .rodata1        : { *(.rodata1) }
  .eh_frame_hdr : { *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*) }
  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) *(.eh_frame.*) }
  .gcc_except_table   : ONLY_IF_RO { *(.gcc_except_table
  .gcc_except_table.*) }
  .gnu_extab   : ONLY_IF_RO { *(.gnu_extab*) }
  /* These sections are generated by the Sun/Oracle C++ compiler.  */
  .exception_ranges   : ONLY_IF_RO { *(.exception_ranges
  .exception_ranges*) }
  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));
  /* Exception handling  */
  .eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) *(.eh_frame.*) }
  .gnu_extab      : ONLY_IF_RW { *(.gnu_extab) }
  .gcc_except_table   : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }
  .exception_ranges   : ONLY_IF_RW { *(.exception_ranges .exception_ranges*) }
  /* Thread Local Storage sections  */
  .tdata	  : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
  .tbss		  : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  }
  .init_array     :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  }
  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }
  .ctors          :
  {
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    /* We don't want to include the .ctor section from
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  }
  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  }
  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*) }
  .dynamic        : { *(.dynamic) }
  .got            : { *(.got) *(.igot) }
  . = DATA_SEGMENT_RELRO_END (SIZEOF (.got.plt) >= 24 ? 24 : 0, .);
  .got.plt        : { *(.got.plt)  *(.igot.plt) }
  .data           :
  {
    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  }
  .data1          : { *(.data1) }
  _edata = .; PROVIDE (edata = .);
  . = .;
  __bss_start = .;
  .bss            :
  {
   *(.dynbss)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
   /* Align here to ensure that the .bss section occupies space up to
      _end.  Align after .bss to ensure correct alignment even if the
      .bss section disappears because there are no input sections.
      Note: Why do we need it? When there is no .bss section, we don't
      pad the .data section.  */
   . = ALIGN(. != 0 ? 64 / 8 : 1);
  }
  .lbss   :
  {
    *(.dynlbss)
    *(.lbss .lbss.* .gnu.linkonce.lb.*)
    *(LARGE_COMMON)
  }
  . = ALIGN(64 / 8);
  . = SEGMENT_START("ldata-segment", .);
  .lrodata   ALIGN(CONSTANT (MAXPAGESIZE)) + (. & (CONSTANT (MAXPAGESIZE) - 1)) :
  {
    *(.lrodata .lrodata.* .gnu.linkonce.lr.*)
  }
  .ldata   ALIGN(CONSTANT (MAXPAGESIZE)) + (. & (CONSTANT (MAXPAGESIZE) - 1)) :
  {
    *(.ldata .ldata.* .gnu.linkonce.l.*)
    . = ALIGN(. != 0 ? 64 / 8 : 1);
  }
  . = ALIGN(64 / 8);
  _end = .; PROVIDE (end = .);
  . = DATA_SEGMENT_END (.);
  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end ) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  /* DWARF Extension.  */
  .debug_macro    0 : { *(.debug_macro) }
  .debug_addr     0 : { *(.debug_addr) }
  .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }

}
