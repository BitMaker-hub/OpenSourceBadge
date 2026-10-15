#pragma once
// GENERADO por tools/exportar_sprites.js a partir del prototipo de pantallas.
// No se edita a mano: se cambia el prototipo y se vuelve a exportar.
//
// Cada personaje mide 32 x 32. ' ' = transparente, '#' = contorno, y cada
// letra es un color de la paleta que va justo debajo del dibujo.
#include <stdint.h>

struct PaletteEntry { char letter; uint16_t color; };

static const char* const AVATAR_DEV[32] = {
  "                                ",
  "                                ",
  "                                ",
  "            ########            ",
  "          ##aaaabaaa##          ",
  "        ##aaaaaaaaaacc##        ",
  "       #aaaaaaaaaaaacccc#       ",
  "       #aaaaaaaaaaaacccc#       ",
  "       #aaaaaaaaaaaacccc#       ",
  "    ####aaaaaaaaaaaacccc#       ",
  "   #cccccccccccccccccccc#       ",
  "   #cccccdeeeeeeeeeefffd#       ",
  "    #####dedddeeeedddffd#       ",
  "       #edggggggeggggggd#       ",
  "      #eedghiiiggghiiigdf#      ",
  "      #eeegiiiigegiiiigff#      ",
  "      #eeeggggggfggggggff#      ",
  "       #eeeeeeeefeeeffff#       ",
  "        ##eeeeeeeeeeff##        ",
  "         #eeeejjjjeeff#         ",
  "          #eeeeeeeeef#          ",
  "          ###eeeeee###          ",
  "        ##kkkkkkkkkkkk##        ",
  "       #kkkkkkkkkkkkkkkk#       ",
  "      #kkkkkkkkkkkkkkkkkk#      ",
  "      ##kkkkkkkkkkkkkkkk##      ",
  "     #llllkkkmkkkkkmkknnnn#     ",
  "    #llllllllmlllllmnnnnnnn#    ",
  "   #lllllllllmlololmnnnnnnnn#   ",
  "  #llllllllllmolllomnnnnnnnnn#  ",
  "  #llllllllllllolollnnnnnnnnn#  ",
  "  #lllllllllllllllllnnnnnnnnn#  ",
};
static const PaletteEntry AVATAR_DEV_PALETTE[] = {
  {'#', 0x0041},   // #07080c
  {'a', 0x1C9F},   // #1e90ff
  {'b', 0x0A72},   // #0f4c91
  {'c', 0x1338},   // #1667c2
  {'d', 0x3943},   // #3b2a1e
  {'e', 0xF611},   // #f2c18f
  {'f', 0xD4CC},   // #d79a63
  {'g', 0x1083},   // #111318
  {'h', 0xE7BF},   // #e0f7ff
  {'i', 0x7E9F},   // #7dd3fc
  {'j', 0x99E5},   // #9c3d2e
  {'k', 0x3A4C},   // #3a4865
  {'l', 0x29CA},   // #2e3a52
  {'m', 0xE73D},   // #e5e7eb
  {'n', 0x2147},   // #222b3d
  {'o', 0x5691},   // #57d38c
  {0, 0}
};

static const char* const AVATAR_MINER[32] = {
  "                                ",
  "                                ",
  "              ####              ",
  "           ###aaaa###           ",
  "         ##aaaaaaaaab##         ",
  "        #aaaaaccccaabbb#        ",
  "       #aaaaaacddcaabbbb#       ",
  "      #aaaaaaacddcaabbbbb#      ",
  "      #aaaaaaaccccaabbbbb#      ",
  "      #aaaaaaaaaaaaabbbbb#      ",
  "      #aaaaaaaaaaaaabbbbb#      ",
  "     #eeeeeeeeeeeeeeeeeeee#     ",
  "     #eeeeeeeeeeeeeeeeeeee#     ",
  "      ##ffffffffffffgggg##      ",
  "      #fffffhiffffhiggggg#      ",
  "      #fffffiiffffiiggggg#      ",
  "      #fffffffffgfffggggg#      ",
  "       #ffffffffgfffgjgg#       ",
  "        ##fjffkkkkffgg##        ",
  "         #ffffllllffjg#         ",
  "          #fffffffffg#          ",
  "           ##ffffff##           ",
  "            #gggggg#            ",
  "           ##gggggg##           ",
  "        ###mmggggggmn###        ",
  "      ##mmmmmggggggmnnnn##      ",
  "     #mmmmmmmmmmmmmmnnnnnn#     ",
  "    #mmmmmmmmmmmmmmmnnnnnnn#    ",
  "   #oooooooooooooooooooooooo#   ",
  "  #oooooooooooooooooooooooooo#  ",
  "  #mmmmmmmmmmmmmmmmmnnnnnnnnn#  ",
  "  #mmmmmmmmmmmmmmmmmnnnnnnnnn#  ",
};
static const PaletteEntry AVATAR_MINER_PALETTE[] = {
  {'#', 0x0041},   // #07080c
  {'a', 0xFE62},   // #facc15
  {'b', 0xD501},   // #d4a20a
  {'c', 0xD6BB},   // #d4d4d8
  {'d', 0xFFDD},   // #fffbea
  {'e', 0xED81},   // #eab308
  {'f', 0xE54F},   // #e0a878
  {'g', 0xBC0A},   // #bf8253
  {'h', 0xFFFF},   // #ffffff
  {'i', 0x18E4},   // #1b1d27
  {'j', 0x6A46},   // #6b4a30
  {'k', 0x4984},   // #4a3220
  {'l', 0x89C4},   // #8a3a26
  {'m', 0xFB82},   // #f97316
  {'n', 0xC201},   // #c2410c
  {'o', 0xFF08},   // #fde047
  {0, 0}
};

