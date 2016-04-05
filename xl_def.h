/*

  Copyright (C) 2005-2014 Chengtao Chen
  Copyright (C) 2014 Chengtao Chen, Kai Sun
  Copyright (C) 2015-2016 Chengtao Chen, Kai Sun, Tianyi Hao

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef XL_DEF_H_INCLUDE
#define XL_DEF_H_INCLUDE

#include <stdint.h>

#ifndef MIN
#	define MIN(x,y) ((x <= y) ? x : y)
#endif

#define bool char
#define true 1
#define false 0

typedef struct tagRECT8
{
	char left;
	char top;
	char right;
	char bottom;
} RECT8;

#define _250k 262144
#define _500k 524288 
#define _1M 1048576 
#define _2M 2097152 
#define _4M 4194304 
#define _8M 8388608 
#define _16M 16777216 
#define _32M 33554432 

#define White 0
#define Black 1
#define Empty 2

#define BW 15 

typedef struct tagPOSFORMAT
{
	uint8_t num;
	uint8_t pos[BW*BW];
}POSFORMAT;

#define POSX(p) (uint8_t)((p)&0x0f)
#define POSY(p) (uint8_t)((p)>>4)
#define MAKEPOS(x,y) (uint8_t)(((y)<<4)+(x))
#define PASS 0xff

#define SIDE(num) (uint8_t)((~(num))&0x01)
#define OPP(side) (uint8_t)((~side) &0x01)

#define PosInBoard(p) (POSX(p)<BW && POSY(p)<BW)
#define ESB(p) for(p=0;p<=(BW-1)*16+BW ;p++) {if(POSX(p)>=BW) continue;
#define ESE() }

#define FSP_DEAD	0
#define FSP_1		1
#define FSP_2		2
#define FSP_d3		3
#define FSP_3		4
#define FSP_d4		5
#define FSP_d4p     6
#define FSP_4		7//6
#define FSP_5		8//7
#define FSP_L		9//8
#define FSP_44		10//9

#define FMP_F		17
#define FMP_L		16

#define FMP_5		15

#define FMP_4		14
#define FMP_44		13

#define FMP_433		12
#define FMP_43		11

#define FMP_d4p		9
#define FMP_d4		8

#define FMP_33		7
#define FMP_3p		6
#define FMP_3		5
#define FMP_pp		4
#define FMP_d3		3
#define FMP_2		2

#define FMP_1		1
#define FMP_DEAD	0

/////////////////////////////////
#define INF 10
#define UNV (-120)
////////////////////////////////

#ifdef _LDBG
#	define TRACE printf
#else
#	define TRACE(...) ((void)0)
#endif 

int xl_main();

#endif //XL_DEF_H_INCLUDE
