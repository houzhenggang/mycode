/*
**   ACSMX2.H
**
**   Version 2.0
**
*/
#ifndef ACSMX2S_H
#define ACSMX2S_H

#include <rte_spinlock.h>
//#include <net/surfront/sft_struct.h>

//#ifdef inline
//#   undef inline
//#endif

//#define inline

/*
*   DEFINES and Typedef's
*/
#define MAX_ALPHABET_SIZE 256

/*
   FAIL STATE for 1,2,or 4 bytes for state transitions

   Uncomment this define to use 32 bit state values
   #define AC32
*/

/* #define AC32 */

#ifdef AC32

typedef  unsigned int   acstate_t;
#   define ACSM_FAIL_STATE2  0xffffffff

#else

typedef    unsigned short acstate_t;
#   define ACSM_FAIL_STATE2 0xffff

#endif

/*
*  AC状态机的pattern，即关键字结构体，AC中叫做模式
*/
typedef struct _acsm_pattern2
{
	struct  _acsm_pattern2 * next; // 链表指针

    unsigned char         * patrn; // 关键字指针
	unsigned char         * casepatrn; // 大小写敏感的关键字指针
	int      n;
	int      nocase; // 关心大小写标志
	int      offset; // 从当前字符块的offset偏移量处开始搜索
	int      depth;  // 搜索长度为相对于offset为depth
	void *   id;     // 关键字结构体指针
	int      iid;    // 当前关键字id

} ACSM_PATTERN2;

/*
*    该结构是存在在状态转换表数组中的其中一条
*    transition nodes  - either 8 or 12 bytes
*    如果定义了AC32就是12byte acstate_t是uint
*/
typedef struct trans_node_s
{
	/* 我只从字面意思认为就是一个ASCII数值，代表某个字符，比如当前状态是0状态，
	   有模式组{he,she,me}，那么当前的key可能是h，s，m中的一个，比如就是h吧，
	   那么next_state可能是s或者m，next指向的那个状态就是输入e后的那个状态，next->key==e */
	/* The character that got us here - sized to keep structure aligned on 4 bytes */
	/* to better the caching opportunities. A value that crosses the cache line */
	/* forces an expensive reconstruction, typing this as acstate_t stops that. */
	acstate_t    key;

	acstate_t    next_state;    /* 输入key后当前状态要跳转到的状态号 */
	struct trans_node_s * next; /* next transition for this state */

} trans_node_t;


/*
*  User specified final storage type for the state transitions
*  存储类型
*/
enum
{
	ACF_FULL,        // 全矩阵
	ACF_SPARSE,      // 稀疏矩阵
	ACF_BANDED,      // 带状矩阵
	ACF_SPARSEBANDS, // 稀疏带状矩阵
};

#define ACT_HAVEEND_MATCH 0
#define ACT_NOEND_MATCH 1

enum
{
	AC_SEQUENCE_BGN, // begin
	AC_SEQUENCE_MID, // middle
	AC_SEQUENCE_END, // end
}; 

/*
*   User specified machine types
*
*   TRIE : Keyword trie
*   NFA  :
*   DFA  :
*/
enum
{
	FSA_TRIE,
	FSA_NFA,
	FSA_DFA,
};

/*
*   Aho-Corasick State Machine Struct - one per group of pattterns
*   ACSM即AC State Machine
*/
typedef struct
{
	int acsmMaxStates; // 最大状态数 == 模式串所有字符个数+1
	int acsmNumStates; // 当前已经有的状态个数

	ACSM_PATTERN2    * acsmPatterns;  // 模式表（关键字链表）
	acstate_t        * acsmFailState; // 失败转换表
	ACSM_PATTERN2   ** acsmMatchList; // 匹配成功表（输出表）

	/* list of transitions in each state, this is used to build the nfa & dfa */
	/* after construction we convert to sparse or full format matrix and free */
	/* the transition lists */
	trans_node_t ** acsmTransTable; // 状态换换表，其实是个指针数组，下标最大是acsmMaxStates

	acstate_t ** acsmNextState;
	int          acsmFormat;            // 初始时是ACF_FULL
	int          acsmSparseMaxRowNodes; // 初始值是256
	int          acsmSparseMaxZcnt;     // 初始值是10
	int          acsmEnd;

	int          acsmNumTrans;
	int          acsmAlphabetSize;      // 初始值是256
	int          acsmFSA;               // 自动机类型初始化是FSA_DFA
}ACSM_STRUCT2;


typedef struct acsm_search_info_s
{
    unsigned int count; // Searched count
    acstate_t state;    // ACSM state
    unsigned char seq;  // Search state
    rte_spinlock_t lock;
}acsm_search_info_t;


/*
* print info
*/
#define ALERT(format, ...) do{printf(format, ## __VA_ARGS__);}while(0)
#define MEMASSERT(p,s) if(!p){printf("ACSM-No Memory: %s!\n", s); return;}
#define MEMASSERT1(p,s) if(!p){printf("ACSM-No Memory: %s!\n", s); return NULL;}
#define MEMASSERT2(p,s) if(!p){printf("ACSM-No Memory: %s!\n", s); return -1;}

/*
*   Prototypes
*/
void Print_DFA(ACSM_STRUCT2 * acsm);
ACSM_STRUCT2 * acsmNew2(void);
int acsmAddPattern2(ACSM_STRUCT2 * p, unsigned char * pat, int n,
					int nocase, int offset, int depth, void *  id, int iid);
int acsmCompile2(ACSM_STRUCT2 * acsm);
int acsmSearch2(ACSM_STRUCT2 * acsm, unsigned char * T, int n,
				int (* Match)(void * id, int index, void * data, void * arg),
				void * data, void * arg);
void acsmSearchInit(acsm_search_info_t * acsm_search_info);
int acsmSearch3(ACSM_STRUCT2 * acsm, unsigned char *Tx, int n,
                acsm_search_info_t * acsm_search_info,
                int(*Match)(void * id, int index, void *data,void *arg),
                void *data,void *arg);

void acsmFree2(ACSM_STRUCT2 * acsm);


int  acsmSelectFormat2(ACSM_STRUCT2 * acsm, int format);
int  acsmSelectFSA2(ACSM_STRUCT2 * acsm, int fsa);

void acsmSetMaxSparseBandZeros2(ACSM_STRUCT2 * acsm, int n);
void acsmSetMaxSparseElements2(ACSM_STRUCT2 * acsm, int n);
int  acsmSetAlphabetSize2(ACSM_STRUCT2 * acsm, int n);
void acsmSetVerbose2(int n);

void acsmPrintInfo2(ACSM_STRUCT2 * p);

int acsmPrintDetailInfo2(ACSM_STRUCT2 *);
int acsmPrintSummaryInfo2(void);

int acsmSearchSparseDFA_Full3(ACSM_STRUCT2 * acsm, unsigned char ** Tx, int n,
							   int (* Match)(void * id, int index, void * data),
							   void * data);


#endif

