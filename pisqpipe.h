/** header with functions and variables for pipe AI */
#ifndef XL_PISQPIPE_H_INCLUDE
#define XL_PISQPIPE_H_INCLUDE
/* information about a game - you should use these variables */
extern int width,height; /* the board size */
extern int info_timeout_turn;  /* 每招时间(ms) */
extern int info_timeout_match; /* 一场比赛的时间 */
extern int info_time_left;  /* 剩余时间 */
extern int info_max_memory; /* 内存限制(字节), 无限制时为0 */
extern int info_game_type;  /* 0:对手是人, 1:对手是机器, 2:tournament, 3:network tournament */
extern int info_exact5;     /* 0:支持长连, 1:长连不算赢 */
extern int info_continuous; /* 0:single game, 1:continuous */
extern int terminate; /* return from brain_turn when terminate>0 */
extern unsigned start_time; /* tick count at the beginning of turn */
extern char dataFolder[256]; /* folder for persistent files */
extern int time_start;
extern int time_move;

/* you have to implement these functions */
void brain_init(); /* create the board and call pipeOut("OK"); */
void brain_restart(); /* delete old board, create new board, call pipeOut("OK"); */
void brain_turn(); /* choose your move and call do_mymove(x,y);
                      0<=x<width, 0<=y<height */
void brain_my(int x,int y); /* put your move to the board */
void brain_opponents(int x,int y); /* put opponent's move to the board */
void brain_block(int x,int y); /* square [x,y] belongs to a winning line (when info_continuous is 1) */
int brain_takeback(int x,int y); /* clear one square; return value: 0:success, 1:not supported, 2:error */
void brain_end();  /* delete temporary files, free resources */

#ifndef ABOUT_FUNC
  extern const char *infotext; /* AI identification (copyright, version) */
#else
  void brain_about(); /* call pipeOut(" your AI info ") */
#endif

/* these functions are in pisqpipe.cpp */
extern int pipeOut(char *fmt,...);
extern void do_mymove(int x,int y);
extern void suggest(int x,int y);

#endif