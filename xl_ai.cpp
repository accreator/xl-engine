/*

  Copyright (C) 2005-2014 Chengtao Chen
  Copyright (C) 2014 Kai Sun

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

#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "pisqpipe.h"
#include "xl_def.h"
#include "xl_ai.h"

// config
#define HASHTT_SIZE _16M
#define VC4TT_SIZE _4M
#define HASH_KEY uint32_t
#define MAX_VC4_NODE 10000
#define NOR_VC4_NODE 10000
#define MAX_SEARCH_NODE 10000
#define MAX_DEPTH 18
#define EXT_DEPTH 3

// value table
static char	_vtable[2][18]=
{
	//dead|1|2|d3|pp|3 |3p|33 |d4|d4p|??|43 |433|44 |4 |5 |L |F
	{  0  ,0,0, 0, 1,5,15,10 ,10,15 ,0 ,10 ,10 ,10 ,10,20,0 , 0 },
	{  0  ,0,0, 0, 1,5,15,-10,10,15 ,0 ,10 ,-10,-10,-10,20,-10,-10}
};

// hash table
typedef struct tagVC4TTITEM
{
	HASH_KEY key;
	char value;
	char p;
}
VC4TTITEM;

#define HTTEXACT 0
#define HTTALPHA 1
#define HTTBETA  2
typedef struct tagHASHTTITEM
{
	HASH_KEY key;
	char depth;
	char std_depth;
	char value;
	uint8_t flag;
}
HASHTTITEM;

// history table
typedef struct tagHISTORYNODE
{
	uint8_t p;
	uint32_t value;
	struct tagHISTORYNODE *pLeft;
	struct tagHISTORYNODE *pRight;
}
HISTORYNODE;

#define ATT_5   5
#define ATT_4   4
#define ATT_0   0


static VC4TTITEM _VC4TT[2][VC4TT_SIZE];
static HASHTTITEM _hashTT[HASHTT_SIZE];

static uint32_t _node;
static uint32_t _vc4_node;
static uint32_t _block_node;
static uint32_t _vc4_limit;
static uint8_t _max_num;
static uint8_t _start_num;
static uint8_t _best_p;
static uint8_t _vc4_p;
static char _root_depth;
static char _std_depth;

static HISTORYNODE* _historyTable[2][256]; // history table
static HISTORYNODE* _historyOrder[2]; //history order


#ifdef _LDBG
int _dbg_vc4tt_hit;
int _dbg_hashtt_hit;
int _dbg_level_hit[20];
int _dbg_eveluate;
#endif


// control
static volatile int      _cancel = 0;
static volatile int      _working = 0;
static volatile int      _initialized = 0;

// board
static uint8_t     _com;
static uint8_t     _num;                // stone count
static uint8_t     _layer1[256];        // (Black,White,Empty)
static uint8_t     _layer2[2][4][256];  // 4, 3 etc.
static uint8_t     _layer3[2][256];     // 43, 44 etc.
static int         _att[2][18];
static uint8_t     _p5[2];

static RECT8       _activeRect[BW*BW+1];
static uint8_t     _moves[BW * BW];

uint8_t            _BEGIN[4][256];
uint8_t            _END[4][256];
uint8_t            _PLUS[4] = {1, 16, 15, 17};

//hash
static uint32_t   _hashIndex;
static HASH_KEY   _hashCheck;


//2层->3层映射表
static uint8_t     _L2toL3[2][65536];

//Zobrist 数组
static uint32_t   _Z_Index[2][256];
static HASH_KEY   _Z_Check[2][256];

//棋型缓存
uint8_t           *_cache[2][BW + 1];

///////////////////////////////////////////////////////
/// 把五子棋的某一行里的一个有效的区域截出来
/// 用二进制表示成 ABCCCCC 的形式作为第一个参数
/// 其中A为左边界 B为右边界 0--被堵住 1--不被堵住
/// CCCCC为棋型	1--有子	2--无子
/// 第二个参数数所求点的坐标
/// 返回值 0--死点 1--1|d2  2--2  3--d3
/// 4--三 5--冲四 6-d4p 7--活四 8-五 *9-长连 *10-四四
/// 低位在右

uint8_t _xl_gene[128][5]=
{
{1,1,1,1,1},//	-----	0
{1,2,2,2,0},//	----0	1
{2,2,2,0,2},//	---0-	2
{3,4,4,0,0},//	---00	3
{2,2,0,2,2},//	--0--	4
{3,4,0,4,0},//	--0-0	5
{4,4,0,0,4},//	--00-	6
{6,7,0,0,0},//	--000	7
{2,0,2,2,2},//	-0---	8
{3,0,4,4,0},//	-0--0	9
{4,0,4,0,4},//	-0-0-	10
{5,0,7,0,0},//	-0-00	11
{4,0,0,4,4},//	-00--	12
{6,0,0,7,0},//	-00-0	13
{7,0,0,0,7},//	-000-	14
{8,0,0,0,0},//	-0000	15
{0,2,2,2,1},//	0----	16
{0,3,3,3,0},//	0---0	17
{0,4,4,0,3},//	0--0-	18
{0,5,6,0,0},//	0--00	19
{0,4,0,4,3},//	0-0--	20
{0,6,0,6,0},//	0-0-0	21
{0,7,0,0,6},//	0-00-	22
{0,8,0,0,0},//	0-000	23
{0,0,4,4,3},//	00---	24
{0,0,6,5,0},//	00--0	25
{0,0,7,0,5},//	00-0-	26
{0,0,8,0,0},//	00-00	27
{0,0,0,7,6},//	000--	28
{0,0,0,8,0},//	000-0	29
{0,0,0,0,8},//	0000-	30
{0,0,0,0,0},//	00000	31
//----------*2--------------
{1,1,1,1,0},//	-----x	0
{1,1,1,1,0},//	----0x	1
{2,2,2,0,1},//	---0-x	2
{3,3,3,0,0},//	---00x	3
{2,2,0,2,1},//	--0--x	4
{3,3,0,3,0},//	--0-0x	5
{4,4,0,0,3},//	--00-x	6
{5,5,0,0,0},//	--000x	7
{2,0,2,2,1},//	-0---x	8
{3,0,3,3,0},//	-0--0x	9
{4,0,4,0,3},//	-0-0-x	10
{5,0,5,0,0},//	-0-00x	11
{4,0,0,4,3},//	-00--x	12
{6,0,0,5,0},//	-00-0x	13
{7,0,0,0,5},//	-000-x	14
{8,0,0,0,0},//	-0000x	15
{0,2,2,2,1},//	0----x	16
{0,3,3,3,0},//	0---0x	17
{0,4,4,0,3},//	0--0-x	18
{0,5,5,0,0},//	0--00x	19
{0,4,0,4,3},//	0-0--x	20
{0,6,0,5,0},//	0-0-0x	21
{0,7,0,0,5},//	0-00-x	22
{0,8,0,0,0},//	0-000x	23
{0,0,4,4,3},//	00---x	24
{0,0,6,5,0},//	00--0x	25
{0,0,7,0,5},//	00-0-x	26
{0,0,8,0,0},//	00-00x	27
{0,0,0,7,6},//	000--x	28
{0,0,0,8,0},//	000-0x	29
{0,0,0,0,8},//	0000-x	30
{0,0,0,0,0},//	00000x	31
//----------*3--------------
{0,1,1,1,1},//	x-----	0
{1,2,2,2,0},//	x----0	1
{1,2,2,0,2},//	x---0-	2
{3,4,4,0,0},//	x---00	3
{1,2,2,0,2},//	x--0--	4
{3,4,0,4,0},//	x--0-0	5
{3,4,0,0,4},//	x--00-	6
{6,7,0,0,0},//	x--000	7
{1,0,2,2,2},//	x-0---	8
{3,0,4,4,0},//	x-0--0	9
{3,0,4,0,4},//	x-0-0-	10
{5,0,7,0,0},//	x-0-00	11
{3,0,0,4,4},//	x-00--	12
{5,0,0,7,0},//	x-00-0	13
{5,0,0,0,7},//	x-000-	14
{8,0,0,0,0},//	x-0000	15
{1,1,1,1,1},//	x0----	16
{0,3,3,3,0},//	x0---0	17
{0,3,3,0,3},//	x0--0-	18
{0,5,6,0,0},//	x0--00	19
{0,3,0,3,3},//	x0-0--	20
{0,5,0,6,0},//	x0-0-0	21
{0,5,0,0,6},//	x0-00-	22
{0,8,0,0,0},//	x0-000	23
{0,0,3,3,3},//	x00---	24
{0,0,5,5,0},//	x00--0	25
{0,0,5,0,5},//	x00-0-	26
{0,0,8,0,0},//	x00-00	27
{0,0,0,5,5},//	x000--	28
{0,0,0,8,0},//	x000-0	29
{0,0,0,0,8},//	x0000-	30
{0,0,0,0,0},//	x00000	31
//----------*4--------------
{0,0,0,0,0},//	x-----x	0
{1,1,1,1,0},//	x----0x	1
{1,1,1,0,1},//	x---0-x	2
{3,3,3,0,0},//	x---00x	3
{1,1,0,1,1},//	x--0--x	4
{3,3,0,3,0},//	x--0-0x	5
{3,3,0,0,3},//	x--00-x	6
{5,5,0,0,0},//	x--000x	7
{1,0,1,1,1},//	x-0---x	8
{3,0,3,3,0},//	x-0--0x	9
{3,0,3,0,3},//	x-0-0-x	10
{5,0,5,0,0},//	x-0-00x	11
{3,0,0,3,3},//	x-00--x	12
{5,0,0,5,0},//	x-00-0x	13
{5,0,0,0,5},//	x-000-x	14
{8,0,0,0,0},//	x-0000x	15
{0,1,1,1,1},//	x0----x	16
{0,3,3,3,0},//	x0---0x	17
{0,3,3,0,3},//	x0--0-x	18
{0,5,5,0,0},//	x0--00x	19
{0,3,0,3,3},//	x0-0--x	20
{0,5,0,5,0},//	x0-0-0x	21
{0,5,0,0,5},//	x0-00-x	22
{0,8,0,0,0},//	x0-000x	23
{0,0,3,3,3},//	x00---x	24
{0,0,5,5,0},//	x00--0x	25
{0,0,5,0,5},//	x00-0-x	26
{0,0,8,0,0},//	x00-00x	27
{0,0,0,5,5},//	x000--x	28
{0,0,0,8,0},//	x000-0x	29
{0,0,0,0,8},//	x0000-x	30
{0,0,0,0,0},//	x00000x	31
};

///////////////////////////////////////////////////////
char XlAlphaBeta(uint8_t side, char alpha, char beta, char depth,uint8_t att);
void XlReset();
void XlInitialize();
void InitBeginEnd();
bool XlIsF(uint8_t);
char QVC4_p(uint8_t p,uint8_t side,uint8_t def[][256]);
char QVC4(uint8_t side,uint8_t def[][256]);
char QVC3(uint8_t side,uint8_t depth);
char QVC3_p(uint8_t p,uint8_t side,uint8_t depth);
bool XlIsMakeVC4(uint8_t p, uint8_t side);
char XlEveluate( uint8_t side,uint8_t att);
char XlSimpleEveluate(uint8_t side);
void XlResetHistoryTable();
char XlBlockC4(uint8_t side,char alpha, char beta, char depth,uint8_t def[][256]);
//char Defend(uint8_t side,uint8_t def[][256]);
char XlReadHashTT(char alpha,char beta,char depth);
void XlWriteHashTT(char depth, char value, char flag);
/////////////////////////////////////////////////////////

void XlQuickSort(char A[],uint8_t move[],int low,int high)
{
	char pivot;
	uint8_t pivot_m;
	int scanUp,scanDown;
	int mid,k;
	if(high-low<=0)
	{
		return;
	}
	else
	{
		if(high-low==1)
		{
			if(A[high]>A[low])
			{
				k=A[high];
				A[high]=A[low];
				A[low]=k;
				k=move[high];
				move[high]=move[low];
				move[low]=k;
				return;
			}
		}
	}
	mid=(low +high)/2;
	pivot=A[mid];
	pivot_m=move[mid];
	k=A[mid];
	A[mid]=A[low];
	A[low]=k;
	k=move[mid];
	move[mid]=move[low];
	move[low]=k;
	scanUp =low+1;
	scanDown = high;
	do{
		while(scanUp<=scanDown && A[scanUp]>=pivot)
			scanUp++;
		while(pivot>A[scanDown])
			scanDown--;
		if(scanUp<scanDown)
		{
			k=A[scanUp];
			A[scanUp]=A[scanDown];
			A[scanDown]=k;	
			k=move[scanUp];
			move[scanUp]=move[scanDown];
			move[scanDown]=k;
		}
	}while(scanUp<scanDown);
	A[low]=A[scanDown];
	A[scanDown]=pivot;
	move[low]=move[scanDown];
	move[scanDown]=pivot_m;

	if(low<scanDown-1)
		XlQuickSort(A,move,low,scanDown-1);
	if(scanDown+1<high)
		XlQuickSort(A,move,scanDown+1,high);
}

void XlReset()
{
	memset(_layer1, Empty, sizeof(_layer1));
	memset(_layer2, FSP_1, sizeof(_layer2));
	memset(_layer3, FMP_1, sizeof(_layer3));
	memset(_att,0,sizeof(_att));
	_num = 0;
	//	_num4[0]=0;
	//	_num4[1]=0;

	_hashIndex=0;
	_hashCheck=0;
	XlResetHistoryTable();
	_node=0;
	_vc4_node=0;
	_block_node=0;
	_vc4_p=0xff;

#ifdef _XLDEBUG
	_dbg_hashtt_hit=0;
//	_dbg_max_depth=0;
	memset(_dbg_level_hit,0,sizeof(_dbg_level_hit));
	_dbg_eveluate=0;
#endif
}

void XlInitBeginEnd()
{
	uint8_t x, y, p;

	for (p = 0; p < 255; p++)
	{
		x = POSX(p);
		y = POSY(p);
		_BEGIN[0][p] = x;
		_BEGIN[1][p] = y * 16;
		_BEGIN[2][p] = MIN(BW - x - 1, y) * 15;
		_BEGIN[3][p] = MIN(x, y) * 17;

		_END[0][p] = BW - x - 1;
		_END[1][p] = (BW - y - 1) * 16;
		_END[2][p] = MIN(x, BW - y - 1) * 15;
		_END[3][p] = MIN(BW - x - 1, BW - y - 1) * 17;
	}
}

void  XlInitL2toL3()
{
	uint8_t            side, *p;
	uint32_t          n0, n1, n2, n3, idx;

	// TRACE("initializing
	// mapping
	// table...");
	for (side = 0; side <= 1; side++)
	{
		for (n0 = 0; n0 < 11; n0++)
			for (n1 = 0; n1 < 11; n1++)
				for (n2 = 0; n2 < 11; n2++)
					for (n3 = 0; n3 < 11; n3++)
					{
						int CountOfType[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0};

						CountOfType[n0]++;
						CountOfType[n1]++;
						CountOfType[n2]++;
						CountOfType[n3]++;
						idx = n0;
						idx |= n1 << 4;
						idx |= n2 << 8;
						idx |= n3 << 12;
						// TRACE("idx=%d",idx);
						p = &(_L2toL3[side][idx]);

						if (side == Black)
						{
							if (CountOfType[FSP_5])
							{
								*p = FMP_5;
							}
							else if (CountOfType[FSP_L])
								*p = FMP_L;
							else if (CountOfType[FSP_44]
								|| CountOfType[FSP_4] + CountOfType[FSP_d4]+CountOfType[FSP_d4p]>= 2)
							{
								*p = FMP_44;
							}
							else if ((CountOfType[FSP_d4] || CountOfType[FSP_4]||CountOfType[FSP_d4p])
								&& CountOfType[FSP_3] >= 2)
							{
								*p = FMP_433;
							}
							else if (CountOfType[FSP_3] >= 2)
							{
								*p = FMP_33;
							}
							else if (CountOfType[FSP_4])
							{
								*p = FMP_4;
							}
							else if ((CountOfType[FSP_d4] || CountOfType[FSP_4] ||CountOfType[FSP_d4p])
								&& CountOfType[FSP_3])
							{
								*p = FMP_43;
							}
							else if (CountOfType[FSP_d4p]||CountOfType[FSP_d4] && (CountOfType[FSP_2] || CountOfType[FSP_d3]))
							{
								*p = FMP_d4p;
							}
							else if (CountOfType[FSP_d4])
							{
								*p = FMP_d4;
							}
							else if (CountOfType[FSP_3] && (CountOfType[FSP_2]
								|| CountOfType[FSP_d3]))
							{
								*p = FMP_3p;
							}
							else if (CountOfType[FSP_3])
								*p = FMP_3;
							else if (CountOfType[FSP_2] + CountOfType[FSP_d3] >= 2)
								*p = FMP_pp;
							else if (CountOfType[FSP_d3])
								*p = FMP_d3;
							else if (CountOfType[FSP_2])
								*p = FMP_2;
							else if (CountOfType[FSP_1])
								*p = FMP_1;
							else
								*p = FMP_DEAD;
						}
						else	// 白方
						{
							if (CountOfType[FSP_5] || CountOfType[FSP_L])
							{
								*p = FMP_5;
							}
							else if (CountOfType[FSP_44]
								|| CountOfType[FSP_4] + CountOfType[FSP_d4]+CountOfType[FSP_d4p] >= 2)
							{
								*p = FMP_44;
							}
							else if (CountOfType[FSP_4])
							{
								*p = FMP_4;
							}
							else if ((CountOfType[FSP_d4] ||CountOfType[FSP_d4p])&& CountOfType[FSP_3])
							{
								*p = FMP_43;
							}
							else if (CountOfType[FSP_d4p]||CountOfType[FSP_d4] && (CountOfType[FSP_2]|| CountOfType[FSP_d3]))
							{
								*p = FMP_d4p;
							}
							else if (CountOfType[FSP_d4])
							{
								*p = FMP_d4;
							}
							else if (CountOfType[FSP_3] >= 2)
							{
								*p = FMP_33;
							}
							else if (CountOfType[FSP_3] && (CountOfType[FSP_2]
								|| CountOfType[FSP_d3]))
							{
								*p = FMP_3p;
							}
							else if (CountOfType[FSP_3])
								*p = FMP_3;
							else if (CountOfType[FSP_2] + CountOfType[FSP_d3] >= 2)
								*p = FMP_pp;
							else if (CountOfType[FSP_d3])
								*p = FMP_d3;
							else if (CountOfType[FSP_2])
								*p = FMP_2;
							else if (CountOfType[FSP_1])
								*p = FMP_1;
							else
								*p = FMP_DEAD;
						}
					}
	}
	// TRACE("done.\n");
}

void  XlInitZobrist()
{
	int             i;

	uint8_t           *p = (uint8_t *) _Z_Index;

	for (i = 0; i < sizeof(_Z_Index); i++)
	{
		p[i] = rand() & 0xFF;
	}
	p = (uint8_t *) _Z_Check;
	for (i = 0; i < sizeof(_Z_Check); i++)
	{
		p[i] = rand() & 0xFF;
	}
	// TRACE("done.\n");
}

void XlInitCacheLine(uint8_t * dst, uint32_t src, char len, uint8_t side,bool reverse)
{
#define _STONE(n) ((src>>(n))&1)

	char            n, i, blank, blank1dis;
	uint8_t            idx, j, m, temp[15];
	uint32_t          rev;


	// 0100101001001000 B
	// ....|<->|.......
	//     |idx|<- n ->

	memset(dst, 0, len);

	for (n = 0; (n + 4) < len; n++)
	{
		if (n > 0 && side)
			if (_STONE(n - 1))
				continue;

			blank = 0;
			idx = 0;

			for (i = 0; i < 5; i++)
			{
				if (_STONE(n + i))
				{
					idx |= 1 << (4 - i);
				}
				else
				{
					blank++;
					if (blank == 1)
						blank1dis = i;
				}
			}

			if (n + 5 < len && _STONE(n + 5) && side)
			{
				// 这个区右边还有棋子
				if (blank == 1)
					dst[n + blank1dis] = FSP_L;	// 长连
			}
			else if (n + 5 < len && !_STONE(n + 5) && side && blank == 0)
			{
				dst[n + 5] = FSP_L;
			}
			else
			{
				if (n == 0)
					idx |= 64;
				else
				{
					if (_STONE(n - 1))
						idx |= 64;	// 左边界
					else if (n > 1 && side)
						if (_STONE(n - 2))
							idx |= 64;
				}

				if (n + 5 >= len)
					idx |= 32;
				else
				{
					if (_STONE(n + 5))
						idx |= 32;	// 右边界
					else if (n + 6 < len)
						if (_STONE(n + 6) && side)
							idx |= 32;	// 右边界
				}
				for (i = 0; i < 5; i++)
				{
					j = _xl_gene[idx][i];
					m = dst[n + i];
					if ((j == FSP_d4|| j==FSP_d4p) && (m == FSP_d4||m == FSP_d4p))
						dst[n + i] = FSP_44;
					else if(j == FSP_d4 && m == FSP_d3 || m == FSP_d4 && j == FSP_d3)
						dst[n + i] = FSP_d4p;
					else if (j > m)
						dst[n + i] = j;
				}
			}
			if (blank >= 1)
				n += blank1dis;
			// 与反向计算一起防止白重复算4和漏算
	}
	if (side || !reverse)
		return;

	// 反向算一次

	rev = 0;
	for (i = 0; i < len; i++)
		if (_STONE(i))
			rev |= 1 << (len - i - 1);
		XlInitCacheLine(temp, rev, len, side, false);
		for (i = 0; i < len; i++)
			if (dst[i] < temp[len - i - 1])
				dst[i] = temp[len - i - 1];
}

int  XlInitCache()
{
	char            side, n;
	uint32_t          idx, pow_n;

	for (n = 0; n <= BW; n++)
	{
		_cache[0][n] = 0;
		_cache[1][n] = 0;
	}

	for (side = 0; side <= 1; side++)
	{
		for (n = 5; n <= BW; n++)
		{
			pow_n = 1 << n;
			_cache[side][n] = (uint8_t *)malloc(n * pow_n);

			if (!_cache[side][n])
			{
				XlDispose();
				return 0;
			}

			for (idx = 0; idx < pow_n; idx++)
			{
				XlInitCacheLine((_cache[side][n]) + idx * n, idx, n, side, true);
			}
		}
	}

	return 1;
}

void  XlUpdateL2Area(uint8_t p, char line, char len, uint8_t side)
{
	uint8_t            plus, p1, *l2, *pC;
	char            n;
	uint32_t          idx;

	plus = _PLUS[line];
	l2 = _layer2[side][line];

	if (len < 5)
	{
		// <5的死区，将L2清0
		for (n = 0; n < len; n++)
		{
			if (l2[p])
			{
				//L2清0
				l2[p] = 0;

				idx = (uint32_t) _layer2[side][0][p];
				idx |= ((uint32_t) _layer2[side][1][p]) << 4;
				idx |= ((uint32_t) _layer2[side][2][p]) << 8;
				idx |= ((uint32_t) _layer2[side][3][p]) << 12;
				_att[side][_layer3[side][p]]--;
				//				if(_layer3[side][p]>=FMP_d4) _num4[side]--;
				_layer3[side][p] = _L2toL3[side][idx];
				_att[side][_layer3[side][p]]++;
				if(_layer3[side][p]==FMP_5) _p5[side]=p;
			}
			p += plus;
		}
	}
	else
	{
		uint32_t          area = 0;
		p1 = p;
		for (n = 0; n < len; n++)
		{
			if (_layer1[p1] == side)
				area |= 0x00000001 << n;
			p1 += plus;
		}

		pC = _cache[side][len];
		pC += len * area;

		for (n = 0; n < len; n++)
		{
			if (l2[p] != *pC)
			{
				l2[p] = *pC;

				idx = (uint32_t) _layer2[side][0][p];
				idx |= ((uint32_t) _layer2[side][1][p]) << 4;
				idx |= ((uint32_t) _layer2[side][2][p]) << 8;
				idx |= ((uint32_t) _layer2[side][3][p]) << 12;
				_att[side][_layer3[side][p]]--;
				//				if(_layer3[side][p]>=FMP_d4) _num4[side]--;
				_layer3[side][p] = _L2toL3[side][idx];
				_att[side][_layer3[side][p]]++;
				if(_layer3[side][p]==FMP_5) _p5[side]=p;
			}
			p += plus;
			pC++;
		}
	}
}

void XlPutStoneLine(uint8_t p, char line, uint8_t side)
{
	uint8_t            begin, end, left, right, plus;
	char            len;

	plus = _PLUS[line];
	// 找到行的开始和结尾
	begin = p - _BEGIN[line][p];
	end = p + _END[line][p];

	///////////////////////////////////////////
	//                v
	//  X O  X  X     O    X    O   X
	//           [ area1  ]
	//     [ area2   ] [ area3 ]
	//////////////////////////////////////////

	// area1

	for (left = p; left != begin; left -= plus)
		if (_layer1[left - plus] == !side)
			break;

		for (right = p; right != end; right += plus)
			if (_layer1[right + plus] == !side)
				break;

			len = (right - left) / plus + 1;

			XlUpdateL2Area(left, line, len, side);

			// area2
			for (left = p; left != begin; left -= plus)
				if (_layer1[left - plus] == side)
					break;

				len = (p - left) / plus;

				XlUpdateL2Area(left, line, len, OPP(side));

				// area3
				for (right = p; right != end; right += plus)
					if (_layer1[right + plus] == side)
						break;

					len = (right - p) / plus;

					XlUpdateL2Area((uint8_t)(p + plus), line, len, OPP(side));
}

void  XlPutStone(uint8_t p, uint8_t side)
{
	char x,y,left,right,top,bottom;
	uint8_t pre;
	_layer1[p] = side;

	_hashIndex ^= _Z_Index[side][p];
	_hashCheck ^= _Z_Check[side][p];

	_moves[_num] = p;

	_num++;

	//更新hotRect
#define HOTRECT_RANGE 2
	x=POSX(p);
	y=POSY(p);
	pre =_num -1;

	left = x - HOTRECT_RANGE;
	top = y -  HOTRECT_RANGE;
	right = x+ HOTRECT_RANGE;
	bottom = y + HOTRECT_RANGE;

	//添加
	_activeRect[_num].left  = left <_activeRect[pre].left? left :_activeRect[pre].left;
	_activeRect[_num].top= top  < _activeRect[pre].top  ?top :_activeRect[pre].top;
	_activeRect[_num].right = right >_activeRect[pre].right ? right :	_activeRect[pre].right;
	_activeRect[_num].bottom = bottom  >  _activeRect[pre].bottom? bottom :_activeRect[pre].bottom;

	//越界剪裁
	if(_activeRect[_num].left <0 ) _activeRect[_num].left =0;
	if(_activeRect[_num].top <0 ) _activeRect[_num].top = 0;
	if(_activeRect[_num].right >= BW ) _activeRect[_num].right= BW-1;
	if(_activeRect[_num].bottom >=BW  )_activeRect[_num].bottom = BW-1;

	XlPutStoneLine(p, 0, side);
	XlPutStoneLine(p, 1, side);
	XlPutStoneLine(p, 2, side);
	XlPutStoneLine(p, 3, side);

	// update the site where put stone, it is not part of any area
	/*	_layer2[side][0][p] = FSP_DEAD;
	_layer2[side][1][p] = FSP_DEAD;
	_layer2[side][2][p] = FSP_DEAD;
	_layer2[side][3][p] = FSP_DEAD;
	_att[side][_layer3[side][p]] --;
	_layer3[side][p] = FMP_DEAD;
	_att[side][FMP_DEAD] ++;
	*/
	_layer2[OPP(side)][0][p] = FSP_DEAD;
	_layer2[OPP(side)][1][p] = FSP_DEAD;
	_layer2[OPP(side)][2][p] = FSP_DEAD;
	_layer2[OPP(side)][3][p] = FSP_DEAD;
	_att[OPP(side)][_layer3[OPP(side)][p]] --;
	//	if(_layer3[side][p]>=FMP_d4) _num4[side]--;
	_layer3[OPP(side)][p] = FMP_DEAD;
	_att[OPP(side)][FMP_DEAD] ++;
}

