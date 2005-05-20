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
#include <bonobo/bonobo-main.h>

#include "evolution-mail-session.h"
#include "evolution-mail-store.h"
#include "evolution-mail-folder.h"

#include "e-corba-utils.h"

#include <camel/camel-store.h>
#include <camel/camel-session.h>

#include <e-util/e-account.h>

#include "mail/mail-component.h"

#define FACTORY_ID "OAFIID:GNOME_Evolution_Mail_Store_Factory:" BASE_VERSION
#define MAIL_STORE_ID  "OAFIID:GNOME_Evolution_Mail_Store:" BASE_VERSION

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_store_get_type()))

struct _EvolutionMailStorePrivate {
	CamelStore *store;
	EvolutionMailSession *session;

	GHashTable *folders;
	/* sorted array of folders by full_name */
	GPtrArray *folders_array;

	guint32 folder_opened;
	guint32 folder_created;
	guint32 folder_deleted;
	guint32 folder_renamed;
	guint32 folder_subscribed;
	guint32 folder_unsubscribed;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)object;
	struct _EvolutionMailStorePrivate *p = _PRIVATE(object);

	p = p;

	/* FIXME: unref store
	   unhook events */

	if (ems->account) {
		g_object_unref(ems->account);
		ems->account = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(object);

	g_warning("EvolutionMailStore is finalised!\n");

	if (p->folders) {
		/* FIXME: bonobo unref? */
		g_hash_table_foreach(p->folders, (GHFunc)g_object_unref, NULL);
		g_hash_table_destroy(p->folders);
		g_ptr_array_free(p->folders_array, TRUE);
	}

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Evolution.Mail.Store */

static CORBA_boolean
impl_getProperties(PortableServer_Servant _servant,
		   const GNOME_Evolution_Mail_PropertyNames* names,
		   GNOME_Evolution_Mail_Properties **propsp,
		   CORBA_Environment * ev)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)bonobo_object_from_servant(_servant);
	int i;
	GNOME_Evolution_Mail_Properties *props;
	/*struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);*/
	CORBA_boolean ok = CORBA_TRUE;

	*propsp = props = GNOME_Evolution_Mail_Properties__alloc();
	props->_length = names->_length;
	props->_maximum = props->_length;
	props->_buffer = GNOME_Evolution_Mail_Properties_allocbuf(props->_maximum);
	CORBA_sequence_set_release(props, CORBA_TRUE);

	for (i=0;i<names->_length;i++) {
		const CORBA_char *name = names->_buffer[i];
		GNOME_Evolution_Mail_Property *prop = &props->_buffer[i];

		printf("getting property '%s'\n", name);

		if (!strcmp(name, "name")) {
			e_mail_property_set_string(prop, name, evolution_mail_store_get_name(ems));
		} else if (!strcmp(name, "uid")) {
			e_mail_property_set_string(prop, name, evolution_mail_store_get_uid(ems));
		} else {
			e_mail_property_set_null(prop, name);
			ok = CORBA_FALSE;
		}
	}

	return ok;
}

static void
ems_add_folders(struct _EvolutionMailStorePrivate *p, CamelFolderInfo *fi)
{
	while (fi) {
		if (g_hash_table_lookup(p->folders, fi->full_name) == NULL) {

			/* FIXME: store of folder??? */
			EvolutionMailFolder *emf = evolution_mail_folder_new(fi->name, fi->full_name);

			g_hash_table_insert(p->folders, emf->full_name, emf);
			g_ptr_array_add(p->folders_array, emf);
		}

		if (fi->child)
			ems_add_folders(p, fi->child);

		fi = fi->next;
	}
}

static void
ems_remove_folders(struct _EvolutionMailStorePrivate *p, CamelFolderInfo *fi)
{
	EvolutionMailFolder *emf;

	while (fi) {
		emf = g_hash_table_lookup(p->folders, fi->full_name);
		if (emf) {
			g_hash_table_remove(p->folders, fi->full_name);
			g_ptr_array_remove(p->folders_array, emf);
			/* FIXME: pass emf to the store changed folder removed code */
		} else {
			g_warning("Folder removed I didn't know existed '%s'\n", fi->full_name);
		}

		if (fi->child)
			ems_remove_folders(p, fi->child);

		fi = fi->next;
	}
}

