/* -*- Mode: C -*-
  ======================================================================
  FILE: stow.c
  CREATOR: eric 29 April 2000
  
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
 
 The Initial Developer of the Original Code is Eric Busboom

 ======================================================================*/


#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h> /* for PATH_MAX */
#include <assert.h>
#include <stdlib.h>
#include <sys/utsname.h> /* for uname */
#include <sys/stat.h> /* for stat */
#include <unistd.h> /* for stat, getpid */

#include "ical.h"
#include "icalcalendar.h"


char* program_name;
#define TMPSIZE 2048

enum options {
    STORE_IN_DIR,
    STORE_IN_DB,
    INPUT_IS_EMAIL,
    INPUT_IS_ICAL,
    INPUT_FROM_STDIN,
    INPUT_FROM_FILE
};

struct options_struct
{
	enum options storage;
	enum options input_type;
	char* input_file;
	char* input_text;
	char* calid;
	char* caldir;
};



char* lowercase(char* str)
{
    char* p = 0;
    char* new = strdup(str);

    if(str ==0){
	return 0;
    }

    for(p = new; *p!=0; p++){
	*p = tolower(*p);
    }

    return new;
}

char* get_local_attendee(struct options_struct *opt)
{
    char attendee[PATH_MAX];
    char* user = getenv("USER");

    struct utsname uts;

    uname(&uts);

    /* HACK nodename may not be a fully qualified domain name */
    snprintf(attendee,PATH_MAX,"%s@%s",user,uts.nodename);
   
    return lowercase(attendee);
}

void usage(char *message)
{
}

icalcomponent* get_first_real_component(icalcomponent *comp)
{
    icalcomponent *c;

    for(c = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){
	if (icalcomponent_isa(c) == ICAL_VEVENT_COMPONENT ||
	    icalcomponent_isa(c) == ICAL_VTODO_COMPONENT ||
	    icalcomponent_isa(c) == ICAL_VJOURNAL_COMPONENT )
	{
	    return c;
	}
    }

    return 0;
}



char* make_mime(char* to, char* from, char* subject, 
		char* text_message, char* method, char* ical_message)
{
    size_t size = strlen(to)+strlen(from)+strlen(subject)+
	strlen(text_message)+ strlen(ical_message)+TMPSIZE;

    char mime_part_1[TMPSIZE];
    char mime_part_2[TMPSIZE];
    char content_id[TMPSIZE];
    char boundary[TMPSIZE];
    struct utsname uts;
    char* m;


    if ((m = malloc(sizeof(char)*size)) == 0){
	fprintf(stderr,"%s: Can't allocate memory: %s\n",program_name,strerror(errno));
	exit(1);
    }

    uname(&uts);

    srand(time(0)<<getpid());
    sprintf(content_id,"%d-%d@%s",time(0),rand(),uts.nodename);
    sprintf(boundary,"%d-%d-%s",time(0),rand(),uts.nodename);

    sprintf(mime_part_1,"Content-ID: %s\n\
Content-type: text/plain\n\
Content-Description: Text description of error message\n\n\
%s\n\n%s",
	    content_id,text_message,boundary);

    if(ical_message != 0 && method != 0){
	sprintf(mime_part_2,"\nContent-ID: %s\n\
Content-type: text/calendar; method=%s\n\
Content-Description: ICal component reply\n\n\
%s\n\n%s--\n",
		content_id,method,ical_message,boundary);
    }

    sprintf(m,"To: %s\n\
From: %s\n\
Subject: %s\n\
MIME-Version: 1.0\n\
Content-ID: %s\n\
Content-Type:  multipart/mixed; boundary=\"%s\"\n\
\n\
 This is a multimedia message in MIME format\n\
\n\
%s
",
	    to,from,subject,content_id,boundary,boundary,
	    mime_part_1);

    if(ical_message != 0 && method != 0){
	strcat(m, mime_part_2);
    } else {
	strcat(m,"--\n");
    }

    return m;
}

/* The incoming component had fatal errors */
void return_failure(icalcomponent* comp,  char* message, struct options_struct *opt)
{
    

    fputs(make_mime("Dest", "Source", "iMIP error", 
		    message, "reply",
		    icalcomponent_as_ical_string(comp)),stdout);

}

