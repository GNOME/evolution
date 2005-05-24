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
#include "evolution-mail-store.h"
#include "evolution-mail-messageiterator.h"
#include "evolution-mail-session.h"

#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include "e-corba-utils.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

#define _PRIVATE(o) (g_type_instance_get_private ((GTypeInstance *)o, evolution_mail_folder_get_type()))

struct _EvolutionMailFolderPrivate {
	CamelFolder *folder;

	guint32 folder_changed;
};

/* GObject methods */

static void
impl_dispose (GObject *object)
{
	struct _EvolutionMailFolderPrivate *p = _PRIVATE(object);

	if (p->folder) {
		camel_object_remove_event(p->folder, p->folder_changed);
		camel_object_unref(p->folder);
		p->folder = NULL;
	}

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

static GNOME_Evolution_Mail_MessageIterator
impl_getMessages(PortableServer_Servant _servant, const CORBA_char * pattern, CORBA_Environment * ev)
{
	EvolutionMailFolder *emf = (EvolutionMailFolder *)bonobo_object_from_servant(_servant);
	struct _CamelFolder *folder;
	EvolutionMailMessageIterator *emi;
	GNOME_Evolution_Mail_MessageIterator iter;

	folder = evolution_mail_folder_get_folder(emf);
	if (folder == NULL) {
		GNOME_Evolution_Mail_FAILED *x;

		x = GNOME_Evolution_Mail_FAILED__alloc();
		x->why = CORBA_string_dup("Unable to open folder");
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Mail_FAILED, x);

		return CORBA_OBJECT_NIL;
	}

	emi = evolution_mail_messageiterator_new(folder, pattern);
	camel_object_unref(folder);

	/* NB: How do we destroy the object once we're done? */

	iter = bonobo_object_corba_objref((BonoboObject *)emi);

	return iter;
}

static void
impl_changeMessages(PortableServer_Servant _servant, const GNOME_Evolution_Mail_MessageInfoSets *infos, CORBA_Environment * ev)
{
	EvolutionMailFolder *emf = (EvolutionMailFolder *)bonobo_object_from_servant(_servant);
	struct _CamelFolder *folder;
	int i, j;

	folder = evolution_mail_folder_get_folder(emf);
	if (folder == NULL) {
		GNOME_Evolution_Mail_FAILED *x;

		x = GNOME_Evolution_Mail_FAILED__alloc();
		x->why = CORBA_string_dup("Unable to open folder");
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Mail_FAILED, x);
		return;
	}

	camel_folder_freeze(folder);
	for (i=0;i<infos->_length;i++) {
		CamelMessageInfo *mi;
		GNOME_Evolution_Mail_MessageInfoSet *mis = &infos->_buffer[i];

		mi = camel_folder_get_message_info(folder, mis->uid);
		if (mi == NULL)
			continue;

		if (mis->flagMask)
			camel_message_info_set_flags(mi, mis->flagMask, mis->flagSet);

		for (j=0;j<mis->userFlagSet._length;j++)
			camel_message_info_set_user_flag(mi, mis->userFlagSet._buffer[j], TRUE);
		for (j=0;j<mis->userFlagUnset._length;j++)
			camel_message_info_set_user_flag(mi, mis->userFlagUnset._buffer[j], FALSE);
		for (j=0;j<mis->userTags._length;j++)
			camel_message_info_set_user_tag(mi, mis->userTags._buffer[j].name, mis->userTags._buffer[j].value[0]?mis->userTags._buffer[j].value:NULL);

		camel_message_info_free(mi);
	}
	camel_folder_thaw(folder);

	camel_object_unref(folder);
}

/* Initialization */

