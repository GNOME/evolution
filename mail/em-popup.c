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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "em-popup.h"
#include "libedataserver/e-msgport.h"
#include "em-utils.h"
#include "em-composer-utils.h"

#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-url.h>
#include <camel/camel-stream-mem.h>

#include <camel/camel-vee-folder.h>
#include <camel/camel-vtrash-folder.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libedataserver/e-data-server-util.h>
#include <e-util/e-util.h>
#include "e-attachment.h"

static GObjectClass *emp_parent;

static void
emp_init(GObject *o)
{
	/*EMPopup *emp = (EMPopup *)o; */
}

static void
emp_finalise(GObject *o)
{
	((GObjectClass *)emp_parent)->finalize(o);
}

static void
emp_target_free(EPopup *ep, EPopupTarget *t)
{
	switch (t->type) {
	case EM_POPUP_TARGET_SELECT: {
		EMPopupTargetSelect *s = (EMPopupTargetSelect *)t;

		if (s->folder)
			camel_object_unref(s->folder);
		g_free(s->uri);
		if (s->uids)
			em_utils_uids_free(s->uids);
		break; }
	case EM_POPUP_TARGET_URI: {
		EMPopupTargetURI *s = (EMPopupTargetURI *)t;

		g_free(s->uri);
		break; }
	case EM_POPUP_TARGET_PART: {
		EMPopupTargetPart *s = (EMPopupTargetPart *)t;

		camel_object_unref(s->part);
		g_free(s->mime_type);
		break; }
	case EM_POPUP_TARGET_FOLDER: {
		EMPopupTargetFolder *s = (EMPopupTargetFolder *)t;

		g_free(s->uri);
		break; }
	}

	((EPopupClass *)emp_parent)->target_free(ep, t);
}

static void
emp_class_init(GObjectClass *klass)
{
	klass->finalize = emp_finalise;
	((EPopupClass *)klass)->target_free = emp_target_free;
}

GType
em_popup_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMPopupClass),
			NULL, NULL,
			(GClassInitFunc)emp_class_init,
			NULL, NULL,
			sizeof(EMPopup), 0,
			(GInstanceInitFunc)emp_init
		};
		emp_parent = g_type_class_ref(e_popup_get_type());
		type = g_type_register_static(e_popup_get_type(), "EMPopup", &info, 0);
	}

	return type;
}

EMPopup *em_popup_new(const gchar *menuid)
{
	EMPopup *emp = g_object_new(em_popup_get_type(), NULL);

	e_popup_construct(&emp->popup, menuid);

	return emp;
}

/**
 * em_popup_target_new_select:
 * @folder: The selection will ref this for the life of it.
 * @folder_uri:
 * @uids: The selection will free this when done with it.
 *
 * Create a new selection popup target.
 *
 * Return value:
 **/
EMPopupTargetSelect *
em_popup_target_new_select(EMPopup *emp, CamelFolder *folder, const gchar *folder_uri, GPtrArray *uids)
{
	EMPopupTargetSelect *t = e_popup_target_new(&emp->popup, EM_POPUP_TARGET_SELECT, sizeof(*t));
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	guint32 mask = ~0;
	gboolean draft_or_outbox;
	gint i;
	const gchar *tmp;

	t->uids = uids;
	t->folder = folder;
	t->uri = g_strdup(folder_uri);

	if (folder == NULL) {
		t->target.mask = mask;

		return t;
	}

	camel_object_ref(folder);
	mask &= ~EM_POPUP_SELECT_FOLDER;

	if (em_utils_folder_is_sent(folder, folder_uri))
		mask &= ~EM_POPUP_SELECT_EDIT;

	draft_or_outbox = em_utils_folder_is_drafts(folder, folder_uri) || em_utils_folder_is_outbox(folder, folder_uri);
	if (!draft_or_outbox && uids->len == 1)
		mask &= ~EM_POPUP_SELECT_ADD_SENDER;

	if (uids->len == 1)
		mask &= ~EM_POPUP_SELECT_ONE;

	if (uids->len >= 1)
		mask &= ~EM_POPUP_SELECT_MANY;

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info = camel_folder_get_message_info(folder, uids->pdata[i]);
		guint32 flags;

		if (info == NULL)
			continue;

		flags = camel_message_info_flags(info);
		if (flags & CAMEL_MESSAGE_SEEN)
			mask &= ~EM_POPUP_SELECT_MARK_UNREAD;
		else
			mask &= ~EM_POPUP_SELECT_MARK_READ;

		if ((store->flags & CAMEL_STORE_VJUNK) && !draft_or_outbox) {
			if ((flags & CAMEL_MESSAGE_JUNK))
				mask &= ~EM_POPUP_SELECT_NOT_JUNK;
			else
				mask &= ~EM_POPUP_SELECT_JUNK;
		} else if (draft_or_outbox) {
			/* Show none option */
			mask |= EM_POPUP_SELECT_NOT_JUNK;
			mask |= EM_POPUP_SELECT_JUNK;
		} else {
			/* Show both options */
			mask &= ~EM_POPUP_SELECT_NOT_JUNK;
			mask &= ~EM_POPUP_SELECT_JUNK;
		}

		if (flags & CAMEL_MESSAGE_DELETED)
			mask &= ~EM_POPUP_SELECT_UNDELETE;
		else
			mask &= ~EM_POPUP_SELECT_DELETE;

		if (flags & CAMEL_MESSAGE_FLAGGED)
			mask &= ~EM_POPUP_SELECT_MARK_UNIMPORTANT;
		else
			mask &= ~EM_POPUP_SELECT_MARK_IMPORTANT;

		tmp = camel_message_info_user_tag(info, "follow-up");
		if (tmp && *tmp) {
			mask &= ~EM_POPUP_SELECT_FLAG_CLEAR;
			tmp = camel_message_info_user_tag(info, "completed-on");
			if (tmp == NULL || *tmp == 0)
				mask &= ~EM_POPUP_SELECT_FLAG_COMPLETED;
		} else
			mask &= ~EM_POPUP_SELECT_FLAG_FOLLOWUP;

		if (i == 0 && uids->len == 1
		    && (tmp = camel_message_info_mlist(info))
		    && tmp[0] != 0)
			mask &= ~EM_POPUP_SELECT_MAILING_LIST;

		camel_folder_free_message_info(folder, info);
	}

	t->target.mask = mask;

	return t;
}

