/* expint.c Provides all the specific code to handle support for
   the expints
   
   Registration is done with Region, cardID and a bit number is 
   assigned. Need to be able to detach based on cardID, need to 
   be able to trigger based on region and bit
   
   When a proxy arrives, go through our list of regions.
   For each region, read the INTA and compare the result to the 
   bits we've defined. If any are active, search through the list 
   of programs that have registered on that bit and Trigger the 
   appropriate proxy or proxies.
   
   If a proxy fails (Trigger() returns -1, errno==ESRCH)
*/
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/kernel.h>
#include "intserv.h"
#include "internal.h"
#include "nortlib.h"
#include "subbus.h"

typedef struct carddef {
  struct carddef *next;
  char cardID[ cardID_MAX ];
  unsigned short address;
  unsigned int reg_id;
  unsigned int bitno;
  pid_t proxy;
  pid_t owner;
} card_def;

typedef struct {
 unsigned short address;
 unsigned short bits;
 card_def *def[8];
} region;
#define MAX_REGIONS 4

card_def *carddefs;
region regions[ MAX_REGIONS ];

static void delete_card( card_def **cdp ) {
  card_def *cd;

  if ( cdp == 0 || *cdp == 0 )
	nl_error( 4, "Invalid pointer to delete_card" );
  cd = *cdp;
  if ( sbwra( cd->address, 0 ) == 0 )
	nl_error( 1, "No acknowledge unprogramming %s (0x%03X)",
	  cd->cardID, cd->address );
  regions[ cd->reg_id ].def[ cd->bitno ] = NULL;
  regions[ cd->reg_id ].bits &= ~(1 << cd->bitno );
  *cdp = cd->next;
  free( cd );
}

static card_def **find_card( char *cardID, unsigned short address ) {
  card_def *cd, **cdp;

  for ( cdp = &carddefs, cd = carddefs;
		cd != 0;
		cdp = &cd->next, cd = cd->next ) {
	if ( strcmp( cd->cardID, cardID ) == 0
		  || cd->address == address ) {
	  return cdp;
	}
  }
  return NULL;
}

void service_expint( void ) {
  int i, bitno, bit;
  unsigned short mask;

  while ( Creceive( expint_proxy, 0, 0 ) != -1 );
  for ( i = 0; i < MAX_REGIONS && regions[i].address != 0; i++ ) {
	if ( regions[i].bits != 0 ) {
	  mask = sbb( regions[i].address ) & regions[i].bits;
	  for ( bitno = 0, bit = 1; i < 7 && mask; bitno++, bit <<= 1 ) {
		if ( mask & bit ) {
		  if ( Trigger( regions[i].def[bitno]->proxy ) == -1 ) {
			if ( errno == ESRCH ) {
			  card_def **cdp, *cd;

			  cd = regions[i].def[bitno];
			  nl_error( 1, "Proxy %d for card %s died",
				cd->proxy, cd->cardID );
			  cdp = find_card( cd->cardID, cd->address );
			  delete_card( cdp );
			} else
			  nl_error( 2, "Unexpected error %d on Trigger", errno );
		  }
		  mask &= ~bit;
		}
	  }
	}
  }
}

void expint_attach( pid_t who, char *cardID, unsigned short address,
					  int region, pid_t proxy, IntSrv_reply *rep ) {
  card_def **cdp, *cd;
  int bitno, bit, i;

  if ( subbus_subfunction == 0 ) {
	nl_error( 1, "Request for expint w/o subbus" );
	rep->status = ELIBACC;
	return;
  }
  /* Verify that the region is legal */
  if ( ( region & (~6) ) != 0x40 || address == 0 ) {
	nl_error( 1,
	  "Illegal region (0x%02X) or address (0x%03X) requested",
	  region, address );
	rep->status = ENXIO;
	return;
  }

  /* First check to make sure it isn't already defined */
  cdp = find_card( cardID, address );
  if ( cdp != 0 ) {
	cd = *cdp;
	if ( cd->address != address ) {
	  nl_error( 2, "Same ID (%s) two addresses (0x%03X, 0x%03X)",
		cardID, cd->address, address );
	  rep->status = ENXIO;
	  return;
	}
	if ( Creceive( cd->owner, NULL, 0 ) == -1 &&
			errno == ESRCH ) {
	  nl_error( 1, "Card ID %s reassigned", cardID );
	  delete_card( cdp );
	} else {
	  nl_error( 1, "Duplicate request for cardID %s", cardID );
	  rep->status = EAGAIN;
	  return;
	}
  }
  
  /* set up the region and select the bit */
  for ( i = 0; i < MAX_REGIONS && regions[i].address != 0; i++ ) {
	if ( regions[i].address == region ) break;
  }
  if ( i == MAX_REGIONS ) {
	nl_error( 2, "Too many regions requested!" );
	rep->status = ENOSPC;
	return;
  }
  regions[i].address = region;
  for ( bitno = 0, bit = 1; bitno < 8; bitno++, bit <<= 1 ) {
	if ( ( regions[i].bits & bit ) == 0 ) break;
  }
  if ( bitno == 8 ) {
	nl_error( 2, "Too many requests for region 0x%02X", region );
	rep->status = ENOSPC;
	return;
  }

  /* Now assign a new def. */
  cd = malloc( sizeof( card_def ) );
  if ( cd == 0 ) {
	nl_error( 2, "Out of memory in expint_attach" );
	rep->status = ENOMEM;
	return;
  }
  strncpy( cd->cardID, cardID, cardID_MAX );
  cd->cardID[ cardID_MAX - 1 ] = '\0';
  cd->address = address;
  cd->reg_id = i;
  cd->bitno = bitno;
  cd->proxy = proxy;
  cd->owner = who;

  regions[i].bits |= bit;
  regions[i].def[bitno] = cd;
  
  cd->next = carddefs;
  carddefs = cd;

  /* Now program the card! */
  { unsigned short prog;
	prog = ( (region & 6) << 2 ) | 0x20 | bitno;
	if ( sbwra( address, prog ) == 0 )
	  nl_error( 1, "No acknowledge programming %s(0x%03X)",
		cardID, address );
  }

  rep->status = EOK;
}

void expint_detach( pid_t who, char *cardID, IntSrv_reply *rep ) {
  card_def **cdp;
  
  cdp = find_card( cardID, 0 );
  if ( cdp == 0 ) {
	rep->status = ENOENT;
  } else {
	if ( (*cdp)->owner != who ) {
	  nl_error( 1, "Non-owner %d attempted detach for %s",
				who, cardID );
	  rep->status = EPERM;
	} else {
	  delete_card( cdp );
	  rep->status = EOK;
	}
  }
}