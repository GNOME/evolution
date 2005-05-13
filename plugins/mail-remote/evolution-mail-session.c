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
 * Author: JP Rosevear <jpr@ximian.com>
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
#include "evolution-mail-session.h"

#include <libedataserver/e-account-list.h>

#include "evolution-mail-store.h"

#include <camel/camel-session.h>

#define FACTORY_ID "OAFIID:GNOME_Evolution_Mail_Session_Factory:" BASE_VERSION
#define MAIL_SESSION_ID  "OAFIID:GNOME_Evolution_Mail_Session:" BASE_VERSION

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_session_get_type()))

struct _EvolutionMailSessionPrivate {
	EAccountList *accounts;
	GList *stores;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(object);

	if (p->stores) {
		/* FIXME: free stores */
	}

	/* FIXME: Free accounts */

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution.Mail.Session */

static CORBA_boolean
impl_getProperties(PortableServer_Servant _servant,
		   const GNOME_Evolution_Mail_PropertyNames* names,
		   GNOME_Evolution_Mail_Properties **props,
		   CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);

	ems = ems;

	return CORBA_TRUE;
}

static GNOME_Evolution_Mail_Stores *
impl_getStores(PortableServer_Servant _servant,
	       const CORBA_char * pattern,
	       CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	GNOME_Evolution_Mail_Stores *seq;
	int i, len;
	GList *l;

	seq = GNOME_Evolution_Mail_Stores__alloc();

	/* FIXME: pattern? */

	len = g_list_length(p->stores);

	seq->_length = len;
	seq->_maximum = len;
	seq->_buffer = GNOME_Evolution_Mail_Stores_allocbuf(seq->_length);

	CORBA_sequence_set_release(seq, TRUE);

	l = p->stores;
	for (i=0;l && i<len;i++) {
		seq->_buffer[i] = bonobo_object_corba_objref(l->data);
		bonobo_object_ref(l->data); /* ?? */
		l = g_list_next(l);
	}

	return seq;
}

/* Initialization */

static void
evolution_mail_session_class_init (EvolutionMailSessionClass *klass)
{
	POA_GNOME_Evolution_Mail_Session__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->getProperties = impl_getProperties;
	epv->getStores = impl_getStores;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	g_type_class_add_private(klass, sizeof(struct _EvolutionMailSessionPrivate));
}

static void
evolution_mail_session_init (EvolutionMailSession *ems, EvolutionMailSessionClass *klass)
{
	GConfClient *gconf = gconf_client_get_default();
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	EIterator *iter;
	EvolutionMailStore *store;
	extern CamelSession *session;

	/* FIXME: listen to changes */

	/* local store first */
	p->stores = g_list_append(p->stores, evolution_mail_store_new(ems, NULL));

	p->accounts = e_account_list_new(gconf);
	iter = e_list_get_iterator((EList *)p->accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *ea;

		if ((ea = (EAccount *)e_iterator_get(iter))
		    && (store = evolution_mail_store_new(ems, (struct _EAccount *)ea))) {
			p->stores = g_list_append(p->stores, store);
		}

		e_iterator_next(iter);
	}
	g_object_unref(iter);

	g_object_unref(gconf);

	ems->session = session;
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailSession, GNOME_Evolution_Mail_Session, PARENT_TYPE, evolution_mail_session)
