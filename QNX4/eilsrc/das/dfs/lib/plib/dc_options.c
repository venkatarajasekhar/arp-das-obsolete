/* Data Acquisition System data buffered ring client code - options handler */

/* includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dfs.h"
#include "dc.h"
#include "msg.h"

extern char *opt_string;

int DC_init_options(int argcc, char *argvv[]) {
  /* initialse client parameters from command line args */
  extern char *optarg;
  extern int optind, opterr, optopt;
  topology_type top = RING;
  nid_t node = 0;
  int c;

  opterr = 0;
  optind = 0;

  do {
    c=getopt(argcc,argvv,opt_string);
    switch (c) {
    case 'b': top = STAR; node=(nid_t)atoi(optarg); break;
    case 'u': top = BUS; break;
    case 'i': DC_data_rows=(unsigned)atoi(optarg); break;
    case '?': msg(MSG_EXIT_ABNORM,"Invalid option -%c",optopt);
    default : break;
    }
  }  while (c!=-1);
  opterr = 1;
  return(DC_init(top, node));
}
