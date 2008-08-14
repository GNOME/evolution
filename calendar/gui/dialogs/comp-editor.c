/* Evolution calendar - Framework for a calendar component editor dialog
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/libgnome.h>
#include <e-util/e-util.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-util-private.h>
#include <e-util/gconf-bridge.h>
#include <evolution-shell-component-utils.h>

#include <camel/camel-url.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-stream-fs.h>

#include "mail/mail-tools.h"

#include "../print.h"
#include "../comp-util.h"
#include "save-comp.h"
#include "delete-comp.h"
#include "send-comp.h"
#include "changed-comp.h"
#include "cancel-comp.h"
#include "recur-comp.h"
#include "comp-editor.h"
#include "../e-cal-popup.h"
#include "../calendar-config-keys.h"
#include "cal-attachment-select-file.h"

#include "e-attachment-bar.h"
#include "misc/e-expander.h"
#include "e-util/e-error.h"

#define COMP_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_COMP_EDITOR, CompEditorPrivate))

#define d(x)

/* Private part of the CompEditor structure */
struct _CompEditorPrivate {
	/* Client to use */
	ECal *client;

	/* Source client (where comp lives currently) */
	ECal *source_client;

	/* View to listen for changes */
	ECalView *view;

	/* Calendar object/uid we are editing; this is an internal copy */
	ECalComponent *comp;

	/* The pages we have */
	GList *pages;

	/* Notebook to hold the pages */
	GtkNotebook *notebook;

	/* Attachment handling */
	GtkWidget *attachment_bar;
	GtkWidget *attachment_scrolled_window;
	GtkWidget *attachment_expander;
	GtkWidget *attachment_expander_label;
	GtkWidget *attachment_expander_icon;
	GtkWidget *attachment_expander_num;

	/* Manages menus and toolbars */
	GtkUIManager *manager;

	gchar *summary;

	guint32 attachment_bar_visible : 1;

	/* TODO use this flags for setting all the boolean variables
	   below */
	CompEditorFlags flags;

	gboolean changed;
	gboolean needs_send;

	CalObjModType mod;

 	gboolean existing_org;
 	gboolean user_org;
	gboolean is_group_item;

 	gboolean warned;
};

enum {
	PROP_0,
	PROP_CHANGED,
	PROP_CLIENT,
	PROP_FLAGS,
	PROP_SUMMARY
};

static const gchar *ui =
"<ui>"
"  <menubar action='main-menu'>"
"    <menu action='file-menu'>"
"      <menuitem action='save'/>"
"      <menuitem action='close'/>"
"    </menu>"
"    <menu action='edit-menu'>"
"      <menuitem action='cut'/>"
"      <menuitem action='copy'/>"
"      <menuitem action='paste'/>"
"      <separator/>"
"      <menuitem action='select-all'/>"
"    </menu>"
"    <menu action='view-menu'/>"
"    <menu action='insert-menu'>"
"      <menuitem action='attach'/>"
"      <menuitem action='attach-recent'/>"
"    </menu>"
"    <menu action='options-menu'/>"
"    <menu action='help-menu'>"
"      <menuitem action='help'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='main-toolbar'>"
"    <toolitem action='save'/>"
"    <toolitem action='close'/>"
"    <separator/>"
"    <toolitem action='attach'/>"
"  </toolbar>"
"</ui>";

static void comp_editor_show_help (CompEditor *editor);
static void setup_widgets (CompEditor *editor);

static void real_edit_comp (CompEditor *editor, ECalComponent *comp);
static gboolean real_send_comp (CompEditor *editor, ECalComponentItipMethod method);
static gboolean prompt_and_save_changes (CompEditor *editor, gboolean send);
static void close_dialog (CompEditor *editor);

static void page_dates_changed_cb (CompEditor *editor, CompEditorPageDates *dates, CompEditorPage *page);

static void obj_modified_cb (ECal *client, GList *objs, CompEditor *editor);
static void obj_removed_cb (ECal *client, GList *uids, CompEditor *editor);
static gboolean open_attachment (EAttachmentBar *bar, CompEditor *editor);

G_DEFINE_TYPE (CompEditor, comp_editor, GTK_TYPE_WINDOW)

enum {
	DND_TYPE_MESSAGE_RFC822,
	DND_TYPE_X_UID_LIST,
	DND_TYPE_TEXT_URI_LIST,
	DND_TYPE_NETSCAPE_URL,
	DND_TYPE_TEXT_VCARD,
	DND_TYPE_TEXT_CALENDAR,
};

static GtkTargetEntry drop_types[] = {
	{ "message/rfc822", 0, DND_TYPE_MESSAGE_RFC822 },
	{ "x-uid-list", 0, DND_TYPE_X_UID_LIST },
	{ "text/uri-list", 0, DND_TYPE_TEXT_URI_LIST },
	{ "_NETSCAPE_URL", 0, DND_TYPE_NETSCAPE_URL },
	{ "text/x-vcard", 0, DND_TYPE_TEXT_VCARD },
	{ "text/calendar", 0, DND_TYPE_TEXT_CALENDAR },
};

#define num_drop_types (sizeof (drop_types) / sizeof (drop_types[0]))

static struct {
	char *target;
	GdkAtom atom;
	guint32 actions;
} drag_info[] = {
	{ "message/rfc822", NULL, GDK_ACTION_COPY },
	{ "x-uid-list", NULL, GDK_ACTION_ASK|GDK_ACTION_MOVE|GDK_ACTION_COPY },
	{ "text/uri-list", NULL, GDK_ACTION_COPY },
	{ "_NETSCAPE_URL", NULL, GDK_ACTION_COPY },
	{ "text/x-vcard", NULL, GDK_ACTION_COPY },
	{ "text/calendar", NULL, GDK_ACTION_COPY },
};

enum {
	OBJECT_CREATED,
	LAST_SIGNAL
};

static guint comp_editor_signals[LAST_SIGNAL] = { 0 };

static void
attach_message(CompEditor *editor, CamelMimeMessage *msg)
{
	CamelMimePart *mime_part;
	const char *subject;
	guint i;
	char *filename = NULL;

	mime_part = camel_mime_part_new();
	camel_mime_part_set_disposition(mime_part, "inline");
	subject = camel_mime_message_get_subject(msg);
	if (subject) {
		char *desc = g_strdup_printf(_("Attached message - %s"), subject);

		camel_mime_part_set_description(mime_part, desc);
		g_free(desc);
	} else
		camel_mime_part_set_description(mime_part, _("Attached message"));

	i = e_attachment_bar_get_num_attachments (E_ATTACHMENT_BAR (editor->priv->attachment_bar));
	i++;
	filename = g_strdup_printf ("email%d",i);
	camel_mime_part_set_filename (mime_part, filename);

	camel_medium_set_content_object((CamelMedium *)mime_part, (CamelDataWrapper *)msg);
	camel_mime_part_set_content_type(mime_part, "message/rfc822");
	e_attachment_bar_attach_mime_part(E_ATTACHMENT_BAR(editor->priv->attachment_bar), mime_part);
	camel_object_unref(mime_part);
	g_free (filename);
}

struct _drop_data {
	CompEditor *editor;

	GdkDragContext *context;
	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	guint32 action;
	guint info;
	guint time;

	unsigned int move:1;
	unsigned int moved:1;
	unsigned int aborted:1;
};

static void
drop_action(CompEditor *editor, GdkDragContext *context, guint32 action, GtkSelectionData *selection, guint info, guint time)
{
	char *tmp, *str, **urls;
	CamelMimePart *mime_part;
	CamelStream *stream;
	CamelURL *url;
	CamelMimeMessage *msg;
	char *content_type;
	int i, success=FALSE, delete=FALSE;

	switch (info) {
	case DND_TYPE_MESSAGE_RFC822:
		d(printf ("dropping a message/rfc822\n"));
		/* write the message(s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, (char *)selection->data, selection->length);
		camel_stream_reset (stream);

		msg = camel_mime_message_new ();
		if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg, stream) != -1) {
			attach_message(editor, msg);
			success = TRUE;
			delete = action == GDK_ACTION_MOVE;
		}

		camel_object_unref(msg);
		camel_object_unref(stream);
		break;
	case DND_TYPE_TEXT_URI_LIST:
	case DND_TYPE_NETSCAPE_URL:
		d(printf ("dropping a text/uri-list\n"));
		tmp = g_strndup ((char *)selection->data, selection->length);
		urls = g_strsplit (tmp, "\n", 0);
		g_free (tmp);

		for (i = 0; urls[i] != NULL; i++) {
			str = g_strstrip (urls[i]);
			if (urls[i][0] == '#')
				continue;

			if (!g_ascii_strncasecmp (str, "mailto:", 7)) {
				/* TODO does not handle mailto now */
			} else {
				url = camel_url_new (str, NULL);

				if (url == NULL)
					continue;

				if (!g_ascii_strcasecmp (url->protocol, "file"))
					e_attachment_bar_attach
						(E_ATTACHMENT_BAR (editor->priv->attachment_bar),
					 	url->path,
						"attachment");
				else
					e_attachment_bar_attach_remote_file
						(E_ATTACHMENT_BAR (editor->priv->attachment_bar),
						 str, "attachment");

				camel_url_free (url);
			}
		}

		g_strfreev (urls);
		success = TRUE;
		break;
	case DND_TYPE_TEXT_VCARD:
	case DND_TYPE_TEXT_CALENDAR:
		content_type = gdk_atom_name (selection->type);
		d(printf ("dropping a %s\n", content_type));

		mime_part = camel_mime_part_new ();
		camel_mime_part_set_content (mime_part, (char *)selection->data, selection->length, content_type);
		camel_mime_part_set_disposition (mime_part, "inline");

		e_attachment_bar_attach_mime_part
			(E_ATTACHMENT_BAR (editor->priv->attachment_bar),
			 mime_part);

		camel_object_unref (mime_part);
		g_free (content_type);

		success = TRUE;
		break;
	case DND_TYPE_X_UID_LIST: {
		GPtrArray *uids;
		char *inptr, *inend;
		CamelFolder *folder;
		CamelException ex = CAMEL_EXCEPTION_INITIALISER;

		/* NB: This all runs synchronously, could be very slow/hang/block the ui */

		uids = g_ptr_array_new();

		inptr = (char *)selection->data;
		inend = (char *)(selection->data + selection->length);
		while (inptr < inend) {
			char *start = inptr;

			while (inptr < inend && *inptr)
				inptr++;

			if (start > (char *)selection->data)
				g_ptr_array_add(uids, g_strndup(start, inptr-start));

			inptr++;
		}

		if (uids->len > 0) {
			folder = mail_tool_uri_to_folder((char *)selection->data, 0, &ex);
			if (folder) {
				if (uids->len == 1) {
					msg = camel_folder_get_message(folder, uids->pdata[0], &ex);
					if (msg == NULL)
						goto fail;
					attach_message(editor, msg);
				} else {
					CamelMultipart *mp = camel_multipart_new();
					char *desc;
					char *filename = NULL;
					guint num;

					camel_data_wrapper_set_mime_type((CamelDataWrapper *)mp, "multipart/digest");
					camel_multipart_set_boundary(mp, NULL);
					for (i=0;i<uids->len;i++) {

						msg = camel_folder_get_message(folder, uids->pdata[i], &ex);
						if (msg) {
							mime_part = camel_mime_part_new();
							camel_mime_part_set_disposition(mime_part, "inline");
							camel_medium_set_content_object((CamelMedium *)mime_part, (CamelDataWrapper *)msg);
							camel_mime_part_set_content_type(mime_part, "message/rfc822");
							camel_multipart_add_part(mp, mime_part);
							camel_object_unref(mime_part);
							camel_object_unref(msg);
						} else {
							camel_object_unref(mp);
							goto fail;
						}
					}
					mime_part = camel_mime_part_new();
					camel_medium_set_content_object((CamelMedium *)mime_part, (CamelDataWrapper *)mp);
					/* translators, this count will always be >1 */
					desc = g_strdup_printf(ngettext("Attached message", "%d attached messages", uids->len), uids->len);
					camel_mime_part_set_description(mime_part, desc);
					g_free(desc);

					num = e_attachment_bar_get_num_attachments (E_ATTACHMENT_BAR (editor->priv->attachment_bar));
					num++;
					filename = g_strdup_printf ("email%d", num);
					camel_mime_part_set_filename (mime_part, filename);

					e_attachment_bar_attach_mime_part
						(E_ATTACHMENT_BAR(editor->priv->attachment_bar), mime_part);
					camel_object_unref(mime_part);
					camel_object_unref(mp);
					g_free (filename);
				}
				success = TRUE;
				delete = action == GDK_ACTION_MOVE;
			fail:
				if (camel_exception_is_set(&ex)) {
					char *name;

					camel_object_get(folder, NULL, CAMEL_FOLDER_NAME, &name, NULL);
					e_error_run((GtkWindow *)editor, "mail-editor:attach-nomessages",
						    name?name:(char *)selection->data, camel_exception_get_description(&ex), NULL);
					camel_object_free(folder, CAMEL_FOLDER_NAME, name);
				}
				camel_object_unref(folder);
			} else {
				e_error_run((GtkWindow *)editor, "mail-editor:attach-nomessages",
					    (char *)selection->data, camel_exception_get_description(&ex), NULL);
			}

			camel_exception_clear(&ex);
		}

		g_ptr_array_free(uids, TRUE);

		break; }
	default:
		d(printf ("dropping an unknown\n"));
		break;
	}

	printf("Drag finished, success %d delete %d\n", success, delete);

	gtk_drag_finish(context, success, delete, time);
}

