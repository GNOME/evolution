/*
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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
#ifdef ENABLE_SMIME
	smime_component_init ();
#endif
}
