/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalirip.h
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
  The original code is icalirip.h

======================================================================*/


#ifndef ICALIRIP_H
#define ICALIRIP_H

#include "ical.h"

typedef void* icalirip;

/********************** Server (Reciever) Interfaces *************************/

icalirip* icalirip_new();
void* icalirip_free();

/* Protocol functions */
char* icalirip_process_request(icalirip* irip, char* string);

/* iRIP server stubs */
typedef struct icalirip_response {	
	char code[6];
	char caluid[1024];
	void* result;
} icalirip_response;

icalirip_response icalirip_timedout_stub(icalirip* irip);
icalirip_response icalirip_authenticate_stub(icalirip* irip, char* mechanism, char* data);
icalirip_response icalirip_sendata_stub(icalirip* irip, unsigned int time);
icalirip_response icalirip_dequeue_stub(icalirip* irip, char* caluid,unsigned int time);
icalirip_response icalirip_recipient_stub(icalirip* irip, char* address, unsigned int time);
icalirip_response icalirip_switch_stub(icalirip* irip);
icalirip_response icalirip_disconnect_stub(icalirip* irip);
icalirip_response icalirip_unknown_stub(icalirip* irip, char* command);

/* icalirip_set_stubs makes the module use function pointers to
instead of the above stubs. The set_stubs procedure will requires the
user to link another library with defined the above stubs and
re-directs the call to the appropriate pointer to function. */

typedef struct icalirip_stubs {

	void(*authenticate_stub)(icalirip* irip, char* mechanism, char* data);
	void (*sendata_stub)(icalirip* irip, unsigned int time);
	void (*dequeue_stub)(icalirip* irip, char* caluid, unsigned int time);
	void (*recipient_stub)(icalirip* irip, char* address, unsigned int time);
	void (*switch_stub)(icalirip* irip);
	void (*disconnect_stub)();
	void (*unknown_stub)(icalirip* irip, char* command, char** data);

} icalirip_stubs;

void icalirip_set_stubs(icalirip* irip, icalirip_stubs* stubs);

/********************** Client (Sender) Interfaces **************************/

/* Client API */
icalirip_response icalirip_abort(icalirip* irip);
icalirip_response icalirip_authenticate(icalirip* irip, char* mechanism, char* data);
icalirip_response icalirip_capability(icalirip* irip);
icalirip_response icalirip_continue(icalirip* irip, unsigned int time);
icalirip_response icalirip_sendata(icalirip* irip, icalcomponent* comp, unsigned int time);
icalirip_response icalirip_recipient(icalirip* irip, char* address, unsigned int time);
icalirip_response icalirip_dequeue(icalirip* irip, char* address, unsigned int time);
icalirip_response icalirip_switch(icalirip* irip);
icalirip_response icalirip_disconnect();
icalirip_response icalirip_unknown(icalirip* irip, char* command);

/* client stubs */
void icalirip_send_request(icalirip* irip,char* request);
char* icalirip_get_response(icalirip* irip);

/********************** Configuration Interfaces **************************/

/* Configure capabilities */
void icalirip_add_auth_mechanism(icalirip* irip, char* auth);
void icalirip_set_max_object_size(icalirip* irip, unsigned int size);
void icalirip_set_max_date(icalirip* irip, time_t time);
void icalirip_set_min_date(icalirip* irip, time_t time);


#endif /* !ICALIRIP_H */