void XlRemoveStoneLine(uint8_t p, char line, uint8_t side)
{
	uint8_t            begin, end, left, right, plus;
	char            len;

	plus = _PLUS[line];
	// 找到行的开始和结尾
	begin = p - _BEGIN[line][p];
	end = p + _END[line][p];

	///////////////////////////////////////////////
	//                O
	//  XO  X  X  O   ^    X  O   X
	//          [ area1,W ]
	//             [ area2,B ]
	///////////////////////////////////////////////

	for (left = p; left != begin; left -= plus)
	{
		if (_layer1[left - plus] == !side)
			break;
	}
	for (right = p; right != end; right += plus)
	{
		if (_layer1[right + plus] == !side)
			break;
	}

	len = (right - left) / plus + 1;

	XlUpdateL2Area(left, line, len, side);

	// area2
	for (left = p; left != begin; left -= plus)
	{
		if (_layer1[left - plus] == side)
			break;
	}

	for (right = p; right != end; right += plus)
	{
		if (_layer1[right + plus] == side)
			break;
	}
	len = (right - left) / plus + 1;

	XlUpdateL2Area(left, line, len, OPP(side));

}

void XlRemoveStone()
{
	// 不做合法性判断
	//	char            line;
	uint8_t p, side;

	p = _moves[_num - 1];
	side = _layer1[p];

	_num--;

	_hashIndex ^= _Z_Index[side][p];
	_hashCheck ^= _Z_Check[side][p];

	_layer1[p] = Empty;

	XlRemoveStoneLine(p, 0, side);
	XlRemoveStoneLine(p, 1, side);
	XlRemoveStoneLine(p, 2, side);
	XlRemoveStoneLine(p, 3, side);
}

