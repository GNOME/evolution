/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Calendar Conduit Configuration
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

#ifndef __TODO_CONDUIT_CONFIG_H__
#define __TODO_CONDUIT_CONFIG_H__

#include <gnome.h>
#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>

/* This is the configuration of the GnomeCal conduit. */
typedef struct _ECalConduitCfg ECalConduitCfg;
struct _ECalConduitCfg {
	gboolean open_secret;
	guint32 pilot_id;
	GnomePilotConduitSyncType  sync_type;   /* only used by capplet */
};

static void 
calconduit_load_configuration (ECalConduitCfg **c, guint32 pilot_id) 
{
	gchar prefix[256];
	g_snprintf (prefix, 255, "/gnome-pilot.d/e-todo-conduit/Pilot_%u/",
		    pilot_id);
	
	*c = g_new0 (ECalConduitCfg,1);
	g_assert (*c != NULL);

	gnome_config_push_prefix (prefix);
	(*c)->open_secret = gnome_config_get_bool ("open_secret=FALSE");

        /* set in capplets main */
	(*c)->sync_type = GnomePilotConduitSyncTypeCustom; 
	gnome_config_pop_prefix ();
	
	(*c)->pilot_id = pilot_id;
}

/* Saves the configuration data. */
static void
calconduit_save_configuration (ECalConduitCfg *c) 
{
	gchar prefix[256];

	g_snprintf (prefix, 255, "/gnome-pilot.d/e-todo-conduit/Pilot_%u/",
		    c->pilot_id);

	gnome_config_push_prefix (prefix);
	gnome_config_set_bool ("open_secret", c->open_secret);
	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();
}

/* Creates a duplicate of the configuration data */
static ECalConduitCfg*
calconduit_dupe_configuration (ECalConduitCfg *c) 
{
	ECalConduitCfg *retval;

	g_return_val_if_fail (c != NULL, NULL);

	retval = g_new0 (ECalConduitCfg, 1);
	retval->sync_type = c->sync_type;
	retval->open_secret = c->open_secret;
	retval->pilot_id = c->pilot_id;

	return retval;
}

static void 
calconduit_destroy_configuration (ECalConduitCfg **c) 
{
	g_return_if_fail (c != NULL);
	g_return_if_fail (*c != NULL);

	g_free (*c);
	*c = NULL;
}

#endif __TODO_CONDUIT_CONFIG_H__ 