static int
ems_sort_folders_cmp(const void *ap, const void *bp)
{
	const EvolutionMailFolder *a = ((const EvolutionMailFolder **)ap)[0];
	const EvolutionMailFolder *b = ((const EvolutionMailFolder **)bp)[0];

	return strcmp(a->full_name, b->full_name);
}

static void
ems_sort_folders(struct _EvolutionMailStorePrivate *p)
{
	qsort(p->folders_array->pdata, p->folders_array->len, sizeof(p->folders_array->pdata[0]), ems_sort_folders_cmp);
}

static void
ems_folder_opened(CamelObject *o, void *d, void *data)
{
	EvolutionMailStore *ems = data;
	CamelFolder *folder = d;

	ems = ems;
	folder = folder;
	/* noop */
}

static void
ems_folder_subscribed(CamelObject *o, void *d, void *data)
{
	EvolutionMailStore *ems = data;
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	CamelFolderInfo *fi = d;

	ems_add_folders(p, fi);

	/* FIXME: store folder added event */
}

static void
ems_folder_unsubscribed(CamelObject *o, void *d, void *data)
{
	EvolutionMailStore *ems = data;
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	CamelFolderInfo *fi = d;

	ems_remove_folders(p, fi);

	/* FIXME: store folder deleted event */
}

static void
ems_folder_created(CamelObject *o, void *d, void *data)
{
	CamelStore *store = (CamelStore *)o;

	if (!camel_store_supports_subscriptions(store))
		ems_folder_subscribed(o, d, data);
}

static void
ems_folder_deleted(CamelObject *o, void *d, void *data)
{
	CamelStore *store = (CamelStore *)o;

	if (!camel_store_supports_subscriptions(store))
		ems_folder_subscribed(o, d, data);
}

static void
ems_folder_renamed(CamelObject *o, void *d, void *data)
{
	EvolutionMailStore *ems = data;
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	CamelRenameInfo *reninfo = d;
	int i, oldlen;

	oldlen = strlen(reninfo->old_base);

	for (i=0;i<p->folders_array->len;i++) {
		EvolutionMailFolder *folder = p->folders_array->pdata[i];

		if (!strcmp(folder->full_name, reninfo->old_base)
		    || (strlen(folder->full_name) > oldlen
			&& folder->full_name[oldlen] == '/'
			&& strncmp(folder->full_name, reninfo->old_base, oldlen))) {
			/* renamed folder */
		}
	}

	/* FIXME: store folder changed event? */
}

static GNOME_Evolution_Mail_FolderInfos *
impl_getFolders(PortableServer_Servant _servant,
		const CORBA_char * pattern,
		CORBA_Environment * ev)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	CamelFolderInfo *fi;
	CamelException ex = { 0 };
	GNOME_Evolution_Mail_FolderInfos *folders = NULL;
	int i;

	if (p->store == NULL) {
		if (ems->account == NULL) {
			p->store = mail_component_peek_local_store(NULL);
			camel_object_ref(p->store);
		} else {
			const char *uri;

			uri = e_account_get_string(ems->account, E_ACCOUNT_SOURCE_URL);
			if (uri && *uri) {
				p->store = camel_session_get_store(p->session->session, uri, &ex);
				if (camel_exception_is_set(&ex)) {
					GNOME_Evolution_Mail_FAILED *x;

					camel_exception_clear(&ex);
					x = GNOME_Evolution_Mail_FAILED__alloc();
					x->why = CORBA_string_dup("Unable to get store");
					CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Mail_FAILED, x);
					return CORBA_OBJECT_NIL;
				}
			} else {
				CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Mail_FAILED, NULL);
				return CORBA_OBJECT_NIL;
			}
		}

		p->folder_opened = camel_object_hook_event(p->store, "folder_opened", ems_folder_opened, ems);
		p->folder_created = camel_object_hook_event(p->store, "folder_created", ems_folder_created, ems);
		p->folder_deleted = camel_object_hook_event(p->store, "folder_deleted", ems_folder_deleted, ems);
		p->folder_renamed = camel_object_hook_event(p->store, "folder_renamed", ems_folder_renamed, ems);
		p->folder_subscribed = camel_object_hook_event(p->store, "folder_subscribed", ems_folder_subscribed, ems);
		p->folder_unsubscribed = camel_object_hook_event(p->store, "folder_unsubscribed", ems_folder_unsubscribed, ems);
	}

	if (p->folders == NULL) {
		fi = camel_store_get_folder_info(p->store, "", CAMEL_STORE_FOLDER_INFO_RECURSIVE|CAMEL_STORE_FOLDER_INFO_FAST, &ex);

		if (fi) {
			p->folders = g_hash_table_new(g_str_hash, g_str_equal);
			p->folders_array = g_ptr_array_new();
			ems_add_folders(p, fi);
			camel_store_free_folder_info(p->store, fi);
			ems_sort_folders(p);
		} else {
			GNOME_Evolution_Mail_FAILED *x;

			x = GNOME_Evolution_Mail_FAILED__alloc();
			x->why = CORBA_string_dup("Unable to list folders");
			CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Mail_FAILED, x);

			return CORBA_OBJECT_NIL;
		}
	}

	folders = GNOME_Evolution_Mail_FolderInfos__alloc();
	folders->_length = p->folders_array->len;
	folders->_maximum = folders->_length;
	folders->_buffer = GNOME_Evolution_Mail_FolderInfos_allocbuf(folders->_maximum);
	CORBA_sequence_set_release(folders, CORBA_TRUE);

	for (i=0;i<p->folders_array->len;i++) {
		EvolutionMailFolder *emf = p->folders_array->pdata[i];
			
		folders->_buffer[i].name = CORBA_string_dup(emf->name);
		folders->_buffer[i].full_name = CORBA_string_dup(emf->full_name);
		folders->_buffer[i].folder = CORBA_Object_duplicate(bonobo_object_corba_objref((BonoboObject *)emf), NULL);
		/* object ref?? */
	}

	return folders;
}