static void
drop_popup_copy (EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_data *m = data;
	drop_action(m->editor, m->context, GDK_ACTION_COPY, m->selection, m->info, m->time);
}

static void
drop_popup_move (EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_data *m = data;
	drop_action(m->editor, m->context, GDK_ACTION_MOVE, m->selection, m->info, m->time);
}

static void
drop_popup_cancel(EPopup *ep, EPopupItem *item, void *data)
{
	struct _drop_data *m = data;
	gtk_drag_finish(m->context, FALSE, FALSE, m->time);
}

static EPopupItem drop_popup_menu[] = {
	{ E_POPUP_ITEM, "00.emc.02", N_("_Copy"), drop_popup_copy, NULL, "mail-copy", 0 },
	{ E_POPUP_ITEM, "00.emc.03", N_("_Move"), drop_popup_move, NULL, "mail-move", 0 },
	{ E_POPUP_BAR, "10.emc" },
	{ E_POPUP_ITEM, "99.emc.00", N_("Cancel _Drag"), drop_popup_cancel, NULL, NULL, 0 },
};

static void
drop_popup_free(EPopup *ep, GSList *items, void *data)
{
	struct _drop_data *m = data;

	g_slist_free(items);

	g_object_unref(m->context);
	g_object_unref(m->editor);
	g_free(m->selection->data);
	g_free(m->selection);
	g_free(m);
}

static void
drag_data_received (CompEditor *editor, GdkDragContext *context,
		    int x, int y, GtkSelectionData *selection,
		    guint info, guint time)
{
	if (selection->data == NULL || selection->length == -1)
		return;

	if (context->action == GDK_ACTION_ASK) {
		ECalPopup *ecp;
		GSList *menus = NULL;
		GtkMenu *menu;
		int i;
		struct _drop_data *m;

		m = g_malloc0(sizeof(*m));
		m->context = context;
		g_object_ref(context);
		m->editor = editor;
		g_object_ref(editor);
		m->action = context->action;
		m->info = info;
		m->time = time;
		m->selection = g_malloc0(sizeof(*m->selection));
		m->selection->data = g_malloc(selection->length);
		memcpy(m->selection->data, selection->data, selection->length);
		m->selection->length = selection->length;

		ecp = e_cal_popup_new("org.gnome.evolution.calendar.editor.popup.drop");
		for (i=0;i<sizeof(drop_popup_menu)/sizeof(drop_popup_menu[0]);i++)
			menus = g_slist_append(menus, &drop_popup_menu[i]);

		e_popup_add_items((EPopup *)ecp, menus, NULL, drop_popup_free, m);
		menu = e_popup_create_menu_once((EPopup *)ecp, NULL, 0);
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, 0, time);
	} else {
		drop_action(editor, context, context->action, selection, info, time);
	}
}

static gboolean
drag_motion(GObject *o, GdkDragContext *context, gint x, gint y, guint time, CompEditor *editor)
{
	GList *targets;
	GdkDragAction action, actions = 0;

	for (targets = context->targets; targets; targets = targets->next) {
		int i;

		for (i=0;i<sizeof(drag_info)/sizeof(drag_info[0]);i++)
			if (targets->data == (void *)drag_info[i].atom)
				actions |= drag_info[i].actions;
	}

	actions &= context->actions;
	action = context->suggested_action;
	/* we default to copy */
	if (action == GDK_ACTION_ASK && (actions & (GDK_ACTION_MOVE|GDK_ACTION_COPY)) != (GDK_ACTION_MOVE|GDK_ACTION_COPY))
		action = GDK_ACTION_COPY;

	gdk_drag_status(context, action, time);

	return action != 0;
}

static void
add_to_bar (CompEditor *editor, GPtrArray *file_list, int is_inline)
{
	CompEditorPrivate *priv = editor->priv;
	int i;

	for (i = 0; i < file_list->len; i++) {
		CamelURL *url;

		if (!(url = camel_url_new (file_list->pdata[i], NULL)))
			continue;

		if (!g_ascii_strcasecmp (url->protocol, "file")) {
			e_attachment_bar_attach((EAttachmentBar *)priv->attachment_bar, url->path, is_inline ? "inline" : "attachment");
		} else {
			e_attachment_bar_attach_remote_file ((EAttachmentBar *)priv->attachment_bar, file_list->pdata[i], is_inline ? "inline" : "attachment");
		}

		camel_url_free (url);
	}
}

static GSList *
get_attachment_list (CompEditor *editor)
{
	GSList *parts = NULL, *list = NULL, *p = NULL;
	const char *comp_uid = NULL;
	const char *local_store = e_cal_get_local_attachment_store (editor->priv->client);
	int ticker=0;
	e_cal_component_get_uid (editor->priv->comp, &comp_uid);

	parts = e_attachment_bar_get_parts((EAttachmentBar *)editor->priv->attachment_bar);

	for (p = parts; p!=NULL ; p = p->next) {
		CamelDataWrapper *wrapper;
		CamelStream *stream;
		char *attach_file_url;
		char *safe_fname, *utf8_safe_fname;
		char *filename;

		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (p->data));

		/* Extract the content from the stream and write it down
		 * as a mime part file into the directory denoting the
		 * calendar source */
		utf8_safe_fname = camel_file_util_safe_filename (camel_mime_part_get_filename ((CamelMimePart *) p->data));

		/* It is absolutely fine to get a NULL from the filename of 
		 * mime part. We assume that it is named "Attachment"
		 * in mailer. I'll do that with a ticker */
		if (!utf8_safe_fname)
			safe_fname = g_strdup_printf ("%s-%d", _("attachment"), ticker++);
		else {
			safe_fname = g_filename_from_utf8 ((const char *) utf8_safe_fname, -1, NULL, NULL, NULL);
			g_free (utf8_safe_fname);
		}
		filename = g_strdup_printf ("%s-%s", comp_uid, safe_fname);

		attach_file_url = g_build_path ("/", local_store, filename, NULL);

		g_free (filename);
		g_free (safe_fname);

		/* do not overwrite existing files, this will result in truncation */
		filename = g_filename_from_uri (attach_file_url, NULL, NULL);
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			stream = camel_stream_fs_new_with_name(filename, O_RDWR|O_CREAT|O_TRUNC, 0600);
			if (!stream) {
				/* TODO handle error conditions */
				g_message ("DEBUG: could not open the file to write\n");
				g_free (attach_file_url);
				g_free (filename);
				continue;
			}

			if (camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) stream) == -1) {
				g_free (attach_file_url);
				camel_stream_close (stream);
				camel_object_unref (stream);
				g_message ("DEBUG: could not write to file\n");
			}

			camel_stream_close (stream);
			camel_object_unref (stream);
		}

		list = g_slist_append (list, g_strdup (attach_file_url));
		g_free (attach_file_url);
		g_free (filename);
	}

	if (parts)
		g_slist_free (parts);
	return list;
}

/* This sets the focus to the toplevel, so any field being edited is committed.
   FIXME: In future we may also want to check some of the fields are valid,
   e.g. the EDateEdit fields. */
static void
commit_all_fields (CompEditor *editor)
{
	gtk_window_set_focus (GTK_WINDOW (editor), NULL);
}

static void
listen_for_changes (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *uid = NULL;

	priv = editor->priv;

	/* Discard change listener */
	if (priv->view) {
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      editor);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* Listen for changes */
	if (priv->comp)
		e_cal_component_get_uid (priv->comp, &uid);

	if (uid) {
		char *query;

		query = g_strdup_printf ("(uid? \"%s\")", uid);
		e_cal_get_query (priv->source_client, query, &priv->view, NULL);
		g_free (query);
	}

	if (priv->view) {
		g_signal_connect (
			priv->view, "objects_modified",
			G_CALLBACK (obj_modified_cb), editor);
		g_signal_connect (
			priv->view, "objects_removed",
			G_CALLBACK (obj_removed_cb), editor);

		e_cal_view_start (priv->view);
	}
}

static void
send_timezone (gpointer key, gpointer value, gpointer user_data)
{
	icaltimezone *zone = value;
	CompEditor *editor = user_data;

	e_cal_add_timezone (editor->priv->client, zone, NULL);
}

