/* -*- Mode: C -*-
  ======================================================================
  FILE: icalstore.c
  CREATOR: eric 28 November 1999
  
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


/*
 
  icalstore manages a database of ical components and offers
  interfaces for reading, writting and searching for components.

  icalstore groups components in to clusters based on their DTSTART
  time -- all components that start in the same month are grouped
  together in a single file. All files in a sotre are kept in a single
  directory. ( If a component does not have DTSTART, the store uses
  DTSTAMP or CREATE )

  The primary interfaces are icalstore_first and icalstore_next. These
  routine iterate through all of the components in the store, subject
  to the current gauge. A gauge is an icalcomponent that is tested
  against other componets for a match. If a gauge has been set with
  icalstore_select, icalstore_first and icalstore_next will only
  return componentes that match the gauge.

  The Store generated UIDs for all objects that are stored if they do
  not already have a UID. The UID is the name of the cluster (month &
  year as MMYYYY) plus a unique serial number. The serial number is
  stored as a property of the cluster.

*/

#include "ical.h"
#include "icalstore.h"
#include "pvl.h" 
#include "icalerror.h"
#include "icalparser.h"
#include "icalcluster.h"

#include "filelock.h"

#include <limits.h>
#include <dirent.h> /* for opendir() */
#include <errno.h>
#include <sys/types.h> /* for opendir() */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat, getpid */
#include <time.h> /* for clock() */
#include <stdlib.h> /* for rand(), srand() */
#include <sys/utsname.h> /* for uname */
#include <string.h> /* for strdup */


struct icalstore_impl 
{
	char* dir;
	icalcomponent* gauge;
	icalcluster* cluster;
	int first_component;
	pvl_list directory;
	pvl_elem directory_iterator;
};

struct icalstore_impl* icalstore_new_impl()
{
    struct icalstore_impl* comp;