//只有黑棋才判断真三
bool XlIsTrue3(uint8_t p, char line)
{
	uint8_t begin, end, left, right, plus;

	if (_layer2[Black][line][p] != FSP_3)
		return false;

	XlPutStone(p, Black);

	plus = _PLUS[line];
	begin = p - _BEGIN[line][p];
	end = p + _END[line][p];

	for (left = p; left != begin && _layer1[left] != White; left -= plus)
	{
		if (_layer1[left] == Empty)
		{
			if (_layer2[Black][line][left] == FSP_4)
			{
				if (!XlIsF(left))
				{
					XlRemoveStone();
					return true;
				}
			}
			break;
		}
	}

	for (right = p; right != end && _layer1[right] != White; right += plus)
	{

		if (_layer1[right] == Empty)
		{
			if (_layer2[Black][line][right] == FSP_4)
			{
				if (!XlIsF(right))
				{
					XlRemoveStone();
					return true;
				}
			}
			break;
		}
	}

	XlRemoveStone();
	return false;
}

int XlIs33F(uint8_t p)
{
	// 注：要预先判断棋形是不是33,433
	char true3 = 0;

	if (XlIsTrue3(p, 0))
	{
		true3++;
		if (true3 == 2)
			return 1;
	}
	if (XlIsTrue3(p, 1))
	{
		true3++;
		if (true3 == 2)
			return 1;
	}
	if (XlIsTrue3(p, 2))
	{
		true3++;
		if (true3 == 2)
			return 1;
	}
	if (XlIsTrue3(p, 3))
	{
		true3++;
		if (true3 == 2)
			return 1;
	}
	return 0;
}

