/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnome/gnome-i18n.h>

#include "em-popup.h"
#include "e-util/e-msgport.h"
#include <e-util/e-icon-factory.h>
#include "em-utils.h"
#include "em-composer-utils.h"

#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-url.h>

#include <camel/camel-vee-folder.h>
#include <camel/camel-vtrash-folder.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <gal/util/e-util.h>

static void emp_standard_menu_factory(EPopup *emp, void *data);

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
	case EM_POPUP_TARGET_ATTACHMENTS: {
		EMPopupTargetAttachments *s = (EMPopupTargetAttachments *)t;

		g_slist_foreach(s->attachments, (GFunc)g_object_unref, NULL);
		g_slist_free(s->attachments);
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

EMPopup *em_popup_new(const char *menuid)
{
	EMPopup *emp = g_object_new(em_popup_get_type(), 0);

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
em_popup_target_new_select(EMPopup *emp, struct _CamelFolder *folder, const char *folder_uri, GPtrArray *uids)
{
	EMPopupTargetSelect *t = e_popup_target_new(&emp->popup, EM_POPUP_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	int i;
	const char *tmp;

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
	
	if (!(em_utils_folder_is_drafts(folder, folder_uri)
	      || em_utils_folder_is_outbox(folder, folder_uri))
	    && uids->len == 1)
		mask &= ~EM_POPUP_SELECT_ADD_SENDER;

	if (uids->len == 1)
		mask &= ~EM_POPUP_SELECT_ONE;

	if (uids->len >= 1)
		mask &= ~EM_POPUP_SELECT_MANY;

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info = camel_folder_get_message_info(folder, uids->pdata[i]);

		if (info == NULL)
			continue;

		if (info->flags & CAMEL_MESSAGE_SEEN)
			mask &= ~EM_POPUP_SELECT_MARK_UNREAD;
		else
			mask &= ~EM_POPUP_SELECT_MARK_READ;
		
		if (info->flags & CAMEL_MESSAGE_DELETED)
			mask &= ~EM_POPUP_SELECT_UNDELETE;
		else
			mask &= ~EM_POPUP_SELECT_DELETE;

		if (info->flags & CAMEL_MESSAGE_FLAGGED)
			mask &= ~EM_POPUP_SELECT_MARK_UNIMPORTANT;
		else
			mask &= ~EM_POPUP_SELECT_MARK_IMPORTANT;

		if (info->flags & CAMEL_MESSAGE_JUNK)
			mask &= ~EM_POPUP_SELECT_MARK_NOJUNK;
		else
			mask &= ~EM_POPUP_SELECT_MARK_JUNK;
			
		tmp = camel_tag_get (&info->user_tags, "follow-up");
		if (tmp && *tmp) {
			mask &= ~EM_POPUP_SELECT_FLAG_CLEAR;
			tmp = camel_tag_get(&info->user_tags, "completed-on");
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
em_popup_target_new_uri(EMPopup *emp, const char *uri)
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
		mask &= ~EM_POPUP_URI_NOT_MAILTO;

	t->target.mask = mask;

	return t;
}

EMPopupTargetPart *
em_popup_target_new_part(EMPopup *emp, struct _CamelMimePart *part, const char *mime_type)
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
em_popup_target_new_folder (EMPopup *emp, const char *uri, guint32 info_flags, guint32 popup_flags)
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
		const char *path;
		
		if (popup_flags & EM_POPUP_FOLDER_DELETE)
			mask &= ~EM_POPUP_FOLDER_DELETE;
		
		if (!(info_flags & CAMEL_FOLDER_NOINFERIORS))
			mask &= ~EM_POPUP_FOLDER_INFERIORS;
		
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

/**
 * em_popup_target_new_attachments:
 * @emp: 
 * @attachments: A list of EMsgComposerAttachment objects, reffed for
 * the list.  Will be unreff'd once finished with.
 * 
 * Owns the list @attachments and their items after they're passed in.
 * 
 * Return value: 
 **/
EMPopupTargetAttachments *
em_popup_target_new_attachments(EMPopup *emp, GSList *attachments)
{
	EMPopupTargetAttachments *t = e_popup_target_new(&emp->popup, EM_POPUP_TARGET_ATTACHMENTS, sizeof(*t));
	guint32 mask = ~0;
	int len = g_slist_length(attachments);

	t->attachments = attachments;
	if (len > 0)
		mask &= ~ EM_POPUP_ATTACHMENTS_MANY;
	if (len == 1)
		mask &= ~ EM_POPUP_ATTACHMENTS_ONE;
	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */

static void
emp_part_popup_saveas(EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetPart *t = (EMPopupTargetPart *)ep->target;

	em_utils_save_part(ep->target->widget, _("Save As..."), t->part);
}

static void
emp_part_popup_set_background(EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetPart *t = (EMPopupTargetPart *)ep->target;
	GConfClient *gconf;
	char *str, *filename, *path, *extension;
	unsigned int i=1;
	
	filename = g_strdup(camel_mime_part_get_filename(t->part));
	   
	/* if filename is blank, create a default filename based on MIME type */
	if (!filename || !filename[0]) {
		CamelContentType *ct;

		ct = camel_mime_part_get_content_type(t->part);
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
		char *name;
		name = g_strdup_printf(extension?"%s (%d).%s":"%s (%d)", filename, i++, extension);
		g_free(path);
		path = g_build_filename(g_get_home_dir(), ".gnome2", "wallpapers", name, NULL);
		g_free(name);
	}
	
	g_free(filename);
	
	if (em_utils_save_part_to_file(ep->target->widget, path, t->part)) {
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
emp_part_popup_reply_sender(EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetPart *t = (EMPopupTargetPart *)ep->target;
	CamelMimeMessage *message;
	
	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->part);
	em_utils_reply_to_message(NULL, NULL, message, REPLY_MODE_SENDER, NULL);
}

static void
emp_part_popup_reply_list (EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetPart *t = (EMPopupTargetPart *)ep->target;
	CamelMimeMessage *message;
	
	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->part);
	em_utils_reply_to_message(NULL, NULL, message, REPLY_MODE_LIST, NULL);
}

static void
emp_part_popup_reply_all (EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetPart *t = (EMPopupTargetPart *)ep->target;
	CamelMimeMessage *message;
	
	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *)t->part);
	em_utils_reply_to_message(NULL, NULL, message, REPLY_MODE_ALL, NULL);
}

static void
emp_part_popup_forward (EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetPart *t = (EMPopupTargetPart *)ep->target;
	CamelMimeMessage *message;

	/* TODO: have a emfv specific override so we can get the parent folder uri */
	message = (CamelMimeMessage *)camel_medium_get_content_object((CamelMedium *) t->part);
	em_utils_forward_message(message, NULL);
}

static EMPopupItem emp_standard_object_popups[] = {
	{ E_POPUP_ITEM, "00.part.00", N_("_Save As..."), emp_part_popup_saveas, NULL, "stock_save-as", 0 },
	{ E_POPUP_ITEM, "00.part.10", N_("Set as _Background"), emp_part_popup_set_background, NULL, NULL, EM_POPUP_PART_IMAGE },
	{ E_POPUP_BAR, "10.part", NULL, NULL, NULL, NULL, EM_POPUP_PART_MESSAGE },
	{ E_POPUP_ITEM, "10.part.00", N_("_Reply to sender"), emp_part_popup_reply_sender, NULL, "stock_mail-reply" , EM_POPUP_PART_MESSAGE },
	{ E_POPUP_ITEM, "10.part.01", N_("Reply to _List"), emp_part_popup_reply_list, NULL, NULL, EM_POPUP_PART_MESSAGE},
	{ E_POPUP_ITEM, "10.part.03", N_("Reply to _All"), emp_part_popup_reply_all, NULL, "stock_mail-reply-to-all", EM_POPUP_PART_MESSAGE},
	{ E_POPUP_BAR, "20.part", NULL, NULL, NULL, NULL, EM_POPUP_PART_MESSAGE },
	{ E_POPUP_ITEM, "20.part.00", N_("_Forward"), emp_part_popup_forward, NULL, "stock_mail-forward", EM_POPUP_PART_MESSAGE },
};

static const EPopupItem emp_standard_part_apps_bar = { E_POPUP_BAR, "99.object" };

/* ********************************************************************** */

static void
emp_uri_popup_link_open(EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;
	GError *err = NULL;
		
	gnome_url_show(t->uri, &err);
	if (err) {
		g_warning("gnome_url_show: %s", err->message);
		g_error_free(err);
	}
}

static void
emp_uri_popup_address_send(EPopup *ep, EPopupItem *item, void *data)
{
	EMPopupTargetURI *t = (EMPopupTargetURI *)ep->target;

	/* TODO: have an emfv specific override to get the from uri */
	em_utils_compose_new_message_with_mailto(t->uri, NULL);
}

static void
emp_uri_popup_address_add(EPopup *ep, EPopupItem *item, void *data)
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
	{ E_POPUP_ITEM, "00.uri.00", N_("_Open Link in Browser"), emp_uri_popup_link_open, NULL, NULL, EM_POPUP_URI_NOT_MAILTO },
	{ E_POPUP_ITEM, "00.uri.10", N_("Se_nd message to..."), emp_uri_popup_address_send, NULL, NULL, EM_POPUP_URI_MAILTO },
	{ E_POPUP_ITEM, "00.uri.20", N_("_Add to Addressbook"), emp_uri_popup_address_add, NULL, NULL, EM_POPUP_URI_MAILTO },
};

/* ********************************************************************** */

#define LEN(x) (sizeof(x)/sizeof(x[0]))

#include <libgnomevfs/gnome-vfs-mime-handlers.h>

	EMPopupTargetPart *target;

static void
emp_apps_open_in(EPopup *ep, EPopupItem *item, void *data)
{
	char *path;
	EMPopupTargetPart *target = (EMPopupTargetPart *)ep->target;

	path = em_utils_temp_save_part(target->target.widget, target->part);
	if (path) {
		GnomeVFSMimeApplication *app = item->user_data;
		int douri = (app->expects_uris == GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS);
		char *command;
		
		if (app->requires_terminal) {
			char *term, *args = NULL;
			GConfClient *gconf;
			
			gconf = gconf_client_get_default ();
			if ((term = gconf_client_get_string (gconf, "/desktop/gnome/applications/terminal/exec", NULL)))
				args = gconf_client_get_string (gconf, "/desktop/gnome/applications/terminal/exec_arg", NULL);
			g_object_unref (gconf);
			
			if (term == NULL)
				return;
			
			command = g_strdup_printf ("%s%s%s %s %s%s &", term, args ? " " : "", args ? args : "",
						   app->command, douri ? "file://" : "", path);
			g_free (term);
			g_free (args);
		} else {
			command = g_strdup_printf ("%s %s%s &", app->command, douri ? "file://" : "", path);
		}
		
		/* FIXME: Do not use system here */
		system(command);
		g_free(command);
		g_free(path);
	}
}

static void
emp_apps_popup_free(EPopup *ep, GSList *free_list, void *data)
{
	while (free_list) {
		GSList *n = free_list->next;
		EPopupItem *item = free_list->data;

		g_free(item->path);
		g_free(item->label);
		g_free(item);
		g_slist_free_1(free_list);

		free_list = n;
	}
}

static void
emp_standard_items_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static void
emp_standard_menu_factory(EPopup *emp, void *data)
{
	int i, len;
	EPopupItem *items;
	GSList *menus = NULL;

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
		GList *apps = gnome_vfs_mime_get_short_list_applications(t->mime_type);

		/* FIXME: use the snoop_part stuff from em-format.c */
		if (apps == NULL && strcmp(t->mime_type, "application/octet-stream") == 0) {
			const char *filename, *name_type;
			
			filename = camel_mime_part_get_filename(t->part);

			if (filename) {
				/* GNOME-VFS will misidentify TNEF attachments as MPEG */
				if (!strcmp (filename, "winmail.dat"))
					name_type = "application/vnd.ms-tnef";
				else
					name_type = gnome_vfs_mime_type_from_name(filename);
				if (name_type)
					apps = gnome_vfs_mime_get_short_list_applications(name_type);
			}
		}

		if (apps) {
			GString *label = g_string_new("");
			GSList *open_menus = NULL;
			GList *l;

			menus = g_slist_prepend(menus, (void *)&emp_standard_part_apps_bar);

			for (l = apps, i = 0; l; l = l->next, i++) {
				GnomeVFSMimeApplication *app = l->data;
				EPopupItem *item;

				if (app->requires_terminal)
					continue;

				item = g_malloc0(sizeof(*item));
				item->type = E_POPUP_ITEM;
				item->path = g_strdup_printf("99.object.%02d", i);
				item->label = g_strdup_printf(_("Open in %s..."), app->name);
				item->activate = emp_apps_open_in;
				item->user_data = app;

				open_menus = g_slist_prepend(open_menus, item);
			}

			if (open_menus)
				e_popup_add_items(emp, open_menus, emp_apps_popup_free, NULL);

			g_string_free(label, TRUE);
			g_list_free(apps);
		}

		items = emp_standard_object_popups;
		len = LEN(emp_standard_object_popups);
		break; }
	default:
		items = NULL;
		len = 0;
	}

	for (i=0;i<len;i++) {
		if ((items[i].visible & emp->target->mask) == 0)
			menus = g_slist_prepend(menus, &items[i]);
	}

	if (menus)
		e_popup_add_items(emp, menus, emp_standard_items_free, NULL);
}

