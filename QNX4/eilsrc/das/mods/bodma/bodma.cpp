/*
  Bodma Program : Device Driver for the Bomem Seq36G Card handles the RS422
  data signal from the 'Bomem Radiometer Stuff' and transfers the data to the
  computer via the Bomem Software interface ported by eil: seq36.lib.
*/

// header files 
#include "lib/bo_proto.h"
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <math.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/proxy.h>
#include <sys/kernel.h>
#include <sys/name.h>
#include <sys/times.h>
#include "filename.h"
#include "das.h"
#include "eillib.h"
#include "nortlib.h"
#include "collect.h"
#include "globmsg.h"
#include "bodma.h"

#ifdef MY_DEBUG
#include <iostream.h>
#endif

// defines 
#define HDR "bo"
#define MY_ROOTNAME "bo"
#define OPT_MINE "xQC:i:d:p:L:N:r:m:F:"
#define NULC '\000'
#define BILLION 1000000000.0

#define CLOSEFILE { \
    if (fp!=NULL) fclose(fp); \
    fp=NULL; \
}

// fopen sets errno (=2) if the file dosn't already exist 
#define SWITCHFILE { \
    CLOSEFILE; \
    if ( (fp=fopen(getfilename(name,dirname,rootname,fcount++,filesperdir,1), \
		   "wb"))==NULL ) { \
	msg(MSG_FAIL,"Can't open file %s",basename(name)); \
        goto cleanup; \
    } \
    if (errno == ENOENT) errno = 0; \
}

#define Update_Status(x) \
{ \
buf.status = (x); \
if (bodma_send) Col_send(bodma_send); \
}

// global variables 
extern "C" {
char *opt_string=OPT_MSG_INIT OPT_MINE OPT_CC_INIT;
};
int terminated;
send_id bodma_send;
static bodma_frame buf;
char *rec_buf;
BoGrams_status bostat;
#ifdef NO_FLOAT
long huge *d;
#else
float huge *d;
#endif
char *dp;
int fcount;
char rootname[ROOTLEN];
char dirname[FILENAME_MAX];
int filesperdir;
int logging;
static bo_file_header bohdr;
static FILE *fp;
char name[FILENAME_MAX];

void my_signalfunction(int sig_number) {
  terminated = 1;    
}

#ifdef MY_DEBUG
extern volatile short num;
extern volatile unsigned short d0;
extern volatile unsigned long d1;
extern volatile long d2;
extern volatile long d3;
extern volatile long d4;
extern volatile short n;
#endif

