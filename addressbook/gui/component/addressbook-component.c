/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

/* EPFIXME: Add autocompletion setting.  */


#include <config.h>

#include "addressbook-component.h"
#include "addressbook-config.h"
#include "addressbook-migrate.h"
#include "addressbook-view.h"
#include "addressbook/gui/contact-editor/eab-editor.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "e-util/e-plugin.h"
#include "e-util/e-import.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"
#include "addressbook/gui/widgets/eab-config.h"
#include "addressbook/importers/evolution-addressbook-importers.h"

#include "misc/e-task-bar.h"
#include "misc/e-info-label.h"

#include "shell/e-component-view.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-util.h>
#include <libedataserver/e-url.h>

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#endif

static BonoboObjectClass *parent_class = NULL;

/* Evolution::Component CORBA methods.  */

static void
impl_upgradeFromVersion (PortableServer_Servant servant, short major, short minor, short revision, CORBA_Environment *ev)
{
	GError *err = NULL;

	if (!addressbook_migrate (addressbook_component_peek (), major, minor, revision, &err)) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading Address Book settings or folders."));
		failedex->why = CORBA_string_dup(err->message);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UpgradeFailed, failedex);
	}

	if (err)
		g_error_free(err);
}

/* Initialization.  */

static void
addressbook_component_class_init (AddressbookComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	epv->upgradeFromVersion      = impl_upgradeFromVersion;

	parent_class = g_type_class_peek_parent (class);
}

static void
addressbook_component_init (AddressbookComponent *component)
{
	static int first = TRUE;

#ifdef ENABLE_SMIME
	smime_component_init ();
#endif

	if (first) {
		EImportClass *klass;

		first = FALSE;
		e_plugin_hook_register_type(eab_popup_hook_get_type());
		e_plugin_hook_register_type(eab_menu_hook_get_type());
		e_plugin_hook_register_type(eab_config_hook_get_type());

		klass = g_type_class_ref(e_import_get_type());
		e_import_class_add_importer(klass, evolution_ldif_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_vcard_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_csv_outlook_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_csv_mozilla_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_csv_evolution_importer_peek(), NULL, NULL);
	}
}
