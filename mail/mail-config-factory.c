/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-account-prefs.h"
#include "em-composer-prefs.h"
#include "em-mailer-prefs.h"
#include "em-network-prefs.h"

#include "mail-config-factory.h"

#define CONFIG_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ConfigControlFactory:" BASE_VERSION

BonoboObject *
mail_config_control_factory_cb (BonoboGenericFactory *factory, const gchar *component_id, gpointer user_data)
{
	GNOME_Evolution_Shell shell = (GNOME_Evolution_Shell) user_data;
	EvolutionConfigControl *control;
	GtkWidget *prefs = NULL;

	if (!strcmp (component_id, EM_ACCOUNT_PREFS_CONTROL_ID)) {
		prefs = em_account_prefs_new (shell);
	} else if (!strcmp (component_id, EM_MAILER_PREFS_CONTROL_ID)) {
		prefs = em_mailer_prefs_new ();
	} else if (!strcmp (component_id, EM_COMPOSER_PREFS_CONTROL_ID)) {
		prefs = em_composer_prefs_new ();
	} else if (!strcmp (component_id, EM_NETWORK_PREFS_CONTROL_ID)) {
		prefs = em_network_prefs_new ();
	} else {
		g_return_val_if_reached(NULL);
	}

	gtk_widget_show_all (prefs);

	control = evolution_config_control_new (prefs);

	return BONOBO_OBJECT (control);
}