EMPopupTargetURI *
em_popup_target_new_uri(EMPopup *emp, const gchar *uri)
{
	EMPopupTargetURI *t = e_popup_target_new(&emp->popup, EM_POPUP_TARGET_URI, sizeof(*t));
	guint32 mask = ~0;

	t->uri = g_strdup(uri);

	if (g_ascii_strncasecmp(uri, "http:", 5) == 0
	    || g_ascii_strncasecmp(uri, "https:", 6) == 0)
		mask &= ~EM_POPUP_URI_HTTP;

	if (g_ascii_strncasecmp(uri, "mailto:", 7) == 0)
		mask &= ~EM_POPUP_URI_MAILTO;
	else
		mask &= ~(EM_POPUP_URI_NOT_MAILTO|~mask);

	t->target.mask = mask;

	return t;
}

EMPopupTargetPart *
em_popup_target_new_part(EMPopup *emp, CamelMimePart *part, const gchar *mime_type)
{
	EMPopupTargetPart *t = e_popup_target_new(&emp->popup, EM_POPUP_TARGET_PART, sizeof(*t));
	guint32 mask = ~0;

	t->part = part;
	camel_object_ref(part);
	if (mime_type)
		t->mime_type = g_strdup(mime_type);
	else
		t->mime_type = camel_data_wrapper_get_mime_type((CamelDataWrapper *)part);

	camel_strdown(t->mime_type);

	if (CAMEL_IS_MIME_MESSAGE(camel_medium_get_content_object((CamelMedium *)part)))
		mask &= ~EM_POPUP_PART_MESSAGE;

	if (strncmp(t->mime_type, "image/", 6) == 0)
		mask &= ~EM_POPUP_PART_IMAGE;

	t->target.mask = mask;

	return t;
}

