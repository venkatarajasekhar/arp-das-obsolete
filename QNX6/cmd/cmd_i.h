#ifndef CMD_I_H
#define CMD_I_H
// Internal include file for command server development
// This will be radically refactored for inclusion in
// the library. Some of these definitions will become
// public, some will move inside the library files.

/* These defs must preceed the following includes: */
struct ocb;
struct ioattr_s;
#define IOFUNC_OCB_T struct ocb
#define IOFUNC_ATTR_T struct ioattr_s
#define THREAD_POOL_PARAM_T dispatch_context_t

#include <sys/iofunc.h>
#include <sys/dispatch.h>

#define CMD_MAX_COMMAND_OUT 160 // Maximum command message output length
#define CMD_MAX_COMMAND_IN 300  // Maximum command message input length

typedef struct command_out_s {
  struct command_out_s *next;
  int ref_count;
  char command[CMD_MAX_COMMAND_OUT];
  int cmdlen;
} command_out_t;

typedef struct ocb {
  iofunc_ocb_t hdr;
  command_out_t *next_command;
  struct ocb *next_ocb; // for blocked list
  int rcvid;
  int nbytes_requested;
} ocb_t;

/* IOFUNC_ATTR_T extended to provide blocked-ocb-list
   for each mountpoint */
typedef struct ioattr_s {
  iofunc_attr_t attr;
  ocb_t *blocked;
  command_out_t *first_cmd;
  command_out_t *last_cmd;
  struct ioattr_s *next;
  char *nodename;
} ioattr_t;

extern command_out_t *new_command(void);
extern command_out_t *free_command( command_out_t *cmd );
extern IOFUNC_ATTR_T *cmdsrvr_setup_rdr( char *node );
extern void cmdsrvr_turf( IOFUNC_ATTR_T *handle, char *format, ... );

#define CMDREP_TYPE(x) ((x)/1000)
#define cmd_init()
int cmd_batch(char*cmd, int test);

#endif
