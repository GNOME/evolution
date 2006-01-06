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
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-folder.h>

#include <libedataserver/e-account.h>

#include "mail/mail-component.h"
#include "mail/mail-send-recv.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_store_get_type()))

struct _EvolutionMailStorePrivate {
	CamelStore *store;

	GHashTable *folders;
	/* sorted array of folders by full_name */
	GPtrArray *folders_array;

	guint32 folder_opened;
	guint32 folder_created;
	guint32 folder_deleted;
	guint32 folder_renamed;
	guint32 folder_subscribed;
	guint32 folder_unsubscribed;

	EDList listeners;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)object;
	struct _EvolutionMailStorePrivate *p = _PRIVATE(object);

	/* FIXME: unref store
	   unhook events */

	if (ems->account) {
		g_object_unref(ems->account);
		ems->account = NULL;
	}

	e_mail_listener_free(&p->listeners);

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
		   const Evolution_Mail_PropertyNames* names,
		   Evolution_Mail_Properties **propsp,
		   CORBA_Environment * ev)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)bonobo_object_from_servant(_servant);
	int i;
	Evolution_Mail_Properties *props;
	/*struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);*/
	CORBA_boolean ok = CORBA_TRUE;

	*propsp = props = Evolution_Mail_Properties__alloc();
	props->_length = names->_length;
	props->_maximum = props->_length;
	props->_buffer = Evolution_Mail_Properties_allocbuf(props->_maximum);
	CORBA_sequence_set_release(props, CORBA_TRUE);

	for (i=0;i<names->_length;i++) {
		const CORBA_char *name = names->_buffer[i];
		Evolution_Mail_Property *prop = &props->_buffer[i];

		d(printf("getting property '%s'\n", name));

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
ems_add_folders(EvolutionMailStore *ems, CamelFolderInfo *fi, GPtrArray *added)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	while (fi) {
		if (g_hash_table_lookup(p->folders, fi->full_name) == NULL) {
			EvolutionMailFolder *emf = evolution_mail_folder_new(ems, fi->name, fi->full_name);

			g_hash_table_insert(p->folders, emf->full_name, emf);
			g_ptr_array_add(p->folders_array, emf);
			if (added) {
				g_object_ref(emf);
				g_ptr_array_add(added, emf);
			}
		}

		if (fi->child)
			ems_add_folders(ems, fi->child, added);

		fi = fi->next;
	}
}

