/* Dados binarios das texturas (gerados por tools/gen_art.py), embutidos no executavel. */
__asm__(
    ".section .rodata\n"
    ".balign 4\n"
    ".global mc_art_rle\n"
    "mc_art_rle:\n"
    ".incbin \"data/art.rle\"\n"
    ".global mc_art_rle_end\n"
    "mc_art_rle_end:\n"
    ".balign 4\n"
    ".global mc_title_rle\n"
    "mc_title_rle:\n"
    ".incbin \"data/title.rle\"\n"
    ".global mc_title_rle_end\n"
    "mc_title_rle_end:\n"
    ".previous\n");
