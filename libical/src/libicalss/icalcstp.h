/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalcstp.h
  CREATOR: eric 20 April 1999
  
  $Id$


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

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
ssize_t icalcstp_send(icalcstp* cstp, char* msg);
ssize_t icalcstp_recieve(icalcstp* cstp, char* msg);

int icalcstp_set_timeout(icalcstp* cstp, int sec);

typedef struct icalcstp_response {	
	icalrequeststatus code
	char *arg; /* These strings are owned by libical */
	char *debug_text;
	char *more_text;
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
   which need several exchanges of data

   All of the server abd client command routines will generate
   response codes. On the server side, these responses will be turned
   into text and sent to the client. On the client side, the reponse
   is the one sent from the server.

   Since each command can return multiple responses, the responses are
   stored in the icalcstp object and are accesses by
   icalcstp_first_response() and icalcstp_next_response()

*/

   

/* Process a single line of incomming data */
char* icalcstp_process_incoming(icalcstp* cstp, char* string);

/* Er, they aren't really stubs, but pointers to the rountines that
   icalcstp_process_incoming will call when it recognizes a CSTP
   command in the data. BTW, the CONTINUE command is named 'cont'
   because 'continue' is a C keyword */

struct icalcstp_server_stubs {
  icalerrorenum (*abort)(icalcstp* cstp);
  icalerrorenum (*authenticate)(icalcstp* cstp, char* mechanism, 
                                    char* data);
  icalerrorenum (*calidexpand)(icalcstp* cstp, char* calid);
  icalerrorenum (*capability)(icalcstp* cstp);
  icalerrorenum (*cont)(icalcstp* cstp, unsigned int time);
  icalerrorenum (*identify)(icalcstp* cstp, char* id);
  icalerrorenum (*disconnect)(icalcstp* cstp);
  icalerrorenum (*sendata)(icalcstp* cstp, unsigned int time, 
                               icalcomponent *comp);
  icalerrorenum (*starttls)(icalcstp* cstp, char* command, 
                                char* data);
  icalerrorenum (*upnexpand)(icalcstp* cstp, char* upn);
  icalerrorenum (*unknown)(icalcstp* cstp, char* command, char* data);
}

/********************** Client (Sender) Interfaces **************************/

/* On the client side, the cstp code is connected directly to the
   socket, because the callers point of control is at the interfaces
   below. */

icalerrorenum icalcstp_abort(icalcstp* cstp);
icalerrorenum icalcstp_authenticate(icalcstp* cstp, char* mechanism, 
                                        char* data);
icalerrorenum icalcstp_capability(icalcstp* cstp);
icalerrorenum icalcstp_calidexpand(icalcstp* cstp,char* calid);
icalerrorenum icalcstp_continue(icalcstp* cstp, unsigned int time);
icalerrorenum icalcstp_disconnect(icalcstp* cstp);
icalerrorenum icalcstp_identify(icalcstp* cstp, char* id);
icalerrorenum icalcstp_starttls(icalcstp* cstp, char* command, 
                                    char* data);
icalerrorenum icalcstp_senddata(icalcstp* cstp, unsigned int time,
				icalcomponent *comp);
icalerrorenum icalcstp_upnexpand(icalcstp* cstp,char* calid);
icalerrorenum icalcstp_sendata(icalcstp* cstp, unsigned int time,
                                   icalcomponent *comp);

icalcstp_response icalcstp_first_response(icalcstp* cstp);
icalcstp_response icalcstp_next_response(icalcstp* cstp);



#endif /* !ICALCSTP_H */



