/* -*- Mode: C -*-
  ======================================================================
  FILE: icalmemory.c
  CREATOR: eric 30 June 1999
  
  $Id$
  $Locker$
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is icalmemory.h
 The Initial Developer of the Original Code is Eric Busboom

 (C) COPYRIGHT 1999 The Software Studio. 
 http://www.softwarestudio.org

 ======================================================================*/

/* libical often passes strings back to the caller. To make these
 * interfaces simple, I did not want the caller to have to pass in a
 * memory buffer, but having libical pass out newly allocated memory
 * makes it difficult to de-allocate the memory.
 * 
 * The ring buffer in this scheme makes it possible for libical to pass
 * out references to memory which the caller does not own, and be able
 * to de-allocate the memory later. The ring allows libical to have
 * several buffers active simultaneously, which is handy when creating
 * string representations of components. */

#define ICALMEMORY_C

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "icalmemory.h"
#include "icalerror.h"

#include <stdio.h> /* for printf (debugging) */
#include <stdlib.h> /* for malloc, realloc */
#include <string.h> /* for memset() */

#define BUFFER_RING_SIZE 25
#define MIN_BUFFER_SIZE 200

void* buffer_ring[BUFFER_RING_SIZE+1];
int buffer_pos = 0;
int initialized = 0;

/* Create a new temporary buffer on the ring. Libical owns these and wil deallocate them. */
void*
icalmemory_tmp_buffer (size_t size)
{
    void *rtrn; 
    /* I don't think I need this -- I think static arrays are
       initialized to 0 as a standard part of C, but I am not sure. */

    if (initialized == 0){
	int i;
	for(i=0; i<BUFFER_RING_SIZE; i++){
	    buffer_ring[i]  = 0;
	}
	initialized = 1;
    }
    
    /* Ideally, this routine would re-use an existing buffer if it is
       larger than the requested buffer. Maybe later.... */

    if (size < MIN_BUFFER_SIZE){
	size = MIN_BUFFER_SIZE;
    }
    
    if ( buffer_ring[buffer_pos] != 0){
      /*sprintf(buffer_ring[buffer_pos], "***DEALLOCATED MEMORY***: %d",buffer_pos);*/
	free( buffer_ring[buffer_pos]);
	buffer_ring[buffer_pos] = 0;
    }


    rtrn = buffer_ring[buffer_pos] = (void*)malloc(size);

    memset(rtrn,0,size); 

    if(++buffer_pos > BUFFER_RING_SIZE){
	buffer_pos = 0;
    }

    return rtrn;
}

void icalmemory_free_ring()
{
	
   int i;
   for(i=0; i<BUFFER_RING_SIZE; i++){
    if ( buffer_ring[i] != 0){
       free( buffer_ring[i]);
    }
    buffer_ring[i]  = 0;
   }

   initialized = 1;

}

/* Like strdup, but the buffer is on the ring. */
char*
icalmemory_tmp_copy(char* str)
{
    char* b = icalmemory_tmp_buffer(strlen(str)+1);

    strcpy(b,str);

    return b;
}
    


void
icalmemory_free_tmp_buffer (void* buf)
{
   if(buf == 0)
   {
       return;
   }

   free(buf);
}


/* These buffer routines create memory the old fashioned way -- so the caller will have to delocate the new memory */

void* icalmemory_new_buffer(size_t size)
{
    /* HACK. need to handle out of memory case */
    void *b = malloc(size);

    memset(b,0,size); 

    return b;
}

void* icalmemory_resize_buffer(void* buf, size_t size)
{
    /* HACK. need to handle out of memory case */

    return realloc(buf, size);
}

void icalmemory_free_buffer(void* buf)
{
    free(buf);
}

void 
icalmemory_append_string(char** buf, char** pos, size_t* buf_size, 
			      char* string)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length, string_length;

#ifndef ICAL_NO_INTERNAL_DEBUG
    icalerror_check_arg_rv( (buf!=0),"buf");
    icalerror_check_arg_rv( (*buf!=0),"*buf");
    icalerror_check_arg_rv( (pos!=0),"pos");
    icalerror_check_arg_rv( (*pos!=0),"*pos");
    icalerror_check_arg_rv( (buf_size!=0),"buf_size");
    icalerror_check_arg_rv( (*buf_size!=0),"*buf_size");
    icalerror_check_arg_rv( (string!=0),"string");
#endif 

    string_length = strlen(string);
    data_length = (size_t)*pos - (size_t)*buf;    
    final_length = data_length + string_length; 

    if ( final_length >= (size_t) *buf_size) {

	
	*buf_size  = (*buf_size) * 2  + final_length;

	new_buf = realloc(*buf,*buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	*pos = new_pos;
	*buf = new_buf;
    }
    
    strcpy(*pos, string);

    *pos += string_length;
}


void 
icalmemory_append_char(char** buf, char** pos, size_t* buf_size, 
			      char ch)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length;

#ifndef ICAL_NO_INTERNAL_DEBUG
    icalerror_check_arg_rv( (buf!=0),"buf");
    icalerror_check_arg_rv( (*buf!=0),"*buf");
    icalerror_check_arg_rv( (pos!=0),"pos");
    icalerror_check_arg_rv( (*pos!=0),"*pos");
    icalerror_check_arg_rv( (buf_size!=0),"buf_size");
    icalerror_check_arg_rv( (*buf_size!=0),"*buf_size");
#endif

    data_length = (size_t)*pos - (size_t)*buf;

    final_length = data_length + 2; 

    if ( final_length > (size_t) *buf_size ) {

	
	*buf_size  = (*buf_size) * 2  + final_length +1;

	new_buf = realloc(*buf,*buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	*pos = new_pos;
	*buf = new_buf;
    }

    **pos = ch;
    *pos += 1;
    
}