/* The program had a fatal error and could not process the incoming component*/
void return_error(icalcomponent* comp,  char* message, struct options_struct *opt)
{
    

    fputs(make_mime("Dest", "Source", "iMIP system failure", 
		    message, 0,0),stdout);

}

char* check_component(icalcomponent* comp,  struct options_struct *opt)
{
    static char static_component_error_str[PATH_MAX];
    char* component_error_str=0;
    icalcomponent* inner;
    int errors = 0;
    icalproperty *p;
    int found_attendee = 0;

    /* This do/while loop only executes once because it is being used
       to fake exceptions */

    do {

	/* Check that the root component is a VCALENDAR */
	if(icalcomponent_isa(comp) != ICAL_VCALENDAR_COMPONENT){
	    strcpy(static_component_error_str,
		   "Root component is not a VCALENDAR");
	    component_error_str = static_component_error_str;
	    break;
	}

	/* Check that the component passes iTIP restrictions */
	
	errors = icalcomponent_count_errors(comp);
	icalrestriction_check(comp);
	
	if(errors != icalcomponent_count_errors(comp)){
	    strcpy(static_component_error_str,
		   "The component does not conform to iTIP restrictions");
	    component_error_str = static_component_error_str;
	    break;
	}

	/* Check that the component has a METHOD */

	if (!icalcomponent_get_first_property(comp,ICAL_METHOD_PROPERTY) == 0)
	{
	    strcpy(static_component_error_str,
		   "Component does not have a METHOD property");
	    component_error_str = static_component_error_str;
	    break;
	}
	
	/* Check for this user as an attendee */


	inner = get_first_real_component(comp);

	for(p = icalcomponent_get_first_property(inner,ICAL_ATTENDEE_PROPERTY);
	    p != 0;
	    p = icalcomponent_get_next_property(inner,ICAL_ATTENDEE_PROPERTY)){
	    
	    char* s = icalproperty_get_attendee(p);
	    char* lower_attendee = lowercase(s);
	    char* local_attendee = get_local_attendee(opt);

	    /* Check that attendee begins with "mailto:" */
	    if (strncmp(lower_attendee,"mailto:",7) == 0){
		/* skip over the mailto: part */
		lower_attendee += 7;

		if(strcmp(lower_attendee,local_attendee) == 0){
		    found_attendee = 1;
		}
		
		lower_attendee -= 7;

		free(local_attendee);
		free(lower_attendee);
		
	    } 
	}
	
	if (found_attendee == 0){
	    char* local_attendee = get_local_attendee(opt);
	    snprintf(static_component_error_str,PATH_MAX,
		   "This target user (%s) is not listed as an attendee",
		    local_attendee );
	    component_error_str = static_component_error_str;
	    free(local_attendee);

	    break;
	}

    } while(0);

    return component_error_str;
}

void get_options(int argc, char* argv[], struct options_struct *opt)
{
    opt->storage = STORE_IN_DIR;
    opt->input_type = INPUT_FROM_STDIN;
    opt->input_file = 0;
    opt->input_text = 0;
    opt->calid = 0;
    opt->caldir = 0;
}

char* check_options(struct options_struct *opt)
{
    return 0;
}

void store_component(icalcomponent *comp, icalcalendar* cal, 
		     struct options_struct *opt)
{

    icalcluster *incoming = 0;
    icalerrorenum error; 

    incoming = icalcalendar_get_incoming(cal);

    if (incoming == 0){
	fprintf(stderr,"%s: Failed to get incoming component directory: %s\n",
		program_name, icalerror_strerror(icalerrno));
	exit(1);
    }

    error = icalcluster_add_component(incoming,comp);
    
    if (error != ICAL_NO_ERROR){
	fprintf(stderr,"%s: Failed to write incoming component: %s\n",
		program_name, icalerror_strerror(icalerrno));
	exit(1);
    }
    
    error = icalcluster_commit(incoming);
    
    if (error != ICAL_NO_ERROR){
	fprintf(stderr,"%s: Failed to commit incoming cluster: %s\n",
		program_name, icalerror_strerror(icalerrno));
	exit(1);
    }
    
    return;
}

enum file_type
{
    ERROR,
    NO_FILE,
    DIRECTORY,
    REGULAR,
    OTHER
};

enum file_type test_file(char *path)
{
    struct stat sbuf;
    enum file_type type;
    