// 判断 x,y 点是否禁手
bool  XlIsF(uint8_t p)
{
/*
if (_layer1[p] != Empty)
return false;
	*/
	switch (_layer3[Black][p])
	{
	case FMP_L:
	case FMP_44:
		return true;
	case FMP_33:
	case FMP_433:
		return XlIs33F(p);
	default:
		return false;
	}
}

void XlCancel(int wait)
{
	_cancel = 1;
	if (wait)
		while (_working);
}

int VelueX(uint8_t p)
{
	char i,j,x,y;
	uint8_t p1;
	int ret;

	x=POSX(p);
	y=POSY(p);
	ret=0;
	for(i=x-1;i<=x+1;i++)
	{
		for(j=y-1;j<=y+1;j++)
		{
			p1=MAKEPOS(i,j);
			if(!PosInBoard(p1)) continue;
			ret+=_layer3[0][p1]+_layer3[1][p1];
		}
	}
	/*
	i=XlEveluate(0);
	XlPutStone(p,0
	*/
	return ret;
}
#ifdef _XLDEBUG
void XlBoardTrace()
{
/*
template
ESB(p)
switch(_layer1[p])
{
case Black: TRACE("[ X ] \t");break;
case White: TRACE("[ O ] \t");break;
default: TRACE("%d\t",p);
}
if(POSX(p)==BW-1)TRACE("\n");
ESE()
	*/
	uint8_t p,side;
	uint8_t def[2][256];
	memset(def,0,sizeof(def));

	TRACE("%d,%d---------------\n",_p5[0],_p5[1]);
	return;


	//return;
	side=SIDE(_num);
	_vc4_limit=MAX_VC4_NODE;
//	if(QVC4(OPP(side),def)>=INF)
//		TRACE("\ndef=%d\n",Defend(side,def));
	ESB(p);
	switch(_layer1[p])
	{
		case Black: TRACE("[ X ]\t");break;
		case White: TRACE("[ O ]\t");break;
		default:
			TRACE(".");
			if(def[1][p])TRACE("1");
			if(def[0][p])TRACE("0");
			TRACE("\t");
	}
	if(POSX(p)==BW-1)TRACE("\n\n");
	ESE()

}
#endif

void XlGetLayer3(uint8_t layer3[][256], POSFORMAT pos)
{
	uint8_t            i, p;
	//	char            x, y;
	memset(layer3, 0, 2 * 256);

	if (_working)XlCancel(1);

	if (!_initialized) XlInitialize();
	if(!_initialized)
	{
		TRACE("not enough memory.\n");
		return;
	}

	XlReset();
	for (i = 0; i < pos.num; i++)
	{
		if (!PosInBoard(pos.pos[i]))
			return;
		XlPutStone(pos.pos[i], SIDE(i));
		// TRACE("k");
	}

#ifdef _XLDEBUG
	XlBoardTrace();
#endif

	// XlRemoveStone();
	ESB(p)
		if (_layer1[p] != Empty)
			continue;
		if (XlIsF(p))
			layer3[1][p] = FMP_F;
		else
			layer3[1][p] = _layer3[1][p];
		layer3[0][p] = _layer3[0][p];
		ESE()
}

void XlWriteVc4TT(char value, uint8_t side,uint8_t p)
{
	VC4TTITEM *ptt = &_VC4TT[side][_hashIndex % VC4TT_SIZE];
	ptt->key = _hashCheck;
	ptt->value = value;
	if(value>=INF)
	{
		ptt->p = p;
		_vc4_p = p;
	}
}

