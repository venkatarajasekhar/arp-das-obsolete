#include <sys/types.h>
typedef struct {
    pid_t who;
    token_type n_rows;
    unsigned char rflag;
    unsigned long seq;
    unsigned long list_seen;
    unsigned long data_lost;
} arr;

typedef struct ll {
    msg_hdr_type hdr;
    unsigned long seq;
    union {
		dascmd_type c;
		tstamp_type s;
    } u;
    struct ll *next;
} llist;

/* **************** */
/* global variables */
/* **************** */

/* command line option string */
extern char *opt_string;	    /* global options string */

/* data structures */
extern arr clients[];		    /* clients array */
extern llist *list;			    /* current commands and stamps */
extern llist *t_list;		    /* tail of list of commands stamps */

/* buffer status vars: all integers */
extern int bfr_sz_bytes;	    /* buffer size in bytes */
extern int bfr_sz_rows;		    /* buffer size in rows */
extern int startrow;		    /* row of oldest data */
extern int putrow;		    /* where to put next row of incoming data */
extern int requests;		    /* number of outstanding requests */
extern int stamps_and_cmds;	    /* number of stamps and cmds outstanding */
extern int got_quit;		    /* whether received DASCmd QUIT */
extern unsigned int n_clients;	    /* number of clients */

extern char *bfr;		    /* THE buffer */

/* sequence numbers: data rows and stamps/cmds: assume rollover wont occur */
extern unsigned long data_seq;	    /* the starting data sequence number */
extern unsigned long t_data_seq;    /* the tail data sequence number */
extern unsigned long list_seq;	    /* the starting guarenteed sequence number */
extern unsigned long t_list_seq;    /* the tail guarenteed sequence number */
extern unsigned long list_seen;	    /* minimum number of stamps seen */

extern module_type mt_dg;
extern module_type mt_dc;
extern pid_t nt_dc;

#define BECOME_DG(X) \
{ \
	dbr_info.mod_type = mt_dg; \
	dbr_info.next_tid = (X); \
}

#define BECOME_DC \
{ \
	dbr_info.mod_type = mt_dc; \
	dbr_info.next_tid = nt_dc; \
}

/* functions */
extern int compare(pid_t *w, arr *a);
extern int send_cmd_or_stamp(llist *l, arr *a, unsigned long lseq);
extern int send_data(token_type nrows, arr *a);

