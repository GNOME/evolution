/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcluster.h
 CREATOR: eric 23 December 1999


 $Id$
 $Locker$

 (C) COPYRIGHT 1999 Eric Busboom
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

#ifndef ICALCLUSTER_H
#define ICALCLUSTER_H

#include "ical.h"

typedef void icalcluster;


icalcluster* icalcluster_new(char* path);
void icalcluster_free(icalcluster* cluster);


/* Load a new file into the cluster */
icalerrorenum icalcluster_load(icalcluster* cluster, char* path);

/* Return a reference to the internal component. */
icalcomponent* icalcluster_get_component(icalcluster* cluster);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed*/
void icalcluster_mark(icalcluster* cluster);

/* Write the cluster data back to disk */
icalerrorenum icalcluster_commit(icalcluster* cluster); 

/* manipulate the components in the cluster */
icalerrorenum icalcluster_add_component(icalcomponent* parent,
			       icalcomponent* child);

icalerrorenum icalcluster_remove_component(icalcomponent* parent,
				  icalcomponent* child);

int icalcluster_count_components(icalcomponent* component,
				 icalcomponent_kind kind);

/* Iterate through components */
icalcomponent* icalcluster_get_current_component (icalcomponent* component);

icalcomponent* icalcluster_get_first_component(icalcomponent* component,
					       icalcomponent_kind kind);
icalcomponent* icalcluster_get_next_component(icalcomponent* component,
					      icalcomponent_kind kind);

#endif /* !ICALCLUSTER_H */



