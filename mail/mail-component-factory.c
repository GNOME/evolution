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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "em-composer-utils.h"
#include "evolution-composer.h"
#include "mail-component.h"
#include "em-account-prefs.h"
#include "em-mailer-prefs.h"
#include "em-composer-prefs.h"

#include "mail-config-factory.h"
#include "mail-config.h"
#include "mail-mt.h"

#include "importers/mail-importer.h"

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-shlib-factory.h>

#include <string.h>

/* TODO: clean up these definitions */

#define FACTORY_ID	"OAFIID:GNOME_Evolution_Mail_Factory:" BASE_VERSION
#define COMPONENT_ID	"OAFIID:GNOME_Evolution_Mail_Component:" BASE_VERSION
#define COMPOSER_ID	"OAFIID:GNOME_Evolution_Mail_Composer:" BASE_VERSION
#define FOLDER_INFO_ID	"OAFIID:GNOME_Evolution_FolderInfo:" BASE_VERSION

static BonoboObject *
factory(BonoboGenericFactory *factory, const char *component_id, void *closure)
{
	BonoboObject *o;

	if (strcmp (component_id, COMPONENT_ID) == 0) {
		MailComponent *component = mail_component_peek ();

		bonobo_object_ref (BONOBO_OBJECT (component));
		return BONOBO_OBJECT (component);
	} else if (strcmp (component_id, EM_ACCOUNT_PREFS_CONTROL_ID) == 0
		   || strcmp (component_id, EM_MAILER_PREFS_CONTROL_ID) == 0
		   || strcmp (component_id, EM_COMPOSER_PREFS_CONTROL_ID) == 0) {
		return mail_config_control_factory_cb (factory, component_id, CORBA_OBJECT_NIL);
	} else if (strcmp(component_id, COMPOSER_ID) == 0) {
		/* FIXME: how to remove need for callbacks, probably make the composer more tightly integrated with mail */
		return (BonoboObject *) evolution_composer_new (em_utils_composer_send_cb, em_utils_composer_save_draft_cb);
	}

	o = mail_importer_factory_cb(factory, component_id, NULL);
	if (o == NULL)
		g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);

	return o;
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
