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
#include "evolution-mail-session.h"

#include <e-util/e-account-list.h>

#include "evolution-mail-store.h"
#include "e-corba-utils.h"

#include <camel/camel-session.h>

#define FACTORY_ID "OAFIID:GNOME_Evolution_Mail_Session_Factory:" BASE_VERSION
#define MAIL_SESSION_ID  "OAFIID:GNOME_Evolution_Mail_Session:" BASE_VERSION

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_session_get_type()))

struct _EvolutionMailSessionPrivate {
	EAccountList *accounts;
	GList *stores;

	/* FIXME: locking */
	GSList *listeners;

	guint account_added;
	guint account_changed;
	guint account_removed;
};

static int
is_storage(EAccount *ea)
{
	const char *uri;
	CamelProvider *prov;

	return(uri = e_account_get_string(ea, E_ACCOUNT_SOURCE_URL))
		&& (prov = camel_provider_get(uri, NULL))
		&& (prov->flags & CAMEL_PROVIDER_IS_STORAGE);
}

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
	g_warning("EvolutionMailStore is finalised!\n");

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

static GNOME_Evolution_Mail_StoreInfos *
impl_getStores(PortableServer_Servant _servant,
	       const CORBA_char * pattern,
	       CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	GNOME_Evolution_Mail_StoreInfos *seq;
	int i, len;
	GList *l;

	seq = GNOME_Evolution_Mail_StoreInfos__alloc();

	/* FIXME: pattern? */

	len = g_list_length(p->stores);

	seq->_length = len;
	seq->_maximum = len;
	seq->_buffer = GNOME_Evolution_Mail_StoreInfos_allocbuf(seq->_length);

	CORBA_sequence_set_release(seq, TRUE);

	l = p->stores;
	for (i=0;l && i<len;i++) {
		EvolutionMailStore *store = l->data;

		e_mail_storeinfo_set_store(&seq->_buffer[i], store);
		l = g_list_next(l);
	}

	return seq;
}

static void
impl_addListener(PortableServer_Servant _servant,
		 const GNOME_Evolution_Mail_Listener listener,
		 CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);

	printf("Adding listener to session\n");

	p->listeners = g_slist_append(p->listeners, CORBA_Object_duplicate(listener, ev));
}

static void
impl_removeListener(PortableServer_Servant _servant,
		    const GNOME_Evolution_Mail_Listener listener,
		    CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);

	printf("Removing listener from session\n");

	/* FIXME: need to use proper comparison function & free stuff, this works with orbit though */
	p->listeners = g_slist_remove(p->listeners, listener);
	CORBA_Object_release(listener, ev);
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

	epv->addListener = impl_addListener;
	epv->removeListener = impl_removeListener;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	g_type_class_add_private(klass, sizeof(struct _EvolutionMailSessionPrivate));
}

static void
ems_listener_event(EvolutionMailSession *ems, GNOME_Evolution_Mail_ChangeType how, EvolutionMailStore *store)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	GNOME_Evolution_Mail_SessionChange *change;
	GSList *l;

	for (l=p->listeners;l;l=g_slist_next(l)) {
		CORBA_Environment ev;

		change = GNOME_Evolution_Mail_SessionChange__alloc();
		change->type = how;

		change->stores._length = 1;
		change->stores._maximum = 1;
		change->stores._buffer = GNOME_Evolution_Mail_StoreInfos_allocbuf(change->stores._maximum);
		CORBA_sequence_set_release(&change->stores, TRUE);
		e_mail_storeinfo_set_store(&change->stores._buffer[0], store);

		GNOME_Evolution_Mail_Listener_sessionChanged(l->data, bonobo_object_corba_objref((BonoboObject *)ems), change, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			printf("listener.sessionChanged() failed: %s\n", ev._id);
			/* TODO: if it fails, remove the listener? */
			CORBA_exception_free(&ev);
		} else {
			printf("listener.sessionChanged() successful\n");
		}
	}
}

static void
ems_account_added(EAccountList *eal, EAccount *ea, EvolutionMailSession *ems)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);

	if (ea->enabled && is_storage(ea)) {
		EvolutionMailStore *store;

		printf("Account added %s\n", ea->uid);
		store = evolution_mail_store_new(ems, ea);
		p->stores = g_list_append(p->stores, store);
		ems_listener_event(ems, GNOME_Evolution_Mail_ADDED, store);
	}
}

static void
ems_account_changed(EAccountList *eal, EAccount *ea, EvolutionMailSession *ems)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	EvolutionMailStore *store = NULL;
	GList *l;

	for (l = p->stores;l;l=l->next) {
		if (((EvolutionMailStore *)l->data)->account == ea) {
			store = l->data;
			break;
		}
	}

	if (store) {
		/* account has been disabled? */
		if (!ea->enabled) {
			printf("Account changed, now disabled %s\n", ea->uid);
			p->stores = g_list_remove(p->stores, store);
			ems_listener_event(ems, GNOME_Evolution_Mail_REMOVED, store);
			g_object_unref(store);
		} else {
			printf("Account changed, dont know how %s\n", ea->uid);
			ems_listener_event(ems, GNOME_Evolution_Mail_CHANGED, store);
		}
	} else if (ea->enabled && is_storage(ea)) {
		printf("Account changed, now added %s\n", ea->uid);
		store = evolution_mail_store_new(ems, ea);
		p->stores = g_list_append(p->stores, store);
		ems_listener_event(ems, GNOME_Evolution_Mail_ADDED, store);
	}
}

static void
ems_account_removed(EAccountList *eal, EAccount *ea, EvolutionMailSession *ems)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	GList *l;

	/* for accounts we dont have, we dont care */

	for (l = p->stores;l;l=l->next) {
		EvolutionMailStore *store = l->data;

		if (store->account == ea) {
			printf("Account removed %s\n", ea->uid);
			p->stores = g_list_remove(p->stores, store);
			ems_listener_event(ems, GNOME_Evolution_Mail_REMOVED, store);
			g_object_unref(store);
			break;
		}
	}
}

static void
evolution_mail_session_init (EvolutionMailSession *ems, EvolutionMailSessionClass *klass)
{
	GConfClient *gconf = gconf_client_get_default();
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	EIterator *iter;

	/* FIXME: listen to changes */

	/* local store first */
	p->stores = g_list_append(p->stores, evolution_mail_store_new(ems, NULL));

	p->accounts = e_account_list_new(gconf);
	iter = e_list_get_iterator((EList *)p->accounts);
	while (e_iterator_is_valid (iter)) {
		EAccount *ea;

		if ((ea = (EAccount *)e_iterator_get(iter))
		    && ea->enabled
		    && is_storage(ea)) {
			p->stores = g_list_append(p->stores, evolution_mail_store_new(ems, ea));
		}

		e_iterator_next(iter);
	}
	g_object_unref(iter);

	g_object_unref(gconf);

	p->account_added = g_signal_connect(p->accounts, "account_added", G_CALLBACK(ems_account_added), ems);
	p->account_changed = g_signal_connect(p->accounts, "account_changed", G_CALLBACK(ems_account_changed), ems);
	p->account_removed = g_signal_connect(p->accounts, "account_removed", G_CALLBACK(ems_account_removed), ems);

	ems->session = mail_component_peek_session(NULL);
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailSession, GNOME_Evolution_Mail_Session, PARENT_TYPE, evolution_mail_session)
