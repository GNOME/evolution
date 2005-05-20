/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2005  Novell, Inc.
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
 *
 * Author: Michael Zucchi <notzed@novell.com>
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
#include "evolution-mail-listener.h"

#include <libedataserver/e-account-list.h>

#include "evolution-mail-store.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_listener_get_type()))

struct _EvolutionMailListenerPrivate {
	int dummy;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	struct _EvolutionMailListenerPrivate *p = _PRIVATE(object);

	p = p;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution.Mail.Listener */

static void
impl_sessionChanged(PortableServer_Servant _servant,
		    const GNOME_Evolution_Mail_Session session,
		    const GNOME_Evolution_Mail_SessionChange *change, CORBA_Environment * ev)
{
	EvolutionMailListener *ems = (EvolutionMailListener *)bonobo_object_from_servant(_servant);
	const char *what;
	int i;

	printf("session changed!\n");
	ems = ems;

	switch (change->type) {
	case GNOME_Evolution_Mail_ADDED:
		what = "added";
		break;
	case GNOME_Evolution_Mail_CHANGED:
		what = "changed";
		break;
	case GNOME_Evolution_Mail_REMOVED:
		what = "removed";
		break;
	}

	printf("%d %s\n", change->stores._length, what);
	for (i=0;i<change->stores._length;i++) {
		printf("Store '%s' '%s'\n", change->stores._buffer[i].name, change->stores._buffer[i].uid);
	}
}

static void
impl_storeChanged(PortableServer_Servant _servant,
		  const GNOME_Evolution_Mail_Session session,
		  const GNOME_Evolution_Mail_Store store,
		  const GNOME_Evolution_Mail_StoreChanges * changes,
		  CORBA_Environment * ev)
{
	EvolutionMailListener *ems = (EvolutionMailListener *)bonobo_object_from_servant(_servant);

	printf("store changed!\n");
	ems = ems;
}

static void
impl_folderChanged(PortableServer_Servant _servant,
		   const GNOME_Evolution_Mail_Session session,
		   const GNOME_Evolution_Mail_Store store,
		   const GNOME_Evolution_Mail_Folder folder,
		   const GNOME_Evolution_Mail_FolderChanges *changes, CORBA_Environment * ev)
{
	EvolutionMailListener *ems = (EvolutionMailListener *)bonobo_object_from_servant(_servant);

	printf("folder changed!\n");
	ems = ems;
}

/* Initialization */

static void
evolution_mail_listener_class_init (EvolutionMailListenerClass *klass)
{
	POA_GNOME_Evolution_Mail_Listener__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->sessionChanged = impl_sessionChanged;
	epv->storeChanged = impl_storeChanged;
	epv->folderChanged = impl_folderChanged;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	g_type_class_add_private(klass, sizeof(struct _EvolutionMailListenerPrivate));
}

static void
evolution_mail_listener_init (EvolutionMailListener *ems, EvolutionMailListenerClass *klass)
{
	struct _EvolutionMailListenerPrivate *p = _PRIVATE(ems);

	p = p;
}

EvolutionMailListener *
evolution_mail_listener_new(void)
{
	EvolutionMailListener *eml;
#if 0
	static PortableServer_POA poa = NULL;

	/* NB: to simplify signal handling, we should only run in the idle loop? */

	if (poa == NULL)
		poa = bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL);
	eml = g_object_new(evolution_mail_listener_get_type(), "poa", poa, NULL);
#else
	eml = g_object_new(evolution_mail_listener_get_type(), NULL);
#endif
	return eml;
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailListener, GNOME_Evolution_Mail_Listener, PARENT_TYPE, evolution_mail_listener)