static gboolean
save_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	CompEditorFlags flags;
	ECalComponent *clone;
	GList *l;
	gboolean result;
	GError *error = NULL;
	GHashTable *timezones;
	const char *orig_uid;
	icalcomponent *icalcomp;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	flags = comp_editor_get_flags (editor);

	/* Stop listening because we are about to change things */
	if (priv->view) {
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      editor);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* Update on the server */
	timezones = g_hash_table_new (g_str_hash, g_str_equal);

	clone = e_cal_component_clone (priv->comp);
	for (l = priv->pages; l != NULL; l = l->next) {
		if (!comp_editor_page_fill_component (l->data, clone)) {
			g_object_unref (clone);
			g_hash_table_destroy (timezones);
			comp_editor_show_page (editor, COMP_EDITOR_PAGE (l->data));
			return FALSE;
		}

		/* retrieve all timezones */
		comp_editor_page_fill_timezones (l->data, timezones);
	}

	/* If we are not the organizer, we don't update the sequence number */
	if (!e_cal_component_has_organizer (clone) || itip_organizer_is_user (clone, priv->client) || itip_sentby_is_user (clone))
		e_cal_component_commit_sequence (clone);
	else
		e_cal_component_abort_sequence (clone);

	g_object_unref (priv->comp);
	priv->comp = clone;

	e_cal_component_get_uid (priv->comp, &orig_uid);

	/* send timezones */
	g_hash_table_foreach (timezones, (GHFunc) send_timezone, editor);
	g_hash_table_destroy (timezones);

	/* Attachments*/

	e_cal_component_set_attachment_list (priv->comp,
					     get_attachment_list (editor));
	icalcomp = e_cal_component_get_icalcomponent (priv->comp);
	/* send the component to the server */
	if (!cal_comp_is_on_server (priv->comp, priv->client)) {
		result = e_cal_create_object (priv->client, icalcomp, NULL, &error);
		if (result)
			g_signal_emit_by_name (editor, "object_created");
	} else {
		if (priv->mod == CALOBJ_MOD_THIS) {
			e_cal_component_set_rdate_list (priv->comp, NULL);
			e_cal_component_set_rrule_list (priv->comp, NULL);
			e_cal_component_set_exdate_list (priv->comp, NULL);
			e_cal_component_set_exrule_list (priv->comp, NULL);
		}
		result = e_cal_modify_object (priv->client, icalcomp, priv->mod, &error);

		if (result && priv->mod == CALOBJ_MOD_THIS) {
			/* FIXME do we really need to do this ? */
			if ((flags & COMP_EDITOR_DELEGATE) || !e_cal_component_has_organizer (clone) || itip_organizer_is_user (clone, priv->client) || itip_sentby_is_user (clone))
				e_cal_component_commit_sequence (clone);
			else
				e_cal_component_abort_sequence (clone);
		}
	}

	/* If the delay delivery is set, the items will not be created in the server immediately,
	   so we need not show them in the view. They will appear as soon as the server creates
	   it after the delay period */
	if (result && e_cal_component_has_attendees (priv->comp)) {
		gboolean delay_set = FALSE;
		icalproperty *icalprop;
		icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
		while (icalprop) {
			const char *x_name;

			x_name = icalproperty_get_x_name (icalprop);
			if (!strcmp (x_name, "X-EVOLUTION-OPTIONS-DELAY")) {
				delay_set = TRUE;
				break;
			}

			icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
		}
		if (delay_set)
			return TRUE;
	}

	if (!result) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (
			NULL, 0,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s", (error != NULL) ? error->message :
			_("Could not update object"));
		gtk_dialog_run (GTK_DIALOG(dialog));
		gtk_widget_destroy (dialog);

		if (error)
			g_error_free (error);

		return FALSE;
	} else {
		if (priv->source_client &&
		    !e_source_equal (e_cal_get_source (priv->client),
				     e_cal_get_source (priv->source_client)) &&
		    cal_comp_is_on_server (priv->comp, priv->source_client)) {
			/* Comp found a new home. Remove it from old one. */

			if (e_cal_component_is_instance (priv->comp) || e_cal_component_has_recurrences (priv->comp))
				e_cal_remove_object_with_mod (priv->source_client, orig_uid, NULL,
						CALOBJ_MOD_ALL, NULL);
			else
				e_cal_remove_object (priv->source_client, orig_uid, NULL);

			/* Let priv->source_client point to new home, so we can move it
			 * again this session. */
			g_object_unref (priv->source_client);
			priv->source_client = g_object_ref (priv->client);

			listen_for_changes (editor);
		}

		priv->changed = FALSE;
	}

	return TRUE;
}

static gboolean
save_comp_with_send (CompEditor *editor)
{
	CompEditorPrivate *priv;
	CompEditorFlags flags;
	gboolean send;
	gboolean delegate;

	priv = editor->priv;

	flags = comp_editor_get_flags (editor);
	send = priv->changed && priv->needs_send;
	delegate = flags & COMP_EDITOR_DELEGATE;

	if (delegate) {
		icalcomponent *icalcomp = e_cal_component_get_icalcomponent (priv->comp);
		icalproperty *icalprop;

		icalprop = icalproperty_new_x ("1");
		icalproperty_set_x_name (icalprop, "X-EVOLUTION-DELEGATED");
		icalcomponent_add_property (icalcomp, icalprop);
	}

	if (!save_comp (editor))
		return FALSE;

	if ((delegate && !e_cal_get_save_schedules (priv->client)) || (send && send_component_dialog ((GtkWindow *) editor, priv->client, priv->comp, !priv->existing_org))) {
 		if ((itip_organizer_is_user (priv->comp, priv->client) || itip_sentby_is_user (priv->comp))) {
 			if (e_cal_component_get_vtype (priv->comp) == E_CAL_COMPONENT_JOURNAL)
				return comp_editor_send_comp (editor, E_CAL_COMPONENT_METHOD_PUBLISH);
			else
				return comp_editor_send_comp (editor, E_CAL_COMPONENT_METHOD_REQUEST);
		} else {
			if (!comp_editor_send_comp (editor, E_CAL_COMPONENT_METHOD_REQUEST))
				return FALSE;

			if (delegate)
				return comp_editor_send_comp (editor, E_CAL_COMPONENT_METHOD_REPLY);
		}
 	}

	return TRUE;
}

static void
update_window_border (CompEditor *editor,
                      const gchar *description)
{
	const gchar *icon_name;
	const gchar *format;
	gchar *title;

	if (editor->priv->comp == NULL) {
		title = g_strdup (_("Edit Appointment"));
		icon_name = "x-office-calendar";
		goto exit;

	} else switch (e_cal_component_get_vtype (editor->priv->comp)) {
		case E_CAL_COMPONENT_EVENT:
			if (editor->priv->is_group_item)
				format = _("Meeting - %s");
			else
				format = _("Appointment - %s");
			icon_name = "appointment-new";
			break;

		case E_CAL_COMPONENT_TODO:
			if (editor->priv->is_group_item)
				format = _("Assigned Task - %s");
			else
				format = _("Task - %s");
			icon_name = "stock_task";
			break;

		case E_CAL_COMPONENT_JOURNAL:
			format = _("Memo - %s");
			icon_name = "stock_insert-note";
			break;

		default:
			g_return_if_reached ();
	}

	if (description == NULL || *description == '\0') {
		ECalComponentText text;

		e_cal_component_get_summary (editor->priv->comp, &text);
		description = text.value;
	}

	if (description == NULL || *description == '\0')
		description = _("No Summary");

	title = g_strdup_printf (format, description);

exit:
	gtk_window_set_icon_name (GTK_WINDOW (editor), icon_name);
	gtk_window_set_title (GTK_WINDOW (editor), title);

	g_free (title);
}

static void
action_attach_cb (GtkAction *action,
                  CompEditor *editor)
{
	GPtrArray *file_list;
	gboolean is_inline = FALSE;
	int i;

	file_list = comp_editor_select_file_attachments (editor, &is_inline);

	if (file_list) {
		add_to_bar (editor, file_list, is_inline);

		for (i = 0; i < file_list->len; i++)
			g_free (file_list->pdata[i]);

		g_ptr_array_free (file_list, TRUE);
	}
}

static void
action_close_cb (GtkAction *action,
                 CompEditor *editor)
{
	commit_all_fields (editor);

	if (prompt_and_save_changes (editor, TRUE))
		close_dialog (editor);
}

static void
action_copy_cb (GtkAction *action,
                CompEditor *editor)
{
	GtkWidget *focus;

	focus = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_ENTRY (focus))
		gtk_editable_copy_clipboard (GTK_EDITABLE (focus));

	if (GTK_IS_TEXT_VIEW (focus))
		g_signal_emit_by_name (focus, "copy-clipboard");
}

static void
action_cut_cb (GtkAction *action,
               CompEditor *editor)
{
	GtkWidget *focus;

	focus = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_ENTRY (focus))
		gtk_editable_cut_clipboard (GTK_EDITABLE (focus));

	if (GTK_IS_TEXT_VIEW (focus))
		g_signal_emit_by_name (focus, "cut-clipboard");
}

static void
action_help_cb (GtkAction *action,
                CompEditor *editor)
{
	comp_editor_show_help (editor);
}

static void
action_paste_cb (GtkAction *action,
                 CompEditor *editor)
{
	GtkWidget *focus;

	focus = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_ENTRY (focus))
		gtk_editable_paste_clipboard (GTK_EDITABLE (focus));

	if (GTK_IS_TEXT_VIEW (focus))
		g_signal_emit_by_name (focus, "paste-clipboard");
}

static void
action_print_cb (GtkAction *action,
                 CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;
	ECalComponent *comp;
	GList *l;
	icalcomponent *icalcomp = e_cal_component_get_icalcomponent (priv->comp);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));

	for (l = priv->pages; l != NULL; l = l->next)
		 comp_editor_page_fill_component (l->data, comp);

	print_comp (comp, priv->client, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (comp);
}

static void
action_print_preview_cb (GtkAction *action,
                         CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;
	ECalComponent *comp;
	GList *l;
	icalcomponent *icalcomp = e_cal_component_get_icalcomponent (priv->comp);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));
	for (l = priv->pages; l != NULL; l = l->next)
		 comp_editor_page_fill_component (l->data, comp);
	print_comp (comp, priv->client, TRUE);

	g_object_unref (comp);
}

static void
action_save_cb (GtkAction *action,
                CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;
	ECalComponentText text;
	gboolean delegated = FALSE;
	gboolean read_only, correct = FALSE;
	ECalComponent *comp;

	if (e_attachment_bar_get_download_count (E_ATTACHMENT_BAR (editor->priv->attachment_bar)) ){
		gboolean response = 1;
	/*FIXME: Cannot use mail functions from calendar!!!! */
#if 0
		ECalComponentVType vtype = e_cal_component_get_vtype(editor->priv->comp);

		if (vtype == E_CAL_COMPONENT_EVENT)
			response = em_utils_prompt_user((GtkWindow *)widget,
							 NULL,
							 "calendar:ask-send-event-pending-download",
							  NULL);
		else
			response = em_utils_prompt_user((GtkWindow *)widget,
							 NULL,
							 "calendar:ask-send-task-pending-download",
							  NULL);
#endif
	if (!response)
		return;
	}

	if (!e_cal_is_read_only (priv->client, &read_only, NULL) || read_only) {
		e_error_run ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (editor)), "calendar:prompt-read-only-cal-editor", e_source_peek_name (e_cal_get_source (priv->client)), NULL);
		return;
	}

	commit_all_fields (editor);
	if (e_cal_component_is_instance (priv->comp))
		if (!recur_component_dialog (priv->client, priv->comp, &priv->mod, GTK_WINDOW (editor), delegated))
			return;

	comp = comp_editor_get_current_comp (editor, &correct);
	e_cal_component_get_summary (comp, &text);
	g_object_unref (comp);

	if (!correct)
		return;

	if (!text.value)
		if (!send_component_prompt_subject ((GtkWindow *) editor, priv->client, priv->comp))
			return;
	if (save_comp_with_send (editor))
		close_dialog (editor);

}

