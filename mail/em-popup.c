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
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

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

static void emp_standard_menu_factory(EPopup *emp, gpointer data);

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

	e_popup_class_add_factory((EPopupClass *)klass, NULL, emp_standard_menu_factory, NULL);
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

static void
emp_part_popup_saveas(EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *t = ep->target;
	CamelMimePart *part = NULL;

	part = ((EMPopupTargetPart *) t)->part;

	em_utils_save_part(ep->target->widget, _("Save As..."), part);
}

static void
emp_part_popup_set_background(EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *t = ep->target;
	GConfClient *gconf;
	gchar *str, *filename, *path, *extension;
	guint i=1;
	CamelMimePart *part = NULL;

	part = ((EMPopupTargetPart *) t)->part;

	if (!part)
		return;

	filename = g_strdup(camel_mime_part_get_filename(part));

	/* if filename is blank, create a default filename based on MIME type */
	if (!filename || !filename[0]) {
		CamelContentType *ct;

		ct = camel_mime_part_get_content_type(part);
		g_free (filename);
		filename = g_strdup_printf (_("untitled_image.%s"), ct->subtype);
	}

	e_filename_make_safe(filename);

	path = g_build_filename(g_get_home_dir(), ".gnome2", "wallpapers", filename, NULL);

	extension = strrchr(filename, '.');
	if (extension)
		*extension++ = 0;

	/* if file exists, stick a (number) on the end */
	while (g_file_test(path, G_FILE_TEST_EXISTS)) {
		gchar *name;
		name = g_strdup_printf(extension?"%s (%d).%s":"%s (%d)", filename, i++, extension);
		g_free(path);
		path = g_build_filename(g_get_home_dir(), ".gnome2", "wallpapers", name, NULL);
		g_free(name);
	}

	g_free(filename);

	if (em_utils_save_part_to_file(ep->target->widget, path, part)) {
		gconf = gconf_client_get_default();

		/* if the filename hasn't changed, blank the filename before
		*  setting it so that gconf detects a change and updates it */
		if ((str = gconf_client_get_string(gconf, "/desktop/gnome/background/picture_filename", NULL)) != NULL
		     && strcmp (str, path) == 0) {
			gconf_client_set_string(gconf, "/desktop/gnome/background/picture_filename", "", NULL);
		}

		g_free (str);
		gconf_client_set_string(gconf, "/desktop/gnome/background/picture_filename", path, NULL);

		/* if GNOME currently doesn't display a picture, set to "wallpaper"
		 * display mode, otherwise leave it alone */
		if ((str = gconf_client_get_string(gconf, "/desktop/gnome/background/picture_options", NULL)) == NULL
		     || strcmp(str, "none") == 0) {
			gconf_client_set_string(gconf, "/desktop/gnome/background/picture_options", "wallpaper", NULL);
		}

		gconf_client_suggest_sync(gconf, NULL);

		g_free(str);
		g_object_unref(gconf);
	}

	g_free(path);
}

static void
emp_part_popup_reply_sender(EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *t = ep->target;
	CamelMimeMessage *message;
	CamelMimePart *part;

	part = ((EMPopupTargetPart *) t)->part;

	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)part);
	em_utils_reply_to_message(NULL, NULL, message, REPLY_MODE_SENDER, NULL);
}

static void
emp_part_popup_reply_list (EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *t = ep->target;
	CamelMimeMessage *message;
	CamelMimePart *part;

	part = ((EMPopupTargetPart *) t)->part;

	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)part);
	em_utils_reply_to_message(NULL, NULL, message, REPLY_MODE_LIST, NULL);
}

static void
emp_part_popup_reply_all (EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *t = ep->target;
	CamelMimeMessage *message;
	CamelMimePart *part;

	part = ((EMPopupTargetPart *) t)->part;

	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)part);
	em_utils_reply_to_message(NULL, NULL, message, REPLY_MODE_ALL, NULL);
}

static void
emp_part_popup_forward (EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *t = ep->target;
	CamelMimeMessage *message;
	CamelMimePart *part;

	part = ((EMPopupTargetPart *) t)->part;

	/* TODO: have a emfv specific override so we can get the parent folder uri */
	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *) part);
	em_utils_forward_message(message, NULL);
}

