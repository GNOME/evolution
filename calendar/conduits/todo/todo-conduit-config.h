/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __TODO_CONDUIT_CONFIG_H__
#define __TODO_CONDUIT_CONFIG_H__

#include <gnome.h>
#include <libgpilotdCM/gnome-pilot-conduit-management.h>
#include <libgpilotdCM/gnome-pilot-conduit-config.h>

/* This is the configuration of the GnomeCal conduit. */
typedef struct _ToDoConduitCfg ToDoConduitCfg;
struct _ToDoConduitCfg {
	gboolean open_secret;
	guint32 pilotId;
	GnomePilotConduitSyncType  sync_type;   /* only used by capplet */
};

static void 
todoconduit_load_configuration (ToDoConduitCfg **c, guint32 pilotId) 
{
	gchar prefix[256];
	g_snprintf (prefix,255,"/gnome-pilot.d/todo-conduit/Pilot_%u/",pilotId);
	
	*c = g_new0 (ToDoConduitCfg,1);
	g_assert (*c != NULL);
	gnome_config_push_prefix (prefix);
	(*c)->open_secret = gnome_config_get_bool ("open_secret=FALSE");
	(*c)->sync_type = GnomePilotConduitSyncTypeCustom; /* set in capplets main */
	gnome_config_pop_prefix ();
	
	(*c)->pilotId = pilotId;
}

/* Saves the configuration data. */
static void
todoconduit_save_configuration (ToDoConduitCfg *c) 
{
	gchar prefix[256];

	g_snprintf(prefix,255,"/gnome-pilot.d/todo-conduit/Pilot_%u/",c->pilotId);

	gnome_config_push_prefix(prefix);
	gnome_config_set_bool ("open_secret", c->open_secret);
	gnome_config_pop_prefix();

	gnome_config_sync();
	gnome_config_drop_all();
}

/* Creates a duplicate of the configuration data */
static ToDoConduitCfg*
todoconduit_dupe_configuration (ToDoConduitCfg *c) 
{
	ToDoConduitCfg *retval;
	g_return_val_if_fail (c!=NULL,NULL);
	retval = g_new0 (ToDoConduitCfg,1);
	retval->sync_type = c->sync_type;
	retval->open_secret = c->open_secret;
	retval->pilotId = c->pilotId;
	return retval;
}

static void 
todoconduit_destroy_configuration (ToDoConduitCfg **c) 
{
	g_return_if_fail (c!=NULL);
	g_return_if_fail (*c!=NULL);
	g_free (*c);
	*c = NULL;
}

#endif __TODO_CONDUIT_CONFIG_H__ 