static void
action_select_all_cb (GtkAction *action,
                      CompEditor *editor)
{
	GtkWidget *focus;

	focus = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_ENTRY (focus)) {
		gtk_editable_set_position (GTK_EDITABLE (focus), -1);
		gtk_editable_select_region (GTK_EDITABLE (focus), 0, -1);
	}

	if (GTK_IS_TEXT_VIEW (focus))
		g_signal_emit_by_name (focus, "select-all", TRUE);
}

static void
action_view_categories_cb (GtkToggleAction *action,
                           CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_categories != NULL)
		class->show_categories (editor, active);
}

static void
action_view_role_cb (GtkToggleAction *action,
                     CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_role != NULL)
		class->show_role (editor, active);
}

static void
action_view_rsvp_cb (GtkToggleAction *action,
                     CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_rsvp != NULL)
		class->show_rsvp (editor, active);
}

static void
action_view_status_cb (GtkToggleAction *action,
                       CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_status != NULL)
		class->show_status (editor, active);
}

static void
action_view_time_zone_cb (GtkToggleAction *action,
                          CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_time_zone != NULL)
		class->show_time_zone (editor, active);
}

static void
action_view_type_cb (GtkToggleAction *action,
                     CompEditor *editor)
{
	CompEditorClass *class;
	gboolean active;

	class = COMP_EDITOR_GET_CLASS (editor);
	active = gtk_toggle_action_get_active (action);

	if (class->show_type != NULL)
		class->show_type (editor, active);
}

