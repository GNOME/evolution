/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - Calendar Conduit Configuration
 *
 * Copyright (C) 1998 Free Software Foundation
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Eskil Heyn Olsen <deity@eskil.dk> 
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#ifndef __CAL_CONDUIT_CONFIG_H__
#define __CAL_CONDUIT_CONFIG_H__

#include <gnome.h>
#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>

/* Configuration info */
typedef struct _ECalConduitCfg ECalConduitCfg;
struct _ECalConduitCfg {
	guint32 pilot_id;
	GnomePilotConduitSyncType  sync_type;

	gboolean open_secret;
	gchar *last_uri;
};

#ifdef CAL_CONFIG_LOAD
/* Loads the configuration data */
static void 
calconduit_load_configuration (ECalConduitCfg **c, guint32 pilot_id) 
{
	GnomePilotConduitManagement *management;
	GnomePilotConduitConfig *config;
	gchar prefix[256];
	g_snprintf (prefix, 255, "/gnome-pilot.d/e-calendar-conduit/Pilot_%u/",
		    pilot_id);
	
	*c = g_new0 (ECalConduitCfg,1);
	g_assert (*c != NULL);

	(*c)->pilot_id = pilot_id;

	management = gnome_pilot_conduit_management_new ("e_calendar_conduit", GNOME_PILOT_CONDUIT_MGMT_ID);
	config = gnome_pilot_conduit_config_new (management, pilot_id);
	if (!gnome_pilot_conduit_config_is_enabled (config, &(*c)->sync_type))
		(*c)->sync_type = GnomePilotConduitSyncTypeNotSet;
	gtk_object_unref (GTK_OBJECT (config));
	gtk_object_unref (GTK_OBJECT (management));

	/* Custom settings */
	gnome_config_push_prefix (prefix);

	(*c)->open_secret = gnome_config_get_bool ("open_secret=FALSE");
	(*c)->last_uri = gnome_config_get_string ("last_uri");

	gnome_config_pop_prefix ();
}
#endif

#ifdef CAL_CONFIG_SAVE
/* Saves the configuration data. */
static void
calconduit_save_configuration (ECalConduitCfg *c) 
{
	gchar prefix[256];

	g_snprintf (prefix, 255, "/gnome-pilot.d/e-calendar-conduit/Pilot_%u/",
		    c->pilot_id);

	gnome_config_push_prefix (prefix);
	gnome_config_set_bool ("open_secret", c->open_secret);
	gnome_config_set_string ("last_uri", c->last_uri);
	gnome_config_pop_prefix ();

	gnome_config_sync ();
	gnome_config_drop_all ();
}
#endif

#ifdef CAL_CONFIG_DUPE
/* Creates a duplicate of the configuration data */
static ECalConduitCfg*
calconduit_dupe_configuration (ECalConduitCfg *c) 
{
	ECalConduitCfg *retval;

	g_return_val_if_fail (c != NULL, NULL);

	retval = g_new0 (ECalConduitCfg, 1);
	retval->sync_type = c->sync_type;
	retval->pilot_id = c->pilot_id;

	retval->open_secret = c->open_secret;
	retval->last_uri = g_strdup (c->last_uri);
	
	return retval;
}
#endif

#ifdef CAL_CONFIG_DESTROY
/* Destroy a configuration */
static void 
calconduit_destroy_configuration (ECalConduitCfg **c) 
{
	g_return_if_fail (c != NULL);
	g_return_if_fail (*c != NULL);

	g_free ((*c)->last_uri);
	g_free (*c);
	*c = NULL;
}
#endif

#endif /* __CAL_CONDUIT_CONFIG_H__ */







