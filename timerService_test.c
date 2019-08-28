#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "timerService.h"

typedef struct
{
	timeServiceDesc_t b;     	// Descriptor for TIME
	char           nm[16]; 		// Name
	int            v;     		// Value  
    TIMER_TYPE     type;
} tsTest_t;

static long long getMsCounter(void)
{
	static struct timeval tv0;
	static int initDone = 0;
	struct timeval  tv1, tv;
	if (0 == initDone)
	{
        timeServiceGetUptime(&tv0);
    	initDone = 1;
  	}
  	timeServiceGetUptime(&tv1);
  	TIMERSUB(&tv1, &tv0, &tv);
	return (((long long)tv.tv_sec)*1000) + (((long long)tv.tv_usec)/1000);
}

static void fct_incr(tsTest_t * p)
{
  	struct timeval  tv;
  	timerServiceGetEvtRemain(&p->b, &tv);
  	fprintf(stdout, "fct_incr(): [%s] tv=%lusec/%luusec called at %llu milliseconds\n", p->nm, tv.tv_sec, tv.tv_usec, getMsCounter());

  	p->v++;
}

static void TestTimer(unsigned int sec, unsigned long usec, tsTest_t * pDsc)
{
  	int rc = 0;

  	pDsc->b.tm.tv_sec  = sec;
  	pDsc->b.tm.tv_usec = usec;
  	pDsc->b.interval.tv_sec  = sec;
  	pDsc->b.interval.tv_usec = usec;
  	pDsc->b.fcb         = (TIMER_Fcb_t)fct_incr;
  	sprintf(pDsc->nm,"s_%u",sec);
  	pDsc->b.name        = pDsc->nm;
  	pDsc->b.ind        = -1;
    if(sec % 2)
    {
        pDsc->b.type       = CYCLE_TIMER;
  	}
    else
    {
        pDsc->b.type       = ONCE_TIMER;
    }
    rc = startTimerEvt(&pDsc->b);
	if(rc)
	{
		fprintf(stderr, "Failed to create timer %s\n", pDsc->b.name);
		return;
	}
}

int main(void)
{
  	tsTest_t  * pDsc;
  	unsigned int    evtMax = 40;
    unsigned int i = 0;

	pDsc = (tsTest_t *) malloc( (evtMax + 1) * sizeof(tsTest_t) );

	if(pDsc == NULL)
	{
		fprintf(stderr, "Failed to get tsTest Dsc\n");
		return 1;
	}

    timeServiceInit();

    for(i=0; i<10; i++)
    {
        
	    TestTimer(i+1, lrand48() % 1000000, &pDsc[i]);
    }
//sleep(100);
	
	while(1)
    {
        sleep(1);
    }

    return 0;
}