static void
impl_sendMessage(PortableServer_Servant _servant,
		 const Bonobo_Stream msg, const CORBA_char * from,
		 const CORBA_char * recipients,
		 CORBA_Environment * ev)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	p = p;

	printf("Sending message from '%s' to '%s'\n", from, recipients);
	if (ems->account == NULL) {
		printf("Local mail can only store ...\n");
	} else if (ems->account->transport && ems->account->transport->url) {
		printf("via '%s'\n", ems->account->transport->url);
	} else {
		printf("Account not setup for sending '%s'\n", ems->account->name);
	}
}

/* Initialization */

static void
evolution_mail_store_class_init (EvolutionMailStoreClass *klass)
{
	POA_GNOME_Evolution_Mail_Store__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->getProperties = impl_getProperties;
	epv->getFolders = impl_getFolders;
	epv->sendMessage = impl_sendMessage;

	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	g_type_class_add_private(klass, sizeof(struct _EvolutionMailStorePrivate));
}

static void
evolution_mail_store_init(EvolutionMailStore *component, EvolutionMailStoreClass *klass)
{
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailStore, GNOME_Evolution_Mail_Store, PARENT_TYPE, evolution_mail_store)

EvolutionMailStore *
evolution_mail_store_new(struct _EvolutionMailSession *s, struct _EAccount *ea)
{
	EvolutionMailStore *ems;
	struct _EvolutionMailStorePrivate *p;
	static PortableServer_POA poa = NULL;

	if (poa == NULL)
		poa = bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL);

	ems = g_object_new (EVOLUTION_MAIL_TYPE_STORE, "poa", poa, NULL);
	p = _PRIVATE(ems);

	if (ea) {
		ems->account = ea;
		g_object_ref(ea);
	}

	p->session = s;

	return ems;
}

const char *evolution_mail_store_get_name(EvolutionMailStore *ems)
{
	if (ems->account)
		return ems->account->name;
	else
		return ("On This Computer");
}

const char *evolution_mail_store_get_uid(EvolutionMailStore *ems)
{
	if (ems->account)
		return ems->account->uid;
	else
		return "local@local";
}

#if 0
static BonoboObject *
factory (BonoboGenericFactory *factory,
	 const char *component_id,
	 void *closure)
{
	if (strcmp (component_id, MAIL_STORE_ID) == 0) {
		BonoboObject *object = BONOBO_OBJECT (g_object_new (EVOLUTION_MAIL_TYPE_STORE, NULL));
		bonobo_object_ref (object);
		return object;
	}
	
	g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);

	return NULL;
}

BONOBO_ACTIVATION_SHLIB_FACTORY (FACTORY_ID, "Evolution Calendar component factory", factory, NULL)
#endif
