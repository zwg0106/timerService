#ifndef TIMERSERVICE_H
#define TIMERSERVICE_H

#define ONE_MILLION					1000000
#define TIMER_SERVICE_INIT_SECONDS  1000000000L

typedef enum 
{
    CYCLE_TIMER = 0,
    ONCE_TIMER
}TIMER_TYPE;

//-----------------------------------------------------------------------------
// Name   : timer evt tree
// Usage  : macros to manage the gTimerEvtT's binary tree
//-----------------------------------------------------------------------------
#define EVT_TREE_PATER(e)       (((e)-1)/2)			// father
#define EVT_TREE_LEFT_FILIUS(e) (((e)*2)+1)			// left son
#define EVT_TREE_LEQ(t1,t2)   (((t1).tv_sec  <  (t2).tv_sec) || \
                               ((t1).tv_sec  == (t2).tv_sec  && \
                                (t1).tv_usec <= (t2).tv_usec)) // [ t1 <= t2 ]
#define EVT_TREE_LTH(t1,t2)   (((t1).tv_sec  <  (t2).tv_sec) || \
                               ((t1).tv_sec  == (t2).tv_sec  && \
                                (t1).tv_usec <  (t2).tv_usec)) // [ t1 < t2 ]

#define EVT_TREE_SUBSTRACT(t1,t2,tr)                              \
  do {                                                            \
  (tr).tv_sec = (t1).tv_sec  - (t2).tv_sec;                       \
  if ( (t1).tv_usec >= (t2).tv_usec )                             \
    {                                                             \
    (tr).tv_usec = (t1).tv_usec  - (t2).tv_usec;                  \
    }                                                             \
  else                                                            \
    {                                                             \
    (tr).tv_usec = ONE_MILLION + (t1).tv_usec - (t2).tv_usec;     \
    (tr).tv_sec --;                                               \
    }                                                             \
  } while(0) // tr = t1 - t2

#define TIMERSUB(a, b, result)                          		\
  do {                                       					\
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;               \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;            \
    if ((result)->tv_usec < 0) {                         		\
      --(result)->tv_sec;                            			\
      (result)->tv_usec += 1000000;                          	\
    }                                            				\
  } while (0)

typedef struct timeServiceDesc_s timeServiceDesc_t;
typedef void (*TIMER_Fcb_t)(timeServiceDesc_t *);
struct timeServiceDesc_s
{
	struct timeval tm;			// value to be expired		
	struct timeval interval;    // interval to be expired
    TIMER_Fcb_t    fcb;			// callback on timer expiration
	const char *   name;		// timer name
	int            ind;			// index to be moved in heap tree
	TIMER_TYPE	   type;		// Timer Type
};

typedef struct
{
	timeServiceDesc_t * pDesc;	//pointer to timer descriptor 
} timerServiceEvt_t;

#define TIME_SERVICE_INIT_DESC(PDesc,PName,Fcb,Sec,USec) \
	do {\
    	(PDesc)->ind = -1;\
        (PDesc)->name = PName;\
        (PDesc)->fcb = Fcb;\
        (PDesc)->tm.tv_sec = Sec;\
        (PDesc)->tm.tv_usec = USec;\
        (PDesc)->interval.tv_sec = Sec;\
        (PDesc)->interval.tv_usec = USec;\
    } while(0)

void timeServiceInit(void);

extern int startTimerEvt(timeServiceDesc_t * pDesc);
extern int stopTimerEvt(timeServiceDesc_t * pDesc);

extern void timerServiceGetEvtRemain (
           const timeServiceDesc_t *pDesc,  // IN  - Static timer descriptor
           struct timeval       *tvp        // OUT - Remaining time
               );

extern void timeServiceGetUptime(struct timeval * tvp);
#endif
