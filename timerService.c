#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
# include <sys/sysinfo.h>
#include <errno.h>

#include "timerService.h"

// initialize interval time value
static volatile struct timeval gTimeVal = {TIMER_SERVICE_INIT_SECONDS,0};

// table of timer service
static timerServiceEvt_t * gTimerEvtT = NULL;

// maxium number of element in timer table
static unsigned int gTimerEvtMax = 128;

// used in table
static unsigned int gTimerEvtUsed = 0;

// mutex to timer service table
static pthread_mutex_t gTimerMtx;

static unsigned long gUptime;

// pipe to wake select
static int pipeFd[2];

//the timer should not be expired because it's loaded a highest value
static void timerServiceSigHandler(int sig)
{
	fprintf(stderr, "Sig handle triggered\n");
	(void)sig;
	abort();
}

static void getTimerVal(void)
{
	struct itimerval  iti;
  	if (-1 == getitimer(ITIMER_REAL, &iti))
  	{
    	fprintf(stderr, "Error in getitimer\n");
		return;
  	}

	// update gTimeVal
  	gTimeVal = iti.it_value;
}


//-----------------------------------------------------------------------------
// Name   : timeServiceExtractEvtWl
// Usage  : Extract an event timer service without locking
//-----------------------------------------------------------------------------
static int timeServiceExtractEvtWl(timeServiceDesc_t * pDesc)
{
	timerServiceEvt_t    evt;
  	struct timeval tm;
  	unsigned int   ic;
  	unsigned int   il;
  	unsigned int   ip;

  	ic = pDesc->ind;

	// Check the arguments
  	if ((ic >= gTimerEvtUsed) || (gTimerEvtT[ic].pDesc != pDesc))
  	{
		fprintf(stderr, "arguments error in timer table\n");
    	return -1;
  	}

  	// Replace the event <ic> by the latest one <gEvtUsed> and make a
  	// a copy of this previously last event in <evt>
  	gTimerEvtUsed --;
  	gTimerEvtT[ic] = gTimerEvtT[gTimerEvtUsed];
  	gTimerEvtT[ic].pDesc->ind = ic;
  	evt = gTimerEvtT[ic];
  	tm = evt.pDesc->tm;

  	//
  	// Move <evt> up
  	//
  	ip = EVT_TREE_PATER(ic);
  	while ( ic > 0 )
    {
    	if (EVT_TREE_LEQ(tm , gTimerEvtT[ip].pDesc->tm))
      	{
      		break;
      	}
    	gTimerEvtT[ic] = gTimerEvtT[ip];
    	gTimerEvtT[ic].pDesc->ind = ic;
    	ic = ip;
    	ip = EVT_TREE_PATER(ic);
   	}
  	gTimerEvtT[ic] = evt;
  	gTimerEvtT[ic].pDesc->ind = ic;
	
  	//
  	// Move <evt> down to its right place
  	//
  	il  = EVT_TREE_LEFT_FILIUS(ic);
  	while( il < gTimerEvtUsed )
    {
    	if ( il < (gTimerEvtUsed - 1) &&
        	EVT_TREE_LTH(gTimerEvtT[il].pDesc->tm , gTimerEvtT[il+1].pDesc->tm))
      	{
      		il ++;
      	}
    	if (EVT_TREE_LEQ(gTimerEvtT[il].pDesc->tm , tm))
      	{
      		break;
      	}
    	//
    	gTimerEvtT[ic] = gTimerEvtT[il];
    	gTimerEvtT[ic].pDesc->ind = ic;
    	ic = il;
    	il = EVT_TREE_LEFT_FILIUS(ic);
    }
  	gTimerEvtT[ic] = evt;
  	gTimerEvtT[ic].pDesc->ind = ic;

  	pDesc->ind = -1; // mark it as cleared

	return 0;
}

static int addTimerIntoEvt(timeServiceDesc_t * pDesc)
{
    timerServiceEvt_t evt;
    struct timeval tm;
    unsigned int ic, ip;    

    EVT_TREE_SUBSTRACT(gTimeVal, pDesc->interval, tm);
    pDesc->tm = tm;
    evt.pDesc = pDesc;

    // Reserve and fill the latest event entry with <evt>
    gTimerEvtT[gTimerEvtUsed] = evt;
    pDesc->ind = gTimerEvtUsed;

    // Move <evt> up to its right place
    ic = gTimerEvtUsed;
    ip = EVT_TREE_PATER(gTimerEvtUsed);
    while(ic > 0)
    {
        if(EVT_TREE_LEQ(tm, gTimerEvtT[ip].pDesc->tm))
        {
            break;
        }
        gTimerEvtT[ic] = gTimerEvtT[ip];
        gTimerEvtT[ic].pDesc->ind = ic;
        ic = ip;
        ip = EVT_TREE_PATER(ic);
    }
    gTimerEvtT[ic] = evt;
    gTimerEvtT[ic].pDesc->ind = ic;	

    gTimerEvtUsed ++;
    
    return ic;
}