static const char* const AVATAR_PLEB[32] = {
  "              #aa#              ",
  "             #aaaa#             ",
  "            #aaaaaa#            ",
  "          ##bbaaaabb##          ",
  "         #bbbbaaaabbcc#         ",
  "        #bbbbbbbbbbbccc#        ",
  "       #bbbbbbbaabbbcccc#       ",
  "       #bbbbbbbaaabbcccc#       ",
  "      #bbbbbbbbaabbbccccc#      ",
  "      #bbbbbbbbbbbbbccccc#      ",
  "      #dddddddddddddddddd#      ",
  "      #dddddddddddddddddd#      ",
  "      #dddddddddddddddddd#      ",
  "       #eeeeeeeeeeeeffff#       ",
  "      #eeeeeageeeeagfffff#      ",
  "      #eeeeeggeeeeggfffff#      ",
  "      #eeeeeeeeefeeefffff#      ",
  "       #eeeheeeefeeeifff#       ",
  "        ##eeheeeeeeefi##        ",
  "         #eeejaaaajeff#         ",
  "          #eejjjjjjef#          ",
  "           ##eeeeee##           ",
  "            #ffffff#            ",
  "           ##ffffff##           ",
  "        ###bbffffffbk###        ",
  "      ##bbbbbffffffbkkkk##      ",
  "     #bbbbbbblbbbbblkkkkkk#     ",
  "    #bbbbbbbblbbbbblkkkkkkk#    ",
  "   #bbbbbbbbblbbbbblkkkkkkkk#   ",
  "  #bbbbbbbbbblbbbbblkkkkkkkkk#  ",
  "  #bbbbbbbbbbbbbbbbbkkkkkkkkk#  ",
  "  #bbbbbbbbbbbbbbbbbkkkkkkkkk#  ",
};
static const PaletteEntry AVATAR_PLEB_PALETTE[] = {
  {'#', 0x0041},   // #07080c
  {'a', 0xFFFF},   // #ffffff
  {'b', 0xF483},   // #f7931a
  {'c', 0xDBA0},   // #d97706
  {'d', 0xC201},   // #c2410c
  {'e', 0xF611},   // #f2c18f
  {'f', 0xD4CC},   // #d79a63
  {'g', 0x18E4},   // #1b1d27
  {'h', 0xD429},   // #d0864f
  {'i', 0xC3A7},   // #c0763f
  {'j', 0x7963},   // #7a2e1d
  {'k', 0xCB61},   // #c96f08
  {'l', 0xFFBC},   // #fff4e0
  {0, 0}
};

static const char* const AVATAR_WHALE[32] = {
  "                                ",
  "                                ",
  "             ######             ",
  "           ##aaaaaa##           ",
  "         ##aaaaaaaaaa##         ",
  "        #aaaaaaaaaaaaaa#        ",
  "       #aaaaabbbbbbaaaaa#       ",
  "       #aaaaaaaaaaaaaaaa#       ",
  "       #aaaaaaaaaaaaaaaa#       ",
  "       #aaaaaaaaaaaaaaaa#       ",
  "        #aaaaaaaaaaaaaa#        ",
  "        #aaaaaaaaaaaaac#        ",
  "        #ddaaaddddaaacc#        ",
  "       #deeeeeeeeeeeeeee#       ",
  "      #ddeefffeeeeefffeec#      ",
  "      #ddeeeeeeeeeeeeeeec#      ",
  "      #ddeeeeeeeeeeeeeeec#      ",
  "       #ddddddddcdddcccc#       ",
  "        ##dddggggggdcc##        ",
  "         #dddhhhhhhdcc#         ",
  "          #ddhhhhhhdc#          ",
  "           ##dddddd##           ",
  "            #cccccc#            ",
  "           ##cccccc##           ",
  "        ###iiccjjccik###        ",
  "      ##iiliicljjclikklk##      ",
  "     #iiiiiliijljliiklkkkk#     ",
  "    #iiiiiiililllliilkkkkkk#    ",
  "   #iiiiiiiiilllllllkkkkkkkk#   ",
  "  #iiiiiiiiiilllmmlikkkkkkkkk#  ",
  "  #iiiiiiiiijjllmljjkkkkkkkkk#  ",
  "  #iiiiiiiiijjlllljjkkkkkkkkk#  ",
};
static const PaletteEntry AVATAR_WHALE_PALETTE[] = {
  {'#', 0x0041},   // #07080c
  {'a', 0x1926},   // #1f2430
  {'b', 0x3A0A},   // #3b4256
  {'c', 0xD4CC},   // #d79a63
  {'d', 0xF611},   // #f2c18f
  {'e', 0x0842},   // #0b0b10
  {'f', 0x6B90},   // #6b7280
  {'g', 0x7963},   // #7a2e1d
  {'h', 0xFFFF},   // #ffffff
  {'i', 0x10C3},   // #16181f
  {'j', 0xF7BE},   // #f4f4f2
  {'k', 0x0862},   // #0d0e13
  {'l', 0xFE62},   // #facc15
  {'m', 0x7AC0},   // #7a5a00
  {0, 0}
};