static void
ems_remove_folders(EvolutionMailStore *ems, CamelFolderInfo *fi, GPtrArray *removed)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	EvolutionMailFolder *emf;

	while (fi) {
		emf = g_hash_table_lookup(p->folders, fi->full_name);
		if (emf) {
			g_hash_table_remove(p->folders, fi->full_name);
			g_ptr_array_remove(p->folders_array, emf);
			if (removed)
				g_ptr_array_add(removed, emf);
			else
				g_object_unref(emf);
		} else {
			g_warning("Folder removed I didn't know existed '%s'\n", fi->full_name);
		}

		if (fi->child)
			ems_remove_folders(ems, fi->child, removed);

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
ems_set_changes(Evolution_Mail_StoreChange *change, Evolution_Mail_ChangeType how, GPtrArray *changed)
{
	int i;

	change->type = how;
	change->folders._maximum = changed->len;
	change->folders._length = changed->len;
	change->folders._buffer = Evolution_Mail_FolderInfos_allocbuf(change->folders._maximum);
	CORBA_sequence_set_release(&change->folders, TRUE);

	for (i=0;i<changed->len;i++)
		e_mail_folderinfo_set_folder(&change->folders._buffer[i], changed->pdata[i]);
}

static Evolution_Mail_StoreChanges *
ems_create_changes(EvolutionMailStore *ems, Evolution_Mail_ChangeType how, GPtrArray *changed)
{
	Evolution_Mail_StoreChanges *changes;

	/* NB: we only ever create 1 changetype at the moment */

	changes = Evolution_Mail_StoreChanges__alloc();
	changes->_maximum = 1;
	changes->_length = 1;
	changes->_buffer = Evolution_Mail_StoreChanges_allocbuf(1);
	CORBA_sequence_set_release(changes, TRUE);

	ems_set_changes(&changes->_buffer[0], how, changed);

	return changes;
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
	CamelFolderInfo *fi = d;
	GPtrArray *added;
	int i;

	added = g_ptr_array_new();
	ems_add_folders(ems, fi, added);

	if (added) {
		if (added->len) {
			Evolution_Mail_StoreChanges *changes = ems_create_changes(ems, Evolution_Mail_ADDED, added);

			evolution_mail_store_changed(ems, changes);
			CORBA_free(changes);

			for (i=0;i<added->len;i++)
				g_object_unref(added->pdata[i]);
		}
		g_ptr_array_free(added, TRUE);
	}
}

static void
ems_folder_unsubscribed(CamelObject *o, void *d, void *data)
{
	EvolutionMailStore *ems = data;
	CamelFolderInfo *fi = d;
	GPtrArray *removed = NULL;
	int i;

	removed = g_ptr_array_new();
	ems_remove_folders(ems, fi, removed);

	if (removed) {
		if (removed->len) {
			Evolution_Mail_StoreChanges *changes = ems_create_changes(ems, Evolution_Mail_REMOVED, removed);

			evolution_mail_store_changed(ems, changes);
			CORBA_free(changes);

			for (i=0;i<removed->len;i++)
				g_object_unref(removed->pdata[i]);
		}
		g_ptr_array_free(removed, TRUE);
	}
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
		ems_folder_unsubscribed(o, d, data);
}

static void
get_folders(CamelFolderInfo *fi, GPtrArray *folders)
{
	while (fi) {
		g_ptr_array_add(folders, fi);

		if (fi->child)
			get_folders(fi->child, folders);

		fi = fi->next;
	}
}

static int
folder_cmp(const void *ap, const void *bp)
{
	const CamelFolderInfo *a = ((CamelFolderInfo **)ap)[0];
	const CamelFolderInfo *b = ((CamelFolderInfo **)bp)[0];

	return strcmp(a->full_name, b->full_name);
}

static void
ems_folder_renamed(CamelObject *o, void *d, void *data)
{
	EvolutionMailStore *ems = data;
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	CamelRenameInfo *reninfo = d;
	int i, oldlen, newlen;
	GPtrArray *renamed = g_ptr_array_new(), *folders = g_ptr_array_new();
	CamelFolderInfo *top;
	GString *name = g_string_new("");

	/* flatten/sort folders to make sure they're in the right order */
	get_folders(reninfo->new, folders);
	qsort(folders->pdata, folders->len, sizeof(folders->pdata[0]), folder_cmp);
	top = folders->pdata[0];

	oldlen = strlen(reninfo->old_base);
	newlen = strlen(top->full_name);

	for (i=0;i<folders->len;i++) {
		CamelFolderInfo *fi = folders->pdata[i];
		EvolutionMailFolder *emf;

		if (strlen(fi->full_name) >= newlen) {
			g_string_printf(name, "%s%s", reninfo->old_base, fi->full_name + newlen);
			if ((emf = g_hash_table_lookup(p->folders, name->str))) {
				/* FIXME: locking / or api to rename */
				g_hash_table_remove(p->folders, emf->full_name);
				g_free(emf->full_name);
				g_free(emf->name);
				emf->full_name = g_strdup(fi->full_name);
				emf->name = g_strdup(fi->name);
				g_hash_table_insert(p->folders, emf->full_name, emf);

				g_object_ref(emf);
				g_ptr_array_add(renamed, emf);
			}
		}
	}

	g_string_free(name, TRUE);
	g_ptr_array_free(folders, TRUE);

	if (renamed) {
		if (renamed->len) {
			Evolution_Mail_StoreChanges *changes = ems_create_changes(ems, Evolution_Mail_CHANGED, renamed);

			evolution_mail_store_changed(ems, changes);
			CORBA_free(changes);

			for (i=0;i<renamed->len;i++)
				g_object_unref(renamed->pdata[i]);
		}
		g_ptr_array_free(renamed, TRUE);
	}
}

static Evolution_Mail_FolderInfos *
impl_getFolders(PortableServer_Servant _servant,
		const CORBA_char * pattern,
		const Evolution_Mail_FolderListener listener,
		CORBA_Environment * ev)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)bonobo_object_from_servant(_servant);
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);
	CamelFolderInfo *fi;
	CamelException ex = { 0 };
	Evolution_Mail_FolderInfos *folders = NULL;
	int i;
	CamelStore *store;

	store = evolution_mail_store_get_store(ems, ev);
	if (store == NULL) {
		return CORBA_OBJECT_NIL;
	}

	if (p->folders == NULL) {
		fi = camel_store_get_folder_info(store, "", CAMEL_STORE_FOLDER_INFO_RECURSIVE|CAMEL_STORE_FOLDER_INFO_FAST, &ex);

		if (fi) {
			p->folders = g_hash_table_new(g_str_hash, g_str_equal);
			p->folders_array = g_ptr_array_new();
			ems_add_folders(ems, fi, NULL);
			camel_store_free_folder_info(store, fi);
			ems_sort_folders(p);
		} else {
			e_mail_exception_xfer_camel(ev, &ex);
			camel_object_unref(store);
			return CORBA_OBJECT_NIL;
		}
	}

	folders = Evolution_Mail_FolderInfos__alloc();
	folders->_length = p->folders_array->len;
	folders->_maximum = folders->_length;
	folders->_buffer = Evolution_Mail_FolderInfos_allocbuf(folders->_maximum);
	CORBA_sequence_set_release(folders, CORBA_TRUE);

	for (i=0;i<p->folders_array->len;i++) {
		EvolutionMailFolder *emf = p->folders_array->pdata[i];

		evolution_mail_folder_addlistener(emf, listener);
		e_mail_folderinfo_set_folder(&folders->_buffer[i], emf);
	}

	camel_object_unref(store);

	return folders;
}