//这个函数用查找VCF路径的防点
void XlFindDef_D4(uint8_t p,uint8_t side,uint8_t def[][256])
{
	uint8_t line,plus,p1,l2[4],begin,end,left,right;
	bool is43;
	int n;
	//冲四点一定是防点
	def[OPP(side)][p]=1;

	l2[0]=_layer2[side][0][p];
	l2[1]=_layer2[side][1][p];
	l2[2]=_layer2[side][2][p];
	l2[3]=_layer2[side][3][p];


	XlPutStone(p,side);
	if(side)
	{
		is43=_layer3[side][p]==FMP_43;
		//如果是黑方，要考虑利用禁手的防法
		for(line=0;line<4;line++)
		{
			plus = _PLUS[line];
			begin = p - _BEGIN[line][p];
			end = p + _END[line][p];

			switch(l2[line])
			{
			case FSP_d4p:
				//d4p有可能利用一条线的44防
				for (left = p; left != begin; left -= plus)
				{
					//	if(_layer3[side][left]==FMP_L) def[1][left]=1;
					if (_layer1[left - plus] == White)break;
				}
				for (right = p; right != end; right += plus)
				{
					//	if(_layer3[side][right]==FMP_L) def[1][right]=1;
					if (_layer1[right + plus] == White)	break;
				}

				for(p1=left;p1<=right;p1+=plus)
				{
					if(_layer2[1][line][p1]>=FSP_d4 && _layer2[1][line][p1]<=FSP_5)
					{
						//同一线冲四点
						XlRemoveStone();
						XlPutStone(p1,1);
						if(_layer3[1][p]==FMP_44) def[1][p1]=1;
						XlRemoveStone();
						XlPutStone(p,1);
					}
				}
			case FSP_d4:
				//防5点
				for(p1 = p-_BEGIN[line][p];;p1+=plus)
				{
					if(	_layer3[side][p1]==FMP_5 )
					{
						def[OPP(side)][p1]=1;
						n=_att[1][FMP_L];
						XlPutStone(p1,1);
						if(_att[1][FMP_L]>n)
						{
							//有新增长连
							for (left = p; left != begin; left -= plus)
							{
								if(_layer3[side][left]==FMP_L) def[1][left]=1;
								if (_layer1[left - plus] == White)break;
							}
							for (right = p; right != end; right += plus)
							{
								if(_layer3[side][right]==FMP_L) def[1][right]=1;
								if (_layer1[right + plus] == White)	break;
							}
						}
						XlRemoveStone();
						//继续考虑长连，新增的长连点同时也是做长连的点
						break;
					}
				}
				break;

			case FSP_3:
			case FSP_d3:
				//做四四

				for (left = p; left != begin; left -= plus)
				{
					if(_layer3[side][left]>=FMP_d4 && _layer3[side][left]<=FMP_5) def[1][left]=1;
					if (_layer1[left - plus] == White)break;
				}
				for (right = p; right != end; right += plus)
				{
					if(_layer3[side][right]>=FMP_d4 && _layer3[side][right]<=FMP_5) def[1][right]=1;
					if (_layer1[right + plus] == White)	break;
				}
				break;
			case FSP_2:
				//做三三,前提是已经有一个三
				if(!is43) break;

				plus = _PLUS[line];
				begin = p - _BEGIN[line][p];
				end = p + _END[line][p];

				for (left = p; left != begin; left -= plus)
				{
					if(_layer3[side][left]==FMP_3) def[1][left]=1;
					if (_layer1[left - plus] == White)break;
				}
				for (right = p; right != end; right += plus)
				{
					if(_layer3[side][right]==FMP_3) def[1][right]=1;
					if (_layer1[right + plus] == White)	break;
				}
				break;

			}
		}
	}
	else
	{
		for(line=0;line<4;line++)
		{
			switch(l2[line])
			{
			case FMP_d4p:
			case FMP_d4:
				//防5点
				plus= _PLUS[line];
				for(p1 = p-_BEGIN[line][p];;p1+=plus)
				{
					if(	_layer3[side][p1]==FMP_5 )
					{
						def[0][p1]=1;
						break;
					}
				}
				break;
			}
		}
	}
	XlRemoveStone();
}

void XlFindDef_4(uint8_t p,uint8_t side,uint8_t def[][256])
{
	//防止对方成4，即堵对方的三
	uint8_t left,right,begin,end,pp,plus,p1,l2[4];
	char line;


	def[OPP(side)][p] = 1;//成四的那一点可以防

	//找到活四的那一行
	line = 0;
	while( _layer2[side][line][p] != FSP_4 ) line ++;

	plus = _PLUS[line];
	begin = p - _BEGIN[line][p];
	end = p+_END[line][p];

	//p = _LineBegin[line][x][y];

	//找到另一个活四(如果有)
	p1=p;
	for(pp=begin; pp<=end ; pp+=plus )
	{
		if( _layer3[side][pp] == FMP_4 && pp!=p )
		{
			p1=pp;
			break;
		}
	}

	for(pp=begin ; pp<=end ; pp+=plus )
	{
		if( _layer1[pp] == Empty  )
		{
			XlPutStone(pp, OPP(side) );
			if( _layer3[side][p] != FMP_4 && _layer3[side][p1] != FMP_4 )
				def[OPP(side)][pp] = 1;
			XlRemoveStone();
		}
	}

	if(!side) return;

	//如果是黑方，还得找出利用禁手防
	l2[0]=_layer2[1][0][p];
	l2[1]=_layer2[1][1][p];
	l2[2]=_layer2[1][2][p];
	l2[3]=_layer2[1][3][p];

	XlPutStone(p,1);
	//如果是黑方，要考虑利用禁手的防法
	for(line=0;line<4;line++)
	{
		plus = _PLUS[line];
		begin = p - _BEGIN[line][p];
		end = p + _END[line][p];
		switch(l2[line])
		{
		case FSP_4:
			for (left = p; left != begin; left -= plus)
			{
				if (_layer1[left - plus] == White)break;
			}
			for (right = p; right != end; right += plus)
			{
				if (_layer1[right + plus] == White)	break;
			}

			for(p1=left;p1<=right;p1+=plus)
			{
				if(_layer1[p1]==Empty)
				{
					XlPutStone(p1,1);
					if(_layer3[1][p]==FMP_44||_layer3[1][p]==FMP_L) def[1][p1]=1;
					XlRemoveStone();
				}
			}
			break;
		case FSP_3:
		case FSP_d3:
			//做四四
			for (left = p; left != begin; left -= plus)
			{
				if(_layer3[side][left]>=FMP_d4 && _layer3[side][left]<=FMP_5) def[1][left]=1;
				if (_layer1[left - plus] == White)break;
			}
			for (right = p; right != end; right += plus)
			{
				if(_layer3[side][right]>=FMP_d4 && _layer3[side][right]<=FMP_5) def[1][right]=1;
				if (_layer1[right + plus] == White)	break;
			}
			break;
		case FSP_2:
			//做三三,前提是已经有一个三
			if(l2[0]!=FSP_3 &&l2[1]!=FSP_3 &&l2[2]!=FSP_3 &&l2[3]!=FSP_3 )
				break;

			plus = _PLUS[line];
			begin = p - _BEGIN[line][p];
			end = p + _END[line][p];

			for (left = p; left != begin; left -= plus)
			{
				if(_layer3[side][left]==FMP_3) def[1][left]=1;
				if (_layer1[left - plus] == White)break;
			}
			for (right = p; right != end; right += plus)
			{
				if(_layer3[side][right]==FMP_3) def[1][right]=1;
				if (_layer1[right + plus] == White)	break;
			}
			break;
		}
	}
	XlRemoveStone();
}

void XlFindDef_44(uint8_t p,uint8_t side,uint8_t def[][256])
{
	char line;
	def[1][p] = 1;

	//要考虑单线的44
	XlPutStone( p, side );

	//找齐四个线的五连点
	for(line = 0; line < 4; line ++ )
	{
		uint8_t pp,end,plus;
		plus=_PLUS[line];
		end=p+_END[line][p];
		for( pp = p-_BEGIN[line][p];pp<=end ;pp+=plus )
			if( _layer3[side][pp] == FMP_5 )
				def[1][pp] = 1;
	}

	XlRemoveStone();
}

char QVC4(uint8_t side,uint8_t def[][256])
{
	uint8_t ml1[BW*BW],ml2[BW*BW],ml3[BW*BW];
	uint8_t n1=0,n2=0,n3=0,i,p;
	VC4TTITEM *ptt;
	char ret,max=-INF;

	ptt = &_VC4TT[side][_hashIndex % VC4TT_SIZE];

	//查tt
	if(ptt->key==_hashCheck)
	{
#ifdef _XLDEBUG
		_dbg_vc4tt_hit++;
#endif
		p=ptt->p;
		switch(ptt->value)
		{
		case INF:
			if(def)
			{
				switch(_layer3[side][p])
				{
				case FMP_4:
					XlFindDef_4(p,side,def);
					break;
				case FMP_44:
					XlFindDef_44(p,side,def);
					break;
				case FMP_5:
					def[OPP(side)][p]=1;
					break;
				default:
					QVC4_p(ptt->p,side,def);
					XlFindDef_D4(ptt->p,side,def);
				}
			}
			_vc4_p=ptt->p;
			return INF;
		default:
			return ptt->value;
		}

	}

	//TRACE("=i\n");

	ESB(p);
	switch(_layer3[side][p])
	{
	case FMP_4:
		if(def)XlFindDef_4(p,side,def);
		XlWriteVc4TT(INF,side,p);
		return INF;
	case FMP_44:
		if(side)break;
		if(def)XlFindDef_44(p,side,def);
		XlWriteVc4TT(INF,side,p);
		return INF;
	case FMP_5:
		if(def)def[OPP(side)][p]=1;
		XlWriteVc4TT(INF,side,p);
		return INF;
	case FMP_433:
		if(XlIsF(p)) break;
	case FMP_43:
		ml1[n1]=p;
		n1++;
		break;
	case FMP_d4p:
		ml2[n2]=p;
		n2++;
		break;
	case FMP_d4:
		ml3[n3]=p;
		n3++;
		break;
	}
	ESE();

//	TRACE("=j\n");

	if(n1+n2+n3==0) return XlSimpleEveluate(side);

	//TRACE("=k\n");
	for(i=0;i<n1;i++)
	{
		if((ret=QVC4_p(ml1[i],side,def))==UNV) return UNV;
		if(ret>=INF)
		{
			if(def)XlFindDef_D4(ml1[i],side,def);
			XlWriteVc4TT(INF,side,ml1[i]);
			return INF;
		}
		if(ret>max)max=ret;
	}
	for(i=0;i<n2;i++)
	{
		//TRACE("=m\n");
		if((ret=QVC4_p(ml2[i],side,def))==UNV) return UNV;
			//TRACE("=n\n");
		if(ret>=INF)
		{
			if(def)XlFindDef_D4(ml2[i],side,def);
			XlWriteVc4TT(INF,side,ml2[i]);
			return INF;
		}
		if(ret>max)max=ret;
	}

	for(i=0;i<n3;i++)
	{
		if((ret=QVC4_p(ml3[i],side,def))==UNV) return UNV;
		if(ret>=INF)
		{
			if(def)XlFindDef_D4(ml3[i],side,def);
			XlWriteVc4TT(INF,side,ml3[i]);
			return INF;
		}
		if(ret>max)max=ret;
	}
//	TRACE("=l\n");

	if(!_vc4_limit||_cancel) return UNV;
	ptt->key=_hashCheck;
	ptt->value=max;
	return max;

}

