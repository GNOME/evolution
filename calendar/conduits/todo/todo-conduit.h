/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __TODO_CONDUIT_H__
#define __TODO_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <gnome.h>
#include <pi-todo.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>
#include <cal-client/cal-client.h>


/* This is the local record structure for the Evolution ToDo conduit. */
typedef struct _EToDoLocalRecord EToDoLocalRecord;
struct _EToDoLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	LocalRecord local;

	/* The corresponding Comp object */
	CalComponent *comp;

        /* pilot-link todo structure, used for implementing Transmit. */
	struct ToDo *todo;
};

/* This is the context for all the GnomeCal conduit methods. */
typedef struct _EToDoConduitContext EToDoConduitContext;
struct _EToDoConduitContext {
	struct ToDoAppInfo ai;
	ToDoConduitCfg *cfg;
	CalClient *client;

	gboolean calendar_load_tried;
	gboolean calendar_load_success;

	char *calendar_file;
};

#endif __TODO_CONDUIT_H__ 


