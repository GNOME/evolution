/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - ToDo Conduit Capplet
 *
 * Copyright (C) 1998 Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Eskil Heyn Olsen <deity@eskil.dk> 
 *          JP Rosevear <jpr@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __TODO_CONDUIT_H__
#define __TODO_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <gnome.h>
#include <pi-todo.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-sync-abs.h>
#include <cal-client/cal-client.h>
#include <e-pilot-map.h>

/* This is the local record structure for the Evolution ToDo conduit. */
typedef struct _EToDoLocalRecord EToDoLocalRecord;
struct _EToDoLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	GnomePilotDesktopRecord local;

	/* The corresponding Comp object */
	CalComponent *comp;

        /* pilot-link todo structure, used for implementing Transmit. */
	struct ToDo *todo;
};

/* This is the context for all the GnomeCal conduit methods. */
typedef struct _EToDoConduitContext EToDoConduitContext;
struct _EToDoConduitContext {
	EToDoConduitCfg *cfg;

	struct ToDoAppInfo ai;

	CalClient *client;
	char *calendar_file;
	gboolean calendar_load_tried;
	gboolean calendar_load_success;

	GList *uids;
	GList *changed;
	GHashTable *changed_hash;
	
	EPilotMap *map;
};

#endif __TODO_CONDUIT_H__ 






