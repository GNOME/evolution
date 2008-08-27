/*
 * 
 * Authors: Srinivasa Ragavan <sragavan@novell.com>
 *
 * */

#include <stdio.h>
#include <glib.h>
#include <string.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include "evo-dbus.h"
#include <dbind.h>
#include <dbind-any.h>
#include "camel-object-remote.h"

#define d(x) x

static DBindContext *ctx = NULL;

int
evolution_dbus_init ()
{
	DBusError error;

	if (ctx)
		return 0;

	ctx = dbind_create_context (DBUS_BUS_SESSION, NULL);
	if (!ctx) {
		g_warning ("DBind context setup failed\n");
		return -1;
	}

	d(printf("DBind context setup: done\n"));

	return 0;
}

DBindContext *
evolution_dbus_peek_context ()
{
	if (!ctx) {
		evolution_dbus_init ();
		if (!ctx)
			return NULL;
	}

	return ctx;
}