void *timerServicehandle(void *arg)
{
	struct timeval    sleepForever = {TIMER_SERVICE_INIT_SECONDS,0};
	struct timeval    tm;
	timeServiceDesc_t  * pDesc;	
	TIMER_Fcb_t 	  fcb;
	fd_set            readfds;
	char              buf[10];
	int 			  rc;
    (void)arg;

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(pipeFd[0], &readfds);

		pthread_mutex_lock(&gTimerMtx);	
		getTimerVal();

		while(gTimerEvtUsed && EVT_TREE_LEQ(gTimeVal, gTimerEvtT[0].pDesc->tm))
		{
			pDesc = gTimerEvtT[0].pDesc;
			rc = timeServiceExtractEvtWl(pDesc);
            if(pDesc->type == CYCLE_TIMER)
            {
                rc = addTimerIntoEvt(pDesc);
            }
			fcb = pDesc->fcb;
			if(fcb)
			{
				pthread_mutex_unlock(&gTimerMtx);
				fcb(pDesc);								// run user timer callback
				pthread_mutex_lock(&gTimerMtx);
			}
		}
		if(gTimerEvtUsed)
		{
			EVT_TREE_SUBSTRACT(gTimeVal, gTimerEvtT[0].pDesc->tm, tm);
		}
		else
		{
			tm = sleepForever;
		}
		pthread_mutex_unlock(&gTimerMtx);
        
		rc = select(pipeFd[0]+1, &readfds, NULL, NULL, &tm);
		if(rc > 0) 		// new timer
		{
			if(read (pipeFd[0], buf, 10) <= 0)
			{
				fprintf(stderr, "Error data in pipe\n");
			}
		}	
		else if(rc < 0)
		{
			fprintf(stderr, "select wake up by sig %d\n", errno);
		}
	}

    return NULL;
}


void timeServiceInit(void)
{
	struct itimerval  iti;
	struct itimerval  tmp;
	struct sigaction  action;
	pthread_t timerServiceThread;

  	struct sysinfo info;

  	sysinfo(&info);
  	gUptime = info.uptime;

	// Initialize the time event table
	gTimerEvtT = (timerServiceEvt_t *) malloc(sizeof(timerServiceEvt_t) * gTimerEvtMax);
	if(NULL == gTimerEvtT)
	{
		fprintf(stderr, "Failed to allocate memory\n");
		return;
	}
	
	memset(gTimerEvtT, 0x0, gTimerEvtMax * sizeof(gTimerEvtT[0]));

	// create pthread mutex
	if(pthread_mutex_init(&gTimerMtx, NULL))
	{
		fprintf(stderr, "Failed to create mutex\n");
		return;		
	}
	
	// attach a handler on timer expiration
	memset(&action, 0, sizeof(action));
	action.sa_handler = timerServiceSigHandler;
	if(-1 == sigaction(SIGALRM, &action, NULL))
	{
		fprintf(stderr, "Failed to register signal handler\n");
		return;	
	}

	iti.it_interval.tv_sec  = 0;
	iti.it_interval.tv_usec = 0;
	iti.it_value.tv_sec     = TIMER_SERVICE_INIT_SECONDS;
	iti.it_value.tv_usec    = 0;
	if(setitimer(ITIMER_REAL, &iti, NULL) != 0)
	{
		fprintf(stderr, "Failed to initilize timer\n");
		return;
	}

    if (-1 == getitimer(ITIMER_REAL, &tmp))
    {
        fprintf(stderr, "Failed to get timer\n");
        return;
    }

    fprintf(stdout, "ittimer is %ld-%ld.\n", (unsigned long)tmp.it_value.tv_sec, (unsigned long)tmp.it_value.tv_usec);

	//create pipe
	if (0 != pipe(pipeFd))
	{
		fprintf(stderr, "Failed to open a pipe\n");
		return;
	}

	// create a thread to handle timer event
	if(pthread_create(&timerServiceThread, NULL, timerServicehandle, NULL) != 0)
	{
		fprintf(stderr, "Failed to create thread\n");
		return;
	}
    
    // wait for thread to setup
    usleep(100);
}