    if ( ( comp = (struct icalstore_impl*)
	   malloc(sizeof(struct icalstore_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    return comp;
}



void icalstore_lock_dir(char* dir)
{
}


void icalstore_unlock_dir(char* dir)
{
}

/* Load the contents of the store directory into the store's internal directory list*/
icalerrorenum icalstore_read_directory(struct icalstore_impl* impl)
{
   struct dirent *de;
   DIR* dp;
   char *str;
 
   dp = opendir(impl->dir);
   
    if ( dp == 0) {
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }

    /* clear contents of directory list */
    while((str = pvl_pop(impl->directory))){
	free(str);
    }
    
    /* load all of the cluster names in the directory list */
    for(de = readdir(dp);
	de != 0;
	de = readdir(dp)){

	/* Remove known directory names  '.' and '..'*/
	if (strcmp(de->d_name,".") == 0 ||
	    strcmp(de->d_name,"..") == 0 ){
	    continue;
	}

	pvl_push(impl->directory, (void*)strdup(de->d_name));	
    }

    closedir(dp);

    return ICAL_NO_ERROR;
}

icalstore* icalstore_new(char* dir)
{
    struct icalstore_impl *impl = icalstore_new_impl();
    struct stat sbuf;

    if (impl == 0){
	return 0;
    }

    icalerror_check_arg_rz( (dir!=0), "dir");

    if (stat(dir,&sbuf) != 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return 0;
    }
    
    /* dir is not the name of a direectory*/
    if (!S_ISDIR(sbuf.st_mode)){ 
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }	    

    icalstore_lock_dir(dir);

    impl = icalstore_new_impl();

    if (impl ==0){
	icalerror_set_errno(ICAL_ALLOCATION_ERROR);
	return 0;
    }
    
    impl->directory = pvl_newlist();
    impl->directory_iterator = 0;
    impl->dir = (char*)strdup(dir);
    impl->gauge = 0;
    impl->first_component = 0;
    impl->cluster = 0;

    icalstore_read_directory(impl);

    return (icalstore*) impl;
}

void icalstore_free(icalstore* s)
{
    struct icalstore_impl *impl = (struct icalstore_impl*)s;
    char* str;

    icalstore_unlock_dir(impl->dir);

    if(impl->dir !=0){
	free(impl->dir);
    }

    if(impl->gauge !=0){
	icalcomponent_free(impl->gauge);
    }

    if(impl->cluster !=0){
	icalcluster_free(impl->cluster);
    }

    while( (str=pvl_pop(impl->directory)) != 0){
	free(str);
    }

    pvl_free(impl->directory);

    impl->directory = 0;
    impl->directory_iterator = 0;
    impl->dir = 0;
    impl->gauge = 0;
    impl->first_component = 0;

    free(impl);

}

/* icalstore_next_uid_number updates a serial number in the Store
   directory in a file called SEQUENCE */

int icalstore_next_uid_number(icalstore* store)
{
    struct icalstore_impl *impl = (struct icalstore_impl*)store;
    char sequence = 0;
    char temp[128];
    char filename[PATH_MAX];
    char *r;
    FILE *f;
    struct stat sbuf;

    icalerror_check_arg_rz( (store!=0), "store");

    sprintf(filename,"%s/%s",impl->dir,"SEQUENCE");

    /* Create the file if it does not exist.*/
    if (stat(filename,&sbuf) == -1 || !S_ISREG(sbuf.st_mode)){

	f = fopen(filename,"w");
	if (f != 0){
	    fprintf(f,"0");
	    fclose(f);
	} else {
	    icalerror_warn("Can't create SEQUENCE file in icalstore_next_uid_number");
	    return 0;
	}
	
    }
    
    if ( (f = fopen(filename,"r+")) != 0){

	rewind(f);
	r = fgets(temp,128,f);

	if (r == 0){
	    sequence = 1;
	} else {
	    sequence = atoi(temp)+1;
	}

	rewind(f);

	fprintf(f,"%d",sequence);

	fclose(f);

	return sequence;
	
    } else {
	icalerror_warn("Can't create SEQUENCE file in icalstore_next_uid_number");
	return 0;
    }

}

icalerrorenum icalstore_next_cluster(icalstore* store)
{
    struct icalstore_impl *impl = (struct icalstore_impl*)store;
    char path[PATH_MAX];

    if (impl->directory_iterator == 0){
	icalerror_set_errno(ICAL_INTERNAL_ERROR);
	return ICAL_INTERNAL_ERROR;
    }	    
    impl->directory_iterator = pvl_next(impl->directory_iterator);

    if (impl->directory_iterator == 0){
	/* There are no more clusters */
	impl->cluster = 0;
	return ICAL_NO_ERROR;
    }
	    

    sprintf(path,"%s/%s",impl->dir,(char*)pvl_data(impl->directory_iterator));

    return icalcluster_load(impl->cluster,path);
}

void icalstore_add_uid(icalstore* store, icalstore* comp)
{
    char uidstring[PATH_MAX];
    icalproperty *uid;
    struct utsname unamebuf;

    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (comp!=0), "comp");

    uid = icalcomponent_get_first_property(comp,ICAL_UID_PROPERTY);
    
    if (uid == 0) {
	
	uname(&unamebuf);
	
	sprintf(uidstring,"%d-%s",getpid(),unamebuf.nodename);
	
	uid = icalproperty_new_uid(uidstring);
	icalcomponent_add_property(comp,uid);
    } else {
	
	strcpy(uidstring,icalproperty_get_uid(uid));
    }
}

icalerrorenum icalstore_add_component(icalstore* store, icalstore* comp)
{
    struct icalstore_impl *impl;
    char clustername[PATH_MAX];
    icalproperty *dt, *count, *lm;
    icalvalue *v;
    struct icaltimetype tm;
    icalerrorenum error = ICAL_NO_ERROR;

    impl = (struct icalstore_impl*)store;
    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (comp!=0), "comp");

    errno = 0;
    
    icalstore_add_uid(store,comp);

    /* Determine which cluster this object belongs in */

    dt = icalcomponent_get_first_property(comp,ICAL_DTSTART_PROPERTY);

    if (dt == 0){
	dt = icalcomponent_get_first_property(comp,ICAL_DTSTAMP_PROPERTY);
    }

    if (dt == 0){
	dt = icalcomponent_get_first_property(comp,ICAL_CREATED_PROPERTY);
    }
    
    if (dt == 0){
	icalerror_warn("The component does not have a DTSTART, DTSTAMP or a CREATED property, so it cannot be added to the store");
	icalerror_set_errno(ICAL_BADARG_ERROR);
	return ICAL_BADARG_ERROR;
    }

    v = icalproperty_get_value(dt);

    tm = icalvalue_get_datetime(v);

    sprintf(clustername,"%s/%04d%02d",impl->dir,tm.year,tm.month);

    /* Load the cluster and insert the object */

    if (impl->cluster == 0){
	impl->cluster = icalcluster_new(clustername);

	if (impl->cluster == 0){
	    error = icalerrno;
	}
    } else {
	error = icalcluster_load(impl->cluster,
				 clustername);

    }

    
    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return error;
    }

    /* Update or add the LAST-MODIFIED property */

    lm = icalcomponent_get_first_property(comp,
					  ICAL_LASTMODIFIED_PROPERTY);

    if (lm == 0){
	lm = icalproperty_new_lastmodified(icaltimetype_from_timet( time(0),1));
	icalcomponent_add_property(comp,lm);
    } else {
	icalproperty_set_lastmodified(comp,icaltimetype_from_timet( time(0),1));
    }


    /* Add the component to the cluster */

    icalcluster_add_component(impl->cluster,comp);


    /* Increment the clusters count value */
    count = icalcomponent_get_first_property(
	icalcluster_get_component(impl->cluster),
	ICAL_XLICCLUSTERCOUNT_PROPERTY);

    if (count == 0){
	icalerror_set_errno(ICAL_INTERNAL_ERROR);
	return ICAL_INTERNAL_ERROR;
    }

    icalproperty_set_xlicclustercount(count,
	icalproperty_get_xlicclustercount(count)+1);

    
    icalcluster_mark(impl->cluster);

    return ICAL_NO_ERROR;    
}

/* Remove a component in the current cluster */
icalerrorenum icalstore_remove_component(icalstore* store, icalstore* comp)
{
    struct icalstore_impl *impl = (struct icalstore_impl*)store;
    icalproperty *count;

    icalerror_check_arg_re((store!=0),"store",ICAL_BADARG_ERROR);
    icalerror_check_arg_re((comp!=0),"comp",ICAL_BADARG_ERROR);
    icalerror_check_arg_re((impl->cluster!=0),"Cluster pointer",ICAL_USAGE_ERROR);

/* HACK The following code should be used to ensure that the component
the caller is trying to remove is actually in the cluster, but it
resets the internal iterators, which immediately ends any loops over
the cluster the caller may have in progress 

    for(c = icalcluster_get_first_component(
	    impl->cluster,
	    ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcluster_get_next_component(
	    impl->cluster,
	    ICAL_ANY_COMPONENT)){

	if (c == comp){
	    found = 1;
	}

    }

    if (found != 1){
	icalerror_warn("icalstore_remove_component: component is not part of current cluster");
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return ICAL_USAGE_ERROR;
    }

*/

    icalcluster_remove_component(impl->cluster,
				   comp);

    icalcluster_mark(impl->cluster);

   /* Decrement the clusters count value */
    count = icalcomponent_get_first_property(
	icalcluster_get_component(impl->cluster),
	ICAL_XLICCLUSTERCOUNT_PROPERTY);

    if (count == 0){
	icalerror_set_errno(ICAL_INTERNAL_ERROR);
	return ICAL_INTERNAL_ERROR;
    }

    icalproperty_set_xlicclustercount(count,
	icalproperty_get_xlicclustercount(count)-1);

    return ICAL_NO_ERROR;
}

/* Convert a VQUERY component into a gauge */
icalcomponent* icalstore_make_gauge(icalcomponent* query);

/* icalstore_test compares a component against a gauge, and returns
   true if the component passes the test 

   The gauge is a VCALENDAR component that specifies how to test the
   target components. The guage holds a collection of VEVENT, VTODO or
   VJOURNAL sub-components. Each of the sub-components has a
   collection of properties that are compared to corresponding
   properties in the target component, according to the
   X-LIC-COMPARETYPE parameters to the gauge's properties.

   When a gauge has several sub-components, the results of testing the
   target against each of them is ORed together - the target
   component will pass if it matches any of the sub-components in the
   gauge. However, the results of matching the proeprties in a
   sub-component are ANDed -- the target must match every property in
   a gauge sub-component to match the sub-component.

   Here is an example:

   BEGIN:VCOMPONENT
   BEGIN:VEVENT
   DTSTART;X-LIC-COMPARETYPE=LESS:19981025T020000
   ORGANIZER;X-LIC-COMPARETYPE=EQUAL:mrbig@host.com 
   END:VEVENT
   BEGIN:VEVENT
   LOCATION;X-LIC-COMPARETYPE=EQUAL:McNary's Pub
   END:VEVENT
   END:VCALENDAR

   This gauge has two sub-components; one which will match a VEVENT
   based on start time, and organizer, and another that matches based
   on LOCATION. A target component will pass the test if it matched
   either of the gauge.
   
  */

int icalstore_test(icalcomponent* comp, icalcomponent* gauge)
{
    int pass = 0,localpass = 0;
    icalcomponent *c;
    icalproperty *p;
    icalcomponent *child;    

    icalerror_check_arg_rz( (comp!=0), "comp");
    icalerror_check_arg_rz( (gauge!=0), "gauge");

    for(c = icalcomponent_get_first_component(gauge,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(gauge,ICAL_ANY_COMPONENT)){


	/* Test properties. For each property in the gauge, search through
	   the component for a similar property. If one is found, compare
	   the two properties value with the comparison specified in the
	   gauge with the X-LIC-COMPARETYPE parameter */

	for(p = icalcomponent_get_first_property(c,ICAL_ANY_PROPERTY);
	    p != 0;
	    p = icalcomponent_get_next_property(c,ICAL_ANY_PROPERTY)){
	
	    icalproperty* targetprop; 
	    icalparameter* compareparam;
	    icalparameter_xliccomparetype compare;
	    int rel; /* The realtionship between the gauge and target values.*/

	    /* Extract the comparison type from the gauge. If there is no
	       comparison type, assume that it is "EQUAL" */

	    compareparam = icalproperty_get_first_parameter(
		p,
		ICAL_XLICCOMPARETYPE_PARAMETER);

	    if (compareparam!=0){
		compare = icalparameter_get_xliccomparetype(compareparam);
	    } else {
		compare = ICAL_XLICCOMPARETYPE_EQUAL;
	    }

	    /* Find a property in the component that has the same type as
	       the gauge property */

	    targetprop = icalcomponent_get_first_property(comp,
							  icalproperty_isa(p));


	    if(targetprop == 0){
		continue;
	    }

	    /* Compare the values of the gauge property and the target
	       property */

	    rel = icalvalue_compare(icalproperty_get_value(p),
				    icalproperty_get_value(targetprop));
		
	    /* Now see if the comparison is equavalent to the comparison
	       specified in the gauge */

	    if (rel == compare){ 
		localpass++; 
	    } else if (compare == ICAL_XLICCOMPARETYPE_LESSEQUAL && 
		       ( rel == ICAL_XLICCOMPARETYPE_LESS ||
			 rel == ICAL_XLICCOMPARETYPE_EQUAL)) {
		localpass++;
	    } else if (compare == ICAL_XLICCOMPARETYPE_GREATEREQUAL && 
		       ( rel == ICAL_XLICCOMPARETYPE_GREATER ||
			 rel == ICAL_XLICCOMPARETYPE_EQUAL)) {
		localpass++;
	    } else if (compare == ICAL_XLICCOMPARETYPE_NOTEQUAL && 
		       ( rel == ICAL_XLICCOMPARETYPE_GREATER ||
			 rel == ICAL_XLICCOMPARETYPE_LESS)) {
		pass++;
	    } else {
		localpass = 0;
	    }

	    pass += localpass;
	}


	/* test subcomponents. Look for a child component that has a
	   counterpart in the gauge. If one is found, recursively call
	   icalstore_test */
	
	for(child = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	    child != 0;
	    child = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){
	
	    pass += icalstore_test(child,gauge);
	    
	}
    }

    return pass>0;

}
    
icalcomponent* icalstore_query(icalstore* store, icalstore* query);


icalcomponent* icalstore_fetch(icalstore* store, char* uid)
{
    icalcomponent *gauge;
    icalcomponent *old_gauge;
    icalcomponent *c;
    struct icalstore_impl *impl = (struct icalstore_impl*)store;

    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (uid!=0), "uid");

    gauge = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_uid(
		    uid,
		    icalparameter_new_xliccomparetype(
			ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    0);

    old_gauge = impl->gauge;
    impl->gauge = gauge;

    c= icalstore_get_first_component(store);

    impl->gauge = old_gauge;

    icalcomponent_free(gauge);

    return c;
}


int icalstore_has_uid(icalstore* store, char* uid)
{
    icalcomponent *c;

    icalerror_check_arg_rz( (store!=0), "store");
    icalerror_check_arg_rz( (uid!=0), "uid");
    
    /* HACK. This is a temporary implementation. _has_uid should use a
       database, and _fetch should use _has_uid, not the other way
       around */
    c = icalstore_fetch(store,uid);

    return c!=0;

}


icalerrorenum icalstore_select(icalstore* store, icalcomponent* gauge)
{
    struct icalstore_impl *impl = (struct icalstore_impl*)store;

    icalerror_check_arg_re( (store!=0), "store",ICAL_BADARG_ERROR);
    icalerror_check_arg_re( (gauge!=0), "gauge",ICAL_BADARG_ERROR);

    if (!icalcomponent_is_valid(gauge)){
	return ICAL_BADARG_ERROR;
    }

    impl->gauge = gauge;

    return ICAL_NO_ERROR;
}



icalcomponent* icalstore_get_first_component(icalstore* store)
{
    struct icalstore_impl *impl = (struct icalstore_impl*)store;
    icalerrorenum error;
    char path[PATH_MAX];

    error = icalstore_read_directory(impl);

    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return 0;
    }

    impl->directory_iterator = pvl_head(impl->directory);
    
    if (impl->directory_iterator == 0){
	icalerror_set_errno(error);
	return 0;
    }
    
    sprintf(path,"%s/%s",impl->dir,(char*)pvl_data(impl->directory_iterator));

   if (impl->cluster == 0){
	impl->cluster = icalcluster_new(path);

	if (impl->cluster == 0){
	    error = icalerrno;
	}
    } else {
	error = icalcluster_load(impl->cluster,path);

    }

    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return 0;
    }

