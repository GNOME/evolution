/* -*- Mode: C -*-
  ======================================================================
  FILE: copycluster.c
  CREATOR: eric 15 January 2000
  
  $Id$
  $Locker$
    
 (C) COPYRIGHT 2000 Eric Busboom
 http://www.softwarestudio.org

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/

#include <stdio.h> /* for printf */
#include "ical.h"
#include "icalcluster.h"
#include <errno.h>
#include <string.h> /* For strerror */
#include "icalrestriction.h"

/* This program copies a file that holds iCal components to an other file. */


void usage(char* arg0) {
    printf("usage: %s cluster-file1 cluster-file2\n",arg0);
}

int main(int c, char *argv[]){

    icalcluster *clusterin, *clusterout;
    icalcomponent *itr;
    int count=0;
    int tostdout = 0;

    if(c < 2 || c > 3){
	usage(argv[0]);
	exit(1);
    }

    if (c == 2){
	tostdout = 1;
    }

    clusterin = icalcluster_new(argv[1]);

    if (clusterin == 0){
	printf("Could not open input cluster \"%s\"",argv[1]);
	       
	exit(1);
    }

    if (!tostdout){
	clusterout = icalcluster_new(argv[2]);
	if (clusterout == 0){
	    printf("Could not open output cluster \"%s\"\n",argv[2]);
	    exit(1);
	}
    }


    for (itr = icalcluster_get_first_component(clusterin,
					       ICAL_ANY_COMPONENT);
	 itr != 0;
	 itr = icalcluster_get_next_component(clusterin,
					      ICAL_ANY_COMPONENT)){

	icalrestriction_check(itr);

	if (itr != 0){

	    if(tostdout){

		printf("--------------\n%s\n",icalcomponent_as_ical_string(itr));
	    } else {

		icalcluster_add_component(clusterout,
					  icalcomponent_new_clone(itr));
	    }
	    
	    count++;

	} else {
	    printf("Got NULL component");
	}
    }


    printf("Transfered %d components\n",count);

    icalcluster_free(clusterin);

    if (!tostdout){
	icalcluster_mark(clusterout);
	icalcluster_free(clusterout);
    }

    return 0;
}

