/*
 * 
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#ifndef EVO_DBUS_H
#define EVO_DBUS_H

#include <stdio.h>
#include <glib.h>
#include <string.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbind.h>
#include <dbind-any.h>

/* Methods to be used when calling the remote server
   process from the client */

int           evolution_dbus_init (void);
DBindContext *evolution_dbus_peek_context (void);

#endif