static void
impl_sendMessage(PortableServer_Servant _servant,
		 const Evolution_Mail_MessageStream message,
		 CORBA_Environment * ev)
{
	EvolutionMailStore *ems = (EvolutionMailStore *)bonobo_object_from_servant(_servant);
	CamelException ex = { 0 };
	CamelMimeMessage *msg;
	CamelInternetAddress *from;
	CamelMessageInfo *info;
	CORBA_Environment wev = { 0 };

	if (ems->account == NULL
	    || ems->account->transport == NULL
	    || ems->account->transport->url == NULL) {
		e_mail_exception_set(ev, Evolution_Mail_NOT_SUPPORTED, _("Account cannot send e-mail"));
		goto done;
	}

	msg = e_messagestream_to_message(message, ev);
	if (msg == NULL)
		goto done;

	from = camel_internet_address_new();
	camel_internet_address_add(from, ems->account->id->name, ems->account->id->address);
	camel_mime_message_set_from(msg, from);
	camel_object_unref(from);

	camel_medium_set_header((CamelMedium *)msg, "X-Evolution-Account", ems->account->uid);

	if (msg->date == 0)
		camel_mime_message_set_date(msg, CAMEL_MESSAGE_DATE_CURRENT, 0);

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, ~0);

	camel_folder_append_message(mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX), msg, info, NULL, &ex);
	camel_message_info_free(info);

	if (camel_exception_is_set(&ex)) {
		e_mail_exception_xfer_camel(ev, &ex);
	} else {
		mail_send();
	}

	camel_object_unref(msg);
done:
	Evolution_Mail_MessageStream_dispose(message, &wev);
	if (wev._major != CORBA_NO_EXCEPTION)
		CORBA_exception_free(&wev);
}

/* Initialization */

static void
evolution_mail_store_class_init (EvolutionMailStoreClass *klass)
{
	POA_Evolution_Mail_Store__epv *epv = &klass->epv;
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
evolution_mail_store_init(EvolutionMailStore *ems, EvolutionMailStoreClass *klass)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	bonobo_object_set_immortal((BonoboObject *)ems, TRUE);
	e_dlist_init(&p->listeners);
}