    errno = 0;

    /* Check if the path already exists and if it is a directory*/
    if (stat(path,&sbuf) != 0){
	
	/* A file by the given name does not exist, or there was
           another error */
	if(errno == ENOENT)
	{
	    type = NO_FILE;
	} else {
	    type = ERROR;
	}

    } else {
	/* A file by the given name exists, but is it a directory? */
	
	if (S_ISDIR(sbuf.st_mode)){ 
	    type = DIRECTORY;
	} else if(S_ISREG(sbuf.st_mode)){ 
	    type = REGULAR;
	} else {
	    type = OTHER;
	}
    }

    return type;
}

icalcalendar* get_calendar(icalcomponent* comp, struct options_struct *opt)
{
    
    struct stat sbuf;
    char calpath[PATH_MAX];
    char facspath[PATH_MAX];
    char* home = getenv("HOME");
    char* user = getenv("USER");
    enum file_type type;
    icalcalendar* cal; 

    snprintf(facspath,PATH_MAX,"%s/.facs",home);

    type = test_file(facspath);

    errno = 0;
    if (type == NO_FILE){

	if(mkdir(facspath,0775) != 0){
	    fprintf(stderr,"%s: Failed to create calendar store directory %s: %s\n",
		    program_name,facspath, strerror(errno));
	    exit(1);
	} else {
	    printf("%s: Creating calendar store directory %s\n",program_name,facspath);
	}

    } else if(type==REGULAR || type == ERROR){
	    fprintf(stderr,"%s: Cannot create calendar store directory %s\n",
		    program_name,facspath);
	    exit(1);
    } 



    snprintf(calpath,PATH_MAX,"%s/%s",facspath,user);

    type = test_file(calpath);

    errno = 0;

    if (type == NO_FILE){

	if(mkdir(calpath,0775) != 0){
	    fprintf(stderr,"%s: Failed to create calendar directory %s: %s\n",
		    program_name,calpath, strerror(errno));
	} else {
	    printf("%s: Creating calendar store directory %s\n",program_name,facspath);
	}
    } else if(type==REGULAR || type == ERROR){
	    fprintf(stderr,"%s: Cannot create calendar directory %s\n",
		    program_name,calpath);
	    exit(1);
    } 

    cal = icalcalendar_new(calpath);

    if(cal == 0){
	fprintf(stderr,"%s: Failed to open calendar at %s: %s",
		program_name,calpath,icalerror_strerror(icalerrno));
	exit(1);
    }

    return cal;

}

char* read_stream(char *s, size_t size, void *d)
{
  char *c = fgets(s,size, (FILE*)d);

  return c;
}

icalcomponent* read_component(struct options_struct *opt)
{
    FILE *stream;
    icalcomponent *comp;
    icalparser* parser = icalparser_new();
    char* line;

    if(opt->input_type == INPUT_FROM_FILE){
 	stream = fopen(opt->input_file,"r"); 	

	if (stream == 0){
	    perror("Can't open input file");
	    exit(1);
	}

    } else {
 	stream = stdin;  	 	
    }

    assert(stream != 0);
    icalparser_set_gen_data(parser,stream);
   
    do {	
	line = icalparser_get_line(parser,read_stream);
	
	comp = icalparser_add_line(parser,line);
	
	if (comp != 0){
	    icalparser_claim(parser);
	    return comp;
	}
	
    } while ( line != 0);

    if(opt->input_type == INPUT_FROM_FILE){
	fclose(stream);
    }

    return comp;
 }

int main(int argc, char* argv[] )
{
    char* options_error_str;
    char* component_error_str;
    icalcalendar* cal;
    icalcomponent* comp, *reply;
    struct options_struct opt;

    program_name = argv[0];

    get_options(argc, argv, &opt);

    if ( (options_error_str = check_options(&opt)) != 0 ){
	usage(options_error_str);
	exit(1);
    }    

    comp = read_component(&opt);

    if ( (component_error_str = check_component(comp,&opt)) != 0){
	return_failure(comp, component_error_str, &opt);
	exit(1);
    }

    cal = get_calendar(comp,&opt);

    store_component(comp,cal, &opt);

    icalcomponent_free(comp);
    icalcalendar_free(cal);

    exit(0);
}

