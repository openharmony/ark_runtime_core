/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
SECTIONS {
    /*
        ASAN reserves (mmap) addresses for its own needs,
        so we have different start addresses for asan and default buildings
    */
    . = DEFINED(LSFLAG_ASAN) ? 0x100100000000 : 0x100010000;

    .interp : { *(.interp) }
    .note : { *SORT(.note*) }
    .hash : { *(.hash) *(.gnu.hash) }

    .dynsym : { *(.dynsym) }
    .dynstr : { *(.dynstr) }

    .gnu.version : { *(.gnu.version*) }

    .rela.dyn : {
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
    .rela.plt : {
        *(.rela.plt)
        PROVIDE_HIDDEN (__rela_iplt_start = .);
        *(.rela.iplt)
        PROVIDE_HIDDEN (__rela_iplt_end = .);
    }

    .init : { KEEP (*(SORT_NONE(.init))) }
    .plt : { *(.plt) *(.iplt) }
    .plt.got : { *(.plt.got) }
    .plt.sec : { *(.plt.sec) }
    .text : { *(.text .stub .text.*) }
    .fini : { KEEP (*(SORT_NONE(.fini))) }

    .rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
    .rodata1 : { *(.rodata1) }

    .tdata : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
    .tbss : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }

    .preinit_array : {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    }
    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
    }
    .fini_array : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);
    }

    .ctors : {
        KEEP (*crtbegin.o(.ctors))
        KEEP (*crtbegin?.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
    }
    .dtors : {
        KEEP (*crtbegin.o(.dtors))
        KEEP (*crtbegin?.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
    }

    .jcr : { KEEP (*(.jcr)) }

    .dynamic : { *(.dynamic) }

    .got : { *(.got) *(.igot) }
    .got.plt : { *(.got.plt) *(.igot.plt) }

    .data : { *(.data .data.* .gnu.linkonce.d.*) }
    .data1 : { *(.data1) }
    .bss : { *(.dynbss) *(.bss .bss.* .gnu.linkonce.b.*) *(COMMON) }

    .lbss : { *(.dynlbss) *(.lbss .lbss.* .gnu.linkonce.lb.*) *(LARGE_COMMON) }
    .lrodata : { *(.lrodata .lrodata.* .gnu.linkonce.lr.*) }
    .ldata : { *(.ldata .ldata.* .gnu.linkonce.l.*) }

    .comment : { *(.comment) }
}