char QVC4_p(uint8_t p,uint8_t side,uint8_t def[][256])
{
	uint8_t p1;
	char ret,max=-INF;

	if(!_vc4_limit||_cancel) return UNV;
	_vc4_limit--;
	_vc4_node++;

	if(_att[OPP(side)][FMP_5])
	{
		if(_layer3[OPP(side)][p]!=FMP_5) return -INF;
	}

	switch(_layer3[side][p])
	{
	case FMP_5:
	case FMP_4:
		//TRACE("%d:5,4\n",_num);
		return INF;
	case FMP_44:
		if( !side)
		{
			//TRACE("%d:44\n",_num);
			return INF;
		}
		break;
	case FMP_433:
		if( side)
			if(XlIsF(p)) break;
	case FMP_43:
	case FMP_d4p:
	case FMP_d4:

		XlPutStone(p,side);
		p1=_p5[side];
		switch(_layer3[OPP(side)][p1])
		{
		case FMP_5:
		case FMP_4:
			break;
		case FMP_44:
			if(OPP(side))
			{
				XlRemoveStone();
				return INF;
			}
			else
			{
				break;
			}
		case FMP_433:
			if(OPP(side) && XlIsF(p1))
			{
				XlRemoveStone();
				return INF;
			}
		case FMP_43:
		case FMP_d4p:
		case FMP_d4:

			XlPutStone(p1,OPP(side));
			if(_num>_max_num)_max_num=_num;

			p1=_p5[OPP(side)];
			ret=QVC4_p(p1,side,def);
			XlRemoveStone();

			if( ret>=INF)
			{
				XlRemoveStone();
				return INF;
			}
			if( ret>max) max=ret;

			break;
		case FMP_33:
			if(!side &&XlIsF(p1))
			{
				XlRemoveStone();
				return INF;
			}
		default:
			XlPutStone(p1, OPP(side) );
			ret=QVC4(side,def);
			XlRemoveStone();
			if( ret>=INF)
			{
				XlRemoveStone();
				return INF;
			}
			if( ret>max) max=ret;
		}

		XlRemoveStone();
	}
	return max;
}

char XlBlockC4_p(uint8_t p,uint8_t side,char alpha, char beta, char depth ,uint8_t def[][256])
{
	uint8_t p1;
	char ret;

	if(_cancel) return UNV;
	_block_node++;

	//TRACE("0\t");
	switch(_layer3[side][p])
	{
	case FMP_5:
	case FMP_4:
		return INF;
	case FMP_44:
		if( !side)
		{
			return INF;
		}
		break;
	case FMP_433:
		if( side)
			if(XlIsF(p)) break;
	case FMP_43:
	case FMP_d4p:
	case FMP_d4:
	//	TRACE("1\t");
		XlPutStone(p,side);
		p1=_p5[side];
		switch(_layer3[OPP(side)][p1])
		{
		case FMP_5:
		case FMP_4:
			break;
		case FMP_44:
			if(OPP(side))
			{
				XlRemoveStone();
				return INF;
			}
			else
			{
				break;
			}
		case FMP_433:
			if(OPP(side) && XlIsF(p1))
			{
				XlRemoveStone();
				return INF;
			}
		case FMP_43:
		case FMP_d4p:
		case FMP_d4:
			XlPutStone(p1,OPP(side));
			if(def[side][p]||def[OPP(side)][p1])
			{
				ret=XlAlphaBeta( side, alpha, beta, depth,ATT_5);
			}
			else
			{
				//需要进一步找反
				ret=XlBlockC4_p(_p5[OPP(side)],side,alpha,beta,depth,def);
			}
			XlRemoveStone();

			if( ret>=beta)
			{
				XlRemoveStone();
				return beta;
			}
			if( ret>alpha) alpha=ret;
			break;
		case FMP_33:
			if(OPP(side) &&XlIsF(p1))
			{
				XlRemoveStone();
				return INF;
			}
		default:
			//TRACE("2\t");
			XlPutStone(p1, OPP(side) );
			if(def[side][p]||def[OPP(side)][p1])
			{
				ret=XlAlphaBeta( side, alpha, beta, depth,ATT_5);
			}
			else
			{
				ret=XlBlockC4(side, alpha, beta,depth,def);
			}
			XlRemoveStone();
			if( ret>=beta)
			{
				XlRemoveStone();
				return beta;
			}
			if( ret>alpha) alpha=ret;
		}

		XlRemoveStone();
		break;
	}
	return alpha;
}

/*void XlWriteBlockTT(char value, uint8_t side)
{
	BLOCKTTITEM *ptt = &_BLOCKTT[side][_hashIndex % BLOCKTT_SIZE];
	ptt->key = _hashCheck;
	ptt->value = value;
}
*/

char XlBlockC4(uint8_t side,char alpha, char beta, char depth ,uint8_t def[][256])
{
	//用冲四的方法把冲到某一点，挡住对方的VCF
	char ret;
	uint8_t p,hflag=HTTALPHA;

	if((ret=XlReadHashTT(alpha, beta,depth)) != UNV)
	{
		return ret;
	}


	ESB(p);
	switch(_layer3[side][p])
	{
	case FMP_433:
		if(side)
		{
			if(XlIsF(p)) break;
		}
	case FMP_44:
		if(side) break;
	case FMP_4:
	case FMP_5:
		//XlWriteBlockTT(INF,side);
//		XlWriteHashTT(depth,INF,HTTEXACT);
//		return INF;
	case FMP_43:
	case FMP_d4p:
	case FMP_d4:
		if((ret=XlBlockC4_p(p,side,alpha,beta,depth,def))>=beta)
		{
			XlWriteHashTT(depth,INF,HTTBETA);
			return beta;
		}
		if(ret>alpha)
		{
			hflag=HTTEXACT;
			alpha=ret;
		}
	}
	ESE();
	XlWriteHashTT(depth,INF,hflag);
	return alpha;
}

void XlResetHistoryTable()
{
	HISTORYNODE *p;

	int ddiv = _historyOrder[0]->value / 8;

	if( ddiv )
	{
		for( p =_historyOrder[0] ; p ; p = p->pRight)
		{
			p->value /= ddiv;
		}
	}

	ddiv = _historyOrder[1]->value / 8;
	if( ddiv )
	{
		for( p =_historyOrder[1] ; p ; p = p->pRight)
		{
			p->value /= ddiv;
		}
	}

}

void XlAdjustHistoryOrder(uint8_t pt, uint8_t side, uint32_t value)
{
	//定位所求的点
	HISTORYNODE *pl,*p;
	p = _historyTable[side][pt];

	p->value += value;
	value = p->value;

	pl = p->pLeft;
	if( !pl ) return;

	//使p脱离链表
	pl->pRight = p->pRight;
	if( p->pRight )
	{
		p->pRight->pLeft = pl;
	}

	while( pl )
	{
		if( pl->value < value )
		{
			pl = pl->pLeft;
		}
		else
		{
			//插在*pl右边
			p->pRight = pl->pRight;
			if( p->pRight)
				p->pRight->pLeft = p;

			pl->pRight = p;
			p->pLeft = pl;
			return;
		}
	}

	//pl == NULL 表示应该插在最左
	p->pLeft = NULL;
	p->pRight = _historyOrder[side];
	_historyOrder[side]->pLeft = p;
	_historyOrder[side] = p;
}

char XlSimpleEveluate(uint8_t side)
{
	int i,value;

	for(i=4,value=0;i<17;i++)
	{
		value+=_vtable[side][i]* _att[side][i];
		value-=_vtable[OPP(side)][i]* _att[OPP(side)][i];
	}

	value/=20;
	value+=2;

	if(value>INF)value=INF;
	if(value<-INF)value=-INF;
	return (char)value;
}


char XlEveluate( uint8_t side,uint8_t att)
{
	char value,ret;

#ifdef _XLDEBUG
	_dbg_eveluate++;
#endif
	//_vc4_limit=NOR_VC4_NODE;
	//value= QVC4(side,NULL);
	value=XlSimpleEveluate(side);
	_vc4_limit=NOR_VC4_NODE;
	ret=QVC4(OPP(side),NULL);
	if(ret>=INF)
	{
		//对方有VCF
		//if(_com==side)
			value-=3;
	}

	return value;
}

