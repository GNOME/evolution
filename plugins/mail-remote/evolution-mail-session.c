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

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_session_get_type()))

struct _listener {
	struct _listener *next;
	struct _listener *prev;

	CORBA_long flags;
	GNOME_Evolution_Mail_Listener listener;
};

struct _EvolutionMailSessionPrivate {
	EAccountList *accounts;
	GList *stores;

	/* FIXME: locking */
	EDList listeners;

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

	/* FIXME: free listners */

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
		 CORBA_long flags,
		 CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	struct _listener *l;

	printf("Adding listener to session %p\n", listener);
	l = g_malloc0(sizeof(*l));
	l->listener = CORBA_Object_duplicate(listener, ev);
	l->flags = flags?flags:~0;

	e_dlist_addtail(&p->listeners, (EDListNode *)l);
}

static void
remove_listener(struct _listener *l)
{
	CORBA_Environment ev = { 0 };

	CORBA_Object_release(l->listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		CORBA_exception_free(&ev);
	e_dlist_remove((EDListNode *)l);
	g_free(l);
}

static void
impl_removeListener(PortableServer_Servant _servant,
		    const GNOME_Evolution_Mail_Listener listener,
		    CORBA_Environment * ev)
{
	EvolutionMailSession *ems = (EvolutionMailSession *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	struct _listener *l;

	printf("Removing listener from session\n");

	l = (struct _listener *)p->listeners.head;
	while (l->next) {
		/* FIXME: need to use proper comparison function & free stuff, this works with orbit though */
		if (l->listener == listener) {
			remove_listener(l);
			break;
		}
		
		l = l->next;
	}
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
ems_set_changes(GNOME_Evolution_Mail_SessionChange *change, GNOME_Evolution_Mail_ChangeType how, EvolutionMailStore *store)
{
	change->type = how;
	change->stores._length = 1;
	change->stores._maximum = 1;
	change->stores._buffer = GNOME_Evolution_Mail_StoreInfos_allocbuf(change->stores._maximum);
	CORBA_sequence_set_release(&change->stores, TRUE);

	e_mail_storeinfo_set_store(&change->stores._buffer[0], store);
}

static void
ems_listener_session_event(EvolutionMailSession *ems, GNOME_Evolution_Mail_ChangeType how, EvolutionMailStore *store)
{
	GNOME_Evolution_Mail_SessionChanges *changes;
	CORBA_long flags = 0;

	switch (how) {
	case GNOME_Evolution_Mail_ADDED:
		flags = GNOME_Evolution_Mail_Session_SESSION_ADDED;
		break;
	case GNOME_Evolution_Mail_CHANGED:
		flags = GNOME_Evolution_Mail_Session_SESSION_CHANGED;
		break;
	case GNOME_Evolution_Mail_REMOVED:
		flags = GNOME_Evolution_Mail_Session_SESSION_REMOVED;
		break;
	}

	if ((evolution_mail_session_listening(ems) & flags) == 0)
		return;

	/* NB: we only ever create 1 changetype at the moment */

	changes = GNOME_Evolution_Mail_SessionChanges__alloc();
	changes->_maximum = 1;
	changes->_length = 1;
	changes->_buffer = GNOME_Evolution_Mail_SessionChanges_allocbuf(1);
	CORBA_sequence_set_release(changes, TRUE);
	ems_set_changes(&changes->_buffer[0], how, store);

	evolution_mail_session_session_changed(ems, changes);

	CORBA_free(changes);
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
		ems_listener_session_event(ems, GNOME_Evolution_Mail_ADDED, store);
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
			ems_listener_session_event(ems, GNOME_Evolution_Mail_REMOVED, store);
			g_object_unref(store);
		} else {
			printf("Account changed, dont know how %s\n", ea->uid);
			ems_listener_session_event(ems, GNOME_Evolution_Mail_CHANGED, store);
		}
	} else if (ea->enabled && is_storage(ea)) {
		printf("Account changed, now added %s\n", ea->uid);
		store = evolution_mail_store_new(ems, ea);
		p->stores = g_list_append(p->stores, store);
		ems_listener_session_event(ems, GNOME_Evolution_Mail_ADDED, store);
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
			ems_listener_session_event(ems, GNOME_Evolution_Mail_REMOVED, store);
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

	printf("EvolutionMailSession.init\n");

	e_dlist_init(&p->listeners);

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

void
evolution_mail_session_session_changed(EvolutionMailSession *ems, GNOME_Evolution_Mail_SessionChanges *changes)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	struct _listener *l, *n;
	CORBA_Environment ev;

	l=(struct _listener *)p->listeners.head;
	n=l->next;
	while (n) {
		if (l->flags & (GNOME_Evolution_Mail_Session_SESSION_CHANGED|GNOME_Evolution_Mail_Session_SESSION_ADDED|GNOME_Evolution_Mail_Session_SESSION_REMOVED)) {
			memset(&ev, 0, sizeof(ev));
			GNOME_Evolution_Mail_Listener_sessionChanged(l->listener, bonobo_object_corba_objref((BonoboObject *)ems), changes, &ev);
			if (ev._major != CORBA_NO_EXCEPTION) {
				printf("listener.sessionChanged() failed, removing listener: %s\n", ev._id);
				CORBA_exception_free(&ev);

				remove_listener(l);
			} else {
				printf("listener.sessionChanged() successful\n");
			}
		}
		l = n;
		n = n->next;
	}
}

void
evolution_mail_session_store_changed(EvolutionMailSession *ems, GNOME_Evolution_Mail_Store store, GNOME_Evolution_Mail_StoreChanges *changes)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	struct _listener *l, *n;
	CORBA_Environment ev;

	l=(struct _listener *)p->listeners.head;
	n=l->next;
	while (n) {
		if (l->flags & (GNOME_Evolution_Mail_Session_STORE_CHANGED|GNOME_Evolution_Mail_Session_STORE_ADDED|GNOME_Evolution_Mail_Session_STORE_REMOVED)) {
			memset(&ev, 0, sizeof(ev));
			GNOME_Evolution_Mail_Listener_storeChanged(l->listener, bonobo_object_corba_objref((BonoboObject *)ems),
								   store, changes, &ev);

			if (ev._major != CORBA_NO_EXCEPTION) {
				printf("listener.storeChanged() failed, removing listener: %s\n", ev._id);
				CORBA_exception_free(&ev);

				remove_listener(l);
			} else {
				printf("listener.storeChanged() successful\n");
			}
		}
		l = n;
		n = n->next;
	}
}

void
evolution_mail_session_folder_changed(EvolutionMailSession *ems, GNOME_Evolution_Mail_Store store, GNOME_Evolution_Mail_Folder folder, GNOME_Evolution_Mail_FolderChanges *changes)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	struct _listener *l, *n;
	CORBA_Environment ev;

	l=(struct _listener *)p->listeners.head;
	n=l->next;
	while (n) {
		if (l->flags & (GNOME_Evolution_Mail_Session_FOLDER_CHANGED|GNOME_Evolution_Mail_Session_FOLDER_ADDED|GNOME_Evolution_Mail_Session_FOLDER_REMOVED)) {
			memset(&ev, 0, sizeof(ev));
			GNOME_Evolution_Mail_Listener_folderChanged(l->listener, bonobo_object_corba_objref((BonoboObject *)ems),
								    store, folder, changes, &ev);

			if (ev._major != CORBA_NO_EXCEPTION) {
				printf("listener.folderChanged() failed, removing listener: %s\n", ev._id);
				CORBA_exception_free(&ev);
				remove_listener(l);
			} else {
				printf("listener.folderChanged() successful\n");
			}
		}
		l = n;
		n = n->next;
	}
}

/**
 * evolution_mail_session_listening:
 * @ems: 
 * 
 * Check if anything is listening for events.  Used to optimise the
 * code so it doesn't generate events if it doesn't need to.
 * 
 * Return value: 
 **/
CORBA_long evolution_mail_session_listening(EvolutionMailSession *ems)
{
	struct _EvolutionMailSessionPrivate *p = _PRIVATE(ems);
	struct _listener *l;
	CORBA_long flags = 0;

	for (l=(struct _listener *)p->listeners.head;l->next;l=l->next)
		flags |= l->flags;

	return flags;
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailSession, GNOME_Evolution_Mail_Session, PARENT_TYPE, evolution_mail_session)