static GtkActionEntry core_entries[] = {

	{ "close",
	  GTK_STOCK_CLOSE,
	  NULL,
	  NULL,
	  N_("Click here to close the current window"),
	  G_CALLBACK (action_close_cb) },

	{ "copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected text to the clipboard"),
	  G_CALLBACK (action_copy_cb) },

	{ "cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut selected text to the clipboard"),
	  G_CALLBACK (action_cut_cb) },

	{ "help",
	  GTK_STOCK_HELP,
	  NULL,
	  NULL,
	  N_("Click here to view help available"),
	  G_CALLBACK (action_help_cb) },

	{ "paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste text from the clipboard"),
	  G_CALLBACK (action_paste_cb) },

	{ "print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (action_print_cb) },

	{ "print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  NULL,
	  G_CALLBACK (action_print_preview_cb) },

	{ "save",
	  GTK_STOCK_SAVE,
	  NULL,
	  NULL,
	  N_("Click here to save the current window"),
	  G_CALLBACK (action_save_cb) },

	{ "select-all",
	  GTK_STOCK_SELECT_ALL,
	  NULL,
	  NULL,
	  N_("Select all text"),
	  G_CALLBACK (action_select_all_cb) },

	/* Menus */

	{ "classification-menu",
	  NULL,
	  N_("_Classification"),
	  NULL,
	  NULL,
	  NULL },

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL },

	{ "help-menu",
	  NULL,
	  N_("_Help"),
	  NULL,
	  NULL,
	  NULL },

	{ "insert-menu",
	  NULL,
	  N_("_Insert"),
	  NULL,
	  NULL,
	  NULL },

	{ "options-menu",
	  NULL,
	  N_("_Options"),
	  NULL,
	  NULL,
	  NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkActionEntry individual_entries[] = {

	{ "attach",
	  "mail-attachment",
	  N_("_Attachment..."),
	  "<Control>m",
	  N_("Click here to attach a file"),
	  G_CALLBACK (action_attach_cb) }
};

static GtkToggleActionEntry individual_toggle_entries[] = {

	{ "view-categories",
	  NULL,
	  N_("_Categories"),
	  NULL,
	  N_("Toggles whether to display categories"),
	  G_CALLBACK (action_view_categories_cb),
	  FALSE },

	{ "view-time-zone",
	  "stock_timezone",
	  N_("Time _Zone"),
	  NULL,
	  N_("Toggles whether the time zone is displayed"),
	  G_CALLBACK (action_view_time_zone_cb),
	  FALSE }
};

static GtkRadioActionEntry classification_radio_entries[] = {

	{ "classify-public",
	  NULL,
	  N_("Pu_blic"),
	  NULL,
	  N_("Classify as public"),
	  E_CAL_COMPONENT_CLASS_PUBLIC },

	{ "classify-private",
	  NULL,
	  N_("_Private"),
	  NULL,
	  N_("Classify as private"),
	  E_CAL_COMPONENT_CLASS_PRIVATE },

	{ "classify-confidential",
	  NULL,
	  N_("_Confidential"),
	  NULL,
	  N_("Classify as confidential"),
	  E_CAL_COMPONENT_CLASS_CONFIDENTIAL }
};

static GtkToggleActionEntry coordinated_toggle_entries[] = {

	{ "view-role",
	  NULL,
	  N_("R_ole Field"),
	  NULL,
	  N_("Toggles whether the Role field is displayed"),
	  G_CALLBACK (action_view_role_cb),
	  FALSE },

	{ "view-rsvp",
	  NULL,
	  N_("_RSVP"),
	  NULL,
	  N_("Toggles whether the RSVP field is displayed"),
	  G_CALLBACK (action_view_rsvp_cb),
	  FALSE },

	{ "view-status",
	  NULL,
	  N_("_Status Field"),
	  NULL,
	  N_("Toggles whether the Status field is displayed"),
	  G_CALLBACK (action_view_status_cb),
	  FALSE },

	{ "view-type",
	  NULL,
	  N_("_Type Field"),
	  NULL,
	  N_("Toggles whether the Attendee Type is displayed"),
	  G_CALLBACK (action_view_type_cb),
	  FALSE }
};

static void
comp_editor_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			comp_editor_set_changed (
				COMP_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_CLIENT:
			comp_editor_set_client (
				COMP_EDITOR (object),
				g_value_get_object (value));
			return;

		case PROP_FLAGS:
			comp_editor_set_flags (
				COMP_EDITOR (object),
				g_value_get_int (value));
			return;

		case PROP_SUMMARY:
			comp_editor_set_summary (
				COMP_EDITOR (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
comp_editor_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHANGED:
			g_value_set_boolean (
				value, comp_editor_get_changed (
				COMP_EDITOR (object)));
			return;

		case PROP_CLIENT:
			g_value_set_object (
				value, comp_editor_get_client (
				COMP_EDITOR (object)));
			return;

		case PROP_FLAGS:
			g_value_set_int (
				value, comp_editor_get_flags (
				COMP_EDITOR (object)));
			return;

		case PROP_SUMMARY:
			g_value_set_string (
				value, comp_editor_get_summary (
				COMP_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
comp_editor_dispose (GObject *object)
{
	CompEditorPrivate *priv;

	priv = COMP_EDITOR_GET_PRIVATE (object);

	if (priv->client) {
		g_object_unref (priv->client);
		priv->client = NULL;
	}

	if (priv->source_client) {
		g_object_unref (priv->source_client);
		priv->source_client = NULL;
	}

	if (priv->view) {
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      object);

		g_object_unref (priv->view);
		priv->view = NULL;
	}

	/* We want to destroy the pages after the widgets get destroyed,
	   since they have lots of signal handlers connected to the widgets
	   with the pages as the data. */
	g_list_foreach (priv->pages, (GFunc) g_object_unref, NULL);
	g_list_free (priv->pages);
	priv->pages = NULL;

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->manager != NULL) {
		g_object_unref (priv->manager);
		priv->manager = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (comp_editor_parent_class)->dispose (object);
}

static void
comp_editor_finalize (GObject *object)
{
	CompEditorPrivate *priv;

	priv = COMP_EDITOR_GET_PRIVATE (object);

	g_free (priv->summary);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (comp_editor_parent_class)->finalize (object);
}

static void
comp_editor_map (GtkWidget *widget)
{
	CompEditor *editor = COMP_EDITOR (widget);
	GConfBridge *bridge = gconf_bridge_get ();
	GtkAction *action;

	/* Give subclasses a chance to construct their pages before
	 * we fiddle with their widgets.  That's why we don't do this
	 * until after object construction. */

	action = comp_editor_get_action (editor, "view-categories");
	gconf_bridge_bind_property (
		bridge, CALENDAR_CONFIG_SHOW_CATEGORIES,
		G_OBJECT (action), "active");

	action = comp_editor_get_action (editor, "view-role");
	gconf_bridge_bind_property (
		bridge, CALENDAR_CONFIG_SHOW_ROLE,
		G_OBJECT (action), "active");

	action = comp_editor_get_action (editor, "view-rsvp");
	gconf_bridge_bind_property (
		bridge, CALENDAR_CONFIG_SHOW_RSVP,
		G_OBJECT (action), "active");

	action = comp_editor_get_action (editor, "view-status");
	gconf_bridge_bind_property (
		bridge, CALENDAR_CONFIG_SHOW_STATUS,
		G_OBJECT (action), "active");

	action = comp_editor_get_action (editor, "view-time-zone");
	gconf_bridge_bind_property (
		bridge, CALENDAR_CONFIG_SHOW_TIMEZONE,
		G_OBJECT (action), "active");

	action = comp_editor_get_action (editor, "view-type");
	gconf_bridge_bind_property (
		bridge, CALENDAR_CONFIG_SHOW_TYPE,
		G_OBJECT (action), "active");

	/* Chain up to parent's map() method. */
	GTK_WIDGET_CLASS (comp_editor_parent_class)->map (widget);
}

static void
comp_editor_class_init (CompEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	int i;

	for (i = 0; i < G_N_ELEMENTS (drag_info); i++)
		drag_info[i].atom = gdk_atom_intern(drag_info[i].target, FALSE);

	g_type_class_add_private (class, sizeof (CompEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = comp_editor_set_property;
	object_class->get_property = comp_editor_get_property;
	object_class->dispose = comp_editor_dispose;
	object_class->finalize = comp_editor_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = comp_editor_map;

	class->help_section = "usage-calendar";
	class->edit_comp = real_edit_comp;
	class->send_comp = real_send_comp;
	class->object_created = NULL;

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			NULL,
			NULL,
			E_TYPE_CAL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* FIXME: Use a proper flags type instead of int. */
	g_object_class_install_property (
		object_class,
		PROP_FLAGS,
		g_param_spec_int (
			"flags",
			NULL,
			NULL,
			G_MININT,
			G_MAXINT,
			0,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SUMMARY,
		g_param_spec_string (
			"summary",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	comp_editor_signals[OBJECT_CREATED] =
		g_signal_new ("object_created",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CompEditorClass, object_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
comp_editor_init (CompEditor *editor)
{
	CompEditorPrivate *priv;
	GtkActionGroup *action_group;
	GtkAction *action;
	GError *error = NULL;

	editor->priv = priv = COMP_EDITOR_GET_PRIVATE (editor);

	priv->pages = NULL;
	priv->changed = FALSE;
	priv->needs_send = FALSE;
	priv->mod = CALOBJ_MOD_ALL;
 	priv->existing_org = FALSE;
 	priv->user_org = FALSE;
 	priv->warned = FALSE;
	priv->is_group_item = FALSE;

	priv->attachment_bar = e_attachment_bar_new (NULL);
	priv->manager = gtk_ui_manager_new ();

	action_group = gtk_action_group_new ("core");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, core_entries,
		G_N_ELEMENTS (core_entries), editor);
	gtk_ui_manager_insert_action_group (
		priv->manager, action_group, 0);
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("individual");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, individual_entries,
		G_N_ELEMENTS (individual_entries), editor);
	gtk_action_group_add_toggle_actions (
		action_group, individual_toggle_entries,
		G_N_ELEMENTS (individual_toggle_entries), editor);
	gtk_action_group_add_radio_actions (
		action_group, classification_radio_entries,
		G_N_ELEMENTS (classification_radio_entries),
		E_CAL_COMPONENT_CLASS_PUBLIC,
		NULL, NULL);  /* no callback */
	action = e_attachment_bar_recent_action_new (
		E_ATTACHMENT_BAR (priv->attachment_bar),
		"attach-recent", _("Recent Docu_ments"));
	gtk_action_group_add_action (action_group, action);
	gtk_ui_manager_insert_action_group (
		priv->manager, action_group, 0);
	g_object_unref (action_group);

	action_group = gtk_action_group_new ("coordinated");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (
		action_group, coordinated_toggle_entries,
		G_N_ELEMENTS (coordinated_toggle_entries), editor);
	gtk_ui_manager_insert_action_group (
		priv->manager, action_group, 0);
	g_object_unref (action_group);

	/* Fine Tuning */

	action = comp_editor_get_action (editor, "attach");
	g_object_set (G_OBJECT (action), "short-label", _("Attach"), NULL);

	/* Desensitize the "save" action. */
	action = comp_editor_get_action (editor, "save");
	gtk_action_set_sensitive (action, FALSE);

	gtk_ui_manager_add_ui_from_string (priv->manager, ui, -1, &error);
	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	setup_widgets (editor);

	/* DND support */
	gtk_drag_dest_set (GTK_WIDGET (editor), GTK_DEST_DEFAULT_ALL,  drop_types, num_drop_types, GDK_ACTION_COPY|GDK_ACTION_ASK|GDK_ACTION_MOVE);
	g_signal_connect(editor, "drag_data_received", G_CALLBACK (drag_data_received), NULL);
	g_signal_connect(editor, "drag-motion", G_CALLBACK(drag_motion), editor);

	gtk_window_set_type_hint (GTK_WINDOW (editor), GDK_WINDOW_TYPE_HINT_NORMAL);
}

static gboolean
prompt_and_save_changes (CompEditor *editor, gboolean send)
{
	CompEditorPrivate *priv;
	gboolean read_only, correct = FALSE;
	ECalComponent *comp;
	ECalComponentText text;

	priv = editor->priv;

	if (!priv->changed)
		return TRUE;

	switch (save_component_dialog (GTK_WINDOW(editor), priv->comp)) {
	case GTK_RESPONSE_YES: /* Save */
		if (!e_cal_is_read_only (priv->client, &read_only, NULL) || read_only) {
			e_error_run ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (editor)), "calendar:prompt-read-only-cal-editor", e_source_peek_name (e_cal_get_source (priv->client)), NULL);
			/* don't discard changes when selected readonly calendar */
			return FALSE;
		}

		comp = comp_editor_get_current_comp (editor, &correct);
		e_cal_component_get_summary (comp, &text);
		g_object_unref (comp);

		if (!correct)
			return FALSE;

		if (!text.value)
			if (!send_component_prompt_subject ((GtkWindow *) editor, priv->client, priv->comp))
				return FALSE;

		if (e_cal_component_is_instance (priv->comp))
			if (!recur_component_dialog (priv->client, priv->comp, &priv->mod, GTK_WINDOW (editor), FALSE))
				return FALSE;

		if (send && save_comp_with_send (editor))
			return TRUE;
		else if (!send && save_comp (editor))
			return TRUE;
		else
			return FALSE;
	case GTK_RESPONSE_NO: /* Discard */
		return TRUE;
	case GTK_RESPONSE_CANCEL: /* Cancel */
	default:
		return FALSE;
	}
}

static int
delete_event_cb (GtkWidget *widget,
                 GdkEvent *event,
                 CompEditor *editor)
{
	commit_all_fields (editor);

	if (prompt_and_save_changes (editor, TRUE))
		close_dialog (editor);

	return TRUE;
}

static void
attachment_bar_changed_cb (EAttachmentBar *bar,
                           CompEditor *editor)
{
	guint attachment_num = e_attachment_bar_get_num_attachments (
		E_ATTACHMENT_BAR (editor->priv->attachment_bar));
	if (attachment_num) {
		gchar *num_text = g_strdup_printf (
			ngettext ("<b>%d</b> Attachment", "<b>%d</b> Attachments", attachment_num),
			attachment_num);
		gtk_label_set_markup (GTK_LABEL (editor->priv->attachment_expander_num),
				      num_text);
		g_free (num_text);

		gtk_widget_show (editor->priv->attachment_expander_icon);
		e_expander_set_expanded(E_EXPANDER(editor->priv->attachment_expander),TRUE);

	} else {
		gtk_label_set_text (GTK_LABEL (editor->priv->attachment_expander_num), "");
		gtk_widget_hide (editor->priv->attachment_expander_icon);
		e_expander_set_expanded(E_EXPANDER(editor->priv->attachment_expander),FALSE);
	}


	/* Mark the editor as changed so it prompts about unsaved
           changes on close */
	comp_editor_set_changed (editor, TRUE);

}

static void
attachment_expander_activate_cb (EExpander *expander,
				 void *data)
{
	CompEditor *editor = COMP_EDITOR (data);
	gboolean show = e_expander_get_expanded (expander);

	/* Update the expander label */
	if (show)
		gtk_label_set_text_with_mnemonic (GTK_LABEL (editor->priv->attachment_expander_label),
					          _("Hide Attachment _Bar"));
	else
		gtk_label_set_text_with_mnemonic (GTK_LABEL (editor->priv->attachment_expander_label),
						  _("Show Attachment _Bar"));
}

static gboolean
open_attachment (EAttachmentBar *bar, CompEditor *editor)
{
	GnomeIconList *icon_list;
	GList *p;
	int num;
	char *attach_file_url;
	GError *error = NULL;

	if (E_IS_ATTACHMENT_BAR (bar)) {
		icon_list = GNOME_ICON_LIST (bar);
		p = gnome_icon_list_get_selection (icon_list);
		if (p) {
			EAttachment *attachment;
			GSList *list;
			const char *comp_uid = NULL;
			char *filename = NULL;
			const char *local_store = e_cal_get_local_attachment_store (editor->priv->client);

			e_cal_component_get_uid (editor->priv->comp, &comp_uid);
			num = GPOINTER_TO_INT (p->data);
			list = e_attachment_bar_get_attachment (bar, num);
			attachment = list->data;
			g_slist_free (list);

			filename = g_strdup_printf ("%s-%s",
						    comp_uid,
						    camel_mime_part_get_filename(attachment->body));

			attach_file_url = g_build_path ("/", local_store, filename, NULL);

			/* launch the url now */
			/* TODO should send GError and handle error conditions
			 * here */
			gnome_url_show (attach_file_url, &error);
			if (error)
				g_message ("DEBUG: gnome_url_show(%s) failed\n", attach_file_url);

			g_free (filename);
			g_free (attach_file_url); }
		return TRUE;
	} else
		return FALSE;
}

static	gboolean
attachment_bar_icon_clicked_cb (EAttachmentBar *bar, GdkEvent *event, CompEditor *editor)
{
	if (E_IS_ATTACHMENT_BAR (bar) && event->type == GDK_2BUTTON_PRESS)
		if (open_attachment (bar, editor))
				return TRUE;
	return FALSE;
}

/* Callbacks.  */

static void
cab_open(EPopup *ep, EPopupItem *item, void *data)
{
	EAttachmentBar *bar = data;
	CompEditor *editor = COMP_EDITOR (gtk_widget_get_toplevel (GTK_WIDGET (bar)));

	if (!open_attachment (bar, editor))
		g_message ("\n Open failed");
}

static void
cab_add(EPopup *ep, EPopupItem *item, void *data)
{
	EAttachmentBar *bar = data;
        CompEditor *editor = COMP_EDITOR (gtk_widget_get_toplevel (GTK_WIDGET (bar)));
	GPtrArray *file_list;
	gboolean is_inline = FALSE;
	int i;

	file_list = comp_editor_select_file_attachments (editor, &is_inline);
	/*TODO add a good implementation here */
	if (!file_list)
		return;
	for (i = 0; i < file_list->len; i++) {
		CamelURL *url;

		url = camel_url_new (file_list->pdata[i], NULL);
		if (url == NULL)
			continue;

		if (!g_ascii_strcasecmp (url->protocol, "file"))
			 e_attachment_bar_attach (bar, url->path, is_inline ? "inline" : "attachment");
		else
			 e_attachment_bar_attach_remote_file (bar, file_list->pdata[i], is_inline ? "inline" : "attachment");
		g_free (file_list->pdata[i]);
		camel_url_free (url);
	}

	g_ptr_array_free (file_list, TRUE);
}

static void
cab_properties(EPopup *ep, EPopupItem *item, void *data)
{
	EAttachmentBar *bar = data;

	e_attachment_bar_edit_selected(bar);
}

static void
cab_remove(EPopup *ep, EPopupItem *item, void *data)
{
	EAttachmentBar *bar = data;

	e_attachment_bar_remove_selected(bar);
}

/* Popup menu handling.  */
static EPopupItem cab_popups[] = {
	{ E_POPUP_ITEM, "10.attach", N_("_Open"), cab_open, NULL, GTK_STOCK_OPEN, E_CAL_POPUP_ATTACHMENTS_ONE},
	{ E_POPUP_ITEM, "20.attach", N_("_Remove"), cab_remove, NULL, GTK_STOCK_REMOVE, E_CAL_POPUP_ATTACHMENTS_MANY | E_CAL_POPUP_ATTACHMENTS_MODIFY },
	{ E_POPUP_ITEM, "30.attach", N_("_Properties"), cab_properties, NULL, GTK_STOCK_PROPERTIES, E_CAL_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_BAR, "40.attach.00", NULL, NULL, NULL, NULL, E_CAL_POPUP_ATTACHMENTS_MANY|E_CAL_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_ITEM, "40.attach.01", N_("_Add attachment..."), cab_add, NULL, GTK_STOCK_ADD, E_CAL_POPUP_ATTACHMENTS_MODIFY},
};

static void
cab_popup_position(GtkMenu *menu, int *x, int *y, gboolean *push_in, gpointer user_data)
{
	EAttachmentBar *bar = user_data;
	GnomeIconList *icon_list = user_data;
	GList *selection;
	GnomeCanvasPixbuf *image;

	gdk_window_get_origin (((GtkWidget*) bar)->window, x, y);

	selection = gnome_icon_list_get_selection (icon_list);
	if (selection == NULL)
		return;

	image = gnome_icon_list_get_icon_pixbuf_item (icon_list, GPOINTER_TO_INT(selection->data));
	if (image == NULL)
		return;

	/* Put menu to the center of icon. */
	*x += (int)(image->item.x1 + image->item.x2) / 2;
	*y += (int)(image->item.y1 + image->item.y2) / 2;
}

static void
cab_popups_free(EPopup *ep, GSList *l, void *data)
{
	g_slist_free(l);
}

/* if id != -1, then use it as an index for target of the popup */
static void
cab_popup(EAttachmentBar *bar, GdkEventButton *event, int id)
{
	GSList *attachments = NULL, *menus = NULL;
	int i;
	ECalPopup *ecp;
	ECalPopupTargetAttachments *t;
	GtkMenu *menu;
	CompEditor *editor = COMP_EDITOR (gtk_widget_get_toplevel (GTK_WIDGET (bar)));

        attachments = e_attachment_bar_get_attachment(bar, id);

	for (i=0;i<sizeof(cab_popups)/sizeof(cab_popups[0]);i++)
		menus = g_slist_prepend(menus, &cab_popups[i]);

	/** @HookPoint-ECalPopup: Calendar Attachment Bar Context Menu
	 * @Id: org.gnome.evolution.calendar.attachmentbar.popup
	 * @Class: org.gnome.evolution.mail.popup:1.0
	 * @Target: ECalPopupTargetAttachments
	 *
	 * This is the context menu on the calendar attachment bar.
	 */
	ecp = e_cal_popup_new("org.gnome.evolution.calendar.attachmentbar.popup");
	e_popup_add_items((EPopup *)ecp, menus, NULL, cab_popups_free, bar);
	t = e_cal_popup_target_new_attachments(ecp, editor, attachments);
	t->target.widget = (GtkWidget *)bar;
	menu = e_popup_create_menu_once((EPopup *)ecp, (EPopupTarget *)t, 0);

	if (event == NULL)
		gtk_menu_popup(menu, NULL, NULL, cab_popup_position, bar, 0, gtk_get_current_event_time());
	else
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
}

/* GtkWidget methods.  */

static gboolean
popup_menu_event (EAttachmentBar *bar)
{
	cab_popup (bar, NULL, -1);

	return TRUE;
}


static int
button_press_event (EAttachmentBar *bar,
                    GdkEventButton *event)
{
	GnomeIconList *icon_list = GNOME_ICON_LIST (bar);
	int icon_number = -1;

	if (event->button != 3)
		return FALSE;

	if (!gnome_icon_list_get_selection (icon_list)) {
		icon_number = gnome_icon_list_get_icon_at (icon_list, event->x, event->y);
		if (icon_number >= 0) {
			gnome_icon_list_unselect_all(icon_list);
			gnome_icon_list_select_icon (icon_list, icon_number);
		}
	}

	cab_popup(bar, event, icon_number);

	return TRUE;
}

static gint
key_press_event (EAttachmentBar *bar,
                 GdkEventKey *event)
{
	if (event->keyval == GDK_Delete) {
                e_attachment_bar_remove_selected (bar);
                return TRUE;
        }

        return FALSE;
}

static gint
editor_key_press_event (CompEditor *editor,
                        GdkEventKey *event)
{
        if (event->keyval == GDK_Escape) {
		commit_all_fields (editor);

		if (prompt_and_save_changes (editor, TRUE))
			close_dialog (editor);

                return TRUE;
        }

        return FALSE;
}

/* Menu callbacks */

static void
setup_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	GtkWidget *expander_hbox;
	GtkWidget *widget;
	GtkWidget *vbox;

	priv = editor->priv;

	/* Useful vbox */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (editor), vbox);
	gtk_widget_show (vbox);

	/* Main Menu */
	widget = comp_editor_get_managed_widget (editor, "/main-menu");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Main Toolbar */
	widget = comp_editor_get_managed_widget (editor, "/main-toolbar");
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	/* Notebook */
	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);
	priv->notebook = GTK_NOTEBOOK (widget);

	g_signal_connect (editor, "delete_event", G_CALLBACK (delete_event_cb), editor);
	g_signal_connect (editor, "key_press_event", G_CALLBACK (editor_key_press_event), editor);

	/*Attachments */
	priv->attachment_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->attachment_scrolled_window),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->attachment_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	g_signal_connect (priv->attachment_bar, "button_press_event", G_CALLBACK (button_press_event), NULL);
        g_signal_connect (priv->attachment_bar, "key_press_event", G_CALLBACK (key_press_event), NULL);
        g_signal_connect (priv->attachment_bar, "popup-menu", G_CALLBACK (popup_menu_event), NULL);

	GTK_WIDGET_SET_FLAGS (priv->attachment_bar, GTK_CAN_FOCUS);
	gtk_container_add (GTK_CONTAINER (priv->attachment_scrolled_window),
			   priv->attachment_bar);
	gtk_widget_show (priv->attachment_bar);
	g_signal_connect (priv->attachment_bar, "changed",
			  G_CALLBACK (attachment_bar_changed_cb), editor);
	g_signal_connect (GNOME_ICON_LIST (priv->attachment_bar), "event",
			  G_CALLBACK (attachment_bar_icon_clicked_cb), editor);
	priv->attachment_expander_label =
		gtk_label_new_with_mnemonic (_("Show Attachment _Bar"));
	priv->attachment_expander_num = gtk_label_new ("");
	gtk_label_set_use_markup (GTK_LABEL (priv->attachment_expander_num), TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->attachment_expander_label), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (priv->attachment_expander_num), 1.0, 0.5);
	expander_hbox = gtk_hbox_new (FALSE, 0);

	priv->attachment_expander_icon = gtk_image_new_from_icon_name ("mail-attachment", GTK_ICON_SIZE_MENU);
	gtk_misc_set_alignment (GTK_MISC (priv->attachment_expander_icon), 1, 0.5);
	gtk_widget_set_size_request (priv->attachment_expander_icon, 100, -1);

	gtk_box_pack_start (GTK_BOX (expander_hbox), priv->attachment_expander_label,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (expander_hbox), priv->attachment_expander_icon,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (expander_hbox), priv->attachment_expander_num,
			    TRUE, TRUE, 0);
	gtk_widget_show_all (expander_hbox);
	gtk_widget_hide (priv->attachment_expander_icon);

	priv->attachment_expander = e_expander_new ("");
	e_expander_set_label_widget (E_EXPANDER (priv->attachment_expander), expander_hbox);
	atk_object_set_name (gtk_widget_get_accessible (priv->attachment_expander), _("Show Attachments"));
	atk_object_set_description (gtk_widget_get_accessible (priv->attachment_expander), _("Press space key to toggle attachment bar"));
	gtk_container_add (GTK_CONTAINER (priv->attachment_expander), priv->attachment_scrolled_window);

	gtk_box_pack_start (GTK_BOX (vbox), priv->attachment_expander, FALSE, FALSE, 4);
	gtk_widget_show (priv->attachment_expander);
	e_expander_set_expanded (E_EXPANDER (priv->attachment_expander), FALSE);
	g_signal_connect_after (priv->attachment_expander, "activate",
				G_CALLBACK (attachment_expander_activate_cb), editor);
}


static void
comp_editor_show_help (CompEditor *editor)
{
	CompEditorClass *class;

	class = COMP_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class->help_section != NULL);

	e_display_help (GTK_WINDOW (editor), class->help_section);
}


/* Closes the dialog box and emits the appropriate signals */
static void
close_dialog (CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;

	/* FIXME Unfortunately we do this here because otherwise corba
	   calls happen during destruction and we might get a change
	   notification back when we are in an inconsistent state */
	if (priv->view)
		g_signal_handlers_disconnect_matched (G_OBJECT (priv->view),
						      G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	gtk_widget_destroy (GTK_WIDGET (editor));
}



void
comp_editor_set_existing_org (CompEditor *editor, gboolean existing_org)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->existing_org = existing_org;
}

gboolean
comp_editor_get_existing_org (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->existing_org;
}

void
comp_editor_set_user_org (CompEditor *editor,
                          gboolean user_org)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->user_org = user_org;
}

gboolean
comp_editor_get_user_org (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->user_org;
}

void
comp_editor_set_group_item (CompEditor *editor,
                            gboolean group_item)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->is_group_item = group_item;
}

