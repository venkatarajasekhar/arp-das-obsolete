/* digdelay.c Digital Delay filter
 * This provides a digital delay on an unsigned short datum (or
 * types compatible with that.) For other types, additional
 * functions will be required. Probably the ideal approach would
 * be making the bulk of this file an include file, digdelay.h
 * then making a number of short .c files which would define the
 * function names and the arg types. The structure, the three
 * functions and the type would need to be parametrized.
 * $Log$
 */
#include "nortlib.h"
#include "nl_dsp.h"
#pragma off (unreferenced)
  static char rcsid[] =
	"$Id$";
#pragma on (unreferenced)

dig_dly *new_dig_delay(unsigned short n_points) {
  dig_dly *dd;
  
  dd = new_memory(sizeof(dig_dly));
  if (dd != 0) {
	dd->last_idx = dd->n_points = n_points;
	dd->value = new_memory(sizeof(unsigned short)*dd->n_points);
	if (dd->value == 0) {
	  free_memory(dd);
	  dd = NULL;
	}
  }
  return dd;
}

unsigned short dig_delay(dig_dly *dd, unsigned short v) {
  int i;
  unsigned short vout;
  
  if (dd->last_idx >= dd->n_points) {
	for (i = 0; i < dd->n_points; i++)
	  dd->value[i] = v;
	dd->last_idx = 0;
	vout = v;
  } else {
	if (++dd->last_idx >= dd->n_points)
	  dd->last_idx = 0;
	vout = dd->value[dd->last_idx];
	dd->value[dd->last_idx] = v;
  }
  return(vout);
}

void free_dig_delay(dig_dly *dd) {
  if (dd != 0) {
	if (dd->value != 0) free_memory(dd->value);
	free_memory(dd);
  }
}