static void
evolution_mail_folder_class_init (EvolutionMailFolderClass *klass)
{
	POA_GNOME_Evolution_Mail_Folder__epv *epv = &klass->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	epv->getProperties = impl_getProperties;
	epv->getMessages = impl_getMessages;
	epv->changeMessages = impl_changeMessages;

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
evolution_mail_folder_new(EvolutionMailStore *ems, const char *name, const char *full_name)
{
	EvolutionMailFolder *emf = g_object_new (EVOLUTION_MAIL_TYPE_FOLDER, NULL);

	emf->name = g_strdup(name);
	emf->full_name = g_strdup(full_name);

	emf->store = ems;

	return emf;
}

static void
emf_set_change(GNOME_Evolution_Mail_FolderChange *change, GNOME_Evolution_Mail_ChangeType how, CamelFolder *folder, GPtrArray *uids)
{
	int total = 0, i;

	change->type = how;
	change->messages._maximum = uids->len;
	change->messages._buffer = GNOME_Evolution_Mail_MessageInfos_allocbuf(uids->len);

	for (i=0;i<uids->len;i++) {
		CamelMessageInfo *info = camel_folder_get_message_info(folder, uids->pdata[i]);

		if (info) {
			e_mail_messageinfo_set_message(&change->messages._buffer[total], info);
			camel_message_info_free(info);
			total++;
		} else {
			printf("couldn't get info for changed uid '%s'?\n", (char *)uids->pdata[i]);
		}
	}

	change->messages._length = total;
}

static void
emf_folder_changed(CamelObject *o, void *d, void *data)
{
	EvolutionMailFolder *emf = data;
	CamelFolder *folder = (CamelFolder *)o;
	CamelFolderChangeInfo *ci = d;
	int count = 0;
	GNOME_Evolution_Mail_FolderChanges *changes;
	CORBA_long flags;

	flags = evolution_mail_session_listening(emf->store->session);

	if ((flags & (GNOME_Evolution_Mail_Session_FOLDER_ADDED|GNOME_Evolution_Mail_Session_FOLDER_CHANGED|GNOME_Evolution_Mail_Session_FOLDER_REMOVED)) == 0)
		return;

	changes = GNOME_Evolution_Mail_FolderChanges__alloc();
	changes->_maximum = 3;
	changes->_buffer = GNOME_Evolution_Mail_FolderChanges_allocbuf(3);
	CORBA_sequence_set_release(changes, TRUE);

	/* could be a race if a new listener is added */
	if (ci->uid_added->len && (flags & GNOME_Evolution_Mail_Session_FOLDER_ADDED)) {
		emf_set_change(&changes->_buffer[count], GNOME_Evolution_Mail_ADDED, folder, ci->uid_added);
		count++;
	}
	if (ci->uid_removed->len && (flags & GNOME_Evolution_Mail_Session_FOLDER_REMOVED)) {
		emf_set_change(&changes->_buffer[count], GNOME_Evolution_Mail_REMOVED, folder, ci->uid_removed);
		count++;
	}
	if (ci->uid_changed->len && (flags & GNOME_Evolution_Mail_Session_FOLDER_CHANGED)) {
		emf_set_change(&changes->_buffer[count], GNOME_Evolution_Mail_CHANGED, folder, ci->uid_changed);
		count++;
	}

	changes->_length = count;

	evolution_mail_session_folder_changed(emf->store->session,
					      bonobo_object_corba_objref((BonoboObject *)emf->store),
					      bonobo_object_corba_objref((BonoboObject *)emf),
					      changes);

	CORBA_free(changes);
}

struct _CamelFolder *evolution_mail_folder_get_folder(EvolutionMailFolder *emf)
{
	struct _EvolutionMailFolderPrivate *p = _PRIVATE(emf);
	CamelStore *store;
	CamelException ex;

	if (p->folder == NULL) {
		store = evolution_mail_store_get_store(emf->store);
		if (store == NULL)
			return NULL;

		camel_exception_init(&ex);
		p->folder = camel_store_get_folder(store, emf->full_name, 0, &ex);
		if (p->folder) {
			p->folder_changed = camel_object_hook_event(p->folder, "folder_changed", emf_folder_changed, emf);
		} else {
			camel_exception_clear(&ex);
		}
		camel_object_unref(store);
	}

	if (p->folder)
		camel_object_ref(p->folder);

	return p->folder;
}
