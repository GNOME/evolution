/* -*- Mode: C -*-
  ======================================================================
  FILE: icalcluster.c
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


#include "icalcluster.h"
#include <errno.h>
#include <limits.h> /* For PATH_MAX */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat, getpid */
#include <stdlib.h>
#include <string.h>

struct icalcluster_impl {
	char *path;
	icalcomponent* cluster;
	int changed;
};

icalcluster* icalcluster_new_impl()
{
    struct icalcluster_impl* comp;

    if ( ( comp = (struct icalcluster_impl*)
	   malloc(sizeof(struct icalcluster_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }

    return comp;
}

icalerrorenum icalcluster_create_cluster(char *path)
{

    FILE* f;
    int r;
    icalcomponent *c;
    struct icaltimetype tt;

    icalerror_clear_errno();

    f = fopen(path,"w");

    if (f == 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }

    /* Create the root component in the cluster. This component holds
       all of the other components and stores a count of
       components. */

    memset(&tt,0,sizeof(struct icaltimetype));

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_xlicclustercount(0),
	icalproperty_new_dtstart(tt), /* dtstart of earliest comp */
	icalproperty_new_dtend(tt), /* dtend of latest comp, excl. recuring */
	0
	);
	
    if (c == 0){
	fclose(f);
	icalerror_set_errno(ICAL_INTERNAL_ERROR);
	return ICAL_INTERNAL_ERROR;
    }


    /* Write the base component to the file */
    r = fputs(icalcomponent_as_ical_string(c),f);
	    
    fclose(f);

    icalcomponent_free(c);

    if (r == EOF){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }

    return ICAL_NO_ERROR;
}

FILE* parser_file; /*HACK. Not Thread Safe */
char* read_from_file(char *s, size_t size)
{
    char *c = fgets(s,size, parser_file);
    return c;
}

icalerrorenum icalcluster_load(icalcluster* cluster, char* path)
{
    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;
    icalerrorenum error;
    errno = 0;
 
    icalerror_check_arg_rz((cluster!=0),"cluster");
    icalerror_check_arg_rz((path!=0),"path");

    if(impl->path != 0 && strcmp(impl->path,path) == 0){
	/* Already have the right cluster, so return */
	return ICAL_NO_ERROR;
    }

    error = icalcluster_commit(cluster);
    
    if (error != ICAL_NO_ERROR){
	icalerror_set_errno(error);
	return error;
    }
    
    free(impl->path);
    
    impl->path= (char*)strdup(path);

    parser_file = fopen(impl->path,"r");
    
    /* HACK. Yeah, the following code is horrible....*/
    if (parser_file ==0 || errno != 0){
	
	/* Try to create the cluster */
	error = icalcluster_create_cluster(path);
	
	if (error == ICAL_NO_ERROR){
	    /* Try to open the parser again. */
	    errno = 0;
	    parser_file = fopen(impl->path,"r");
	    
	    if (parser_file ==0 || errno != 0){
		impl->cluster = 0;
		icalerror_set_errno(ICAL_FILE_ERROR);
		return ICAL_FILE_ERROR;
	    }
	} else {
	    impl->cluster = 0;
	    icalerror_set_errno(error); /* Redundant, actually */
	    return error;
	}
    }
    
    impl->cluster = icalparser_parse(read_from_file);
    
    fclose(parser_file);
    
    if (impl->cluster == 0){
	icalerror_set_errno(ICAL_PARSE_ERROR);
	return ICAL_PARSE_ERROR;
    }
    
    return ICAL_NO_ERROR;
}


icalcluster* icalcluster_new(char* path)
{
    struct icalcluster_impl *impl = icalcluster_new_impl(); 
    struct stat sbuf;
    int createclusterfile = 0;
    icalerrorenum error;
    
    icalerror_clear_errno();
    icalerror_check_arg_rz( (path!=0), "path");

    if (impl == 0){
	return 0;
    }

    /*impl->path = strdup(path); icalcluster_load does this */
    impl->changed  = 0;
    impl->cluster = 0;
    impl->path = 0;
        
    /* Check if the path already exists and if it is a regular file*/
    if (stat(path,&sbuf) != 0){
	
	/* A file by the given name does not exist, or there was
           another error */
	
	if (errno == ENOENT) {
	    /* It was because the file does not exist */
	    createclusterfile = 1;
	} else {
	    /* It was because of another error */
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return 0;
	}
    } else {
	/* A file by the given name exists, but is it a regular file */
	
	if (!S_ISREG(sbuf.st_mode)){ 
	    /* Nope, not a directory */
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return 0;
	} else {
	    /* Lets assume that it is a file of the right type */
	    createclusterfile = 0;
	}	
    }
    
    /* if cluster does not already exist, create it */
    
    if (createclusterfile == 1) {
	error = icalcluster_create_cluster(path);

	if (error != ICAL_NO_ERROR){
	    icalerror_set_errno(error);
	    return 0;
	}
    }
    
    error = icalcluster_load(impl,path);
    
    if (error != ICAL_NO_ERROR){
	return 0;
    }
    
    return impl;
}
	
void icalcluster_free(icalcluster* cluster)
{
    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rv((cluster!=0),"cluster");

    if (impl->cluster != 0){
	icalcluster_commit(cluster);
	icalcomponent_free(impl->cluster);
	impl->cluster=0;
    }

    if(impl->path != 0){
	free(impl->path);
	impl->path = 0;
    }

    free(impl);
}

icalerrorenum icalcluster_commit(icalcluster* cluster)
{
    int ws; /* Size in char of file written to disk */
    FILE *f;

    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);

    if (impl->changed != 0 ){
	/* write the cluster to disk */

	/* Construct a filename and write out the file */
	
	if ( (f = fopen(impl->path,"w")) != 0){

	    char* str = icalcomponent_as_ical_string(impl->cluster);
	    
	    ws = fwrite(str,sizeof(char),strlen(str),f);
	    
	    if ( ws < strlen(str)){
		fclose(f);
		return ICAL_FILE_ERROR;
	    }
	    
	    fclose(f);
            impl->changed = 0;
	    return ICAL_NO_ERROR;
	} else {
	    icalerror_set_errno(ICAL_FILE_ERROR);
	    return ICAL_FILE_ERROR;
	}

    } 

    return ICAL_NO_ERROR;
}

void icalcluster_mark(icalcluster* cluster){

    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rv((impl!=0),"cluster");

    impl->changed = 1;

}

icalcomponent* icalcluster_get_component(icalcluster* cluster){
   struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;

   icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);

   return impl->cluster;
}


