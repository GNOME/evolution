/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: JP Rosevear <jpr@novell.com>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>

#define GCONF_KEY "/apps/evolution/mail/display/thread_subject"

GtkWidget *org_gnome_subject_thread_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);

static void
toggled_cb (GtkWidget *widget, EConfig *config)
{
	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) config->target;
	
	/* Save the new setting to gconf */
	gconf_client_set_bool (target->gconf, GCONF_KEY, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)), NULL);
}

GtkWidget *
org_gnome_subject_thread_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	GtkWidget *check;
	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) hook_data->config->target;
	
	/* Create the checkbox we will display, complete with mnemonic that is unique in the dialog */
	check = gtk_check_button_new_with_mnemonic (_("Fall back to threading messages by sub_ject"));

	/* Set the toggle button to the current gconf setting */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), gconf_client_get_bool (target->gconf, GCONF_KEY, NULL));

	/* Listen for the item being toggled on and off */
	g_signal_connect (GTK_TOGGLE_BUTTON (check), "toggled", G_CALLBACK (toggled_cb), hook_data->config);
	
	/* Pack the checkbox in the parent widget and show it */
	gtk_box_pack_start (GTK_BOX (hook_data->parent), check, FALSE, FALSE, 0);
	gtk_widget_show (check);
	
	return check;
}
