/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2005  Novell, Inc.
 *
 * Authors: Michael Zucchi <notzed@novell.com>
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

#include <errno.h>
#include <string.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-arg.h>
#include "evolution-mail-folder.h"

#include <libedataserver/e-account.h>

#define FACTORY_ID "OAFIID:GNOME_Evolution_Mail_Folder_Factory:" BASE_VERSION
#define MAIL_FOLDER_ID  "OAFIID:GNOME_Evolution_Mail_Folder:" BASE_VERSION

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_folder_get_type()))

struct _EvolutionMailFolderPrivate {
	int dummy;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionMailFolder *emf = (EvolutionMailFolder *)object;
	struct _EvolutionMailFolderPrivate *p = _PRIVATE(object);

	p = p;
	g_warning("EvolutionMailFolder is finalised!\n");

	g_free(emf->full_name);
	g_free(emf->name);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution.Mail.Folder */

static CORBA_boolean
impl_getProperties(PortableServer_Servant _servant,
		   const GNOME_Evolution_Mail_PropertyNames* names,
		   GNOME_Evolution_Mail_Properties **propsp,
		   CORBA_Environment * ev)
{
	EvolutionMailFolder *emf = (EvolutionMailFolder *)bonobo_object_from_servant(_servant);
	int i;
	GNOME_Evolution_Mail_Properties *props;
	CORBA_boolean ok = CORBA_TRUE;

	*propsp = props = GNOME_Evolution_Mail_Properties__alloc();
	props->_length = names->_length;
	props->_maximum = props->_length;
	props->_buffer = GNOME_Evolution_Mail_Properties_allocbuf(props->_maximum);
	CORBA_sequence_set_release(props, CORBA_TRUE);

	for (i=0;i<names->_length;i++) {
		const CORBA_char *name = names->_buffer[i];
		GNOME_Evolution_Mail_Property *prop = &props->_buffer[i];

		prop->value._release = CORBA_TRUE;

		if (!strcmp(name, "name")) {
			prop->value._type = TC_CORBA_string;
			prop->value._value = CORBA_string_dup(emf->name);
		} else if (!strcmp(name, "full_name")) {
			prop->value._type = TC_CORBA_string;
			prop->value._value = CORBA_string_dup(emf->full_name);
		} else {
			prop->value._type = TC_null;
			ok = CORBA_FALSE;
		}

		prop->name = CORBA_string_dup(name);
	}

	return ok;
}

/* Initialization */

static void
evolution_mail_folder_class_init (EvolutionMailFolderClass *klass)
{
	POA_GNOME_Evolution_Mail_Folder__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->getProperties = impl_getProperties;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	g_type_class_add_private(klass, sizeof(struct _EvolutionMailFolderPrivate));
}

static void
evolution_mail_folder_init(EvolutionMailFolder *component, EvolutionMailFolderClass *klass)
{
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailFolder, GNOME_Evolution_Mail_Folder, PARENT_TYPE, evolution_mail_folder)

EvolutionMailFolder *
evolution_mail_folder_new(const char *name, const char *full_name)
{
	EvolutionMailFolder *emf = g_object_new (EVOLUTION_MAIL_TYPE_FOLDER, NULL);

	emf->name = g_strdup(name);
	emf->full_name = g_strdup(full_name);

	return emf;
}
