#ifndef DG_CMD_H
#define DG_CMD_H

#include <signal.h>
#include "DG_Resmgr.h"

class DG_cmd : public DG_dispatch_client {
  private:
    struct sigevent cmd_ev;
    int cmd_fd;
    int dev_id;
    static iofunc_attr_t cmd_attr;
		static resmgr_connect_funcs_t connect_funcs;
		static resmgr_io_funcs_t io_funcs;
  public:
    DG_cmd();
    ~DG_cmd();
    void attach(DG_dispatch *disp); // add to dispatch list
    void service_pulse(int triggered); // return non-zero on quit
    int execute(char *buf);
    int ready_to_quit(); // virtual function of DG_dispatch_client
    static int const DG_CMD_BUFSIZE = 80;
};

extern "C" {
	int DG_cmd_pulse_func( message_context_t *ctp, int code,
			unsigned flags, void *handle );
	int DG_cmd_io_write(
	    resmgr_context_t *ctp,
			io_write_t *msg,
			RESMGR_OCB_T *ocb );
}

#endif