gboolean
comp_editor_get_group_item (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->is_group_item;
}

void
comp_editor_set_classification (CompEditor *editor,
                                ECalComponentClassification classification)
{
	GtkAction *action;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	switch (classification) {
		case E_CAL_COMPONENT_CLASS_PUBLIC:
		case E_CAL_COMPONENT_CLASS_PRIVATE:
		case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
			break;
		default:
			classification = E_CAL_COMPONENT_CLASS_PUBLIC;
			break;
	}

	action = comp_editor_get_action (editor, "classify-public");
	gtk_radio_action_set_current_value (
		GTK_RADIO_ACTION (action), classification);
}

ECalComponentClassification
comp_editor_get_classification (CompEditor *editor)
{
	GtkAction *action;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), 0);

	action = comp_editor_get_action (editor, "classify-public");
	return gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
}

void
comp_editor_set_summary (CompEditor *editor,
                         const gchar *summary)
{
	gboolean show_warning;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	g_free (editor->priv->summary);
	editor->priv->summary = g_strdup (summary);

	show_warning =
		!editor->priv->warned &&
		editor->priv->existing_org &&
		!editor->priv->user_org;

	if (show_warning) {
		e_notice (
			editor->priv->notebook, GTK_MESSAGE_INFO,
			_("Changes made to this item may be "
			  "discarded if an update arrives"));
		editor->priv->warned = TRUE;
	}

	update_window_border (editor, summary);

	g_object_notify (G_OBJECT (editor), "summary");
}

const gchar *
comp_editor_get_summary (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->summary;
}

/**
 * comp_editor_set_changed:
 * @editor: A component editor
 * @changed: Value to set the changed state to
 *
 * Set the dialog changed state to the given value
 **/