/* TODO: This should be based on the CamelFolderInfo, but ... em-folder-tree doesn't keep it? */
EMPopupTargetFolder *
em_popup_target_new_folder (EMPopup *emp, const gchar *uri, guint32 info_flags, guint32 popup_flags)
{
	EMPopupTargetFolder *t = e_popup_target_new(&emp->popup, EM_POPUP_TARGET_FOLDER, sizeof(*t));
	guint32 mask = ~0;
	CamelURL *url;

	t->uri = g_strdup(uri);

	if (popup_flags & EM_POPUP_FOLDER_STORE)
		mask &= ~(EM_POPUP_FOLDER_STORE|EM_POPUP_FOLDER_INFERIORS);
	else
		mask &= ~EM_POPUP_FOLDER_FOLDER;

	url = camel_url_new(uri, NULL);
	if (url == NULL)
		goto done;

	if (!(popup_flags & EM_POPUP_FOLDER_STORE)) {
		const gchar *path;

		if (popup_flags & EM_POPUP_FOLDER_DELETE)
			mask &= ~EM_POPUP_FOLDER_DELETE;

		if (!(info_flags & CAMEL_FOLDER_NOINFERIORS))
			mask &= ~EM_POPUP_FOLDER_INFERIORS;

		if (info_flags & CAMEL_FOLDER_TYPE_OUTBOX)
			mask &= ~EM_POPUP_FOLDER_OUTBOX;
		else
			mask &= ~EM_POPUP_FOLDER_NONSTATIC;

		if (!(info_flags & CAMEL_FOLDER_NOSELECT))
			mask &= ~EM_POPUP_FOLDER_SELECT;

		if (info_flags & CAMEL_FOLDER_VIRTUAL)
			mask |= EM_POPUP_FOLDER_DELETE|EM_POPUP_FOLDER_INFERIORS;

		if ((path = url->fragment ? url->fragment : url->path)) {
			if ((!strcmp (url->protocol, "vfolder") && !strcmp (path, CAMEL_UNMATCHED_NAME))
			    || (!strcmp (url->protocol, "maildir") && !strcmp (path, "."))) /* hack for maildir toplevel folder */
				mask |= EM_POPUP_FOLDER_DELETE|EM_POPUP_FOLDER_INFERIORS;
		}
	}

	camel_url_free(url);
done:
	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */

/* Popup menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="emp_view_emacs"/>
  </menu>
  </extension>

*/

static gpointer emph_parent_class;
#define emph ((EMPopupHook *)eph)

static const EPopupHookTargetMask emph_select_masks[] = {
	{ "one", EM_POPUP_SELECT_ONE },
	{ "many", EM_POPUP_SELECT_MANY },
	{ "mark_read", EM_POPUP_SELECT_MARK_READ },
	{ "mark_unread", EM_POPUP_SELECT_MARK_UNREAD },
	{ "delete", EM_POPUP_SELECT_DELETE },
	{ "undelete", EM_POPUP_SELECT_UNDELETE },
	{ "mailing_list", EM_POPUP_SELECT_MAILING_LIST },
	{ "resend", EM_POPUP_SELECT_EDIT },
	{ "mark_important", EM_POPUP_SELECT_MARK_IMPORTANT },
	{ "mark_unimportant", EM_POPUP_SELECT_MARK_UNIMPORTANT },
	{ "flag_followup", EM_POPUP_SELECT_FLAG_FOLLOWUP },
	{ "flag_completed", EM_POPUP_SELECT_FLAG_COMPLETED },
	{ "flag_clear", EM_POPUP_SELECT_FLAG_CLEAR },
	{ "add_sender", EM_POPUP_SELECT_ADD_SENDER },
	{ "folder", EM_POPUP_SELECT_FOLDER },
	{ "junk", EM_POPUP_SELECT_JUNK },
	{ "not_junk", EM_POPUP_SELECT_NOT_JUNK },
	{ NULL }
};

static const EPopupHookTargetMask emph_uri_masks[] = {
	{ "http", EM_POPUP_URI_HTTP },
	{ "mailto", EM_POPUP_URI_MAILTO },
	{ "notmailto", EM_POPUP_URI_NOT_MAILTO },
	{ NULL }
};

static const EPopupHookTargetMask emph_part_masks[] = {
	{ "message", EM_POPUP_PART_MESSAGE },
	{ "image", EM_POPUP_PART_IMAGE },
	{ NULL }
};

static const EPopupHookTargetMask emph_folder_masks[] = {
	{ "folder", EM_POPUP_FOLDER_FOLDER },
	{ "store", EM_POPUP_FOLDER_STORE },
	{ "inferiors", EM_POPUP_FOLDER_INFERIORS },
	{ "delete", EM_POPUP_FOLDER_DELETE },
	{ "select", EM_POPUP_FOLDER_SELECT },
	{ "outbox", EM_POPUP_FOLDER_OUTBOX },
	{ "nonstatic", EM_POPUP_FOLDER_NONSTATIC },
	{ NULL }
};

static const EPopupHookTargetMap emph_targets[] = {
	{ "select", EM_POPUP_TARGET_SELECT, emph_select_masks },
	{ "uri", EM_POPUP_TARGET_URI, emph_uri_masks },
	{ "part", EM_POPUP_TARGET_PART, emph_part_masks },
	{ "folder", EM_POPUP_TARGET_FOLDER, emph_folder_masks },
	{ NULL }
};

static void
emph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)emph_parent_class)->finalize(o);
}

static void
emph_class_init(EPluginHookClass *klass)
{
	gint i;

	((GObjectClass *)klass)->finalize = emph_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.mail.popup:1.0";

	for (i=0;emph_targets[i].type;i++)
		e_popup_hook_class_add_target_map((EPopupHookClass *)klass, &emph_targets[i]);

	((EPopupHookClass *)klass)->popup_class = g_type_class_ref(em_popup_get_type());
}

GType
em_popup_hook_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMPopupHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EMPopupHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_popup_hook_get_type());
		type = g_type_register_static(e_popup_hook_get_type(), "EMPopupHook", &info, 0);
	}

	return type;
}