static EMPopupItem emp_standard_object_popups[] = {
	{ E_POPUP_ITEM, (gchar *) "00.part.00", (gchar *) N_("_Save As..."), emp_part_popup_saveas, NULL, (gchar *) "document-save-as", 0 },
	{ E_POPUP_ITEM, (gchar *) "00.part.10", (gchar *) N_("Set as _Background"), emp_part_popup_set_background, NULL, NULL, EM_POPUP_PART_IMAGE },
	{ E_POPUP_BAR, (gchar *) "10.part", NULL, NULL, NULL, NULL, EM_POPUP_PART_MESSAGE },
	{ E_POPUP_ITEM, (gchar *) "10.part.00", (gchar *) N_("_Reply to sender"), emp_part_popup_reply_sender, NULL, (gchar *) "mail-reply-sender" , EM_POPUP_PART_MESSAGE },
	{ E_POPUP_ITEM, (gchar *) "10.part.01", (gchar *) N_("Reply to _List"), emp_part_popup_reply_list, NULL, NULL, EM_POPUP_PART_MESSAGE},
	{ E_POPUP_ITEM, (gchar *) "10.part.03", (gchar *) N_("Reply to _All"), emp_part_popup_reply_all, NULL, (gchar *) "mail-reply-all", EM_POPUP_PART_MESSAGE},
	{ E_POPUP_BAR, (gchar *) "20.part", NULL, NULL, NULL, NULL, EM_POPUP_PART_MESSAGE },
	{ E_POPUP_ITEM, (gchar *) "20.part.00", (gchar *) N_("_Forward"), emp_part_popup_forward, NULL, (gchar *) "mail-forward", EM_POPUP_PART_MESSAGE },
};

static const EPopupItem emp_standard_part_apps_bar = { E_POPUP_BAR, (gchar *) "99.object" };

/* ********************************************************************** */

static void
emp_uri_popup_link_open(EPopup *ep, EPopupItem *item, gpointer data)
{
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;

	/* FIXME Pass a parent window. */
	e_show_uri (NULL, t->uri);
}

static void
emp_uri_popup_address_send(EPopup *ep, EPopupItem *item, gpointer data)
{
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;

	/* TODO: have an emfv specific override to get the from uri */
	em_utils_compose_new_message_with_mailto(t->uri, NULL);
}

static void
emp_uri_popup_address_add(EPopup *ep, EPopupItem *item, gpointer data)
{
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;
	CamelURL *url;

	url = camel_url_new(t->uri, NULL);
	if (url == NULL) {
		g_warning("cannot parse url '%s'", t->uri);
		return;
	}

	if (url->path && url->path[0])
		em_utils_add_address(ep->target->widget, url->path);

	camel_url_free(url);
}

static EPopupItem emp_standard_uri_popups[] = {
	{ E_POPUP_ITEM, (gchar *) "00.uri.00", (gchar *) N_("_Open Link in Browser"), emp_uri_popup_link_open, NULL, NULL, EM_POPUP_URI_HTTP },
	{ E_POPUP_ITEM, (gchar *) "00.uri.10", (gchar *) N_("_Send New Message To..."), emp_uri_popup_address_send, NULL, (gchar *) "mail-message-new", EM_POPUP_URI_MAILTO },
	{ E_POPUP_ITEM, (gchar *) "00.uri.20", (gchar *) N_("_Add to Address Book"), emp_uri_popup_address_add, NULL, (gchar *) "contact-new", EM_POPUP_URI_MAILTO },
};

/* ********************************************************************** */

#define LEN(x) (sizeof(x)/sizeof(x[0]))

static void
emp_apps_open_in(EPopup *ep, EPopupItem *item, gpointer data)
{
	gchar *path;
	EPopupTarget *target = ep->target;
	CamelMimePart *part;

	part = ((EMPopupTargetPart *) target)->part;

	path = em_utils_temp_save_part(target->widget, part, TRUE);
	if (path) {
		GAppInfo *app = item->user_data;
		GList *uris = NULL;
		GError *error = NULL;

		if (g_app_info_supports_files (app)) {
			GFile *file = g_file_new_for_path (path);

			uris = g_list_append (uris, file);
			g_app_info_launch (app, uris, NULL, &error);
			g_object_unref (file);
		} else {
			gchar *uri;

			uri = e_util_filename_to_uri (path);
			uris = g_list_append (uris, uri);

			g_app_info_launch_uris (app, uris, NULL, &error);
			g_free (uri);
		}

		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}

		g_list_free (uris);
		g_free (path);
	}
}

static void
emp_apps_popup_free(EPopup *ep, GSList *free_list, gpointer data)
{
	while (free_list) {
		GSList *n = free_list->next;
		EPopupItem *item = free_list->data;

		if (item->user_data && item->activate == emp_apps_open_in)
			g_object_unref (item->user_data);

		g_free(item->path);
		g_free(item->label);
		g_free(item);
		g_slist_free_1(free_list);

		free_list = n;
	}
}

static void
emp_standard_items_free(EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free(items);
}

