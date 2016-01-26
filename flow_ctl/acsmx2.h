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
*  AC״̬����pattern�����ؼ��ֽṹ�壬AC�н���ģʽ
*/
typedef struct _acsm_pattern2
{
	struct  _acsm_pattern2 * next; // ����ָ��

    unsigned char         * patrn; // �ؼ���ָ��
	unsigned char         * casepatrn; // ��Сд���еĹؼ���ָ��
	int      n;
	int      nocase; // ���Ĵ�Сд��־
	int      offset; // �ӵ�ǰ�ַ����offsetƫ��������ʼ����
	int      depth;  // ��������Ϊ�����offsetΪdepth
	void *   id;     // �ؼ��ֽṹ��ָ��
	int      iid;    // ��ǰ�ؼ���id

} ACSM_PATTERN2;

/*
*    �ýṹ�Ǵ�����״̬ת���������е�����һ��
*    transition nodes  - either 8 or 12 bytes
*    ���������AC32����12byte acstate_t��uint
*/
typedef struct trans_node_s
{
	/* ��ֻ��������˼��Ϊ����һ��ASCII��ֵ������ĳ���ַ������統ǰ״̬��0״̬��
	   ��ģʽ��{he,she,me}����ô��ǰ��key������h��s��m�е�һ�����������h�ɣ�
	   ��ônext_state������s����m��nextָ����Ǹ�״̬��������e����Ǹ�״̬��next->key==e */
	/* The character that got us here - sized to keep structure aligned on 4 bytes */
	/* to better the caching opportunities. A value that crosses the cache line */
	/* forces an expensive reconstruction, typing this as acstate_t stops that. */
	acstate_t    key;

	acstate_t    next_state;    /* ����key��ǰ״̬Ҫ��ת����״̬�� */
	struct trans_node_s * next; /* next transition for this state */

} trans_node_t;


/*
*  User specified final storage type for the state transitions
*  �洢����
*/
enum
{
	ACF_FULL,        // ȫ����
	ACF_SPARSE,      // ϡ�����
	ACF_BANDED,      // ��״����
	ACF_SPARSEBANDS, // ϡ���״����
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
*   ACSM��AC State Machine
*/
typedef struct
{
	int acsmMaxStates; // ���״̬�� == ģʽ�������ַ�����+1
	int acsmNumStates; // ��ǰ�Ѿ��е�״̬����

	ACSM_PATTERN2    * acsmPatterns;  // ģʽ���ؼ�������
	acstate_t        * acsmFailState; // ʧ��ת����
	ACSM_PATTERN2   ** acsmMatchList; // ƥ��ɹ��������

	/* list of transitions in each state, this is used to build the nfa & dfa */
	/* after construction we convert to sparse or full format matrix and free */
	/* the transition lists */
	trans_node_t ** acsmTransTable; // ״̬��������ʵ�Ǹ�ָ�����飬�±������acsmMaxStates

	acstate_t ** acsmNextState;
	int          acsmFormat;            // ��ʼʱ��ACF_FULL
	int          acsmSparseMaxRowNodes; // ��ʼֵ��256
	int          acsmSparseMaxZcnt;     // ��ʼֵ��10
	int          acsmEnd;

	int          acsmNumTrans;
	int          acsmAlphabetSize;      // ��ʼֵ��256
	int          acsmFSA;               // �Զ������ͳ�ʼ����FSA_DFA
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