void
comp_editor_set_changed (CompEditor *editor,
                         gboolean changed)
{
	GtkAction *action;
	gboolean show_warning;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->changed = changed;

	action = comp_editor_get_action (editor, "save");
	g_return_if_fail (action != NULL);
	gtk_action_set_sensitive (action, changed);

	show_warning =
		changed && !editor->priv->warned &&
		editor->priv->existing_org && !editor->priv->user_org;

	if (show_warning) {
		e_notice (
			editor->priv->notebook, GTK_MESSAGE_INFO,
			_("Changes made to this item may be "
			  "discarded if an update arrives"));
		editor->priv->warned = TRUE;
	}

	g_object_notify (G_OBJECT (editor), "changed");
}

/**
 * comp_editor_get_changed:
 * @editor: A component editor
 *
 * Gets the changed state of the dialog
 *
 * Return value: A boolean indicating if the dialog is in a changed
 * state
 **/
gboolean
comp_editor_get_changed (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->changed;
}

void
comp_editor_set_flags (CompEditor *editor,
                       CompEditorFlags flags)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->flags = flags;

	g_object_notify (G_OBJECT (editor), "flags");
}


CompEditorFlags
comp_editor_get_flags (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->flags;
}

GtkUIManager *
comp_editor_get_ui_manager (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->manager;
}

GtkAction *
comp_editor_get_action (CompEditor *editor,
                        const gchar *action_name)
{
	GtkAction *action = NULL;
	GList *iter;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	iter = gtk_ui_manager_get_action_groups (editor->priv->manager);
	while (iter != NULL && action == NULL) {
		GtkActionGroup *action_group = iter->data;

		action = gtk_action_group_get_action (
			action_group, action_name);
		iter = g_list_next (iter);
	}
	g_return_val_if_fail (action != NULL, NULL);

	return action;
}

GtkActionGroup *
comp_editor_get_action_group (CompEditor *editor,
                              const gchar *group_name)
{
	GList *iter;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	iter = gtk_ui_manager_get_action_groups (editor->priv->manager);
	while (iter != NULL) {
		GtkActionGroup *action_group = iter->data;
		const gchar *name;

		name = gtk_action_group_get_name (action_group);
		if (strcmp (name, group_name) == 0)
			return action_group;
		iter = g_list_next (iter);
	}

	g_return_val_if_reached (NULL);
}

GtkWidget *
comp_editor_get_managed_widget (CompEditor *editor,
                                const gchar *widget_path)
{
	GtkUIManager *manager;
	GtkWidget *widget;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);

	manager = comp_editor_get_ui_manager (editor);
	widget = gtk_ui_manager_get_widget (manager, widget_path);
	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

/**
 * comp_editor_set_needs_send:
 * @editor: A component editor
 * @needs_send: Value to set the needs send state to
 *
 * Set the dialog needs send state to the given value
 **/
void
comp_editor_set_needs_send (CompEditor *editor,
                            gboolean needs_send)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));

	editor->priv->needs_send = needs_send;
}

/**
 * comp_editor_get_needs_send:
 * @editor: A component editor
 *
 * Gets the needs send state of the dialog
 *
 * Return value: A boolean indicating if the dialog is in a needs send
 * state
 **/
gboolean
comp_editor_get_needs_send (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	return editor->priv->needs_send;
}

static void
page_mapped_cb (GtkWidget *page_widget,
                CompEditorPage *page)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (page_widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	if (page->accel_group) {
		gtk_window_add_accel_group (GTK_WINDOW (toplevel),
					    page->accel_group);
	}
}

static void
page_unmapped_cb (GtkWidget *page_widget,
                  CompEditorPage *page)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (page_widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	if (page->accel_group) {
		gtk_window_remove_accel_group (GTK_WINDOW (toplevel),
					       page->accel_group);
	}
}

/**
 * comp_editor_append_page:
 * @editor: A component editor
 * @page: A component editor page
 * @label: Label of the page. Should be NULL if add is FALSE.
 * @add: Add's the page into the notebook if TRUE
 *
 * Appends a page to the notebook if add is TRUE else
 * just adds it to the list of pages.
 **/
void
comp_editor_append_page (CompEditor *editor,
			 CompEditorPage *page,
			 const char *label,
			 gboolean add)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	GtkWidget *label_widget = NULL;
	gboolean is_first_page;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	g_object_ref (page);

	/* If we are editing something, fill the widgets with current info */
	if (priv->comp != NULL) {
		ECalComponent *comp;

		comp = comp_editor_get_current_comp (editor, NULL);
		comp_editor_page_fill_widgets (page, comp);
		g_object_unref (comp);
	}

	page_widget = comp_editor_page_get_widget (page);
	g_return_if_fail (page_widget != NULL);

	if (label)
		label_widget = gtk_label_new_with_mnemonic (label);

	is_first_page = (priv->pages == NULL);

	priv->pages = g_list_append (priv->pages, page);

	if (add)
		gtk_notebook_append_page (priv->notebook, page_widget, label_widget);

	/* Listen for things happening on the page */
	g_signal_connect_swapped (
		page, "dates_changed",
		G_CALLBACK (page_dates_changed_cb), editor);

	/* Listen for when the page is mapped/unmapped so we can
	   install/uninstall the appropriate GtkAccelGroup. */
	g_signal_connect (
		page_widget, "map",
		G_CALLBACK (page_mapped_cb), page);
	g_signal_connect(
		page_widget, "unmap",
		G_CALLBACK (page_unmapped_cb), page);

	/* The first page is the main page of the editor, so we ask it to focus
	 * its main widget.
	 */
	if (is_first_page)
		comp_editor_page_focus_main_widget (page);
}

/**
 * comp_editor_remove_page:
 * @editor: A component editor
 * @page: A component editor page
 *
 * Removes the page from the component editor
 **/
void
comp_editor_remove_page (CompEditor *editor, CompEditorPage *page)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	gint page_num;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	page_widget = comp_editor_page_get_widget (page);
	page_num = gtk_notebook_page_num (priv->notebook, page_widget);
	if (page_num == -1)
		return;

	/* Disconnect all the signals added in append_page(). */
	g_signal_handlers_disconnect_matched (page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	g_signal_handlers_disconnect_matched (page_widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, page);

	gtk_notebook_remove_page (priv->notebook, page_num);

	priv->pages = g_list_remove (priv->pages, page);
	g_object_unref (page);
}

/**
 * comp_editor_show_page:
 * @editor:
 * @page:
 *
 *
 **/
void
comp_editor_show_page (CompEditor *editor, CompEditorPage *page)
{
	CompEditorPrivate *priv;
	GtkWidget *page_widget;
	gint page_num;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (IS_COMP_EDITOR_PAGE (page));

	priv = editor->priv;

	page_widget = comp_editor_page_get_widget (page);
	page_num = gtk_notebook_page_num (priv->notebook, page_widget);
	gtk_notebook_set_current_page (priv->notebook, page_num);
}

/**
 * comp_editor_set_client:
 * @editor: A component editor
 * @client: The calendar client to use
 *
 * Sets the calendar client used by the editor to update components
 **/
void
comp_editor_set_client (CompEditor *editor,
                        ECal *client)
{
	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (client == NULL || E_IS_CAL (client));

	if (client != NULL)
		g_object_ref (client);

	if (editor->priv->client != NULL)
		g_object_unref (editor->priv->client);

	editor->priv->client = client;

	if (editor->priv->source_client == NULL && client != NULL)
		editor->priv->source_client = g_object_ref (client);

	g_object_notify (G_OBJECT (editor), "client");
}

/**
 * comp_editor_get_client:
 * @editor: A component editor
 *
 * Returns the calendar client of the editor
 *
 * Return value: The calendar client of the editor
 **/
ECal *
comp_editor_get_client (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->client;
}

static void
set_attachment_list (CompEditor *editor, GSList *attach_list)
{
	GSList *p = NULL;
	const char *comp_uid= NULL;

	e_cal_component_get_uid (editor->priv->comp, &comp_uid);

	if (e_attachment_bar_get_num_attachments (E_ATTACHMENT_BAR (editor->priv->attachment_bar))) {
		/* To prevent repopulating the
		 * bar due to redraw functions in fill_widget.
		 * Assumes it can be set only once.
		 */
		return;
	}

	for (p = attach_list; p != NULL; p = p->next) {
		char *attach_filename;
		CamelMimePart *part;
		CamelDataWrapper *wrapper;
		CamelStream *stream;
		struct stat statbuf;
		char *mime_type, *file_name;
		char *ptr;

		attach_filename = (char *) p->data;
		/* should we assert if g_str_has_prefix (attach_filename, "file://"))
		 * here
		*/
		/* get url sans protocol and add it to the bar.
		 * how to set the filename properly */
		file_name = g_filename_from_uri (attach_filename, NULL, NULL);
		if (!file_name)
			continue;

		if (g_stat (file_name, &statbuf) < 0) {
			g_warning ("Cannot attach file %s: %s", file_name, g_strerror (errno));
			g_free (file_name);
			continue;
		}

		/* return if it's not a regular file */
		if (!S_ISREG (statbuf.st_mode)) {
			g_warning ("Cannot attach file %s: not a regular file", file_name);
			g_free (file_name);
			return;
		}

		stream = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0);
		if (!stream) {
			g_warning ("Cannot attach file %s: %s", file_name, g_strerror (errno));
			g_free (file_name);
			return;
		}

		mime_type = e_util_guess_mime_type (file_name);
		if (mime_type) {
			if (!g_ascii_strcasecmp (mime_type, "message/rfc822")) {
				wrapper = (CamelDataWrapper *) camel_mime_message_new ();
			} else {
				wrapper = camel_data_wrapper_new ();
			}

			camel_data_wrapper_construct_from_stream (wrapper, stream);
			camel_data_wrapper_set_mime_type (wrapper, mime_type);
			g_free (mime_type);
		} else {
			wrapper = camel_data_wrapper_new ();
			camel_data_wrapper_construct_from_stream (wrapper, stream);
			camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
		}

		camel_object_unref (stream);

		part = camel_mime_part_new ();
		camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
		camel_object_unref (wrapper);

		camel_mime_part_set_disposition (part, "attachment");

		ptr = strstr (file_name, comp_uid);
		if (ptr) {
			ptr += strlen(comp_uid);
			if (*ptr++ == '-')
				camel_mime_part_set_filename (part, ptr);
		}
		g_free (file_name);

		e_attachment_bar_attach_mime_part ((EAttachmentBar *) editor->priv->attachment_bar, part);
		e_expander_set_expanded (E_EXPANDER (editor->priv->attachment_expander), TRUE);

		camel_object_unref (part);
	}
}

static void
fill_widgets (CompEditor *editor)
{
	CompEditorPrivate *priv;
	GList *l;

	priv = editor->priv;

	/*Check if attachments are available here and set them*/
	if (e_cal_component_has_attachments (priv->comp)) {
		GSList *attachment_list = NULL;
		e_cal_component_get_attachment_list (priv->comp, &attachment_list);
		set_attachment_list (editor, attachment_list);
		g_slist_foreach (attachment_list, (GFunc)g_free, NULL);
		g_slist_free (attachment_list);
	}

	for (l = priv->pages; l != NULL; l = l->next)
		comp_editor_page_fill_widgets (l->data, priv->comp);
}

