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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "icalcluster.h"
#include <errno.h>
#include <limits.h> /* For PATH_MAX */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat, getpid */
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* for fcntl */
#include <unistd.h> /* for fcntl */

icalerrorenum icalcluster_create_cluster(char *path);

struct icalcluster_impl {
	char *path;
	icalcomponent* cluster;
	int changed;
	FILE* stream;
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

char* read_from_file(char *s, size_t size, void *d)
{
    char *c = fgets(s,size, (FILE*)d);
    return c;
}

icalcluster* icalcluster_new(char* path)
{
    struct icalcluster_impl *impl = icalcluster_new_impl(); 
    struct stat sbuf;
    int createclusterfile = 0;
    icalerrorenum error = ICAL_NO_ERROR;
    icalparser *parser;
    struct icaltimetype tt;
    off_t cluster_file_size;

    memset(&tt,0,sizeof(struct icaltimetype));

    icalerror_clear_errno();
    icalerror_check_arg_rz( (path!=0), "path");

    if (impl == 0){
	return 0;
    }

    /*impl->path = strdup(path); icalcluster_load does this */
    impl->changed  = 0;

    impl->cluster = 0;

    impl->path = 0;
    impl->stream = 0;
        
    /* Check if the path already exists and if it is a regular file*/
    if (stat(path,&sbuf) != 0){
	
	/* A file by the given name does not exist, or there was
           another error */
	cluster_file_size = 0;
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
          cluster_file_size = sbuf.st_size;
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

    impl->path = (char*)strdup(path);

    errno = 0;
    impl->stream = fopen(impl->path,"r");
    
    if (impl->stream ==0 || errno != 0){
	impl->cluster = 0;
	icalerror_set_errno(ICAL_FILE_ERROR); /* Redundant, actually */
	return 0;
    }

    icalcluster_lock(impl);

    if(cluster_file_size > 0){
      parser = icalparser_new();
      icalparser_set_gen_data(parser,impl->stream);
      impl->cluster = icalparser_parse(parser,read_from_file);
      icalparser_free(parser);

      if (icalcomponent_isa(impl->cluster) != ICAL_XROOT_COMPONENT){
        /* The parser got a single component, so it did not put it in
           an XROOT. */
        icalcomponent *cl = impl->cluster;
        impl->cluster = icalcomponent_new(ICAL_XROOT_COMPONENT);
        icalcomponent_add_component(impl->cluster,cl);
      }

    } else {

      impl->cluster = icalcomponent_new(ICAL_XROOT_COMPONENT);
    }      

    if (impl->cluster == 0){
	icalerror_set_errno(ICAL_PARSE_ERROR);
	return 0;
    }
    
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

    if(impl->stream != 0){
	icalcluster_unlock(impl);
	fclose(impl->stream);
	impl->stream = 0;
    }

    free(impl);
}

char* icalcluster_path(icalcluster* cluster)
{
    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;
    icalerror_check_arg_rz((cluster!=0),"cluster");

    return impl->path;
}


int icalcluster_lock(icalcluster *cluster)
{
    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;
    struct flock lock;
    int fd; 

    icalerror_check_arg_rz((impl->stream!=0),"impl->stream");

    fd  = fileno(impl->stream);

    lock.l_type = F_WRLCK;     /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = 0;  /* byte offset relative to l_whence */
    lock.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = 0;       /* #bytes (0 means to EOF) */

    return (fcntl(fd, F_SETLKW, &lock)); 
}

int icalcluster_unlock(icalcluster *cluster)
{
    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;
    int fd;
    struct flock lock;
    icalerror_check_arg_rz((impl->stream!=0),"impl->stream");

    fd  = fileno(impl->stream);

    lock.l_type = F_WRLCK;     /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = 0;  /* byte offset relative to l_whence */
    lock.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = 0;       /* #bytes (0 means to EOF) */

    return (fcntl(fd, F_UNLCK, &lock)); 

}

icalerrorenum icalcluster_create_cluster(char *path)
{

    FILE* f;
    int r;
    icalcomponent *c;

    icalerror_clear_errno();

    f = fopen(path,"w");

    if (f == 0){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }

    
    /* This used to write data to the file... */

	    
    fclose(f);

    return ICAL_NO_ERROR;
}

icalerrorenum icalcluster_commit(icalcluster* cluster)
{
    FILE *f;
    char tmp[PATH_MAX]; /* HACK Buffer overflow potential */
    char *str;
    icalparser *parser;
    icalcomponent *c;
    
    struct icalcluster_impl *impl = (struct icalcluster_impl*)cluster;

    icalerror_check_arg_re((impl!=0),"cluster",ICAL_BADARG_ERROR);

    if (impl->changed == 0 ){
	return ICAL_NO_ERROR;
    }
    
#ifdef ICAL_SAFESAVES
    snprintf(tmp,PATH_MAX,"%s-tmp",impl->path);
#else	
    strcpy(tmp,impl->path);
#endif
    
    if ( (f = fopen(tmp,"w")) < 0 ){
	icalerror_set_errno(ICAL_FILE_ERROR);
	return ICAL_FILE_ERROR;
    }
    
    for(c = icalcomponent_get_first_component(impl->cluster,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(impl->cluster,ICAL_ANY_COMPONENT)){

	str = icalcomponent_as_ical_string(c);
    
	if (  fwrite(str,sizeof(char),strlen(str),f) < strlen(str)){
	    fclose(f);
	    return ICAL_FILE_ERROR;
	}
    }
    
    fclose(f);
    impl->changed = 0;    
        
#ifdef ICAL_SAFESAVES
    rename(tmp,impl->path); /* HACK, should check for error here */
#endif
    
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

