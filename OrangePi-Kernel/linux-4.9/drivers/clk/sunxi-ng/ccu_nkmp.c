/*
 * Copyright (C) 2016 Maxime Ripard
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/clk-provider.h>
#include <linux/rational.h>

#include "ccu_gate.h"
#include "ccu_nkmp.h"

struct nkmp_factors {
	uint8_t n;
	uint8_t k;
	uint8_t m;
	uint8_t p;
};

static const struct nkmp_factors nkmp_factors_freq_map[] = {
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 0 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 6 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 12 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 18 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 24 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 30 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 36 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 42 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 48 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 54 => 60 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 2 }, // 60 => 60 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 2 }, // 66 => 66 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 2 }, // 72 => 72 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 2 }, // 78 => 78 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 2 }, // 84 => 84 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 2 }, // 90 => 90 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 2 }, // 96 => 96 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 2 }, // 102 => 102 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 2 }, // 108 => 108 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 2 }, // 114 => 114 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 1 }, // 120 => 120 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 1 }, // 126 => 132 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 1 }, // 132 => 132 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 1 }, // 138 => 144 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 1 }, // 144 => 144 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 1 }, // 150 => 156 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 1 }, // 156 => 156 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 1 }, // 162 => 168 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 1 }, // 168 => 168 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 1 }, // 174 => 180 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 1 }, // 180 => 180 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 1 }, // 186 => 192 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 1 }, // 192 => 192 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 1 }, // 198 => 204 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 1 }, // 204 => 204 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 1 }, // 210 => 216 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 1 }, // 216 => 216 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 1 }, // 222 => 228 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 1 }, // 228 => 228 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 0 }, // 234 => 240 MHz
	{ .n = 9, .k = 0, .m = 0, .p = 0 }, // 240 => 240 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 0 }, // 246 => 264 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 0 }, // 252 => 264 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 0 }, // 258 => 264 MHz
	{ .n = 10, .k = 0, .m = 0, .p = 0 }, // 264 => 264 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 0 }, // 270 => 288 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 0 }, // 276 => 288 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 0 }, // 282 => 288 MHz
	{ .n = 11, .k = 0, .m = 0, .p = 0 }, // 288 => 288 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 0 }, // 294 => 312 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 0 }, // 300 => 312 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 0 }, // 306 => 312 MHz
	{ .n = 12, .k = 0, .m = 0, .p = 0 }, // 312 => 312 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 0 }, // 318 => 336 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 0 }, // 324 => 336 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 0 }, // 330 => 336 MHz
	{ .n = 13, .k = 0, .m = 0, .p = 0 }, // 336 => 336 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 0 }, // 342 => 360 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 0 }, // 348 => 360 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 0 }, // 354 => 360 MHz
	{ .n = 14, .k = 0, .m = 0, .p = 0 }, // 360 => 360 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 0 }, // 366 => 384 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 0 }, // 372 => 384 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 0 }, // 378 => 384 MHz
	{ .n = 15, .k = 0, .m = 0, .p = 0 }, // 384 => 384 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 0 }, // 390 => 408 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 0 }, // 396 => 408 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 0 }, // 402 => 408 MHz
	{ .n = 16, .k = 0, .m = 0, .p = 0 }, // 408 => 408 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 0 }, // 414 => 432 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 0 }, // 420 => 432 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 0 }, // 426 => 432 MHz
	{ .n = 17, .k = 0, .m = 0, .p = 0 }, // 432 => 432 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 0 }, // 438 => 456 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 0 }, // 444 => 456 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 0 }, // 450 => 456 MHz
	{ .n = 18, .k = 0, .m = 0, .p = 0 }, // 456 => 456 MHz
	{ .n = 19, .k = 0, .m = 0, .p = 0 }, // 462 => 480 MHz
	{ .n = 19, .k = 0, .m = 0, .p = 0 }, // 468 => 480 MHz
	{ .n = 19, .k = 0, .m = 0, .p = 0 }, // 474 => 480 MHz
	{ .n = 19, .k = 0, .m = 0, .p = 0 }, // 480 => 480 MHz
	{ .n = 20, .k = 0, .m = 0, .p = 0 }, // 486 => 504 MHz
	{ .n = 20, .k = 0, .m = 0, .p = 0 }, // 492 => 504 MHz
	{ .n = 20, .k = 0, .m = 0, .p = 0 }, // 498 => 504 MHz
	{ .n = 20, .k = 0, .m = 0, .p = 0 }, // 504 => 504 MHz
	{ .n = 21, .k = 0, .m = 0, .p = 0 }, // 510 => 528 MHz
	{ .n = 21, .k = 0, .m = 0, .p = 0 }, // 516 => 528 MHz
	{ .n = 21, .k = 0, .m = 0, .p = 0 }, // 522 => 528 MHz
	{ .n = 21, .k = 0, .m = 0, .p = 0 }, // 528 => 528 MHz
	{ .n = 22, .k = 0, .m = 0, .p = 0 }, // 534 => 552 MHz
	{ .n = 22, .k = 0, .m = 0, .p = 0 }, // 540 => 552 MHz
	{ .n = 22, .k = 0, .m = 0, .p = 0 }, // 546 => 552 MHz
	{ .n = 22, .k = 0, .m = 0, .p = 0 }, // 552 => 552 MHz
	{ .n = 23, .k = 0, .m = 0, .p = 0 }, // 558 => 576 MHz
	{ .n = 23, .k = 0, .m = 0, .p = 0 }, // 564 => 576 MHz
	{ .n = 23, .k = 0, .m = 0, .p = 0 }, // 570 => 576 MHz
	{ .n = 23, .k = 0, .m = 0, .p = 0 }, // 576 => 576 MHz
	{ .n = 24, .k = 0, .m = 0, .p = 0 }, // 582 => 600 MHz
	{ .n = 24, .k = 0, .m = 0, .p = 0 }, // 588 => 600 MHz
	{ .n = 24, .k = 0, .m = 0, .p = 0 }, // 594 => 600 MHz
	{ .n = 24, .k = 0, .m = 0, .p = 0 }, // 600 => 600 MHz
	{ .n = 25, .k = 0, .m = 0, .p = 0 }, // 606 => 624 MHz
	{ .n = 25, .k = 0, .m = 0, .p = 0 }, // 612 => 624 MHz
	{ .n = 25, .k = 0, .m = 0, .p = 0 }, // 618 => 624 MHz
	{ .n = 25, .k = 0, .m = 0, .p = 0 }, // 624 => 624 MHz
	{ .n = 26, .k = 0, .m = 0, .p = 0 }, // 630 => 648 MHz
	{ .n = 26, .k = 0, .m = 0, .p = 0 }, // 636 => 648 MHz
	{ .n = 26, .k = 0, .m = 0, .p = 0 }, // 642 => 648 MHz
	{ .n = 26, .k = 0, .m = 0, .p = 0 }, // 648 => 648 MHz
	{ .n = 27, .k = 0, .m = 0, .p = 0 }, // 654 => 672 MHz
	{ .n = 27, .k = 0, .m = 0, .p = 0 }, // 660 => 672 MHz
	{ .n = 27, .k = 0, .m = 0, .p = 0 }, // 666 => 672 MHz
	{ .n = 27, .k = 0, .m = 0, .p = 0 }, // 672 => 672 MHz
	{ .n = 28, .k = 0, .m = 0, .p = 0 }, // 678 => 696 MHz
	{ .n = 28, .k = 0, .m = 0, .p = 0 }, // 684 => 696 MHz
	{ .n = 28, .k = 0, .m = 0, .p = 0 }, // 690 => 696 MHz
	{ .n = 28, .k = 0, .m = 0, .p = 0 }, // 696 => 696 MHz
	{ .n = 29, .k = 0, .m = 0, .p = 0 }, // 702 => 720 MHz
	{ .n = 29, .k = 0, .m = 0, .p = 0 }, // 708 => 720 MHz
	{ .n = 29, .k = 0, .m = 0, .p = 0 }, // 714 => 720 MHz
	{ .n = 29, .k = 0, .m = 0, .p = 0 }, // 720 => 720 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 726 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 732 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 738 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 744 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 750 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 756 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 762 => 768 MHz
	{ .n = 15, .k = 1, .m = 0, .p = 0 }, // 768 => 768 MHz
	{ .n = 10, .k = 2, .m = 0, .p = 0 }, // 774 => 792 MHz
	{ .n = 10, .k = 2, .m = 0, .p = 0 }, // 780 => 792 MHz
	{ .n = 10, .k = 2, .m = 0, .p = 0 }, // 786 => 792 MHz
	{ .n = 10, .k = 2, .m = 0, .p = 0 }, // 792 => 792 MHz
	{ .n = 16, .k = 1, .m = 0, .p = 0 }, // 798 => 816 MHz
	{ .n = 16, .k = 1, .m = 0, .p = 0 }, // 804 => 816 MHz
	{ .n = 16, .k = 1, .m = 0, .p = 0 }, // 810 => 816 MHz
	{ .n = 16, .k = 1, .m = 0, .p = 0 }, // 816 => 816 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 822 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 828 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 834 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 840 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 846 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 852 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 858 => 864 MHz
	{ .n = 17, .k = 1, .m = 0, .p = 0 }, // 864 => 864 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 870 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 876 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 882 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 888 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 894 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 900 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 906 => 912 MHz
	{ .n = 18, .k = 1, .m = 0, .p = 0 }, // 912 => 912 MHz
	{ .n = 12, .k = 2, .m = 0, .p = 0 }, // 918 => 936 MHz
	{ .n = 12, .k = 2, .m = 0, .p = 0 }, // 924 => 936 MHz
	{ .n = 12, .k = 2, .m = 0, .p = 0 }, // 930 => 936 MHz
	{ .n = 12, .k = 2, .m = 0, .p = 0 }, // 936 => 936 MHz
	{ .n = 19, .k = 1, .m = 0, .p = 0 }, // 942 => 960 MHz
	{ .n = 19, .k = 1, .m = 0, .p = 0 }, // 948 => 960 MHz
	{ .n = 19, .k = 1, .m = 0, .p = 0 }, // 954 => 960 MHz
	{ .n = 19, .k = 1, .m = 0, .p = 0 }, // 960 => 960 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 966 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 972 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 978 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 984 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 990 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 996 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 1002 => 1008 MHz
	{ .n = 20, .k = 1, .m = 0, .p = 0 }, // 1008 => 1008 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1014 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1020 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1026 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1032 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1038 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1044 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1050 => 1056 MHz
	{ .n = 21, .k = 1, .m = 0, .p = 0 }, // 1056 => 1056 MHz
	{ .n = 14, .k = 2, .m = 0, .p = 0 }, // 1062 => 1080 MHz
	{ .n = 14, .k = 2, .m = 0, .p = 0 }, // 1068 => 1080 MHz
	{ .n = 14, .k = 2, .m = 0, .p = 0 }, // 1074 => 1080 MHz
	{ .n = 14, .k = 2, .m = 0, .p = 0 }, // 1080 => 1080 MHz
	{ .n = 22, .k = 1, .m = 0, .p = 0 }, // 1086 => 1104 MHz
	{ .n = 22, .k = 1, .m = 0, .p = 0 }, // 1092 => 1104 MHz
	{ .n = 22, .k = 1, .m = 0, .p = 0 }, // 1098 => 1104 MHz
	{ .n = 22, .k = 1, .m = 0, .p = 0 }, // 1104 => 1104 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1110 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1116 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1122 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1128 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1134 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1140 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1146 => 1152 MHz
	{ .n = 23, .k = 1, .m = 0, .p = 0 }, // 1152 => 1152 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1158 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1164 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1170 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1176 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1182 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1188 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1194 => 1200 MHz
	{ .n = 24, .k = 1, .m = 0, .p = 0 }, // 1200 => 1200 MHz
	{ .n = 16, .k = 2, .m = 0, .p = 0 }, // 1206 => 1224 MHz
	{ .n = 16, .k = 2, .m = 0, .p = 0 }, // 1212 => 1224 MHz
	{ .n = 16, .k = 2, .m = 0, .p = 0 }, // 1218 => 1224 MHz
	{ .n = 16, .k = 2, .m = 0, .p = 0 }, // 1224 => 1224 MHz
	{ .n = 25, .k = 1, .m = 0, .p = 0 }, // 1230 => 1248 MHz
	{ .n = 25, .k = 1, .m = 0, .p = 0 }, // 1236 => 1248 MHz
	{ .n = 25, .k = 1, .m = 0, .p = 0 }, // 1242 => 1248 MHz
	{ .n = 25, .k = 1, .m = 0, .p = 0 }, // 1248 => 1248 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1254 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1260 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1266 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1272 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1278 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1284 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1290 => 1296 MHz
	{ .n = 26, .k = 1, .m = 0, .p = 0 }, // 1296 => 1296 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1302 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1308 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1314 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1320 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1326 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1332 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1338 => 1344 MHz
	{ .n = 27, .k = 1, .m = 0, .p = 0 }, // 1344 => 1344 MHz
	{ .n = 18, .k = 2, .m = 0, .p = 0 }, // 1350 => 1368 MHz
	{ .n = 18, .k = 2, .m = 0, .p = 0 }, // 1356 => 1368 MHz
	{ .n = 18, .k = 2, .m = 0, .p = 0 }, // 1362 => 1368 MHz
	{ .n = 18, .k = 2, .m = 0, .p = 0 }, // 1368 => 1368 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1374 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1380 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1386 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1392 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1398 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1404 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1410 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1416 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1422 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1428 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1434 => 1440 MHz
	{ .n = 19, .k = 2, .m = 0, .p = 0 }, // 1440 => 1440 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1446 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1452 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1458 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1464 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1470 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1476 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1482 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1488 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1494 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1500 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1506 => 1512 MHz
	{ .n = 20, .k = 2, .m = 0, .p = 0 }, // 1512 => 1512 MHz
	{ .n = 15, .k = 3, .m = 0, .p = 0 }, // 1518 => 1536 MHz
	{ .n = 15, .k = 3, .m = 0, .p = 0 }, // 1524 => 1536 MHz
	{ .n = 15, .k = 3, .m = 0, .p = 0 }, // 1530 => 1536 MHz
	{ .n = 15, .k = 3, .m = 0, .p = 0 }, // 1536 => 1536 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1542 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1548 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1554 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1560 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1566 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1572 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1578 => 1584 MHz
	{ .n = 21, .k = 2, .m = 0, .p = 0 }, // 1584 => 1584 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1590 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1596 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1602 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1608 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1614 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1620 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1626 => 1632 MHz
	{ .n = 16, .k = 3, .m = 0, .p = 0 }, // 1632 => 1632 MHz
	{ .n = 22, .k = 2, .m = 0, .p = 0 }, // 1638 => 1656 MHz
	{ .n = 22, .k = 2, .m = 0, .p = 0 }, // 1644 => 1656 MHz
	{ .n = 22, .k = 2, .m = 0, .p = 0 }, // 1650 => 1656 MHz
	{ .n = 22, .k = 2, .m = 0, .p = 0 }, // 1656 => 1656 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1662 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1668 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1674 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1680 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1686 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1692 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1698 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1704 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1710 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1716 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1722 => 1728 MHz
	{ .n = 23, .k = 2, .m = 0, .p = 0 }, // 1728 => 1728 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1734 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1740 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1746 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1752 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1758 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1764 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1770 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1776 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1782 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1788 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1794 => 1800 MHz
	{ .n = 24, .k = 2, .m = 0, .p = 0 }, // 1800 => 1800 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1806 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1812 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1818 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1824 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1830 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1836 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1842 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1848 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1854 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1860 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1866 => 1872 MHz
	{ .n = 25, .k = 2, .m = 0, .p = 0 }, // 1872 => 1872 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1878 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1884 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1890 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1896 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1902 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1908 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1914 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1920 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1926 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1932 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1938 => 1944 MHz
	{ .n = 26, .k = 2, .m = 0, .p = 0 }, // 1944 => 1944 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1950 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1956 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1962 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1968 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1974 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1980 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1986 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1992 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 1998 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 2004 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 2010 => 2016 MHz
	{ .n = 27, .k = 2, .m = 0, .p = 0 }, // 2016 => 2016 MHz
};

#define N_FACTORS (sizeof(nkmp_factors_freq_map) / sizeof(nkmp_factors_freq_map[0]))

struct _ccu_nkmp {
	unsigned long	n, max_n;
	unsigned long	k, max_k;
	unsigned long	m, max_m;
	unsigned long	p, max_p;
};

static void ccu_nkmp_find_best(unsigned long parent, unsigned long rate,
			       struct _ccu_nkmp *nkmp)
{
	const struct nkmp_factors* factors;
	unsigned int idx = rate / 6000000;

	if (idx >= N_FACTORS)
		idx = N_FACTORS - 1;

	factors = nkmp_factors_freq_map + idx;
	nkmp->n = factors->n + 1;
	nkmp->k = factors->k + 1;
	nkmp->m = factors->m + 1;
	nkmp->p = 1 << factors->p;
}

static void ccu_nkmp_disable(struct clk_hw *hw)
{
	struct ccu_nkmp *nkmp = hw_to_ccu_nkmp(hw);

	return ccu_gate_helper_disable(&nkmp->common, nkmp->enable);
}

static int ccu_nkmp_enable(struct clk_hw *hw)
{
	struct ccu_nkmp *nkmp = hw_to_ccu_nkmp(hw);

	return ccu_gate_helper_enable(&nkmp->common, nkmp->enable);
}

static int ccu_nkmp_is_enabled(struct clk_hw *hw)
{
	struct ccu_nkmp *nkmp = hw_to_ccu_nkmp(hw);

	return ccu_gate_helper_is_enabled(&nkmp->common, nkmp->enable);
}

static unsigned long ccu_nkmp_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct ccu_nkmp *nkmp = hw_to_ccu_nkmp(hw);
	unsigned long n, m, k, p;
	u32 reg;

	reg = readl(nkmp->common.base + nkmp->common.reg);

	n = reg >> nkmp->n.shift;
	n &= (1 << nkmp->n.width) - 1;

	k = reg >> nkmp->k.shift;
	k &= (1 << nkmp->k.width) - 1;

	m = reg >> nkmp->m.shift;
	m &= (1 << nkmp->m.width) - 1;

	p = reg >> nkmp->p.shift;
	p &= (1 << nkmp->p.width) - 1;

	return (parent_rate * (n + 1) * (k + 1) >> p) / (m + 1);
}

static long ccu_nkmp_round_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long *parent_rate)
{
	struct ccu_nkmp *nkmp = hw_to_ccu_nkmp(hw);
	struct _ccu_nkmp _nkmp;

	_nkmp.max_n = 1 << nkmp->n.width;
	_nkmp.max_k = 1 << nkmp->k.width;
	_nkmp.max_m = nkmp->m.max ?: 1 << nkmp->m.width;
	_nkmp.max_p = nkmp->p.max ?: 1 << ((1 << nkmp->p.width) - 1);

	ccu_nkmp_find_best(*parent_rate, rate, &_nkmp);

	return *parent_rate * _nkmp.n * _nkmp.k / (_nkmp.m * _nkmp.p);
}

static int ccu_nkmp_set_rate(struct clk_hw *hw, unsigned long rate,
			   unsigned long parent_rate)
{
	struct ccu_nkmp *nkmp = hw_to_ccu_nkmp(hw);
	struct _ccu_nkmp _nkmp;
	unsigned long flags;
	u32 reg;

	_nkmp.max_n = 1 << nkmp->n.width;
	_nkmp.max_k = 1 << nkmp->k.width;
	_nkmp.max_m = nkmp->m.max ?: 1 << nkmp->m.width;
	_nkmp.max_p = nkmp->p.max ?: 1 << ((1 << nkmp->p.width) - 1);

	ccu_nkmp_find_best(parent_rate, rate, &_nkmp);

	spin_lock_irqsave(nkmp->common.lock, flags);

	reg = readl(nkmp->common.base + nkmp->common.reg);
	reg &= ~GENMASK(nkmp->n.width + nkmp->n.shift - 1, nkmp->n.shift);
	reg &= ~GENMASK(nkmp->k.width + nkmp->k.shift - 1, nkmp->k.shift);
	reg &= ~GENMASK(nkmp->m.width + nkmp->m.shift - 1, nkmp->m.shift);
	reg &= ~GENMASK(nkmp->p.width + nkmp->p.shift - 1, nkmp->p.shift);

	reg |= (_nkmp.n - 1) << nkmp->n.shift;
	reg |= (_nkmp.k - 1) << nkmp->k.shift;
	reg |= (_nkmp.m - 1) << nkmp->m.shift;
	reg |= ilog2(_nkmp.p) << nkmp->p.shift;

	writel(reg, nkmp->common.base + nkmp->common.reg);

	spin_unlock_irqrestore(nkmp->common.lock, flags);

	ccu_helper_wait_for_lock(&nkmp->common, nkmp->lock);

	return 0;
}

const struct clk_ops ccu_nkmp_ops = {
	.disable	= ccu_nkmp_disable,
	.enable		= ccu_nkmp_enable,
	.is_enabled	= ccu_nkmp_is_enabled,

	.recalc_rate	= ccu_nkmp_recalc_rate,
	.round_rate	= ccu_nkmp_round_rate,
	.set_rate	= ccu_nkmp_set_rate,
};