BONOBO_TYPE_FUNC_FULL (EvolutionMailStore, Evolution_Mail_Store, PARENT_TYPE, evolution_mail_store)

EvolutionMailStore *
evolution_mail_store_new(struct _EvolutionMailSession *s, struct _EAccount *ea)
{
	EvolutionMailStore *ems;
	struct _EvolutionMailStorePrivate *p;
	static PortableServer_POA poa = NULL;

	d(printf("EvolutionMailStore.new(\"%s\")\n", ea?ea->name:"local"));

	if (poa == NULL)
		poa = bonobo_poa_get_threaded (ORBIT_THREAD_HINT_PER_REQUEST, NULL);

	ems = g_object_new (EVOLUTION_MAIL_TYPE_STORE, "poa", poa, NULL);
	p = _PRIVATE(ems);

	if (ea) {
		ems->account = ea;
		g_object_ref(ea);
	}

	ems->session = s;

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

CamelStore *evolution_mail_store_get_store(EvolutionMailStore *ems, CORBA_Environment *ev)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	if (p->store == NULL) {
		if (ems->account == NULL) {
			p->store = mail_component_peek_local_store(NULL);
			camel_object_ref(p->store);
		} else {
			const char *uri;
			CamelException ex = { 0 };

			uri = e_account_get_string(ems->account, E_ACCOUNT_SOURCE_URL);
			if (uri && *uri) {
				p->store = camel_session_get_store(ems->session->session, uri, &ex);
				if (camel_exception_is_set(&ex)) {
					e_mail_exception_xfer_camel(ev, &ex);
					return NULL;
				}
			} else {
				e_mail_exception_set(ev, Evolution_Mail_NOT_SUPPORTED, _("No store available"));
				return NULL;
			}
		}

		p->folder_opened = camel_object_hook_event(p->store, "folder_opened", ems_folder_opened, ems);
		p->folder_created = camel_object_hook_event(p->store, "folder_created", ems_folder_created, ems);
		p->folder_deleted = camel_object_hook_event(p->store, "folder_deleted", ems_folder_deleted, ems);
		p->folder_renamed = camel_object_hook_event(p->store, "folder_renamed", ems_folder_renamed, ems);
		p->folder_subscribed = camel_object_hook_event(p->store, "folder_subscribed", ems_folder_subscribed, ems);
		p->folder_unsubscribed = camel_object_hook_event(p->store, "folder_unsubscribed", ems_folder_unsubscribed, ems);
	}

	camel_object_ref(p->store);
	return p->store;
}

int evolution_mail_store_close_store(EvolutionMailStore *ems)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	/* FIXME: locking */
	if (p->store) {
		if (!e_dlist_empty(&p->listeners))
			return -1;

		camel_object_remove_event(p->store, p->folder_opened);
		camel_object_remove_event(p->store, p->folder_created);
		camel_object_remove_event(p->store, p->folder_deleted);
		camel_object_remove_event(p->store, p->folder_renamed);
		camel_object_remove_event(p->store, p->folder_subscribed);
		camel_object_remove_event(p->store, p->folder_unsubscribed);
		camel_object_unref(p->store);
		p->store = NULL;
	}

	/* FIXME: need to close of sub-folders too? */

	return 0;
}

void
evolution_mail_store_addlistener(EvolutionMailStore *ems, Evolution_Mail_StoreListener listener)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	/* FIXME: locking */
	e_mail_listener_add(&p->listeners, listener);
}

void
evolution_mail_store_changed(EvolutionMailStore *ems, Evolution_Mail_StoreChanges *changes)
{
	struct _EvolutionMailStorePrivate *p = _PRIVATE(ems);

	if (!e_mail_listener_emit(&p->listeners, (EMailListenerChanged)Evolution_Mail_StoreListener_changed,
				  bonobo_object_corba_objref((BonoboObject *)ems), changes)) {
		evolution_mail_store_close_store(ems);
		w(printf("No more listeners for store, could dispose store object now?\n"));
	}
}