uint8_t XlMoveGen(uint8_t ml[],uint8_t side,uint8_t att,uint8_t def[][256],bool narrow)
{
	char max=-INF;
	uint8_t p,n=0;
	HISTORYNODE *ph;
	//RECT8 hrc=_activeRect[_num];
	switch(att)
	{
	case ATT_5:
		ml[0]=_p5[OPP(side)];
		return 1;
	case ATT_4:
		for( ph=_historyOrder[side]; ph; ph=ph->pRight  )
		{
			p=ph->p;
			if( _layer1[p] != Empty ) continue;
			if(side)
			{
				if( XlIsF(p)) continue;
			}

			if(!def[side][p])
			{
				if(_layer3[side][p]<FMP_d4) continue;
			}

			ml[n]=p;
			n ++;
		}
		return n;
	default:
		for( ph=_historyOrder[side]; ph; ph=ph->pRight  )
		{
			p=ph->p;
			if( _layer1[p] != Empty ) continue;
			if(side)
			{
				if( XlIsF(p)) continue;
			}
			//if( POSX(p) < hrc.left )continue;
			//if( POSX(p) > hrc.right )continue;
		//	if( POSY(p) < hrc.top )continue;
		//	if( POSY(p) > hrc.bottom)continue;

			if(narrow)
			{
				/*
				XlPutStone(p,side);
				_vc4_limit=NOR_VC4_NODE;
				if( QVC4(side,NULL)<INF)
				{
					XlRemoveStone();
					continue;
				}
				XlRemoveStone();
				*/
				if(_layer3[side][p]<FMP_pp&&_layer3[OPP(side)][p]<FMP_pp&&(_layer3[side][p]<FMP_2||_layer3[OPP(side)][p]<FMP_2))
					 continue;
			}
			else if(_layer3[side][p]<FMP_2&&_layer3[OPP(side)][p]<FMP_2)
			{
				continue;
			}

			ml[n]=p;
			n ++;
		}
		return n;
	}
}

char XlReadHashTT(char alpha,char beta,char depth)
{
	HASHTTITEM *p = &_hashTT[_hashIndex % HASHTT_SIZE];

#ifdef _XLDEBUG
	_dbg_hashtt_hit++;
#endif

	if (p->key == _hashCheck)
	{
		if (p->depth >= depth && p->std_depth >= _std_depth)
		{
			switch(p->flag)
			{
			case HTTEXACT:
				return p->value;
			case HTTALPHA:
				if(p->value<=alpha)
					return alpha;
				break;
			case HTTBETA:
				if(p->value>=beta)
					return beta;
				break;
			}
		}
	}
#ifdef _XLDEBUG
	_dbg_hashtt_hit--;
#endif
	return UNV;
}

void XlWriteHashTT(char depth, char value, char flag)
{
	HASHTTITEM *p = &_hashTT[_hashIndex %HASHTT_SIZE];
	p->key = _hashCheck;
	p->depth = depth;
	p->std_depth = _std_depth;
	p->value = value;
	p->flag = flag;
}

//char XlABMove4(uint8_t p,uint8_t side,char alpha,char beta,char depth)
//{
//	XlPutStone(p,side);
//}

char XlAlphaBeta(uint8_t side, char alpha, char beta, char depth,uint8_t att)
{
	char value;
	uint8_t p,opp_side,cnCount,n,bestMove,hflag=HTTALPHA;
	uint8_t def[2][256],ml[256];
	bool isdef;

	//统计
	_node++;
	if(_num>_max_num)_max_num=_num;
#ifdef _XLDEBUG
	_dbg_level_hit[_root_depth-depth]++;
#endif

	//读TT
	if((value = XlReadHashTT(alpha, beta,depth)) != UNV)
	{
		return value;
	}

	isdef=(att>=ATT_4);

	//五连胜
	if(_att[side][FMP_5]) return INF;
	//看对手先手
	opp_side=OPP(side);
	if(_att[opp_side][FMP_5])
	{
		att=ATT_5;
		ml[0]=_p5[opp_side];
		//if(_layer3[opp_side][ml[0]]!=FMP_5)
		//	TRACE("gggfdggggggggggggggggggggggggggggggggggggggggggggg");
		cnCount=1;
	}
	else
	{
		_vc4_limit=NOR_VC4_NODE;
		if(QVC4(side,NULL)>=INF)
		{
			XlWriteHashTT(depth,INF,HTTEXACT);
			return INF;
		}

		depth--;
		memset(def,'\0',sizeof(def));
		_vc4_limit=NOR_VC4_NODE;
		if(QVC4(opp_side,def)>=INF) att=ATT_4;
		else 
		{
			att=ATT_0;
//			if(!isdef)att0--;
		}
		//生成走法列表
		cnCount=XlMoveGen(ml,side,att,def,false);
		if(!cnCount)
		{
			if(att>ATT_0)
			{
				XlWriteHashTT(depth,-INF,HTTEXACT);
				return -INF;
			}
			else
			{
				value=XlEveluate(side,att);
				XlWriteHashTT(depth,value,HTTEXACT);
				return value;
			}
		}
	}

	//if(depth<=STD_DEPTH)
	//{
		switch(att)
		{
		case ATT_5: //冲5时不返回估值
			if( depth <= 0 )
			{
				value=XlEveluate(side,att);
				XlWriteHashTT(depth,value,HTTEXACT);
				return value;
			}
			break;
		case ATT_4:
			if(depth<=EXT_DEPTH)
			{
				value=XlEveluate(side,att);
				XlWriteHashTT(depth,value,HTTEXACT);
				return value;
			}
			break;
		default:
			if(depth<=_root_depth-_std_depth)
			{
				value=XlEveluate(side,att);
				XlWriteHashTT(depth,value,HTTEXACT);
				return value;
			}
		}
	//}
/*	else if(att==ATT_0 && cnCount>SEARCH_WIDTH)
	{
		//在顶部，且没有做V,早期剪枝
		char v[BW*BW];
		for( n= 0; n < cnCount && ml[n]!=0xff; n++ )
		{
			p=ml[n];
			XlPutStone( p, side );
			v[n] = -XlAlphaBeta( opp_side, -INF, INF, (char)(depth-2));
			if( _cancel )	return 0;
			XlRemoveStone();
		}
		XlQuickSort(v,ml,0,cnCount-1);
		cnCount=SEARCH_WIDTH;
	}
*/
	bestMove =0;
	for( n= 0; n < cnCount && ml[n]!=0xff; n++ )
	{
		p=ml[n];
		if(att==ATT_4 && !def[side][p])
		{
			value=XlBlockC4_p(p,side,alpha,beta,depth,def);
		}
		else
		{
			XlPutStone( p, side );
			value = -XlAlphaBeta( opp_side, (char)-beta, (char)-alpha, depth,att);
			if( _cancel )	return 0;
			XlRemoveStone();
		}


#ifdef _XLDEBUG
		//if(depth==_root_depth) TRACE("\n%d:[%d,%d]=%d",n,POSX(p.p),POSY(p.p),value);
#endif

		if (value >= beta)
		{
			//	if(depth==_root_depth)
			//	_best_p=p;
			XlWriteHashTT(depth,beta,HTTBETA);
			XlAdjustHistoryOrder( p, side, 0x00000001<<(uint32_t)depth  );
			return beta;
		}

		if (value > alpha)
		{
			bestMove=n;
			hflag=HTTEXACT;
			alpha = value;
		}
	}

	XlWriteHashTT(depth,alpha,hflag);
	XlAdjustHistoryOrder( ml[bestMove], side, 0x00000001<<(uint32_t)depth  );
	//	_best_p=ml[bestMove];
	return alpha;
}