static void
real_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	CompEditorPrivate *priv;
	const char *uid;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	if (priv->comp) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (comp)
		priv->comp = e_cal_component_clone (comp);

 	priv->existing_org = e_cal_component_has_organizer (comp);
 	priv->user_org = (itip_organizer_is_user (comp, priv->client) || itip_sentby_is_user (comp));
 	priv->warned = FALSE;

	update_window_border (editor, NULL);
	e_cal_component_get_uid (comp, &uid);

	fill_widgets (editor);

	priv->changed =FALSE;

	listen_for_changes (editor);
}

/* TODO These functions should be available in e-cal-component.c */
static void
set_attendees_for_delegation (ECalComponent *comp, const char *address, ECalComponentItipMethod method)
{
	icalproperty *prop;
	icalparameter *param;
	icalcomponent *icalcomp;

	icalcomp = e_cal_component_get_icalcomponent (comp);

	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
			prop;
			prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const char *attendee = icalproperty_get_attendee (prop);
		const char *delfrom;

		param = icalproperty_get_first_parameter(prop, ICAL_DELEGATEDFROM_PARAMETER);
		delfrom = icalparameter_get_delegatedfrom (param);
		if (!(g_str_equal (itip_strip_mailto (attendee), address) ||
				((delfrom && *delfrom) &&
				 g_str_equal (itip_strip_mailto (delfrom), address)))) {
			icalcomponent_remove_property (icalcomp, prop);
		}

	}

}

static void
get_users_from_memo_comp (ECalComponent *comp, GList **users)
{
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	const char *attendees = NULL;
	char **emails, **iter;

	icalcomp = e_cal_component_get_icalcomponent (comp);

	for (icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY); icalprop;
			icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {
		if (g_str_equal (icalproperty_get_x_name (icalprop), "X-EVOLUTION-RECIPIENTS")) {
			break;
		}
	}

	if (icalprop)	 {
		attendees = icalproperty_get_x (icalprop);
		emails = g_strsplit (attendees, ";", -1);

		iter = emails;
		while (*iter) {
			*users = g_list_append (*users, g_strdup (*iter));
			iter++;
		}
		g_strfreev (emails);
	}
}

static gboolean
real_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	CompEditorPrivate *priv;
	CompEditorFlags flags;
	ECalComponent *send_comp = NULL;
	char *address = NULL;
	GList *users = NULL;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	priv = editor->priv;
	flags = comp_editor_get_flags (editor);

	if (priv->mod == CALOBJ_MOD_ALL && e_cal_component_is_instance (priv->comp)) {
		/* Ensure we send the master object, not the instance only */
		icalcomponent *icalcomp = NULL;
		const char *uid = NULL;

		e_cal_component_get_uid (priv->comp, &uid);
		if (e_cal_get_object (priv->client, uid, NULL, &icalcomp, NULL) && icalcomp) {
			send_comp = e_cal_component_new ();
			if (!e_cal_component_set_icalcomponent (send_comp, icalcomp)) {
				icalcomponent_free (icalcomp);
				g_object_unref (send_comp);
				send_comp = NULL;
			}
		}
	}

	if (!send_comp)
		send_comp = e_cal_component_clone (priv->comp);

	if (e_cal_component_get_vtype (send_comp) == E_CAL_COMPONENT_JOURNAL)
		get_users_from_memo_comp (send_comp, &users);

	/* The user updates the delegated status to the Organizer, so remove all other attendees */
	if (flags & COMP_EDITOR_DELEGATE) {
		address = itip_get_comp_attendee (send_comp, priv->client);

		if (address)
			set_attendees_for_delegation (send_comp, address, method);
	}

	if (!e_cal_component_has_attachments (priv->comp)) {
		if (itip_send_comp (method, send_comp, priv->client,
					NULL, NULL, users)) {
			g_object_unref (send_comp);
			return TRUE;
		}

	} else {
		/* Clone the component with attachments set to CID:...  */
		int num_attachments, i;
		GSList *attach_list = NULL;
		GSList *mime_attach_list;

		num_attachments = e_cal_component_get_num_attachments (send_comp);

		for (i = 0; i < num_attachments ; i++) {
			attach_list = g_slist_append (attach_list, g_strdup ("CID:..."));
		}
		e_cal_component_set_attachment_list (send_comp, attach_list);

		/* mime_attach_list is freed by itip_send_comp */
		mime_attach_list = comp_editor_get_mime_attach_list (editor);
		if (itip_send_comp (method, send_comp, priv->client,
					NULL, mime_attach_list, users)) {
			save_comp (editor);
			g_object_unref (send_comp);
			return TRUE;
		}
	}

	g_object_unref (send_comp);
	g_free (address);
	comp_editor_set_changed (editor, TRUE);

	return FALSE;

}


/**
 * comp_editor_edit_comp:
 * @editor: A component editor
 * @comp: A calendar component
 *
 * Starts the editor editing the given component
 **/
void
comp_editor_edit_comp (CompEditor *editor, ECalComponent *comp)
{
	CompEditorClass *class;

	g_return_if_fail (IS_COMP_EDITOR (editor));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	class = COMP_EDITOR_GET_CLASS (editor);

	if (class->edit_comp)
		class->edit_comp (editor, comp);
}

ECalComponent *
comp_editor_get_comp (CompEditor *editor)
{
	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	return editor->priv->comp;
}

/**
 * comp_editor_get_current_comp
 *
 * @param editor
 * @param correct Set this no non-NULL if you are interested to know if
 *                all pages reported success when filling component.
 * @return Newly allocated component, should be unref-ed by g_object_unref.
 **/
ECalComponent *
comp_editor_get_current_comp (CompEditor *editor, gboolean *correct)
{
	CompEditorPrivate *priv;
	ECalComponent *comp;
	GList *l;
	gboolean all_ok = TRUE;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	priv = editor->priv;

	comp = e_cal_component_clone (priv->comp);
	if (priv->changed) {
		for (l = priv->pages; l != NULL; l = l->next)
			all_ok = comp_editor_page_fill_component (l->data, comp) && all_ok;
	}

	if (correct)
		*correct = all_ok;

	return comp;
}

/**
 * comp_editor_save_comp:
 * @editor:
 *
 *
 **/
gboolean
comp_editor_save_comp (CompEditor *editor, gboolean send)
{
	return prompt_and_save_changes (editor, send);
}

/**
 * comp_editor_delete_comp:
 * @editor:
 *
 *
 **/
void
comp_editor_delete_comp (CompEditor *editor)
{
	CompEditorPrivate *priv;
	const char *uid;

	g_return_if_fail (IS_COMP_EDITOR (editor));

	priv = editor->priv;

	e_cal_component_get_uid (priv->comp, &uid);
	if (e_cal_component_is_instance (priv->comp)|| e_cal_component_has_recurrences (priv->comp))
		e_cal_remove_object_with_mod (priv->client, uid, NULL,
				CALOBJ_MOD_ALL, NULL);
	else
		e_cal_remove_object (priv->client, uid, NULL);
	close_dialog (editor);
}

/**
 * comp_editor_send_comp:
 * @editor:
 * @method:
 *
 *
 **/
gboolean
comp_editor_send_comp (CompEditor *editor, ECalComponentItipMethod method)
{
	CompEditorClass *class;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	class = COMP_EDITOR_GET_CLASS (editor);

	if (class->send_comp)
		return class->send_comp (editor, method);

	return FALSE;
}

gboolean
comp_editor_close (CompEditor *editor)
{
	gboolean close;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), FALSE);

	commit_all_fields (editor);

	close = prompt_and_save_changes (editor, TRUE);
	if (close)
		close_dialog (editor);

	return close;
}


/* Utility function to get the mime-attachment list from the attachment
 * bar for sending the comp via itip. The list and its contents must
 * be freed by the caller.
 */
GSList *
comp_editor_get_mime_attach_list (CompEditor *editor)
{
	struct CalMimeAttach *cal_mime_attach;
	GSList *attach_list = NULL, *l, *parts;

	/* TODO assert sanity of bar */
	parts = e_attachment_bar_get_parts (E_ATTACHMENT_BAR (editor->priv->attachment_bar));
	for (l = parts; l ; l = l->next) {

		CamelDataWrapper *wrapper;
		CamelStreamMem *mstream;
		unsigned char *buffer = NULL;
		const char *desc, *disp;

		cal_mime_attach = g_malloc0 (sizeof (struct CalMimeAttach));
		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (l->data));
		mstream = (CamelStreamMem *) camel_stream_mem_new ();

		camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) mstream);
		buffer = g_memdup (mstream->buffer->data, mstream->buffer->len);

		cal_mime_attach->encoded_data = (char *)buffer;
		cal_mime_attach->length = mstream->buffer->len;
		cal_mime_attach->filename = g_strdup (camel_mime_part_get_filename
			((CamelMimePart *) l->data));
		desc = camel_mime_part_get_description ((CamelMimePart *) l->data);
		if (!desc || *desc == '\0')
			desc = _("attachment");
		cal_mime_attach->description = g_strdup (desc);
		cal_mime_attach->content_type = g_strdup (camel_data_wrapper_get_mime_type (wrapper));

		disp = camel_mime_part_get_disposition ((CamelMimePart *)l->data);
		if (disp && !g_ascii_strcasecmp(disp, "inline"))
			cal_mime_attach->disposition = TRUE;

		attach_list = g_slist_append (attach_list, cal_mime_attach);

		camel_object_unref (mstream);

	}

	g_slist_free (parts);

	return attach_list;
}

static void
page_dates_changed_cb (CompEditor *editor,
		       CompEditorPageDates *dates,
                       CompEditorPage *page)
{
	CompEditorPrivate *priv = editor->priv;
	GList *l;

	for (l = priv->pages; l != NULL; l = l->next)
		if (page != (CompEditorPage *) l->data)
			comp_editor_page_set_dates (l->data, dates);

	if (!priv->warned && priv->existing_org && !priv->user_org) {
		e_notice (priv->notebook, GTK_MESSAGE_INFO,
			  _("Changes made to this item may be discarded if an update arrives"));
		priv->warned = TRUE;
	}
}

static void
obj_modified_cb (ECal *client,
                 GList *objects,
                 CompEditor *editor)
{
	CompEditorPrivate *priv;
	ECalComponent *comp = NULL;

	priv = editor->priv;

	/* We queried based on a specific UID so we definitely changed */
	if (changed_component_dialog ((GtkWindow *) editor, priv->comp, FALSE, priv->changed)) {
		icalcomponent *icalcomp = icalcomponent_new_clone (objects->data);

		comp = e_cal_component_new ();
		if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
			comp_editor_edit_comp (editor, comp);
		} else {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (
				NULL, 0,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s",
				_("Unable to use current version!"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			icalcomponent_free (icalcomp);
		}

		g_object_unref (comp);
	}
}

static void
obj_removed_cb (ECal *client,
                GList *uids,
                CompEditor *editor)
{
	CompEditorPrivate *priv = editor->priv;

	if (changed_component_dialog ((GtkWindow *) editor, priv->comp, TRUE, priv->changed))
		close_dialog (editor);
}

