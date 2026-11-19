OUTPUT_FORMAT(elf32-sh)
OUTPUT_ARCH(sh4)
ENTRY(initialize)

MEMORY
{
    rom (rx)    : o = 0x00300000, l = 2M
    ram (rwx)   : o = 0x08100000, l = 256k
    ilram (rwx) : o = 0xE5200000, l = 4k
    xyram (rwx) : o = 0xE500E000, l = 16k
}

SECTIONS
{
    _reloc_target = ORIGIN(rom) ;
    _reloc_size = SIZEOF(.text) + SIZEOF(.rodata)
                + SIZEOF(.data) + SIZEOF(.ilram)
                + SIZEOF(.xyram) ;
    _reloc_size32 = _reloc_size / 4 ;

    .text : {
        *(.relocate)
        *(.pretext)
        *(.text .text.*)
        . = ALIGN(4);
    } > rom

    .rodata : SUBALIGN(4) {
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } > rom

    .bss : ALIGN(4) {
        _bbss = . ;
        *(.bss .bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = . ;
    } > ram

    .data : ALIGN(4) {
        _datald = LOADADDR(.data) ;
        _sdata = . ;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = . ;
    } > ram AT> rom

    .ilram : ALIGN(4) {
        _ilramld = LOADADDR(.ilram) ;
        _silram = . ;
        *(.ilram .ilram.*)
        . = ALIGN(4);
        _eilram = . ;
    } > ilram AT> rom

    .xyram : ALIGN(4) {
        _xyramld = LOADADDR(.xyram) ;
        _sxyram = . ;
        *(.xyram .xyram.*)
        . = ALIGN(4);
        _exyram = . ;
    } > xyram AT> rom
}
