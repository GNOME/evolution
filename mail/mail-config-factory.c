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

#include "em-account-prefs.h"
#include "em-composer-prefs.h"
#include "em-mailer-prefs.h"

#include "mail-config-factory.h"

#define CONFIG_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_Mail_ConfigControlFactory:" BASE_VERSION

BonoboObject *
mail_config_control_factory_cb (BonoboGenericFactory *factory, const char *component_id, void *user_data)
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
	} else {
		g_assert_not_reached ();
	}
	
	gtk_widget_show_all (prefs);
	
	control = evolution_config_control_new (prefs);
	
	return BONOBO_OBJECT (control);
}
