/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "mail-accounts.h"
#include "mail-preferences.h"
#include "mail-composer-prefs.h"
#include "mail-config-factory.h"

#define CONFIG_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ConfigControlFactory"

static BonoboGenericFactory *factory = NULL;


typedef void (*ApplyFunc) (GtkWidget *prefs);

struct _config_data {
	GtkWidget *prefs;
	ApplyFunc apply;
};

static void
config_control_destroy_cb (struct _config_data *data, GObject *deadbeef)
{
	g_object_unref (data->prefs);
	g_free (data);
}

static void
config_control_apply_cb (EvolutionConfigControl *config_control, void *user_data)
{
	struct _config_data *data = user_data;
	
	data->apply (data->prefs);
}

BonoboObject *
mail_config_control_factory_cb (BonoboGenericFactory *factory, const char *component_id, void *user_data)
{
	GNOME_Evolution_Shell shell = (GNOME_Evolution_Shell) user_data;
	EvolutionConfigControl *control;
	struct _config_data *data;
	GtkWidget *prefs = NULL;
	
	data = g_new (struct _config_data, 1);
	
	/* TODO: should use ascii_str*cmp? */
	if (!strcmp (component_id, MAIL_ACCOUNTS_CONTROL_ID)) {
		prefs = mail_accounts_tab_new (shell);
		data->apply = (ApplyFunc) mail_accounts_tab_apply;
	} else if (!strcmp (component_id, MAIL_PREFERENCES_CONTROL_ID)) {
		prefs = mail_preferences_new ();
		data->apply = (ApplyFunc) mail_preferences_apply;
	} else if (!strcmp (component_id, MAIL_COMPOSER_PREFS_CONTROL_ID)) {
		prefs = mail_composer_prefs_new ();
		data->apply = (ApplyFunc) mail_composer_prefs_apply;
	} else {
		g_assert_not_reached ();
	}

	data->prefs = prefs;
	g_object_ref (prefs);
	
	gtk_widget_show_all (prefs);
	
	control = evolution_config_control_new (prefs);
	
	if (!strcmp (component_id, MAIL_ACCOUNTS_CONTROL_ID)) {
		/* nothing to do here... */
	} else if (!strcmp (component_id, MAIL_PREFERENCES_CONTROL_ID)) {
		MAIL_PREFERENCES (prefs)->control = control;
	} else if (!strcmp (component_id, MAIL_COMPOSER_PREFS_CONTROL_ID)) {
		MAIL_COMPOSER_PREFS (prefs)->control = control;
	} else {
		g_assert_not_reached ();
	}
	
	g_signal_connect (control, "apply", G_CALLBACK (config_control_apply_cb), data);
	g_object_weak_ref ((GObject *) control, (GWeakNotify) config_control_destroy_cb, data);
	
	return BONOBO_OBJECT (control);
}
