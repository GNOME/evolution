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

#ifndef __ADDR_CONDUIT_H__
#define __ADDR_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <gnome.h>
#include <pi-address.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-sync-abs.h>
#include <e-pilot-map.h>

/* This is the local record structure for the Evolution ToDo conduit. */
typedef struct _EAddrLocalRecord EAddrLocalRecord;
struct _EAddrLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	GnomePilotDesktopRecord local;

	/* The corresponding ECard object */
	ECard *ecard;

        /* pilot-link todo structure, used for implementing Transmit. */
	struct Address *addr;
};

/* This is the context for all the GnomeCal conduit methods. */
typedef struct _EAddrConduitContext EAddrConduitContext;
struct _EAddrConduitContext {
	EAddrConduitCfg *cfg;

	struct AddressAppInfo ai;

	EBook *ebook;
	GList *cards;
	
	gboolean address_load_tried;
	gboolean address_load_success;

	EPilotMap *map;
};

#endif __ADDR_CONDUIT_H__ 