void main(int argc, char **argv) {

  // getopt variables 
  extern char *optarg;
  extern int optind, opterr, optopt;

  // local variables
  struct timespec beg, end;
  float elapse, el;
  struct tms cputim;
  reply_type rep;
  int quit_no_dg;
  int name_id;
  pid_t cc_id, from, data_proxy, test_proxy, work_proxy;
  short irq, dma;
  unsigned short port;
  int code;
  long npts;
  float spd;
  short res;
  short sp;
  int i,k;
  long m,n;
  int test_mode;
  long test_scans;
  Seq36_Req test_req;
  timer_t timer_id;
  struct itimerspec timer;
  struct sigevent event;
  int busy;
  unsigned int num_busy;

  // initialise msgs and signals 
  signal(SIGQUIT,my_signalfunction);
  signal(SIGINT,my_signalfunction);
  signal(SIGTERM,my_signalfunction);
  msg_init_options(HDR,argc,argv);
  BEGIN_MSG;
  if (seteuid(0) == -1) msg(MSG_WARN,"Can't set euid to root");
#ifdef NO_FLOAT
  msg(MSG_DEBUG,"Code Version: Test Bed: No Floating Point: Node %lu",getnid());
#else
  msg(MSG_DEBUG,"Code Version: Test Bed: Floating Point: Node %lu",getnid());
#endif
  // var initialisations 
  strcpy(rootname,MY_ROOTNAME);
  getcwd(dirname,FILENAME_MAX);
  rep = REP_NO_REPLY; // No reply needed 
  logging = 1;
  fcount = 0;
  filesperdir = FILESPERDIR;
  data_proxy = 0;
  test_proxy = 0;
  work_proxy = 0;
  terminated = 0;
  cc_id = from = 0;
  bodma_send = 0;
  quit_no_dg = 0;
  irq = 5;
  dma = 5;
  port = 0x220;
  code = 0;
  npts = 0;
  spd = 0.0;
  res = 0;
  sp = 0;
  busy = 0;
  num_busy = 0;
  test_mode = 0;
  test_scans = 1;
  d = NULL;
  if ( (rec_buf = (char *)malloc(MAX_MSG_SIZE)) == NULL)
    msg(MSG_FATAL,"Can't allocate %d bytes of memory",MAX_MSG_SIZE);

  // process args 
  opterr = 0;
  optind = 0;
  do {
    i=getopt(argc,argv,opt_string);
    switch (i) {
    case 'C': test_mode = 1; test_scans = atol(optarg); break;
    case 'x': logging = 0; break;
    case 'd': strncpy(dirname,optarg,FILENAME_MAX-1);  break;
    case 'L': fcount=atoi(optarg) + 1; break;
    case 'F': break;
    case 'r': strncpy(rootname,optarg,ROOTLEN-1);  break;
    case 'N': filesperdir=atoi(optarg); break;
    case 'Q': quit_no_dg = 1; break;
    case 'i': irq = atoi(optarg); 
      switch (irq) {
      case 3: case 5: case 7: case 10: case 11: case 14: case 15: break;
      default: msg(MSG_FATAL,"Invalid IRQ: %d",irq); break;
      }
      break;
    case 'm': dma = atoi(optarg);
      switch (dma) {
      case 5: case 6: case 7: break;
      default: msg(MSG_FATAL,"Invalid DMA channel: %d",dma); break;
      }
      break;
    case 'p': port = atoh(optarg);
      switch (port) {
      case 0x300: case 0x220: case 0x3A0: break;
      default: msg(MSG_FATAL,"Invalid I/O Port Address: %x",port); break;
      }
      break;
    case '?': msg(MSG_FATAL,"Invalid option -%c",optopt);
    default: break;
    }
  } while (i!=-1);
  
  if (logging) {
    if ( (access(dirname,F_OK)) == -1)
      msg(MSG_EXIT_ABNORM,"Error Accessing Directory %s",dirname);
  } else msg(MSG,"Logging Disabled");
  
  msg(MSG,"IRQ: %d; PORT: %X; DMA: %d",irq,port,dma);
#ifdef NO_FLOAT
  if ( (d = (long huge *) halloc(65536L, sizeof(long))) == NULL)
    msg(MSG_FATAL,"Can't allocate %ld bytes of memory",65536L * sizeof(long));
#else
  if ( (d = (float huge *) halloc(65536L, sizeof(float))) == NULL)
    msg(MSG_FATAL,"Can't allocate %ld bytes of memory",65536L * sizeof(float));
#endif

  // set up proxy for data ready notification
  if ( (data_proxy = qnx_proxy_attach(0,NULL,0,-1)) == -1)
    msg(MSG_FATAL, "Can't set Proxy for Data Ready Notification");

  // set up proxy for work
  if ( (work_proxy = qnx_proxy_attach(0,NULL,0,-1)) == -1)
    msg(MSG_FATAL, "Can't set Proxy for ISR");

  // set up test mode 
  if (test_mode) {
    msg(MSG,"Test Mode");
    test_req.hdr = SEQ36;
    test_req.scans = test_scans;
    test_req.david_code = -1;    
    strncpy(test_req.david_pad,"Testing",8);
    // set up test proxy
    if ((test_proxy=qnx_proxy_attach(0, &test_req, sizeof(test_req),-1))==-1)
      msg(MSG_FATAL,"Can't make test proxy");
    event.sigev_signo = -test_proxy;
    if ((timer_id = timer_create(CLOCK_REALTIME, &event)) == -1)
      msg(MSG_FATAL,"Can't attach timer for test mode");
    timer.it_value.tv_sec = 3L;
    timer.it_value.tv_nsec = 0L;
    timer.it_interval.tv_sec = (long)(0.9 * test_scans);
    timer.it_interval.tv_nsec = 
      (long)(((0.9 * test_scans) - floor(0.9 * test_scans)) * BILLION);
    msg(MSG,"Test Mode Timer Interval: %lf secs",
	timer.it_interval.tv_sec + timer.it_interval.tv_nsec/BILLION);
    if (timer_settime(timer_id, 0, &timer, NULL) == -1)
      msg(MSG_FATAL,"Can't Set Timer for Test Mode");
  }

  // register with cmdctrl 
  cc_id = cc_init_options(argc,argv,0,0,SEQ36,SEQ36,FORWARD_QUIT);

  set_response(NLRSP_WARN);	// for norts Col_send_init 

  if (!test_mode)
    if ((bodma_send=Col_send_init(
	   BODMA_STRING,&buf,sizeof(bodma_frame))) ==NULL)
      if (quit_no_dg) {
	msg(MSG_FAIL,"Can't cooperate with DG");
	goto cleanup;
      }
      else {
	msg(MSG_WARN,"Can't cooperate with DG");
	errno = 0;
	set_response(NLRSP_QUIET); // for norts Col_send_init 
      }
    else msg(MSG,"Achieved Cooperation with DG");

  Update_Status(2); // Beginning Initialisation 
  msg(MSG,"Initialising Bomem Data Acquisition System");
  if ( (code = open(6,1,irq,dma,port,15799.7,1048576L/*524288L*//*262144L*/,NULL,60,
	data_proxy,work_proxy))) {
    msg(MSG_FAIL,"Seq36 Open Error Return: %d",code);
    goto cleanup;
  }
  msg(MSG_DEBUG,"End of Initialising Bomem Data Acquisition System");
  if (terminated) goto cleanup;

  // attach name and advertise services 
  if ( (name_id = qnx_name_attach(getnid(),LOCAL_SYMNAME(BODMA))) == -1) {
    msg(MSG_FAIL,"Can't attach symbolic name for %s",BODMA);
    goto cleanup;
  }
  Update_Status(1); // Ready 

  while (!terminated) {

    from = 0;
    strnset(rec_buf,NULC,MAX_MSG_SIZE);
    if ( (from = Receive(0, rec_buf, MAX_MSG_SIZE)) == -1)
      if (errno != EINTR) {
	msg(MSG_FAIL,"Error Receiving");
	goto cleanup;
      }
      else continue;

    rep = REP_OK; // reply needed

    if (from == work_proxy) {
      msg(MSG_DBG(3),"Got Interrupt");
      rep = REP_NO_REPLY;
      work ();
    }
    else if (from == cc_id && rec_buf[0]==DASCMD) {
      // QUIT
      if (rec_buf[1]==DCT_QUIT && rec_buf[2]==DCV_QUIT)
	terminated = 1;
      else {
	msg(MSG_WARN,"Unknown Msg from Cmdctrl: Pid %d: Hdr: %d",cc_id,
	    rec_buf[0]);
	rep = REP_UNKN;
      }
    }
    else if (from == data_proxy) {
      Update_Status(5); // Acquisition is complete
      rep = REP_NO_REPLY;
      // requested data ready proxy from modified seq36.lib
      msg(MSG_DEBUG,"Getting Data");
      if ( (code = get_data(0, &bostat, 65536L, d))) {
	msg(MSG_FAIL,"Seq36 Get_Data Error Return: %d",code);
	goto cleanup;
      }
      busy = 0;
      if (bostat.npts != npts) {
	npts = bostat.npts;
	msg(MSG,"Number of Points in Interferogram: %ld",npts);
      }
      if (bostat.spd != spd) {
	spd = bostat.spd;
	msg(MSG,"Scans per Minute: %f",spd);
      }
      if (bostat.res != res) {
	res = bostat.res;
	msg(MSG,"Resolution: %d cm-1",res);
      }
      if (bostat.sp != sp) {
	sp = bostat.sp;
	msg(MSG,"Samples per Fringe: %d",sp);
      }
#ifdef NO_FLOAT
      msg(MSG_DEBUG,"DOUBLE INTERFEROGRAM: %ld %ld %ld %ld %ld ...",*d,*(d+1),
	  *(d+2), *(d+3), *(d+4));      
#else
      msg(MSG_DEBUG,"DOUBLE INTERFEROGRAM: %f %f %f %f %f ...",*d,*(d+1),
	  *(d+2), *(d+3), *(d+4));      
#endif
      // log data
      bohdr.seq = fcount;
      buf.seq = fcount;
      bohdr.time = bostat.tmf;
      bohdr.scans = buf.scans;
      bohdr.npts = bostat.npts;
      Update_Status(6);
      if (logging) {
	SWITCHFILE;
	if (fwrite(&bohdr, 1, sizeof(bo_file_header), fp)
	    != sizeof(bo_file_header)) {
	  msg(MSG_FAIL,"Error Writing Header to %s",name);
	  goto cleanup;
	}
	m = bostat.npts *
#ifdef NO_FLOAT
 sizeof(long);
#else
 sizeof(float);
#endif
	for (k=1024,n=0; n<m; n+=1024) {
	  dp = (char *)(d + n/
#ifdef NO_FLOAT
sizeof(long));
#else
sizeof(float));
#endif
	  if ((m-n) < 1024) k = m-n;
	  if (fwrite(dp,1,k,fp) != k) {
	    msg(MSG_FAIL,"Error Writing Data to %s",name);
	    goto cleanup;
	  }
	}
	CLOSEFILE;
      }
      Update_Status(7); // End of Acquisition and/or logging
      clock_gettime(CLOCK_REALTIME, &end);
      elapse = (end.tv_sec-beg.tv_sec)+((end.tv_nsec-beg.tv_nsec)/BILLION);
      msg(MSG_DEBUG,"Elapsed Time for Acquisition and Logging: %f", elapse);
      Update_Status(1); // Ready
    }
    else {
      if (rec_buf[0]==SEQ36) {
	if (from == test_proxy) rep = REP_NO_REPLY;
	if ( ((Seq36_Req *)rec_buf)->scans == 0) {
	  msg(MSG_WARN,
          "Bad Request: Asked for Single Interferogram: Process %d", from);
	  if (from != test_proxy) rep = REP_UNKN;
	}
	else if ( ((Seq36_Req *)rec_buf)->scans < 0) {
	  if (rep != REP_NO_REPLY) {
	    Reply(from, &rep, REPLY_SZ);
	    rep = REP_NO_REPLY;
	  }
	  msg(MSG_DEBUG,"Stopping Bomem Data Acquisition");
	  if ( (code = stop())) {
	    msg(MSG_FAIL,"Seq36 Stop Error Return %d",code);
	    goto cleanup;
	  } else {
	    bohdr.david_code = 0;
	    bohdr.scans = 0;
	    strnset(bohdr.david_pad,NULC,8);
	  }
	}
	else {
	  if (busy) {
	    msg(MSG_DBG(2),"Request for data, but already BUSY");
	    num_busy++;
	    if (from != test_proxy) rep = REP_BUSY;
	  }
	  if (rep != REP_NO_REPLY) {
	    Reply(from, &rep, REPLY_SZ);
	    rep = REP_NO_REPLY;
	  }
	  if (!busy) {
	    Update_Status(3); // Beginning Acquisition Sequence
	    msg(MSG_DEBUG,"Requesting Data: Scans: %d",
		  ((Seq36_Req *)rec_buf)->scans);
	    clock_gettime(CLOCK_REALTIME, &beg);  
	    if ( (code = start(1,0,
			       ((Seq36_Req *)rec_buf)->scans,
			       1,0,1,0.0,0.0,0,0,0))) {
	      msg(MSG_FAIL,"Seq36 Start Error Return %d",code);
	      goto cleanup;
	    } else {
	      busy = 1;
	      buf.scans = ((Seq36_Req *)rec_buf)->scans;
	      bohdr.david_code = ((Seq36_Req *)rec_buf)->david_code;
	      strncpy(bohdr.david_pad,((Seq36_Req *)rec_buf)->david_pad,8);
	      Update_Status(4); // Acquisition Begun, waiting for result
	    }
	  }
	}
      }
    }
    if (rep != REP_NO_REPLY) {
      Reply(from, &rep, REPLY_SZ);
      rep = REP_NO_REPLY;
    }
  }

  // cleanup and report stats 
 cleanup:
  Update_Status(0); // We're Quitting 
  CLOSEFILE;
  if (rep != REP_NO_REPLY && from > 0) {
    Reply(from, &rep, REPLY_SZ);
    rep = REP_NO_REPLY;
  }
  if (d) hfree(d);
  if (name_id>0) qnx_name_detach(0,name_id);
  if (bodma_send) Col_send_reset(bodma_send);
  if (data_proxy>0) qnx_proxy_detach(data_proxy);
  if (test_proxy>0) qnx_proxy_detach(test_proxy);
  if (work_proxy>0) qnx_proxy_detach(work_proxy);
  if (rec_buf) free(rec_buf);
  switch (code) {
  case 0: break; // Success Code
  case -1: msg(MSG_FAIL,"Bomem: Not Enough Memory in Open"); break;
  case -2: msg(MSG_FAIL,"Bomem: Micro Code Not Found in Open"); break;
  case -3: msg(MSG_FAIL,"Bomem: Acquisition Board Not Found in Open"); break;
  case -4: msg(MSG_FAIL,"Bomem: Driver Already Open in Open"); break;
  case -5: msg(MSG_FAIL,"Bomem: DMA buffer or channel not available in Open"); break;
  case -6: msg(MSG_FAIL,"Bomem: DSP Error"); break;
  case -7: msg(MSG_FAIL,"Bomem: Parameter Out of Range in Open"); break;
  case -8: msg(MSG_FAIL,"Bomem: Time-Out in Open"); break;
  case -9: msg(MSG_FAIL,"Bomem: Other Error"); break;
  case -100: msg(MSG_FAIL,"Bomem: Driver Not Open in Close"); break;
  case -101: msg(MSG_FAIL,"Bomem: Other Error"); break;
  case -200: msg(MSG_FAIL,"Bomem: Not Enough Memory in Start"); break;
  case -201: msg(MSG_FAIL,"Bomem: Driver Not Open in Start"); break;
  case -204: msg(MSG_FAIL,"Bomem: Status Not Available in Start"); break;
  case -205: msg(MSG_FAIL,"Bomem: Other Error"); break;
  case -206: msg(MSG_FAIL,"Bomem: Time-Out in Start"); break;
  case -207: msg(MSG_FAIL,"Bomem: FIFO too Small for Requested Acquisition in Start"); break;
  case -300: msg(MSG_FAIL,"Bomem: Not Enough Memory in Get_Data"); break;
  case -301: msg(MSG_FAIL,"Bomem: Driver Not Open in Get_Data"); break;
  case -304: msg(MSG_FAIL,"Bomem: Status Not Available in Get_Data"); break;
  case -305: msg(MSG_FAIL,"Bomem: Other Error"); break;
  case -306: msg(MSG_FAIL,"Bomem: Time-Out in Get_Data"); break;
  case -307: msg(MSG_FAIL,"Bomem: Acquisition Not in Progress in Get_Data"); break;
  case -308: msg(MSG_FAIL,"Bomem: FIFO Underrun in Get_Data"); break;
  case -400: msg(MSG_FAIL,"Bomem: Not Enough Memory in Get_Status"); break;
  case -401: msg(MSG_FAIL,"Bomem: Driver Not Open in Get_Status"); break;
  case -404: msg(MSG_FAIL,"Bomem: Status Not Availbale in Get_Status"); break;
  case -405: msg(MSG_FAIL,"Bomem: Other Error"); break;
  case -406: msg(MSG_FAIL,"Bomem: Time-Out in Get_Status"); break;
  case -500: msg(MSG_FAIL,"Bomem: Not Enough Memorry in ABort"); break;
  case -501: msg(MSG_FAIL,"Bomem: Driver Not Open in Abort"); break;
  case -504: msg(MSG_FAIL,"Bomem: Status Not Available in Abort"); break;
  case -505: msg(MSG_FAIL,"Bomem: Time-Out in Abort"); break;
  case -506: msg(MSG_FAIL,"Bomem: Other Error"); break;
  default: msg(MSG_FAIL,"Bomem: Unknown Error Code"); break;
  }

  msg(MSG_DEBUG,"Shutting Down Bomem Acquisition System");
  close();
  msg(MSG,"Number of Data Requests Not Serviced: %u",num_busy);
  el = (float)times(&cputim) / (float)CLK_TCK;
  msg(MSG,"CPU System Time: %f s; CPU User Time: %f s",
     (cputim.tms_stime/(float)CLK_TCK), (cputim.tms_utime/(float)CLK_TCK));
  msg(MSG,"Elapsed Program Time: %f s", el);
  msg(MSG,"Average Use of CPU: %f%%",(((cputim.tms_stime/(float)CLK_TCK)+
	(cputim.tms_utime/(float)CLK_TCK))/el)*100.0);
     
  if (code)
    msg(MSG_FATAL,"Fatal Bomem Seq36 Interface Error");
  else if (!terminated)
    msg(MSG_FATAL,"Fatal Error");
  else
    DONE_MSG;
}