int startTimerEvt(timeServiceDesc_t * pDesc)
{
	pthread_mutex_lock(&gTimerMtx);
	
	if (gTimerEvtUsed >= gTimerEvtMax)
	{
		pthread_mutex_unlock(&gTimerMtx);
		fprintf(stderr, "gTimerEvtUsed %u reached maxium\n",gTimerEvtUsed);
		return -1;
	}	
	
	// Check the arguments
	getTimerVal();
	if ((gTimeVal.tv_sec <= pDesc->tm.tv_sec) || (ONE_MILLION <= pDesc->tm.tv_usec))
	{
		pthread_mutex_unlock(&gTimerMtx);
		fprintf(stderr, "timeval=%lu-%lu\n",(unsigned long)(gTimeVal.tv_sec),(unsigned long)(gTimeVal.tv_usec));
		return -1;
	}

    // 0: indicate the new timer is the first node in Tree. So need to notify thread update timeout of select  
	if(addTimerIntoEvt(pDesc) == 0)
    {
        if(1 != write(pipeFd[1], "a", 1))
	    {
		    fprintf(stderr, "Failed to write pipe\n");
  		    gTimerEvtUsed --;
	    }
    }

	pthread_mutex_unlock(&gTimerMtx);

	return 0;
}

int stopTimerEvt(timeServiceDesc_t * pDesc)
{
    pthread_mutex_lock(&gTimerMtx);
    if(timeServiceExtractEvtWl(pDesc))
    {
        pthread_mutex_unlock(&gTimerMtx);
        fprintf(stderr, "Failed to stop timer\n");
        return -1;
    }
    pthread_mutex_unlock(&gTimerMtx);
    return 0;
}

#if 0
static struct timeval dumpBaseTim;
static struct timeval dumpTim;
static void timerServiceDumpOne(unsigned int ic, timeServiceDesc_t *pFather, unsigned int count)
{
	timeServiceDesc_t *pDesc;

	if(ic >= gTimerEvtUsed) 
		return;

	pDesc = gTimerEvtT[ic].pDesc;
	count ++;
	fprintf(stdout, "%*s", 2*count, " ");
	if(EVT_TREE_LEQ(dumpBaseTim, pDesc->tm))
	{
		fprintf(stderr, "expired since \n");
		EVT_TREE_SUBSTRACT(pDesc->tm, dumpBaseTim, dumpTim);
	}
	else
	{
		if(pFather && EVT_TREE_LTH(pFather->tm, pDesc->tm))
		{
			fprintf(stderr, " !!INCOHERENCY!! \n");
		}
		EVT_TREE_SUBSTRACT(dumpBaseTim, pDesc->tm, dumpTim);
	}

	fprintf(stdout, "%u.%03u [%.16s]  idf(%d)\n", (unsigned int)dumpTim.tv_sec,
				(unsigned int)dumpTim.tv_usec/1000, pDesc->name, pDesc->ind);
	timerServiceDumpOne(EVT_TREE_LEFT_FILIUS(ic), pDesc, count+1);
	timerServiceDumpOne(EVT_TREE_LEFT_FILIUS(ic)+1, pDesc, count+1);
}

void timerServiceDumpTable(void)
{
	pthread_mutex_lock(&gTimerMtx);
	getTimerVal();
	dumpBaseTim = gTimeVal;
	timerServiceDumpOne( 0, NULL, 0 );
	pthread_mutex_unlock(&gTimerMtx);
	fprintf(stdout, "\n   idf means: Internal (i)ndex - Static (d)escriptor - Callback (f)unction\n\n");
}
#endif

//-----------------------------------------------------------------------------
// Name   : btime_get_evt_remain
// Usage  : Get the remaining time of a timer
//-----------------------------------------------------------------------------
void timerServiceGetEvtRemain (
           const timeServiceDesc_t *pDesc, // IN  - Static timer descriptor
           struct timeval       *tvp      	// OUT - Remaining time
               )
{
	pthread_mutex_lock(&gTimerMtx); 
  	getTimerVal();
  	if ((pDesc->ind < 0) || EVT_TREE_LEQ(gTimeVal, pDesc->tm))
  	{
    	tvp->tv_sec = tvp->tv_usec = 0;
  	}
  	else
  	{
    	TIMERSUB(&gTimeVal, &pDesc->tm, tvp);
  	}
 	pthread_mutex_unlock(&gTimerMtx); 
}

void timeServiceGetDecrement(struct timeval * tvp)
{
	pthread_mutex_lock(&gTimerMtx);
  	getTimerVal();
  	*tvp = gTimeVal;
	pthread_mutex_unlock(&gTimerMtx);
}

void timeServiceGetUptime(struct timeval * tvp)
{
  	struct timeval tv1 = {TIMER_SERVICE_INIT_SECONDS, 0};
  	struct timeval tv2;

  	timeServiceGetDecrement(&tv2);
  	TIMERSUB(&tv1, &tv2, tvp);
  	tvp->tv_sec += gUptime;
}