static const char* const AVATAR_NOSTRICH[32] = {
  "                                ",
  "               #                ",
  "             ##a##              ",
  "            #a##aa##            ",
  "           ##aaaaaaa##          ",
  "          #bbbaaaaaacc#         ",
  "         #abbbaaaaddccc#        ",
  "         #aaaaaaaddddcc#        ",
  "        #aaaaaaaddddddcc#       ",
  "        #aaaaaaaddddedcc####    ",
  "        #aaaaaaaadddeccfffff##  ",
  "        #aaaaaaaaaddcfffffffff# ",
  "         #aaaggaaaaacffhhhhhhf# ",
  "         #aaaaaaaaaacccfffff##  ",
  "          #aaaaaaaaacc######    ",
  "           ##aaaaaaa##          ",
  "            #aaaacc#            ",
  "            #aaaacc#            ",
  "            #aaaacc#            ",
  "            #aaaacc#            ",
  "            #aaaacc###          ",
  "          ###aaaacc#bb#         ",
  "         #bb#aaaacc#bb#         ",
  "         #bb#aaaacc###          ",
  "          ###aaaacc##           ",
  "        ###ccaaaacccc###        ",
  "      ##cccccaaaaccccccc##      ",
  "     #cccccccaaaaccccccccc#     ",
  "    #cccccccccccccccccccccc#    ",
  "   #cccccccccccccccccccccccc#   ",
  "   #cccccccccccccccccccccccc#   ",
  "   #cccccccccccccccccccccccc#   ",
};
static const PaletteEntry AVATAR_NOSTRICH_PALETTE[] = {
  {'#', 0x0041},   // #07080c
  {'a', 0x8AFE},   // #8b5cf6
  {'b', 0xA45F},   // #a78bfa
  {'c', 0x695B},   // #6d28d9
  {'d', 0xFFFF},   // #ffffff
  {'e', 0x1083},   // #111318
  {'f', 0xFC87},   // #fb923c
  {'g', 0xF55F},   // #f0abfc
  {'h', 0xC201},   // #c2410c
  {0, 0}
};

static const char* const AVATAR_ANON[32] = {
  "                                ",
  "                                ",
  "            ########            ",
  "          ##aaaaaaaa##          ",
  "         #aaaaaaaaaaab#         ",
  "        #aaaaaaaaaaaabb#        ",
  "       #aaaaaaaaaaaaabbb#       ",
  "      #aaaaaaaaaaaaaabbbb#      ",
  "      #aaaaaaaaaaaaaabbbb#      ",
  "     #aaaaaaaccccccaabbbbb#     ",
  "     #aaaaaaccccccccabbbbb#     ",
  "    #aaaaaaccccccccccbbbbbb#    ",
  "    #aaaaaccccccccccccbbbbb#    ",
  "    #aaaaaccccccccccccbbbbb#    ",
  "    #aaaaacccdcccccdccbbbbb#    ",
  "    #aaaaaccceeccceeccbbbbb#    ",
  "    #aaaaaccccccccccccbbbbb#    ",
  "    #aaaaaccccccccccccbbbbb#    ",
  "    #aaaaaccccccccccccbbbbb#    ",
  "     #aaaaccccccccccccbbbb#     ",
  "     #aaaaaccccccccccbbbbb#     ",
  "      #aaaaaccccccccabbbb#      ",
  "      #aaaaaaccccccaabbbb#      ",
  "       #aaaaaaaaaaaaabbb#       ",
  "        #aaaaaaaaaaaabb#        ",
  "     ###bbaaaaaeeaaaabbb###     ",
  "    #bbbbbbbaaaeeeaabbbbbbb#    ",
  "   #bbbbbbbbbbbeebbbbbbbbbbb#   ",
  "  #bbbbbbbbbbbbeeebbbbbbbbbbb#  ",
  " #bbbbbbbbbbbbbeebbbbbbbbbbbbb# ",
  " #bbbbbbbbbbbbbeeebbbbbbbbbbbb# ",
  " #bbbbbbbbbbbbbbbbbbbbbbbbbbbb# ",
};
static const PaletteEntry AVATAR_ANON_PALETTE[] = {
  {'#', 0x0041},   // #07080c
  {'a', 0x39E9},   // #3f3f48
  {'b', 0x2966},   // #2f2f36
  {'c', 0x0841},   // #08080b
  {'d', 0x7A21},   // #7a4508
  {'e', 0xF483},   // #f7931a
  {0, 0}
};

