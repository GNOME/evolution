/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-component-factory.c
 *
 * Authors: Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "em-utils.h"
#include "evolution-composer.h"
#include "mail-accounts.h"
#include "mail-component.h"
#include "mail-composer-prefs.h"
#include "mail-config-druid.h"
#include "mail-config-factory.h"
#include "mail-config.h"
#include "mail-mt.h"
#include "mail-preferences.h"

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-shlib-factory.h>

#include <string.h>


#define FACTORY_ID	"OAFIID:GNOME_Evolution_Mail_Factory_2"

#define COMPONENT_ID	"OAFIID:GNOME_Evolution_Mail_Component_2"
#define COMPOSER_ID	"OAFIID:GNOME_Evolution_Mail_Composer"
#define FOLDER_INFO_ID	"OAFIID:GNOME_Evolution_FolderInfo"
#define MAIL_CONFIG_ID	"OAFIID:GNOME_Evolution_MailConfig"
#define WIZARD_ID	"OAFIID:GNOME_Evolution_Mail_Wizard"


/* EPFIXME: This stuff is here just to get it to compile, it should be moved
   out of the way (was originally in component-factory.c).  */
EvolutionShellClient *global_shell_client = NULL;

static BonoboObject *
factory (BonoboGenericFactory *factory,
	 const char *component_id,
	 void *closure)
{
	/* EPFIXME this is messy.  The IDs are defined all over the place
	   without a logic... */

	if (strcmp (component_id, COMPONENT_ID) == 0) {
		MailComponent *component = mail_component_peek ();

		bonobo_object_ref (BONOBO_OBJECT (component));
		return BONOBO_OBJECT (component);
	} else if (strcmp(component_id, MAIL_CONFIG_ID) == 0) {
		return (BonoboObject *)g_object_new (evolution_mail_config_get_type (), NULL);
	} else if (strcmp(component_id, WIZARD_ID) == 0) {
		return evolution_mail_config_wizard_new();
	} else if (strcmp (component_id, MAIL_ACCOUNTS_CONTROL_ID) == 0
		   || strcmp (component_id, MAIL_PREFERENCES_CONTROL_ID) == 0
		   || strcmp (component_id, MAIL_COMPOSER_PREFS_CONTROL_ID) == 0) {
		return mail_config_control_factory_cb (factory, component_id, CORBA_OBJECT_NIL);
	} else if (strcmp(component_id, COMPOSER_ID) == 0) {
		/* FIXME: how to remove need for callbacks, probably make the composer more tightly integrated with mail */
		return (BonoboObject *) evolution_composer_new (em_utils_composer_send_cb, em_utils_composer_save_draft_cb);
	}

	g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);
	return NULL;
}

static Bonobo_Unknown
make_factory (PortableServer_POA poa, const char *iid, gpointer impl_ptr, CORBA_Environment *ev)
{
	static int init = 0;

	if (!init) {
		mail_config_init ();
		mail_msg_init ();
		init = 1;
	}

	return bonobo_shlib_factory_std (FACTORY_ID, poa, impl_ptr, factory, NULL, ev);
}

static BonoboActivationPluginObject plugin_list[] = {
	{ FACTORY_ID, make_factory},
	{ NULL }
};

const  BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list, "Evolution Mail component factory"
};