/* ********************************************************************** */

/* Popup menu plugin handler */

/*
<e-plugin
  class="com.ximian.mail.plugin.popup:1.0"
  id="com.ximian.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="com.ximian.mail.popupMenu:1.0"
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

static void *emph_parent_class;
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
	{ "mark_junk", EM_POPUP_SELECT_MARK_JUNK },
	{ "mark_nojunk", EM_POPUP_SELECT_MARK_NOJUNK },
	{ "folder", EM_POPUP_SELECT_FOLDER },
	{ 0 }
};

static const EPopupHookTargetMask emph_uri_masks[] = {
	{ "http", EM_POPUP_URI_HTTP },
	{ "mailto", EM_POPUP_URI_MAILTO },
	{ "notmailto", EM_POPUP_URI_NOT_MAILTO },
	{ 0 }
};

static const EPopupHookTargetMask emph_part_masks[] = {
	{ "message", EM_POPUP_PART_MESSAGE },
	{ "image", EM_POPUP_PART_IMAGE },
	{ 0 }
};

static const EPopupHookTargetMask emph_folder_masks[] = {
	{ "folder", EM_POPUP_FOLDER_FOLDER },
	{ "store", EM_POPUP_FOLDER_STORE },
	{ "inferiors", EM_POPUP_FOLDER_INFERIORS },
	{ "delete", EM_POPUP_FOLDER_DELETE },
	{ "select", EM_POPUP_FOLDER_SELECT },
	{ 0 }
};

static const EPopupHookTargetMask emph_attachments_masks[] = {
	{ "one", EM_POPUP_ATTACHMENTS_ONE },
	{ "many", EM_POPUP_ATTACHMENTS_MANY },
	{ 0 }
};

static const EPopupHookTargetMap emph_targets[] = {
	{ "select", EM_POPUP_TARGET_SELECT, emph_select_masks },
	{ "uri", EM_POPUP_TARGET_URI, emph_uri_masks },
	{ "part", EM_POPUP_TARGET_PART, emph_part_masks },
	{ "folder", EM_POPUP_TARGET_FOLDER, emph_folder_masks },
	{ "attachments", EM_POPUP_TARGET_ATTACHMENTS, emph_attachments_masks },
	{ 0 }
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
	int i;

	((GObjectClass *)klass)->finalize = emph_finalise;
	((EPluginHookClass *)klass)->id = "com.ximian.evolution.mail.popup:1.0";

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
