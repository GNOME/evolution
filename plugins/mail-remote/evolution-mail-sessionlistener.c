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
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include "evolution-mail-sessionlistener.h"

#include "evolution-mail-marshal.h"
#include "e-corba-utils.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_sessionlistener_get_type()))

struct _EvolutionMailSessionListenerPrivate {
	int dummy;
};

enum {
	EML_CHANGED,
	EML_SHUTDOWN,
	EML_LAST_SIGNAL
};

static guint eml_signals[EML_LAST_SIGNAL];

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	struct _EvolutionMailSessionListenerPrivate *p = _PRIVATE(object);

	p = p;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	d(printf("EvolutionMailSessionListener finalised!\n"));

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution.Mail.Listener */
static const char *change_type_name(int type)
{
	switch (type) {
	case Evolution_Mail_ADDED:
		return "added";
		break;
	case Evolution_Mail_CHANGED:
		return "changed";
		break;
	case Evolution_Mail_REMOVED:
		return "removed";
		break;
	default:
		return "";
	}
}

static void
impl_changed(PortableServer_Servant _servant,
	     const Evolution_Mail_Session session,
	     const Evolution_Mail_SessionChanges * changes,
	     CORBA_Environment * ev)
{
	EvolutionMailSessionListener *eml = (EvolutionMailSessionListener *)bonobo_object_from_servant(_servant);
	int i, j;

	d(printf("session changed!\n"));
	for (i=0;i<changes->_length;i++) {
		d(printf(" %d %s", changes->_buffer[i].stores._length, change_type_name(changes->_buffer[i].type)));
		for (j=0;j<changes->_buffer[i].stores._length;j++) {
			d(printf(" %s %s\n", changes->_buffer[i].stores._buffer[j].uid, changes->_buffer[i].stores._buffer[j].name));
		}
	}

	g_signal_emit(eml, eml_signals[EML_CHANGED], 0, session, changes);
}

static void
impl_shutdown(PortableServer_Servant _servant,
	      const Evolution_Mail_Session session,
	      CORBA_Environment * ev)
{
	EvolutionMailSessionListener *eml = (EvolutionMailSessionListener *)bonobo_object_from_servant(_servant);

	d(printf("session shutdown?\n"));

	g_signal_emit(eml, eml_signals[EML_SHUTDOWN], 0, session);
}

/* Initialization */

static void
evolution_mail_sessionlistener_class_init (EvolutionMailSessionListenerClass *klass)
{
	POA_Evolution_Mail_SessionListener__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->changed = impl_changed;
	epv->shutdown = impl_shutdown;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	g_type_class_add_private(klass, sizeof(struct _EvolutionMailSessionListenerPrivate));

	eml_signals[EML_CHANGED] =
		g_signal_new("changed",
			     G_OBJECT_CLASS_TYPE (klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (EvolutionMailSessionListenerClass, changed),
			     NULL, NULL,
			     evolution_mail_marshal_VOID__POINTER_POINTER,
			     G_TYPE_NONE, 2,
			     G_TYPE_POINTER, G_TYPE_POINTER);

	eml_signals[EML_CHANGED] =
		g_signal_new("shutdown",
			     G_OBJECT_CLASS_TYPE (klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (EvolutionMailSessionListenerClass, changed),
			     NULL, NULL,
			     evolution_mail_marshal_VOID__POINTER,
			     G_TYPE_NONE, 1,
			     G_TYPE_POINTER);
}

static void
evolution_mail_sessionlistener_init (EvolutionMailSessionListener *ems, EvolutionMailSessionListenerClass *klass)
{
	struct _EvolutionMailSessionListenerPrivate *p = _PRIVATE(ems);

	p = p;
}

EvolutionMailSessionListener *
evolution_mail_sessionlistener_new(void)
{
	EvolutionMailSessionListener *eml;

	eml = g_object_new(evolution_mail_sessionlistener_get_type(), NULL);

	return eml;
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailSessionListener, Evolution_Mail_SessionListener, PARENT_TYPE, evolution_mail_sessionlistener)