char XlSearchRoot(char alpha, char beta, char depth )
{
	char value;
	uint8_t p,att,opp_side,cnCount,n,bestMove,hflag=HTTALPHA;
	uint8_t side,ml[256],def[2][256];

	//统计
#ifdef _XLDEBUG
	_dbg_level_hit[_root_depth-depth]++;
#endif

	//初始化
	side=SIDE(_num);
	opp_side=OPP(side);
	_best_p=0xff;

	//看对手先手
	memset(def,'\0',sizeof(def));
	_vc4_limit=NOR_VC4_NODE;
	if(QVC4(opp_side,def)>=INF) att=ATT_4;
	else att=ATT_0;
	cnCount=XlMoveGen(ml,side,att,def,true);
	if( !cnCount ) cnCount=XlMoveGen(ml,side,att,def,false);;
	if( !cnCount ) return -INF;

/*	if(att==ATT_0 && cnCount>SEARCH_WIDTH)
	{
		//在顶部，且没有做V,早期剪枝
		char v[BW*BW];
		for( n= 0; n < cnCount && ml[n]!=0xff; n++ )
		{
			p=ml[n];
			XlPutStone( p, side );
			v[n] = -XlAlphaBeta( opp_side, -INF, INF, (char)(depth-2));
			if( _cancel )	return 0;
			XlRemoveStone();
		}
		XlQuickSort(v,ml,0,cnCount-1);
		cnCount=SEARCH_WIDTH;
	}
*/


	bestMove =0;
	for( n= 0; n < cnCount && ml[n]!=0xff; n++ )
	{
#ifdef _XLDEBUG
		if(_sendfunc)
		{
			_sendfunc(SEND_CURMOVE,(uint32_t)ml[n]);
		}
#endif
		p=ml[n];
		if(att==ATT_4 && !def[side][p])
		{
			value=XlBlockC4_p(p,side,alpha,beta,depth,def);
		}
		else
		{
			XlPutStone( ml[n], side );
			value = -XlAlphaBeta( OPP(side), (char)-beta, (char)-alpha, (char)(depth-1),att);

			if(_cancel)return UNV;
			XlRemoveStone();
		}
		//TRACE("[%d]=%d\n",ml[n],value);


		if (value >= beta)
		{
			_best_p=ml[n];
			//XlWriteHashTT(depth,beta,HTTBETA);
			XlAdjustHistoryOrder( ml[n], side, 0x00000001<<(uint32_t)depth  );
			return beta;
		}

		if (value>alpha)
		{
			//if(_sendfunc)
			//	_sendfunc(SEND_MOVEVALUE,(uint32_t)ml[n]|((uint32_t)(value+INF+1)<<8));
			bestMove=n;
			alpha = value;
		}
	}

	//	XlWriteHashTT(depth,beta,hflag);
	XlAdjustHistoryOrder( ml[bestMove], side, 0x00000001<<(uint32_t)depth  );
	_best_p=ml[bestMove];
	//_best_p=0;

	return alpha;
}


char XlMTD_f(uint8_t side,char firstguess, char depth)
{
	char g,lowerbound, upperbound,beta;
	uint8_t bestmove=0xff;

	g=firstguess;
	upperbound=INF;
	lowerbound=-INF;
	while(lowerbound < upperbound)
	{
		bestmove= _best_p;
		if(g==lowerbound)
			beta=g+1;
		else
			beta=g;
		g=XlSearchRoot((char)(beta-1),beta,depth);
		if(g<beta)
			upperbound=g;
		else
			lowerbound=g;
	}
	_best_p=bestmove;
	return g;
}


bool XlLoadPosFormat(POSFORMAT *p_pos)
{
	uint8_t i;
	XlReset();
	for(i=0;i<p_pos->num;i++)
	{
		if(!PosInBoard(p_pos->pos[i])) 
			return false;
		XlPutStone(p_pos->pos[i],SIDE(i));
	}
	return true;
}

uint8_t XlSearch(POSFORMAT pos)
{
	uint8_t p,pre_num;
	char x,y,value;
	uint8_t ret = PASS;

	if(_working)
	{
		TRACE("AI engine is busy.");
		return ret;
	}
	
	_working=1;
	_cancel=0;

	if(!_initialized) XlInitialize();
	if(!_initialized)
	{
		TRACE("Not enough memory.\n");
		return ret;
	}

	if(!XlLoadPosFormat(&pos))
	{
		_working=0;
		return ret;
	}

	_com=SIDE(_num);
	
	//first move
	if(pos.num==0)
	{
		_working=0;
		ret=MAKEPOS(BW/2,BW/2);
		return ret;
	}

	//full
	if(pos.num==BW*BW)
	{
		_working=0;
		return ret;
	}

	//5
	for(x=0;x<BW;x++)
	{
		for(y=0;y<BW;y++)
		{
			p=MAKEPOS(x,y);
			if(//_layer1[p]==Empty &&
				_layer3[SIDE(_num)][p]==FMP_5)
			{
				TRACE("FIR@[%d,%d]\n",x,y);
				_working=0;
				ret=p;
				return ret;
			}
		}
	}

	//defend 4
	for(x=0;x<BW;x++)
	{
		for(y=0;y<BW;y++)
		{
			p =MAKEPOS(x,y);
			if(_layer3[!SIDE(_num)][p]==FMP_5)
			{
				TRACE("DEF@[%d,%d]\n",x,y);
				_working=0;
				ret=p;
				return ret;
			}
		}
	}


	TRACE("Searching... \n");


	_start_num=_num;
	_max_num=_num;
	_root_depth=MAX_DEPTH;
	value=0;
	
	//VCF search
	_vc4_limit=MAX_VC4_NODE;
	if((value=QVC4(SIDE(_num),NULL))>=INF)
	{
		_best_p=_vc4_p;
		//return _vc4_p;
	}
	else
	{
		//if(_sendfunc) _sendfunc(SEND_BEGIN,0);
		//value=XlSearchRoot(-INF,INF,MAX_DEPTH);
		//if(_sendfunc) _sendfunc(SEND_END,0);

		for(_root_depth=8,_std_depth=4;
			 _node < MAX_SEARCH_NODE && value<INF && _root_depth<=MAX_DEPTH && _std_depth<=MAX_DEPTH;)
		{
			pre_num=_max_num;
			TRACE("_root_depth=%d,_std_depth=%d\n",_root_depth,_std_depth);
	
			value=XlSearchRoot(-INF,INF,_root_depth);
		
			if(pre_num==_max_num)
			{
				_std_depth++;
				if(_std_depth+EXT_DEPTH>_root_depth)
					_root_depth=_std_depth+EXT_DEPTH;
			}
			else _root_depth++;
		}

	}

	if( _best_p==0xff)
		TRACE("not found.\n");
	else
		TRACE("MOV@[%d,%d].\n",POSX(_best_p),POSY(_best_p));

	//TRACE("VCF Node = %d k\n",_vc4_limit/1000);
	TRACE("Node = %dk +%dk + %dk\n",_vc4_node/1000,_block_node/1000,_node/1000);
	TRACE("Value = %d\n",value);

#ifdef _XLDEBUG
	TRACE("Eveluate = %dk\n",_dbg_eveluate/1000);
	TRACE("TT hit = %d\n",_dbg_hashtt_hit+_dbg_vc4tt_hit);
	TRACE("depth = %d\n",_max_num-_num);
	for(i=0;i<20 && _dbg_level_hit[i];i++)
	{
		int j;
		TRACE("L%d:",i);
		for(j=0;j<_dbg_level_hit[i]/1000;j++)
			TRACE("#");
		TRACE( " %d\n",_dbg_level_hit[i]);
	}
#endif

	_working=0;
	return _best_p;
}

void  XlInitialize()
{
	HISTORYNODE *p;
	uint8_t side,x,y;

	if (_initialized)
		return;
	TRACE("Initializing... ");
	XlInitBeginEnd();
	XlInitL2toL3();
	XlInitZobrist();
	if (!XlInitCache())
	{
		return;
	}

	_activeRect[0].left=BW/2;
	_activeRect[0].top=BW/2;
	_activeRect[0].right=BW/2;
	_activeRect[0].bottom=BW/2;
	_activeRect[1].left=BW/2-2;
	_activeRect[1].top=BW/2-2;
	_activeRect[1].right=BW/+2;
	_activeRect[1].bottom=BW/+2;

	//_hashTT = malloc(sizeof(HASHTTITEM)* HASHTTSIZE );
	if (!_hashTT)
	{
		XlDispose();
		return;
	}

	//生成历史表格
	_historyOrder[0] = NULL;
	_historyOrder[1] = NULL;
	for( side = 0; side <= 1 ; side++ )
	{
		for( x = 0; x < BW; x++ )
		{
			for( y = 0; y < BW; y++ )
			{
				p = (HISTORYNODE *) malloc(sizeof( HISTORYNODE));
				if (!p)
				{
					XlDispose();
					return;
				}

				p->p=MAKEPOS(x,y);
				p->value = 0;
				p->pLeft = NULL;
				p->pRight = _historyOrder[side];

				if( _historyOrder[side] )
				{
					_historyOrder[side]->pLeft = p;
				}

				_historyOrder[side] = p;
				_historyTable[side][MAKEPOS(x,y)] = p;
			}
		}
	}


	_initialized = 1;

	TRACE("done.\n");
}

void XlDispose()
{
	int side, n;
	HISTORYNODE *p;

	for (side = 0; side <= 1; side++)
	{
		for (n = 5; n <= BW; n++)
		{
			if (_cache[side][n])
				free(_cache[side][n]);
			_cache[side][n] = 0;
		}
	}

	//清除历史表格
	while( _historyOrder[0] )
	{
		p = _historyOrder[0];
		_historyOrder[0] = _historyOrder[0]->pRight;
		free(p);
	}
	while( _historyOrder[1] )
	{
		p = _historyOrder[1];
		_historyOrder[1] = _historyOrder[1]->pRight;
		free(p);
	}

	//清除其他
}

/// Output: 1-forbidden, 2-five, 3-error, 0-other
int XlCheckLastMove(POSFORMAT *p_pos)
{
	uint8_t p;
	uint8_t side;
	bool flag;

	if(p_pos->num<6) return 0;

	p_pos->num--;
	p=p_pos->pos[p_pos->num];
	flag=XlLoadPosFormat(p_pos);
	p_pos->num++;

	if(!flag) return 0;

	if(_layer1[p] != Empty) return 3;

	side=SIDE(_num);

	if(_layer3[side][p]==FMP_5) return 2;

	if(side)
	{
		if(XlIsF(p)) return 1;
	}
	return 0;
}
