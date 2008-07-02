#ifndef DG_TMR_H
#define DG_TMR_H

#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include "DG_Resmgr.h"
#include "nortlib.h"

class DG_tmr : public DG_dispatch_client {
  public:
    DG_tmr();
    ~DG_tmr();
    void attach( DG_dispatch *disp );
    void settime( int per_sec, int per_nsec );
    int ready_to_quit();
  private:
    int timerid; // set to -1 before initialization and after cleanup
    int pulse_code;
};

extern "C" {
	int DG_tmr_pulse_func( message_context_t *ctp, int code,
		unsigned flags, void *handle );
}

#endif