    impl->first_component = 1;

    return icalstore_get_next_component(store);
}

icalcomponent* icalstore_get_next_component(icalstore* store)
{
    struct icalstore_impl *impl;
    icalcomponent *c;
    icalerrorenum error;

    icalerror_check_arg_rz( (store!=0), "store");

    impl = (struct icalstore_impl*)store;

    if(impl->cluster == 0){

	icalerror_warn("icalstore_get_next_component called with a NULL cluster (Caller must call icalstore_get_first_component first");
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;

    }

    /* Set the component iterator for the following for loop */
    if (impl->first_component == 1){
	icalcluster_get_first_component(
	    impl->cluster,
	    ICAL_ANY_COMPONENT);
	impl->first_component = 0;
    } else {
	icalcluster_get_next_component(
	    impl->cluster,
	    ICAL_ANY_COMPONENT);
    }


    while(1){
	/* Iterate through all of the objects in the cluster*/
	for( c = icalcluster_get_current_component(
	    impl->cluster);
	     c != 0;
	     c = icalcluster_get_next_component(
		 impl->cluster,
		 ICAL_ANY_COMPONENT)){
	    
	    /* If there is a gauge defined and the component does not
               pass the gauge, skip the rest of the loop */
	    if (impl->gauge != 0 && icalstore_test(c,impl->gauge) == 0){
		continue;
	    }

	    /* Either there is no gauge, or the component passed the
               gauge, so return it*/

	    return c;
	}

	/* Fell through the loop, so the component we want is not
	   in this cluster. Load a new cluster and try again.*/

	error = icalstore_next_cluster(store);

	if(impl->cluster == 0 || error != ICAL_NO_ERROR){
	    /* No more clusters */
	    return 0;
	} else {
	    c = icalcluster_get_first_component(
		impl->cluster,
		ICAL_ANY_COMPONENT);
	}
    }
}

   




