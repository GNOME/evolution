/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstp.h
  CREATOR: eric 20 April 1999
  
  $Id$


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

  The original author is Eric Busboom
  The original code is icalcstp.h

======================================================================*/


#ifndef ICALCSTP_H
#define ICALCSTP_H

#include "ical.h"

typedef void* icalcstp;

typedef struct icalcstp_stubs;

icalcstp* icalcstp_new(icalcstp_stubs* stubs, 
                       int incoming, int outgoing);

void* icalcstp_free(icalcstp* cstp);

/* Send or recieve data directly to/from the network. These calls are
   needed for the AUTHENTICATE command and possibly others */
int icalcstp_send(char*);
char* icalcstp_recieve(char*);

int icalcstp_set_timeout(icalcstp* cstp, int sec);

typedef struct icalcstp_response {	
	icalrequeststatus code
	char caluid[1024];
	void* result;
} icalcstp_response;


/********************** Server (Reciever) Interfaces *************************/

/* On the server side, the caller will recieve data from the incoming
   socket and pass it to icalcstp_process_incoming. The caller then
   takes the return from _process_incoming and sends it out through
   the socket. This gives the caller a point of control. If the cstp
   code connected to the socket itself, it would be hard for the
   caller to do anything else after the cstp code was started.

   However, some commands will use the sockets directly, though the
   _send and _recieve routines. Example is Authenticate and Starttls,
   which need several exchanges of data*/

/* Process a single line of incomming data */
char* icalcstp_process_incoming(icalcstp* cstp, char* string);

/* Er, they aren't really stubs, but pointers to the rountines that
   icalcstp_process_incoming will call when it recognizes a CSTP
   command in the data. BTW, the CONTINUE command is named 'cont'
   because 'continue' is a C keyword */

struct icalcstp_stubs {
  icalcstp_response (*abort)(icalcstp* cstp);
  icalcstp_response (*authenticate)(icalcstp* cstp, char* mechanism, 
                                    char* data);
  icalcstp_response (*capability)(icalcstp* cstp);
  icalcstp_response (*cont)(icalcstp* cstp, unsigned int time);
  icalcstp_response (*disconnect)(icalcstp* cstp);
  icalcstp_response (*identify)(icalcstp* cstp, char* id);
  icalcstp_response (*starttls)(icalcstp* cstp, char* command, 
                                char* data);
  icalcstp_response (*sendata)(icalcstp* cstp, unsigned int time, 
                               icalcomponent *comp);
  icalcstp_response (*unknown)(icalcstp* cstp, char* command, char* data);
}

/********************** Client (Sender) Interfaces **************************/

/* On the client side, the cstp code is connected directly to the
   socket, because the callers point of control is at the interfaces
   below. */

icalcstp_response icalcstp_abort(icalcstp* cstp);
icalcstp_response icalcstp_authenticate(icalcstp* cstp, char* mechanism, 
                                        char* data);
icalcstp_response icalcstp_capability(icalcstp* cstp);
icalcstp_response icalcstp_continue(icalcstp* cstp, unsigned int time);
icalcstp_response icalcstp_disconnect(icalcstp* cstp);
icalcstp_response icalcstp_identify(icalcstp* cstp, char* id);
icalcstp_response icalcstp_starttls(icalcstp* cstp, char* command, 
                                    char* data);
icalcstp_response icalcstp_sendata(icalcstp* cstp, unsigned int time,
                                   icalcomponent *comp);

#endif /* !ICALCSTP_H */



