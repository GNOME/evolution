/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalset.c
 CREATOR: eric 17 Jul 2000


 Icalset is the "base class" for representations of a collection of
 iCal components. Derived classes (actually delegatees) include:
 
    icalfileset   Store componetns in a single file
    icaldirset    Store components in multiple files in a directory
    icalheapset   Store components on the heap
    icalmysqlset  Store components in a mysql database. 

 $Id$
 $Locker$

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#include "ical.h"
#include "icalset.h"
#include "icalfileset.h"
#include "icaldirset.h"
/*#include "icalheapset.h"*/
/*#include "icalmysqlset.h"*/

icalset* icalset_new_file(char* path);

icalset* icalset_new_dir(char* path);

icalset* icalset_new_heap(void);

icalset* icalset_new_mysql(char* path);

void icalset_free(icalset* set);

char* icalset_path(icalset* set);

void icalset_mark(icalset* set);

icalerrorenum icalset_commit(icalset* set); 

icalerrorenum icalset_add_component(icalset* set, icalcomponent* comp);

icalerrorenum icalset_remove_component(icalset* set, icalcomponent* comp);

int icalset_count_components(icalset* set,
			     icalcomponent_kind kind);

icalerrorenum icalset_select(icalset* set, icalcomponent* gauge);

void icalset_clear_select(icalset* set);

icalcomponent* icalset_fetch(icalset* set, char* uid);

int icalset_has_uid(icalset* set, char* uid);

icalerrorenum icalset_modify(icalset* set, icalcomponent *old,
			       icalcomponent *new);

icalcomponent* icalset_get_current_component(icalset* set);

icalcomponent* icalset_get_first_component(icalset* set);

icalcomponent* icalset_get_next_component(icalset* set);