static void
emp_add_vcard (EPopup *ep, EPopupItem *item, gpointer data)
{
	EPopupTarget *target = ep->target;
	CamelMimePart *part;
	CamelDataWrapper *content;
	CamelStreamMem *mem;

	part = ((EMPopupTargetPart *) target)->part;

	if (!part)
		return;

	content = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	mem = CAMEL_STREAM_MEM (camel_stream_mem_new ());

	if (camel_data_wrapper_decode_to_stream (content, CAMEL_STREAM (mem)) == -1 ||
	    !mem->buffer->data)
		g_warning ("Read part's content failed!");
	else {
		GString *vcard = g_string_new_len ((const gchar *) mem->buffer->data, mem->buffer->len);

		em_utils_add_vcard (target->widget, vcard->str);

		g_string_free (vcard, TRUE);
	}

	camel_object_unref (mem);
}

static void
emp_standard_menu_factory(EPopup *emp, gpointer data)
{
	gint i, len;
	EPopupItem *items;
	GSList *menus = NULL;
	GList *apps = NULL;
	gchar *mime_type = NULL;
	const gchar *filename = NULL;

	switch (emp->target->type) {
#if 0
	case EM_POPUP_TARGET_SELECT:
		return;
		items = emp_standard_select_popups;
		len = LEN(emp_standard_select_popups);
		break;
#endif
	case EM_POPUP_TARGET_URI: {
		/*EMPopupTargetURI *t = (EMPopupTargetURI *)target;*/

		items = emp_standard_uri_popups;
		len = LEN(emp_standard_uri_popups);
		break; }
	case EM_POPUP_TARGET_PART: {
		EMPopupTargetPart *t = (EMPopupTargetPart *)emp->target;
		mime_type = camel_data_wrapper_get_mime_type((CamelDataWrapper *)t->part);
		filename = camel_mime_part_get_filename(t->part);

		items = emp_standard_object_popups;
		len = LEN(emp_standard_object_popups);
		break; }
	default:
		items = NULL;
		len = 0;
	}

	if (mime_type) {
                gchar *cp;

                /* GIO expects lowercase MIME types. */
                for (cp = mime_type; *cp != '\0'; cp++)
                        *cp = g_ascii_tolower (*cp);

		cp = g_content_type_from_mime_type (mime_type);
		apps = g_app_info_get_all_for_type (cp ? cp : mime_type);
		g_free (cp);

		if (apps == NULL || strcmp (mime_type, "application/octet-stream") == 0) {
			if (filename != NULL) {
				gchar *name_type;

				name_type = e_util_guess_mime_type (filename, FALSE);
				cp = g_content_type_from_mime_type (name_type);
				/* show alternative apps first */
				apps = g_list_concat (g_app_info_get_all_for_type (cp ? cp : name_type), apps);
				g_free (cp);
				g_free (name_type);
			}
		}

		if (apps) {
			GString *label = g_string_new("");
			GSList *open_menus = NULL;
			GList *l;

			menus = g_slist_prepend(menus, (gpointer)&emp_standard_part_apps_bar);

			for (l = apps, i = 0; l; l = l->next, i++) {
				GAppInfo *app = l->data;
				EPopupItem *item;

				item = g_malloc0(sizeof(*item));
				item->type = E_POPUP_ITEM;
				item->path = g_strdup_printf("99.object.%02d", i);
				item->label = g_strdup_printf(_("Open in %s..."), g_app_info_get_name (app));
				item->activate = emp_apps_open_in;
				item->user_data = app;

				open_menus = g_slist_prepend(open_menus, item);
			}

			if (open_menus)
				e_popup_add_items(emp, open_menus, NULL, emp_apps_popup_free, NULL);

			g_string_free(label, TRUE);
			g_list_free(apps);
		}

		if (g_ascii_strcasecmp (mime_type, "text/x-vcard") == 0||
		    g_ascii_strcasecmp (mime_type, "text/vcard") == 0) {
			EPopupItem *item;

			item = g_malloc0 (sizeof (*item));
			item->type = E_POPUP_ITEM;
			item->path = (gchar *) "00.00.vcf.00"; /* make it first item */
			item->label = _("_Add to Address Book");
			item->activate = emp_add_vcard;
			item->user_data = NULL;
			item->image = (gchar *) "contact-new";

			e_popup_add_items (emp, g_slist_append (NULL, item), NULL, NULL, NULL);
		}

		g_free (mime_type);
	}

	for (i=0;i<len;i++) {
		if ((items[i].visible & emp->target->mask) == 0)
			menus = g_slist_prepend(menus, &items[i]);
	}

	if (menus)
		e_popup_add_items(emp, menus, NULL, emp_standard_items_free, NULL);
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