/* manipulate the components in the cluster */

icalerrorenum icalcluster_add_component(icalcluster *cluster,
			       icalcomponent* child)
{
    struct icalcluster_impl* impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rv((cluster!=0),"cluster");
    icalerror_check_arg_rv((child!=0),"child");

    icalcomponent_add_component(impl->cluster,child);

    icalcluster_mark(cluster);

    return ICAL_NO_ERROR;

}

icalerrorenum icalcluster_remove_component(icalcluster *cluster,
				  icalcomponent* child)
{
    struct icalcluster_impl* impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rv((cluster!=0),"cluster");
    icalerror_check_arg_rv((child!=0),"child");

    icalcomponent_remove_component(impl->cluster,child);

    icalcluster_mark(cluster);

    return ICAL_NO_ERROR;
}

int icalcluster_count_components(icalcluster *cluster,
				 icalcomponent_kind kind)
{
    struct icalcluster_impl* impl = (struct icalcluster_impl*)cluster;

    if(cluster == 0){
	icalerror_set_errno(ICAL_BADARG_ERROR);
	return -1;
    }

    return icalcomponent_count_components(impl->cluster,kind);
}

/* Iterate through components */
icalcomponent* icalcluster_get_current_component (icalcluster* cluster)
{
    struct icalcluster_impl* impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rz((cluster!=0),"cluster");

    return icalcomponent_get_current_component(impl->cluster);
}

icalcomponent* icalcluster_get_first_component(icalcluster* cluster,
					       icalcomponent_kind kind)
{
    struct icalcluster_impl* impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rz((cluster!=0),"cluster");

    return icalcomponent_get_first_component(impl->cluster,kind);
}

icalcomponent* icalcluster_get_next_component(icalcluster* cluster,
					      icalcomponent_kind kind)
{
    struct icalcluster_impl* impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_rz((cluster!=0),"cluster");

    return icalcomponent_get_next_component(impl->cluster,kind);
}

