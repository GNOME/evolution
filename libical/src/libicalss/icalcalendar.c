/* -*- Mode: C -*-
  ======================================================================
  FILE: icalcalendar.c
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


#include "icalcalendar.h"
#include "icalcluster.h"
#include <limits.h> 
#include <sys/stat.h> /* For mkdir, stat */
#include <sys/types.h> /* For mkdir */
#include <fcntl.h> /* For mkdir */
#include <unistd.h>  /* For mkdir, stat */    
#include <stdlib.h> /* for malloc */
#include <string.h> /* for strcat */
#include <errno.h>

#define BOOKED_DIR "booked"
#define INCOMING_FILE "incoming.ics"
#define PROP_FILE "properties.ics"
#define FBLIST_FILE "freebusy.ics"

struct icalcalendar_impl 
{
	char* dir;
	icalcomponent* freebusy;
	icalcomponent* properties;
	icalstore* booked;
	icalstore* incoming;
};

struct icalcalendar_impl* icalcalendar_new_impl()
{
    struct icalcalendar_impl* impl;

    if ( ( impl = (struct icalcalendar_impl*)
	   malloc(sizeof(struct icalcalendar_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    return impl;
}


icalerrorenum icalcalendar_create(struct icalcalendar_impl* impl)
{
    char path[PATH_MAX];
    struct stat sbuf;
    int r;
    
    icalerror_check_arg_re((impl != 0),"impl",ICAL_BADARG_ERROR);

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,BOOKED_DIR);

    r = stat(path,&sbuf);

    if( r != 0 && errno == ENOENT){

	if(mkdir(path,0777)!=0){
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return ICAL_FILE_ERROR;
	}
    }

    return ICAL_NO_ERROR;
}

icalcalendar* icalcalendar_new(char* dir)
{
    struct icalcalendar_impl* impl;

    icalerror_check_arg_rz((dir != 0),"dir");
    
    impl = icalcalendar_new_impl();

    if (impl == 0){
	return 0;
    }

    impl->dir = (char*)strdup(dir);
    impl->freebusy = 0;
    impl->properties = 0;
    impl->booked = 0;
    impl->incoming = 0;

    if (icalcalendar_create(impl) != ICAL_NO_ERROR){
	free(impl);
	return 0;
    }

    return impl;
}

void icalcalendar_free(icalcalendar* calendar)
{

    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
        
    if (impl->dir !=0){
	free(impl->dir);
    }

    if (impl->freebusy !=0){
	icalcluster_free(impl->freebusy);
    }

    if (impl->properties !=0){
	icalcluster_free(impl->properties);
    }

    if (impl->booked !=0){
	icalstore_free(impl->booked);
    }

    if (impl->incoming !=0){
	icalstore_free(impl->incoming);
    }

    impl->dir = 0;
    impl->freebusy = 0;
    impl->properties = 0;
    impl->booked = 0;
    impl->incoming = 0;


    free(impl);
}


int icalcalendar_lock(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

int icalcalendar_unlock(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

int icalcalendar_islocked(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

int icalcalendar_ownlock(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");
    return 0;
}

icalstore* icalcalendar_get_booked(icalcalendar* calendar)
{
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    char dir[PATH_MAX];

    icalerror_check_arg_rz((impl != 0),"impl");
    
    dir[0] = '\0';
    strcpy(dir,impl->dir);
    strcat(dir,"/");
    strcat(dir,BOOKED_DIR);

    if (impl->booked == 0){
	icalerror_clear_errno();
	impl->booked = icalstore_new(dir);
	assert(icalerrno == ICAL_NO_ERROR);
    }

    return impl->booked;

}

icalcluster* icalcalendar_get_incoming(icalcalendar* calendar)
{
    char path[PATH_MAX];
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,INCOMING_FILE);

    if (impl->properties == 0){
	impl->properties = icalcluster_new(path);
    }

    return impl->properties;
}

icalcluster* icalcalendar_get_properties(icalcalendar* calendar)
{
    char path[PATH_MAX];
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,PROP_FILE);

    if (impl->properties == 0){
	impl->properties = icalcluster_new(path);
    }

    return impl->properties;
}

icalcluster* icalcalendar_get_freebusy(icalcalendar* calendar)
{
    char path[PATH_MAX];
    struct icalcalendar_impl *impl = (struct icalcalendar_impl*)calendar;
    icalerror_check_arg_rz((impl != 0),"impl");

    path[0] = '\0';
    strcpy(path,impl->dir);
    strcat(path,"/");
    strcat(path,FBLIST_FILE);


    if (impl->freebusy == 0){
	impl->freebusy = icalcluster_new(path);
    }

    return impl->freebusy;
}




