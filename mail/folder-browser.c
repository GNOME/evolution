/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Miguel De Icaza <miguel@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000-2003 Ximian, Inc. (www.ximian.com)
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkinvisible.h>
#include <gal/e-table/e-table.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-popup-menu.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libgnomeui/gnome-dialog-util.h>
#include <libgnomeui/gnome-pixmap.h>

#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlobject.h>
#include <gtkhtml/htmlinterval.h>
#include <gtkhtml/htmlengine-edit-cut-and-paste.h>

#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"
#include "filter/filter-label.h"

#include "e-util/e-sexp.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-meta.h"
#include "folder-browser.h"
#include "e-searching-tokenizer.h"
#include "mail.h"
#include "mail-callbacks.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "folder-browser-ui.h"

#include "mail-local.h"
#include "mail-config.h"

#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>

/* maybe this shooudlnt be private ... */
#include "camel/camel-search-private.h"

#define d(x) 

#define PARENT_TYPE (gtk_table_get_type ())

static void folder_changed(CamelObject *o, void *event_data, void *data);
static void main_folder_changed(CamelObject *o, void *event_data, void *data);

#define X_EVOLUTION_MESSAGE_TYPE "x-evolution-message"
#define MESSAGE_RFC822_TYPE      "message/rfc822"
#define TEXT_URI_LIST_TYPE       "text/uri-list"
#define TEXT_PLAIN_TYPE          "text/plain"

/* Drag & Drop types */
enum DndTargetType {
	DND_TARGET_TYPE_X_EVOLUTION_MESSAGE,
	DND_TARGET_TYPE_MESSAGE_RFC822,
	DND_TARGET_TYPE_TEXT_URI_LIST,
};

static GtkTargetEntry drag_types[] = {
	{ X_EVOLUTION_MESSAGE_TYPE, 0, DND_TARGET_TYPE_X_EVOLUTION_MESSAGE },
	{ MESSAGE_RFC822_TYPE, 0, DND_TARGET_TYPE_MESSAGE_RFC822 },
	{ TEXT_URI_LIST_TYPE, 0, DND_TARGET_TYPE_TEXT_URI_LIST },
};

static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

enum PasteTargetType {
	PASTE_TARGET_TYPE_X_EVOLUTION_MESSAGE,
	PASTE_TARGET_TYPE_TEXT_PLAIN,
};

static GtkTargetPair paste_types[] = {
	{ 0, 0, PASTE_TARGET_TYPE_X_EVOLUTION_MESSAGE },
	{ GDK_SELECTION_TYPE_STRING, 0, PASTE_TARGET_TYPE_TEXT_PLAIN },
};

static const int num_paste_types = sizeof (paste_types) / sizeof (paste_types[0]);

static GdkAtom clipboard_atom = GDK_NONE;

static GtkTableClass *parent_class = NULL;

enum {
	FOLDER_LOADED,
	MESSAGE_LOADED,
	LAST_SIGNAL
};

static guint folder_browser_signals [LAST_SIGNAL] = {0, };

static void
folder_browser_finalise (GObject *object)
{
	FolderBrowser *folder_browser;
	
	folder_browser = FOLDER_BROWSER (object);
		
	g_free (folder_browser->loading_uid);
	g_free (folder_browser->pending_uid);
	g_free (folder_browser->new_uid);
	g_free (folder_browser->loaded_uid);
	
	g_free (folder_browser->uri);
	folder_browser->uri = NULL;
		
	if (folder_browser->clipboard_selection)
		g_byte_array_free (folder_browser->clipboard_selection, TRUE);
	
	if (folder_browser->sensitise_state) {
		g_hash_table_destroy (folder_browser->sensitise_state);
		folder_browser->sensitise_state = NULL;
	}
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
folder_browser_destroy (GtkObject *object)
{
	FolderBrowser *folder_browser;
	CORBA_Environment ev;
	
	folder_browser = FOLDER_BROWSER (object);
		
	CORBA_exception_init (&ev);
	
	if (folder_browser->seen_id != 0) {
		gtk_timeout_remove (folder_browser->seen_id);
		folder_browser->seen_id = 0;
	}
	
	if (folder_browser->loading_id != 0) {
		gtk_timeout_remove(folder_browser->loading_id);
		folder_browser->loading_id = 0;
	}
	
	if (folder_browser->message_list) {
		gtk_widget_destroy (GTK_WIDGET (folder_browser->message_list));
		folder_browser->message_list = NULL;
	}
	
	if (folder_browser->mail_display) {
		gtk_widget_destroy (GTK_WIDGET (folder_browser->mail_display));
		folder_browser->mail_display = NULL;
	}

	if (folder_browser->view_instance) {
		g_object_unref (folder_browser->view_instance);
		folder_browser->view_instance = NULL;
	}
	
	if (folder_browser->view_menus) {
		g_object_unref (folder_browser->view_menus);
		folder_browser->view_menus = NULL;
	}
	
	/* wait for all outstanding async events against us */
	if (folder_browser->async_event) {
		mail_async_event_destroy (folder_browser->async_event);
		folder_browser->async_event = NULL;
	}
	
	if (folder_browser->search_full) {
		g_object_unref (folder_browser->search_full);
		folder_browser->search_full = NULL;
	}
	
	if (folder_browser->sensitize_timeout_id) {
		g_source_remove (folder_browser->sensitize_timeout_id);
		folder_browser->sensitize_timeout_id = 0;
	}
	
	if (folder_browser->shell != CORBA_OBJECT_NIL) {
		CORBA_Object_release (folder_browser->shell, &ev);
		folder_browser->shell = CORBA_OBJECT_NIL;
	}
	
	if (folder_browser->shell_view != CORBA_OBJECT_NIL) {
		CORBA_Object_release (folder_browser->shell_view, &ev);
		folder_browser->shell_view = CORBA_OBJECT_NIL;
	}
	
	if (folder_browser->uicomp) {
		bonobo_object_unref (BONOBO_OBJECT (folder_browser->uicomp));
		folder_browser->uicomp = NULL;
	}
	
	if (folder_browser->invisible) {
		g_object_unref (folder_browser->invisible);
		folder_browser->invisible = NULL;
	}
		
	if (folder_browser->get_id != -1) {
		mail_msg_cancel (folder_browser->get_id);
		folder_browser->get_id = -1;
	}
	
	if (folder_browser->folder) {
		camel_object_unhook_event (CAMEL_OBJECT (folder_browser->folder), "folder_changed",
					   folder_changed, folder_browser);
		camel_object_unhook_event (CAMEL_OBJECT (folder_browser->folder), "message_changed",
					   folder_changed, folder_browser);
		mail_sync_folder (folder_browser->folder, NULL, NULL);
		camel_object_unref (folder_browser->folder);
		folder_browser->folder = NULL;
	}

	CORBA_exception_free (&ev);
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
folder_browser_class_init (FolderBrowserClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref(PARENT_TYPE);
	
	object_class->destroy = folder_browser_destroy;
	gobject_class->finalize = folder_browser_finalise;
	
	folder_browser_signals[FOLDER_LOADED] =
		g_signal_new ("folder_loaded",
			      FOLDER_BROWSER_TYPE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (FolderBrowserClass, folder_loaded),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	
	folder_browser_signals[MESSAGE_LOADED] =
		g_signal_new ("message_loaded",
			      FOLDER_BROWSER_TYPE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (FolderBrowserClass, message_loaded),
			      NULL,
			      NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	
	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
	
	if (!paste_types[0].target)
		paste_types[0].target = gdk_atom_intern (X_EVOLUTION_MESSAGE_TYPE, FALSE);
}

static void
add_uid (MessageList *ml, const char *uid, gpointer data)
{
	g_ptr_array_add ((GPtrArray *) data, g_strdup (uid));
}

static void
message_list_drag_data_get (ETree *tree, int row, ETreePath path, int col,
			    GdkDragContext *context, GtkSelectionData *selection_data,
			    guint info, guint time, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	GPtrArray *uids = NULL;
	int i;
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, add_uid, uids);
	if (uids->len == 0) {
		g_ptr_array_free (uids, TRUE);
		return;
	}
	
	switch (info) {
	case DND_TARGET_TYPE_TEXT_URI_LIST:
	{
		const char *filename, *tmpdir;
		CamelMimeMessage *message;
		CamelMimeFilter *filter;
		CamelStream *fstream;
		CamelStream *stream;
		char *uri_list;
		int fd;
		
		tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX");
		
		if (!tmpdir) {
			char *msg = g_strdup_printf (_("Could not create temporary "
						       "directory: %s"),
						     g_strerror (errno));
			gnome_error_dialog (msg);
			/* cleanup and abort */
			for (i = 0; i < uids->len; i++)
				g_free (uids->pdata[i]);
			g_ptr_array_free (uids, TRUE);
			g_free (msg);
			return;
		}
		
		message = camel_folder_get_message (fb->folder, uids->pdata[0], NULL);
		g_free (uids->pdata[0]);
		
		if (uids->len == 1) {
			filename = camel_mime_message_get_subject (message);
			if (!filename)
				filename = _("Unknown");
		} else
			filename = "mbox";
		
		uri_list = g_strdup_printf ("file://%s/%s", tmpdir, filename);
		
		fd = open (uri_list + 7, O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fd == -1) {
			/* cleanup and abort */
			camel_object_unref (message);
			for (i = 1; i < uids->len; i++)
				g_free (uids->pdata[i]);
			g_ptr_array_free (uids, TRUE);
			g_free (uri_list);
			return;
		}
		
		fstream = camel_stream_fs_new_with_fd (fd);
		
		stream = camel_stream_filter_new_with_stream (fstream);
		filter = camel_mime_filter_from_new ();
		camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), filter);
		camel_object_unref (filter);
		
		camel_stream_write (fstream, "From - \n", 8);
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
		camel_object_unref (message);
		camel_stream_flush (stream);
		
		for (i = 1; i < uids->len; i++) {
			message = camel_folder_get_message (fb->folder, uids->pdata[i], NULL);
			camel_stream_write (fstream, "From - \n", 8);
			camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
			camel_object_unref (message);
			camel_stream_flush (stream);
			g_free (uids->pdata[i]);
		}
		
		g_ptr_array_free (uids, TRUE);
		
		camel_object_unref (stream);
		camel_object_unref (fstream);
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					uri_list, strlen (uri_list));
		g_free (uri_list);
	}
	break;
	case DND_TARGET_TYPE_MESSAGE_RFC822:
	{
		CamelMimeFilter *filter;
		CamelStream *stream;
		CamelStream *mem;
		
		mem = camel_stream_mem_new ();
		
		stream = camel_stream_filter_new_with_stream (mem);
		filter = camel_mime_filter_from_new ();
		camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), filter);
		camel_object_unref (filter);
		
		for (i = 0; i < uids->len; i++) {
			CamelMimeMessage *message;
			
			message = camel_folder_get_message (fb->folder, uids->pdata[i], NULL);
			g_free (uids->pdata[i]);
			
			if (message) {			
				camel_stream_write (mem, "From - \n", 8);
				camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
				camel_object_unref (message);
				camel_stream_flush (stream);
			}
		}
		
		g_ptr_array_free (uids, TRUE);
		camel_object_unref (stream);
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					CAMEL_STREAM_MEM (mem)->buffer->data,
					CAMEL_STREAM_MEM (mem)->buffer->len);
		
		camel_object_unref (mem);
	}
	break;
	case DND_TARGET_TYPE_X_EVOLUTION_MESSAGE:
	{
		GByteArray *array;
		
		/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn" */
		
		/* write the uri portion */
		array = g_byte_array_new ();
		g_byte_array_append (array, fb->uri, strlen (fb->uri));
		g_byte_array_append (array, "", 1);
		
		/* write the uids */
		for (i = 0; i < uids->len; i++) {
			g_byte_array_append (array, uids->pdata[i], strlen (uids->pdata[i]));
			g_free (uids->pdata[i]);
			
			if (i + 1 < uids->len)
				g_byte_array_append (array, "", 1);
		}
		
		g_ptr_array_free (uids, TRUE);
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					array->data, array->len);
		
		g_byte_array_free (array, TRUE);
	}
	break;
	default:
		for (i = 0; i < uids->len; i++)
			g_free (uids->pdata[i]);
		
		g_ptr_array_free (uids, TRUE);
		break;
	}
}

static void
message_rfc822_dnd (CamelFolder *dest, CamelStream *stream, CamelException *ex)
{
	CamelMimeParser *mp;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_stream (mp, stream);
	
	while (camel_mime_parser_step (mp, 0, 0) == HSCAN_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_object_unref (CAMEL_OBJECT (msg));
			break;
		}
		
		/* append the message to the folder... */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (dest, msg, info, NULL, ex);
		camel_object_unref (CAMEL_OBJECT (msg));
		
		if (camel_exception_is_set (ex))
			break;
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (CAMEL_OBJECT (mp));
}

static void
message_list_drag_data_received (ETree *tree, int row, ETreePath path, int col,
				 GdkDragContext *context, gint x, gint y,
				 GtkSelectionData *selection_data, guint info,
				 guint time, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	CamelFolder *folder = NULL;
	char *tmp, *url, **urls;
	GPtrArray *uids = NULL;
	CamelStream *stream;
	CamelException ex;
	CamelURL *uri;
	int i, fd;
	
	/* this means we are receiving no data */
	if (!selection_data->data || selection_data->length == -1)
		return;
	
	camel_exception_init (&ex);
	
	switch (info) {
	case DND_TARGET_TYPE_TEXT_URI_LIST:
		tmp = g_strndup (selection_data->data, selection_data->length);
		urls = g_strsplit (tmp, "\n", 0);
		g_free (tmp);
		
		for (i = 0; urls[i] != NULL; i++) {
			/* get the path component */
			url = g_strstrip (urls[i]);
			
			uri = camel_url_new (url, NULL);
			g_free (url);
			url = uri->path;
			uri->path = NULL;
			camel_url_free (uri);
			
			fd = open (url, O_RDONLY);
			if (fd == -1) {
				g_free (url);
				/* FIXME: okay, what do we do in this case? */
				continue;
			}
			
			stream = camel_stream_fs_new_with_fd (fd);
			message_rfc822_dnd (fb->folder, stream, &ex);
			camel_object_unref (CAMEL_OBJECT (stream));
			
			if (context->action == GDK_ACTION_MOVE && !camel_exception_is_set (&ex))
				unlink (url);
			
			g_free (url);
		}
		
		g_free (urls);
		break;
	case DND_TARGET_TYPE_MESSAGE_RFC822:
		/* write the message(s) out to a CamelStream so we can use it */
		stream = camel_stream_mem_new ();
		camel_stream_write (stream, selection_data->data, selection_data->length);
		camel_stream_reset (stream);
		
		message_rfc822_dnd (fb->folder, stream, &ex);
		camel_object_unref (CAMEL_OBJECT (stream));
		break;
	case DND_TARGET_TYPE_X_EVOLUTION_MESSAGE:
		folder = mail_tools_x_evolution_message_parse (selection_data->data, selection_data->length, &uids);
		if (folder == NULL)
			goto fail;
		
		if (uids == NULL) {
			camel_object_unref (CAMEL_OBJECT (folder));
			goto fail;
		}
		
		mail_transfer_messages (folder, uids, context->action == GDK_ACTION_MOVE,
					fb->uri, 0, NULL, NULL);
		
		camel_object_unref (CAMEL_OBJECT (folder));
		break;
	}
	
	camel_exception_clear (&ex);
	
	gtk_drag_finish (context, TRUE, TRUE, GDK_CURRENT_TIME);
	
 fail:
	camel_exception_clear (&ex);
	
	gtk_drag_finish (context, FALSE, TRUE, GDK_CURRENT_TIME);
}

static void
selection_get (GtkWidget *widget, GtkSelectionData *selection_data,
	       guint info, guint time_stamp, FolderBrowser *fb)
{
	if (fb->clipboard_selection == NULL)
		return;
	
	switch (info) {
	default:
	case PASTE_TARGET_TYPE_TEXT_PLAIN:
	{
		/* FIXME: this'll be fucking slow for the user... pthread this? */
		CamelFolder *source;
		CamelStream *stream;
		GByteArray *bytes;
		GPtrArray *uids;
		int i;
		
		bytes = fb->clipboard_selection;
		
		/* Note: source should == fb->folder, but we might as well use `source' instead of fb->folder */
		source = mail_tools_x_evolution_message_parse (bytes->data, bytes->len, &uids);
		if (source == NULL)
			return;
		
		if (uids == NULL) {
			camel_object_unref (CAMEL_OBJECT (source));
			return;
		}
		
		bytes = g_byte_array_new ();
		stream = camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), bytes);
		
		for (i = 0; i < uids->len; i++) {
			CamelMimeMessage *message;
			
			message = camel_folder_get_message (source, uids->pdata[i], NULL);
			g_free (uids->pdata[i]);
			
			if (message) {
				camel_stream_write (stream, "From - \n", 8);
				camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
				camel_object_unref (CAMEL_OBJECT (message));
			}
		}
		
		g_ptr_array_free (uids, TRUE);
		camel_object_unref (CAMEL_OBJECT (stream));
		camel_object_unref (CAMEL_OBJECT (source));
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					bytes->data, bytes->len);
		
		g_byte_array_free (bytes, FALSE);
	}
	break;
	case PASTE_TARGET_TYPE_X_EVOLUTION_MESSAGE:
		/* we already have our data in the correct form */
		gtk_selection_data_set (selection_data,
					selection_data->target, 8,
					fb->clipboard_selection->data,
					fb->clipboard_selection->len);
		break;
	}
}

static void
selection_clear_event (GtkWidget *widget, GdkEventSelection *event, FolderBrowser *fb)
{
	if (fb->clipboard_selection != NULL) {
		g_byte_array_free (fb->clipboard_selection, TRUE);
		fb->clipboard_selection = NULL;
	}
}

static void
selection_received (GtkWidget *widget, GtkSelectionData *selection_data,
		    guint time, FolderBrowser *fb)
{
	CamelFolder *source = NULL;
	GPtrArray *uids = NULL;
	
	if (selection_data == NULL || selection_data->length == -1)
		return;
	
	source = mail_tools_x_evolution_message_parse (selection_data->data, selection_data->length, &uids);
	if (source == NULL)
		return;
	
	if (uids == NULL) {
		camel_object_unref (CAMEL_OBJECT (source));
		return;
	}
	
	mail_transfer_messages (source, uids, FALSE, fb->uri, 0, NULL, NULL);
	
	camel_object_unref (CAMEL_OBJECT (source));
}

void
folder_browser_copy (GtkWidget *menuitem, FolderBrowser *fb)
{
	GPtrArray *uids = NULL;
	GByteArray *bytes;
	gboolean cut;
	int i;
	
	if (fb->message_list == NULL)
		return;
	
	cut = menuitem == NULL;
	
	if (GTK_WIDGET_HAS_FOCUS (fb->mail_display->html)) {
		gtk_html_copy (fb->mail_display->html);
		return;
	}
	
	if (fb->clipboard_selection) {
		g_byte_array_free (fb->clipboard_selection, TRUE);
		fb->clipboard_selection = NULL;
	}
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, add_uid, uids);
	
	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn" */
	
	/* write the uri portion */
	bytes = g_byte_array_new ();
	g_byte_array_append (bytes, fb->uri, strlen (fb->uri));
	g_byte_array_append (bytes, "", 1);
	
	/* write the uids */
	camel_folder_freeze (fb->folder);
	for (i = 0; i < uids->len; i++) {
		if (cut) {
			camel_folder_set_message_flags (fb->folder, uids->pdata[i],
							CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED,
							CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED);
		}
		g_byte_array_append (bytes, uids->pdata[i], strlen (uids->pdata[i]));
		g_free (uids->pdata[i]);
		
		if (i + 1 < uids->len)
			g_byte_array_append (bytes, "", 1);
	}
	camel_folder_thaw (fb->folder);
	
	g_ptr_array_free (uids, TRUE);
	
	fb->clipboard_selection = bytes;
	
	gtk_selection_owner_set (fb->invisible, clipboard_atom, GDK_CURRENT_TIME);
}

void
folder_browser_cut (GtkWidget *menuitem, FolderBrowser *fb)
{
	folder_browser_copy (NULL, fb);
}

void
folder_browser_paste (GtkWidget *menuitem, FolderBrowser *fb)
{
	gtk_selection_convert (fb->invisible, clipboard_atom,
			       paste_types[0].target,
			       GDK_CURRENT_TIME);
}

/* all this crap so we can give the user a whoopee doo status bar */
static void
update_status_bar (FolderBrowser *fb)
{
	extern CamelFolder *outbox_folder, *sent_folder;
	CORBA_Environment ev;
	int tmp, total;
	GString *work;
	
	if (fb->folder == NULL
	    || fb->message_list == NULL
	    || fb->shell_view == CORBA_OBJECT_NIL)
		return;
	
	if (!fb->message_list->hidedeleted || !camel_folder_has_summary_capability (fb->folder)) {
		total = camel_folder_get_message_count (fb->folder);
	} else {
		GPtrArray *sum = camel_folder_get_summary (fb->folder);
		int i;
		
		if (sum) {
			total = 0;
			for (i = 0; i < sum->len; i++) {
				CamelMessageInfo *info = sum->pdata[i];
				
				if ((info->flags & CAMEL_MESSAGE_DELETED) == 0)
					total++;
			}
			camel_folder_free_summary (fb->folder, sum);
		} else {
			total = camel_folder_get_message_count (fb->folder);
		}
	}
	
	work = g_string_new ("");
	g_string_append_printf (work, _("%d new"), camel_folder_get_unread_message_count (fb->folder));
	tmp = message_list_hidden (fb->message_list);
	if (0 < tmp && tmp < total) {
		g_string_append (work, _(", "));
		if (tmp < total / 2)
			g_string_append_printf (work, _("%d hidden"), tmp);
		else  
			g_string_append_printf (work, _("%d visible"), total - tmp);
	}
	tmp = e_selection_model_selected_count (e_tree_get_selection_model (fb->message_list->tree));
	if (tmp) {
		g_string_append (work, _(", "));
		g_string_append_printf (work, _("%d selected"), tmp);
	}
	g_string_append (work, _(", "));
	
	if (fb->folder == outbox_folder)
		g_string_append_printf (work, _("%d unsent"), total);
	else if (fb->folder == sent_folder)
		g_string_append_printf (work, _("%d sent"), total);
	else
		g_string_append_printf (work, _("%d total"), total);
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_ShellView_setFolderBarLabel (fb->shell_view, work->str, &ev);
	CORBA_exception_free (&ev);
	
	if (fb->update_status_bar_idle_id != 0) {
		g_source_remove (fb->update_status_bar_idle_id);
		fb->update_status_bar_idle_id = 0;
	}
	
	g_string_free (work, TRUE);
}

static gboolean
update_status_bar_idle_cb(gpointer data)
{
	FolderBrowser *fb = data;

#if 0	
	if (!GTK_OBJECT_DESTROYED (fb))
#endif
		update_status_bar (fb);
	
	fb->update_status_bar_idle_id = 0;
	g_object_unref (fb);
	
	return FALSE;
}

static void
update_status_bar_idle(FolderBrowser *fb)
{
	if (fb->update_status_bar_idle_id == 0) {
		g_object_ref (fb);
		fb->update_status_bar_idle_id = g_idle_add (update_status_bar_idle_cb, fb);
	}
}

static void main_folder_changed(CamelObject *o, void *event_data, void *data)
{
	FolderBrowser *fb = data;
	
	if (fb->message_list == NULL)
		return;
	
	/* so some corba unref doesnt blow us away while we're busy */
	g_object_ref (fb);
	update_status_bar (fb);
	folder_browser_ui_scan_selection (fb);
	g_object_unref (fb);
}

static void folder_changed (CamelObject *obj, void *event_data, void *user_data)
{
	FolderBrowser *fb = user_data;

	mail_async_event_emit (fb->async_event, MAIL_ASYNC_GUI,
			       (MailAsyncFunc) main_folder_changed,
			       obj, NULL, user_data);
}

static void
got_folder (char *uri, CamelFolder *folder, void *user_data)
{
	FolderBrowser *fb = user_data;
	EMeta *meta;

	fb->get_id = -1;
	
	d(printf ("got folder '%s' = %p, previous folder was %p\n", uri, folder, fb->folder));
	
	if (fb->message_list == NULL)
		goto done;

	if (fb->folder) {
		camel_object_unhook_event (fb->folder, "folder_changed", folder_changed, fb);
		camel_object_unhook_event (fb->folder, "message_changed", folder_changed, fb);
		camel_object_unref (fb->folder);
	}

	if (folder) {
		fb->folder = folder;
		camel_object_ref (folder);
		meta = mail_tool_get_meta_data(fb->uri);
		if (meta != fb->meta) {
			g_object_unref(fb->meta);
			fb->meta = meta;
		} else {
			g_object_unref(meta);
		}
	} else {
		fb->folder = NULL;
		if (fb->meta) {
			g_object_unref(fb->meta);
			fb->meta = NULL;
		}
		goto done;
	}

	
	gtk_widget_set_sensitive (GTK_WIDGET (fb->search), camel_folder_has_search_capability (folder));
	message_list_set_folder (fb->message_list, folder,
				 folder_browser_is_drafts (fb) ||
				 folder_browser_is_sent (fb) ||
				 folder_browser_is_outbox (fb));
	
	camel_object_hook_event (CAMEL_OBJECT (fb->folder), "folder_changed",
				 folder_changed, fb);
	camel_object_hook_event (CAMEL_OBJECT (fb->folder), "message_changed",
				 folder_changed, fb);
	
	if (fb->view_instance != NULL && fb->view_menus != NULL)
		folder_browser_ui_discard_view_menus (fb);
	
	folder_browser_ui_setup_view_menus (fb);
	
	/* when loading a new folder, nothing is selected initially */
	
	if (fb->uicomp)
		folder_browser_ui_set_selection_state (fb, FB_SELSTATE_NONE);
	
 done:	
	g_signal_emit (fb, folder_browser_signals[FOLDER_LOADED], 0, fb->uri);
	g_object_unref (fb);
}


void
folder_browser_reload (FolderBrowser *fb)
{
	g_return_if_fail (IS_FOLDER_BROWSER (fb));
	
	if (fb->folder) {
		mail_refresh_folder (fb->folder, NULL, NULL);
	} else if (fb->uri && fb->get_id == -1) {
		g_object_ref (fb);
		fb->get_id = mail_get_folder (fb->uri, 0, got_folder, fb, mail_thread_new);
	}
}

void
folder_browser_set_folder (FolderBrowser *fb, CamelFolder *folder, const char *uri)
{
	g_return_if_fail (IS_FOLDER_BROWSER (fb));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	
	if (fb->get_id != -1) {
		/* FIXME: cancel the get_folder request? */
	}
	
	g_free (fb->uri);
	fb->uri = g_strdup (uri);
	
	g_object_ref (fb);
	got_folder (NULL, folder, fb);
}

void
folder_browser_set_ui_component (FolderBrowser *fb, BonoboUIComponent *uicomp)
{
	g_return_if_fail (IS_FOLDER_BROWSER (fb));
	
	if (fb->sensitize_timeout_id) {
		g_source_remove (fb->sensitize_timeout_id);
		fb->sensitize_timeout_id = 0;
	}
	
	if (fb->sensitise_state) {
		g_hash_table_destroy (fb->sensitise_state);
		fb->sensitise_state = NULL;
	}
	
	if (fb->uicomp)
		bonobo_object_unref (BONOBO_OBJECT (fb->uicomp));
	
	if (uicomp)
		bonobo_object_ref (BONOBO_OBJECT (uicomp));
	
	fb->uicomp = uicomp;
}

void
folder_browser_set_shell_view(FolderBrowser *fb, GNOME_Evolution_ShellView shell_view)
{
	CORBA_Environment ev;
	
	CORBA_exception_init(&ev);
	if (fb->shell_view != CORBA_OBJECT_NIL)
		CORBA_Object_release (fb->shell_view, &ev);
	CORBA_exception_free (&ev);
	
	fb->shell_view = CORBA_Object_duplicate (shell_view, &ev);
	CORBA_exception_free (&ev);
	
	/* small hack, at this point we've just been activated */
	if (fb->shell_view != CORBA_OBJECT_NIL)
		update_status_bar (fb);
}

extern CamelFolder *drafts_folder, *sent_folder, *outbox_folder;

/**
 * folder_browser_is_drafts:
 * @fb: a FolderBrowser
 *
 * Return value: %TRUE if @fb refers to /local/Drafts or any other
 * configured Drafts folder.
 **/
gboolean
folder_browser_is_drafts (FolderBrowser *fb)
{
	gboolean is_drafts = FALSE;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	
	g_return_val_if_fail (IS_FOLDER_BROWSER (fb), FALSE);
	
	if (fb->uri == NULL || fb->folder == NULL)
		return FALSE;
	
	if (fb->folder == drafts_folder)
		return TRUE;
	
	accounts = mail_config_get_accounts ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		if (account->drafts_folder_uri &&
		    camel_store_uri_cmp (fb->folder->parent_store, account->drafts_folder_uri, fb->uri)) {
			is_drafts = TRUE;
			break;
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return is_drafts;
}

/**
 * folder_browser_is_sent:
 * @fb: a FolderBrowser
 *
 * Return value: %TRUE if @fb refers to /local/Sent or any other
 * configured Sent folder.
 **/
gboolean
folder_browser_is_sent (FolderBrowser *fb)
{
	gboolean is_sent = FALSE;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	
	g_return_val_if_fail (IS_FOLDER_BROWSER (fb), FALSE);
	
	if (fb->uri == NULL || fb->folder == NULL)
		return FALSE;
	
	if (fb->folder == sent_folder)
		return TRUE;
	
	accounts = mail_config_get_accounts ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);
		if (account->sent_folder_uri &&
		    camel_store_uri_cmp (fb->folder->parent_store, account->sent_folder_uri, fb->uri)) {
			is_sent = TRUE;
			break;
		}
		
		e_iterator_next (iter);
	}
	
	g_object_unref (iter);
	
	return is_sent;
}

/**
 * folder_browser_is_outbox:
 * @fb: a FolderBrowser
 *
 * Return value: %TRUE if @fb refers to /local/Outbox or any other
 * configured Outbox folder.
 **/
gboolean
folder_browser_is_outbox (FolderBrowser *fb)
{
	/* There can be only one. */
	return fb->folder == outbox_folder;
}

static int
save_cursor_pos (FolderBrowser *fb)
{
	ETreePath node;
	GtkAdjustment *adj;
	int row, y, height, paned_size;
	GConfClient *gconf;
	
	node = e_tree_get_cursor (fb->message_list->tree);
	if (!node)
		return -1;
	
	row = e_tree_row_of_node (fb->message_list->tree, node);
	
	if (row == -1)
		return 0;
	
	e_tree_get_cell_geometry (fb->message_list->tree, row, 0,
				  NULL, &y, NULL, &height);
	
	gconf = mail_config_get_gconf_client ();
	paned_size = gconf_client_get_int (gconf, "/apps/evolution/mail/display/paned_size", NULL);
	
	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (fb->message_list));
	y += adj->value - ((paned_size - height) / 2);
	
	return y;
}

static void
set_cursor_pos (FolderBrowser *fb, int y)
{
	GtkAdjustment *adj;
	
	if (y == -1)
		return;
	
	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (fb->message_list));
	gtk_adjustment_set_value (adj, (gfloat)y);
}

static gboolean do_message_selected(FolderBrowser *fb);

void
folder_browser_set_message_preview (FolderBrowser *folder_browser, gboolean show_preview)
{
	GConfClient *gconf;
	int paned_size, y;
	
	if (folder_browser->preview_shown == show_preview
	    || folder_browser->message_list == NULL)
		return;
	
	folder_browser->preview_shown = show_preview;
	
	gconf = mail_config_get_gconf_client ();
	paned_size = gconf_client_get_int (gconf, "/apps/evolution/mail/display/paned_size", NULL);

	if (show_preview) {
		y = save_cursor_pos (folder_browser);
		gtk_paned_set_position (GTK_PANED (folder_browser->vpaned), paned_size);
		gtk_widget_show (GTK_WIDGET (folder_browser->mail_display));
		do_message_selected (folder_browser);
		set_cursor_pos (folder_browser, y);
	} else {
		gtk_widget_hide (GTK_WIDGET (folder_browser->mail_display));
		mail_display_set_message (folder_browser->mail_display, NULL, NULL, NULL);
		folder_browser_ui_message_loaded (folder_browser);
	}
}

enum {
	ESB_SAVE,
};

static ESearchBarItem folder_browser_search_menu_items[] = {
	E_FILTERBAR_ADVANCED,
	{ NULL, 0, NULL },
	E_FILTERBAR_SAVE,
	E_FILTERBAR_EDIT,
	{ NULL, 0, NULL },
	{ N_("Create _Virtual Folder From Search..."), ESB_SAVE, NULL  },
	{ NULL, -1, NULL }
};

static void
folder_browser_search_menu_activated (ESearchBar *esb, int id, FolderBrowser *fb)
{
	EFilterBar *efb = (EFilterBar *)esb;
	
	d(printf("menu activated\n"));
	
	switch (id) {
	case ESB_SAVE:
		d(printf("Save vfolder\n"));
		if (efb->current_query) {
			FilterRule *rule = vfolder_clone_rule(efb->current_query);			
			char *name, *text;
			
			text = e_search_bar_get_text(esb);
			name = g_strdup_printf("%s %s", rule->name, (text&&text[0])?text:"''");
			g_free (text);
			filter_rule_set_name(rule, name);
			g_free (name);
			
			filter_rule_set_source(rule, FILTER_SOURCE_INCOMING);
			vfolder_rule_add_source((VfolderRule *)rule, fb->uri);
			vfolder_gui_add_rule((VfolderRule *)rule);
		}
		break;
	}
}

static void
folder_browser_config_search (EFilterBar *efb, FilterRule *rule, int id, const char *query, void *data)
{
	FolderBrowser *fb = FOLDER_BROWSER (data);
	ESearchingTokenizer *st;
	GList *partl;
	struct _camel_search_words *words;
	int i;
	
	st = E_SEARCHING_TOKENIZER (fb->mail_display->html->engine->ht); 
	
	e_searching_tokenizer_set_secondary_search_string (st, NULL);
	
	/* we scan the parts of a rule, and set all the types we know about to the query string */
	partl = rule->parts;
	while (partl) {
		FilterPart *part = partl->data;
		
		if (!strcmp(part->name, "subject")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "subject");
			if (input)
				filter_input_set_value(input, query);
		} else if (!strcmp(part->name, "body")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "word");
			if (input)
				filter_input_set_value(input, query);
			
			words = camel_search_words_split(query);
			for (i=0;i<words->len;i++)
				e_searching_tokenizer_add_secondary_search_string (st, words->words[i]->word);
			camel_search_words_free (words);
		} else if(!strcmp(part->name, "sender")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "sender");
			if (input)
				filter_input_set_value(input, query);
		} else if(!strcmp(part->name, "to")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "recipient");
			if (input)
				filter_input_set_value(input, query);
		}
		
		partl = partl->next;
	}
	
	d(printf("configuring search for search string '%s', rule is '%s'\n", query, rule->name));
	
	mail_display_redisplay (fb->mail_display, FALSE);
}

static void
folder_browser_search_do_search (ESearchBar *esb, FolderBrowser *fb)
{
	char *search_word;
	
	if (fb->message_list == NULL)
		return;
	
	d(printf("do search\n"));
	
	g_object_get (esb, "query", &search_word, NULL);
	
	message_list_set_search (fb->message_list, search_word);
	
	d(printf("query is %s\n", search_word));
	g_free (search_word);
}

static void
folder_browser_query_changed (ESearchBar *esb, FolderBrowser *fb)
{
	int id;
	
	id = e_search_bar_get_item_id (esb);
	if (id == E_FILTERBAR_ADVANCED_ID)
		folder_browser_search_do_search (esb, fb);
}

void
folder_browser_toggle_preview (BonoboUIComponent           *component,
			       const char                  *path,
			       Bonobo_UIComponent_EventType type,
			       const char                  *state,
			       gpointer                     user_data)
{
	FolderBrowser *fb = user_data;
	gboolean bstate;
	GConfClient *gconf;

	if (type != Bonobo_UIComponent_STATE_CHANGED || fb->message_list == NULL)
		return;
	
	bstate = atoi(state);
	e_meta_set_bool(fb->meta, "show_preview", bstate);
	
	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool (gconf, "/apps/evolution/mail/display/show_preview", bstate, NULL);
	
	folder_browser_set_message_preview (fb, bstate);
}

void
folder_browser_toggle_threads (BonoboUIComponent           *component,
			       const char                  *path,
			       Bonobo_UIComponent_EventType type,
			       const char                  *state,
			       gpointer                     user_data)
{
	FolderBrowser *fb = user_data;
	int prev_state;
	gboolean bstate;
	GConfClient *gconf;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED || fb->message_list == NULL)
		return;

	bstate = atoi(state);
	e_meta_set_bool(fb->meta, "thread_list", bstate);
	
	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool (gconf, "/apps/evolution/mail/display/thread_list", bstate, NULL);
	
	message_list_set_threaded (fb->message_list, bstate);
	
	prev_state = fb->selection_state;
	fb->selection_state = FB_SELSTATE_UNDEFINED;
	folder_browser_ui_set_selection_state (fb, prev_state);
}

void
folder_browser_toggle_hide_deleted (BonoboUIComponent           *component,
				    const char                  *path,
				    Bonobo_UIComponent_EventType type,
				    const char                  *state,
				    gpointer                     user_data)
{
	FolderBrowser *fb = user_data;
	GConfClient *gconf;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED || fb->message_list == NULL)
		return;
	
	gconf = mail_config_get_gconf_client ();
	gconf_client_set_bool (gconf, "/apps/evolution/mail/display/show_deleted",
			       !atoi (state), NULL);
	
	if (!(fb->folder && (fb->folder->folder_flags & CAMEL_FOLDER_IS_TRASH)))
		message_list_set_hidedeleted (fb->message_list, atoi (state));
}

void
folder_browser_set_message_display_style (BonoboUIComponent           *component,
					  const char                  *path,
					  Bonobo_UIComponent_EventType type,
					  const char                  *state,
					  gpointer                     user_data)
{
	extern char *message_display_styles[];
	FolderBrowser *fb = user_data;
	GConfClient *gconf;
	int i;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED
	    || atoi (state) == 0
	    || fb->message_list == NULL)
		return;
	
	gconf = mail_config_get_gconf_client ();
	
	for (i = 0; i < MAIL_CONFIG_DISPLAY_MAX; i++) {
		if (strstr (message_display_styles[i], path)) {
			fb->mail_display->display_style = i;
			mail_display_redisplay (fb->mail_display, TRUE);
			
			if (fb->pref_master)
				gconf_client_set_int (gconf, "/apps/evolution/mail/display/message_style", i, NULL);
			
			return;
		}
	}
}

void
folder_browser_charset_changed (BonoboUIComponent           *component,
				const char                  *path,
				Bonobo_UIComponent_EventType type,
				const char                  *state,
				gpointer                     user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	const char *charset;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED
	    || fb->message_list == NULL)
		return;
	
	if (atoi (state)) {
		/* Charset menu names are "Charset-%s" where %s is the charset name */
		charset = path + strlen ("Charset-");
		if (!strcmp (charset, FB_DEFAULT_CHARSET))
			charset = NULL;
		
		mail_display_set_charset (fb->mail_display, charset);
	}
}

static void vfolder_type_uid(CamelFolder *folder, const char *uid, const char *uri, int type);

static void
vfolder_type_current(FolderBrowser *fb, int type)
{
	GPtrArray *uids;
	int i;
	
	/* get uid */
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (uids->len == 1)
		vfolder_type_uid (fb->folder, (char *)uids->pdata[0], fb->uri, type);
	
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}

/* external api to vfolder/filter on X, based on current message */
void vfolder_subject (GtkWidget *w, FolderBrowser *fb) { vfolder_type_current(fb, AUTO_SUBJECT); }
void vfolder_sender (GtkWidget *w, FolderBrowser *fb) { vfolder_type_current(fb, AUTO_FROM); }
void vfolder_recipient (GtkWidget *w, FolderBrowser *fb) { vfolder_type_current(fb, AUTO_TO); }
void vfolder_mlist (GtkWidget *w, FolderBrowser *fb) { vfolder_type_current(fb, AUTO_MLIST); }

static void filter_type_uid (CamelFolder *folder, const char *uid, const char *source, int type);

static void
filter_type_current (FolderBrowser *fb, int type)
{
	GPtrArray *uids;
	int i;
	const char *source;
	
	if (folder_browser_is_sent (fb) || folder_browser_is_outbox (fb))
		source = FILTER_SOURCE_OUTGOING;
	else
		source = FILTER_SOURCE_INCOMING;
	
	/* get uid */
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	
	if (uids->len == 1)
		filter_type_uid (fb->folder, (char *)uids->pdata[0], source, type);
	
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}

void filter_subject (GtkWidget *w, FolderBrowser *fb) { filter_type_current (fb, AUTO_SUBJECT); }
void filter_sender (GtkWidget *w, FolderBrowser *fb) { filter_type_current (fb, AUTO_FROM); }
void filter_recipient (GtkWidget *w, FolderBrowser *fb) { filter_type_current (fb, AUTO_TO); }
void filter_mlist (GtkWidget *w, FolderBrowser *fb) { filter_type_current (fb, AUTO_MLIST); }

/* ************************************************************ */

/* popup api to vfolder/filter on X, based on current selection */
struct _filter_data {
	CamelFolder *folder;
	const char *source;
	char *uid;
	int type;
	char *uri;
	char *mlist;
};

static void
filter_data_free (struct _filter_data *fdata)
{
	g_free (fdata->uid);
	g_free (fdata->uri);
	if (fdata->folder)
		camel_object_unref (fdata->folder);
	g_free (fdata->mlist);
	g_free (fdata);
}

static void
vfolder_type_got_message(CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *d)
{
	struct _filter_data *data = d;
	
	if (msg)
		vfolder_gui_add_from_message(msg, data->type, data->uri);
	
	filter_data_free (data);
}

static void
vfolder_type_uid(CamelFolder *folder, const char *uid, const char *uri, int type)
{
	struct _filter_data *data;
	
	data = g_malloc0(sizeof(*data));
	data->type = type;
	data->uri = g_strdup(uri);
	mail_get_message(folder, uid, vfolder_type_got_message, data, mail_thread_new);
}

static void vfolder_subject_uid (GtkWidget *w, struct _filter_data *fdata)	{ vfolder_type_uid(fdata->folder, fdata->uid, fdata->uri, AUTO_SUBJECT); }
static void vfolder_sender_uid(GtkWidget *w, struct _filter_data *fdata)	{ vfolder_type_uid(fdata->folder, fdata->uid, fdata->uri, AUTO_FROM); }
static void vfolder_recipient_uid(GtkWidget *w, struct _filter_data *fdata)	{ vfolder_type_uid(fdata->folder, fdata->uid, fdata->uri, AUTO_TO); }
static void vfolder_mlist_uid(GtkWidget *w, struct _filter_data *fdata)		{ vfolder_type_uid(fdata->folder, fdata->uid, fdata->uri, AUTO_MLIST); }

static void
filter_type_got_message(CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *d)
{
	struct _filter_data *data = d;
	
	if (msg)
		filter_gui_add_from_message(msg, data->source, data->type);
	
	filter_data_free (data);
}

static void
filter_type_uid(CamelFolder *folder, const char *uid, const char *source, int type)
{
	struct _filter_data *data;
	
	data = g_malloc0(sizeof(*data));
	data->type = type;
	data->source = source;
	mail_get_message(folder, uid, filter_type_got_message, data, mail_thread_new);
}

static void filter_subject_uid (GtkWidget *w, struct _filter_data *fdata)	{ filter_type_uid(fdata->folder, fdata->uid, fdata->source, AUTO_SUBJECT); }
static void filter_sender_uid(GtkWidget *w, struct _filter_data *fdata)		{ filter_type_uid(fdata->folder, fdata->uid, fdata->source, AUTO_FROM); }
static void filter_recipient_uid(GtkWidget *w, struct _filter_data *fdata)	{ filter_type_uid(fdata->folder, fdata->uid, fdata->source, AUTO_TO); }
static void filter_mlist_uid(GtkWidget *w, struct _filter_data *fdata)		{ filter_type_uid(fdata->folder, fdata->uid, fdata->source, AUTO_MLIST); }

void
hide_none(GtkWidget *w, FolderBrowser *fb)
{
	message_list_hide_clear (fb->message_list);
}

void
hide_selected(GtkWidget *w, FolderBrowser *fb)
{
	GPtrArray *uids;
	int i;

	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	message_list_hide_uids (fb->message_list, uids);
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}

void
hide_deleted(GtkWidget *w, FolderBrowser *fb)
{
	MessageList *ml = fb->message_list;
	
	message_list_hide_add(ml, "(match-all (system-flag \"deleted\"))", ML_HIDE_SAME, ML_HIDE_SAME);
}

void
hide_read(GtkWidget *w, FolderBrowser *fb)
{
	MessageList *ml = fb->message_list;
	
	message_list_hide_add(ml, "(match-all (system-flag \"seen\"))", ML_HIDE_SAME, ML_HIDE_SAME);
}

/* dum de dum, about the 3rd copy of this function throughout the mailer/camel */
static const char *
strip_re(const char *subject)
{
	const unsigned char *s, *p;
	
	s = (unsigned char *) subject;
	
	while (*s) {
		while(isspace (*s))
			s++;
		if (s[0] == 0)
			break;
		if ((s[0] == 'r' || s[0] == 'R')
		    && (s[1] == 'e' || s[1] == 'E')) {
			p = s+2;
			while (isdigit(*p) || (ispunct(*p) && (*p != ':')))
				p++;
			if (*p == ':') {
				s = p + 1;
			} else
				break;
		} else
			break;
	}
	return (char *) s;
}

void
hide_subject(GtkWidget *w, FolderBrowser *fb)
{
	const char *subject;
	GString *expr;
	
	if (fb->mail_display->current_message) {
		subject = camel_mime_message_get_subject(fb->mail_display->current_message);
		if (subject) {
			subject = strip_re(subject);
			if (subject && subject[0]) {
				expr = g_string_new ("(match-all (header-contains \"subject\" ");
				e_sexp_encode_string (expr, subject);
				g_string_append (expr, "))");
				message_list_hide_add (fb->message_list, expr->str, ML_HIDE_SAME, ML_HIDE_SAME);
				g_string_free (expr, TRUE);
				return;
			}
		}
	}
}

void
hide_sender (GtkWidget *w, FolderBrowser *fb)
{
	const CamelInternetAddress *from;
	const char *real, *addr;
	GString *expr;
	
	if (fb->mail_display->current_message) {
		from = camel_mime_message_get_from (fb->mail_display->current_message);
		if (camel_internet_address_get (from, 0, &real, &addr)) {
			expr = g_string_new ("(match-all (header-contains \"from\" ");
			e_sexp_encode_string (expr, addr);
			g_string_append (expr, "))");
			message_list_hide_add (fb->message_list, expr->str, ML_HIDE_SAME, ML_HIDE_SAME);
			g_string_free (expr, TRUE);
			return;
		}
	}
}

struct _label_data {
	FolderBrowser *fb;
	const char *label;
};

static void
set_msg_label (GtkWidget *widget, gpointer user_data)
{
	struct _label_data *data = user_data;
	GPtrArray *uids;
	int i;
	
	uids = g_ptr_array_new ();
	message_list_foreach (data->fb->message_list, enumerate_msg, uids);
	for (i = 0; i < uids->len; i++)
		camel_folder_set_message_user_tag (data->fb->folder, uids->pdata[i], "label", data->label);
	g_ptr_array_free (uids, TRUE);
}

static void
label_closures_free (GPtrArray *closures)
{
	struct _label_data *data;
	int i;
	
	for (i = 0; i < closures->len; i++) {
		data = closures->pdata[i];
		g_object_unref (data->fb);
		g_free (data);
	}
	g_ptr_array_free (closures, TRUE);
}

static void
mark_as_seen_cb (GtkWidget *widget, void *user_data)
{
	mark_as_seen (NULL, user_data, NULL);
}

static void
mark_as_unseen_cb (GtkWidget *widget, void *user_data)
{
	mark_as_unseen (NULL, user_data, NULL);
}

static void
mark_as_important_cb (GtkWidget *widget, void *user_data)
{
	mark_as_important (NULL, user_data, NULL);
}

static void
mark_as_unimportant_cb (GtkWidget *widget, void *user_data)
{
	mark_as_unimportant (NULL, user_data, NULL);
}

enum {
	SELECTION_SET              = 1<<1,
	CAN_MARK_READ              = 1<<2,
	CAN_MARK_UNREAD            = 1<<3,
	CAN_DELETE                 = 1<<4,
	CAN_UNDELETE               = 1<<5,
	IS_MAILING_LIST            = 1<<6,
	CAN_RESEND                 = 1<<7,
	CAN_MARK_IMPORTANT         = 1<<8,
	CAN_MARK_UNIMPORTANT       = 1<<9,
	CAN_FLAG_FOR_FOLLOWUP      = 1<<10,
	CAN_FLAG_COMPLETED         = 1<<11,
	CAN_CLEAR_FLAG             = 1<<12,
	CAN_ADD_SENDER             = 1<<13
};

#define MLIST_VFOLDER (3)
#define MLIST_FILTER (8)

static EPopupMenu filter_menu[] = {
	E_POPUP_ITEM_CC (N_("VFolder on _Subject"),        G_CALLBACK (vfolder_subject_uid),   NULL, SELECTION_SET),
	E_POPUP_ITEM_CC (N_("VFolder on Se_nder"),         G_CALLBACK (vfolder_sender_uid),    NULL, SELECTION_SET),
	E_POPUP_ITEM_CC (N_("VFolder on _Recipients"),     G_CALLBACK (vfolder_recipient_uid), NULL, SELECTION_SET),
	E_POPUP_ITEM_CC (N_("VFolder on Mailing _List"),   G_CALLBACK (vfolder_mlist_uid),     NULL, SELECTION_SET | IS_MAILING_LIST),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM_CC (N_("Filter on Sub_ject"),         G_CALLBACK (filter_subject_uid),    NULL, SELECTION_SET),
	E_POPUP_ITEM_CC (N_("Filter on Sen_der"),          G_CALLBACK (filter_sender_uid),     NULL, SELECTION_SET),
	E_POPUP_ITEM_CC (N_("Filter on Re_cipients"),      G_CALLBACK (filter_recipient_uid),  NULL, SELECTION_SET),
	E_POPUP_ITEM_CC (N_("Filter on _Mailing List"),    G_CALLBACK (filter_mlist_uid),      NULL, SELECTION_SET | IS_MAILING_LIST),
	
	E_POPUP_TERMINATOR
};

static EPopupMenu label_menu[] = {
	E_POPUP_PIXMAP_WIDGET_ITEM_CC (N_("None"), NULL, G_CALLBACK (set_msg_label), NULL, 0),
	E_POPUP_SEPARATOR,
	E_POPUP_PIXMAP_WIDGET_ITEM_CC (NULL, NULL, G_CALLBACK (set_msg_label), NULL, 0),
	E_POPUP_PIXMAP_WIDGET_ITEM_CC (NULL, NULL, G_CALLBACK (set_msg_label), NULL, 0),
	E_POPUP_PIXMAP_WIDGET_ITEM_CC (NULL, NULL, G_CALLBACK (set_msg_label), NULL, 0),
	E_POPUP_PIXMAP_WIDGET_ITEM_CC (NULL, NULL, G_CALLBACK (set_msg_label), NULL, 0),
	E_POPUP_PIXMAP_WIDGET_ITEM_CC (NULL, NULL, G_CALLBACK (set_msg_label), NULL, 0),
	E_POPUP_TERMINATOR
};

static EPopupMenu context_menu[] = {
	E_POPUP_ITEM (N_("_Open"),                    G_CALLBACK (open_msg),          0),
	E_POPUP_ITEM (N_("_Edit as New Message..."),  G_CALLBACK (resend_msg),        CAN_RESEND),
	E_POPUP_ITEM (N_("_Save As..."),              G_CALLBACK (save_msg),          0),
	E_POPUP_ITEM (N_("_Print"),                   G_CALLBACK (print_msg),         0),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("_Reply to Sender"),         G_CALLBACK (reply_to_sender),   0),
	E_POPUP_ITEM (N_("Reply to _List"),           G_CALLBACK (reply_to_list),     0),
	E_POPUP_ITEM (N_("Reply to _All"),            G_CALLBACK (reply_to_all),      0),
	E_POPUP_ITEM (N_("_Forward"),                 G_CALLBACK (forward),           0),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("Follo_w Up..."),            G_CALLBACK (flag_for_followup),       CAN_FLAG_FOR_FOLLOWUP),
	E_POPUP_ITEM (N_("Fla_g Completed"),          G_CALLBACK (flag_followup_completed), CAN_FLAG_COMPLETED),
	E_POPUP_ITEM (N_("Cl_ear Flag"),              G_CALLBACK (flag_followup_clear),     CAN_CLEAR_FLAG),
	
	/* separator here? */
	
	E_POPUP_ITEM (N_("Mar_k as Read"),            G_CALLBACK (mark_as_seen_cb),        CAN_MARK_READ),
	E_POPUP_ITEM (N_("Mark as _Unread"),          G_CALLBACK (mark_as_unseen_cb),      CAN_MARK_UNREAD),
	E_POPUP_ITEM (N_("Mark as _Important"),       G_CALLBACK (mark_as_important_cb),   CAN_MARK_IMPORTANT),
	E_POPUP_ITEM (N_("_Mark as Unimportant"),     G_CALLBACK (mark_as_unimportant_cb), CAN_MARK_UNIMPORTANT),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("_Delete"),                  G_CALLBACK (delete_msg),          CAN_DELETE),
	E_POPUP_ITEM (N_("U_ndelete"),                G_CALLBACK (undelete_msg),        CAN_UNDELETE),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("Mo_ve to Folder..."),       G_CALLBACK (move_msg_cb),          0),
	E_POPUP_ITEM (N_("_Copy to Folder..."),       G_CALLBACK (copy_msg_cb),          0),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_SUBMENU (N_("Label"),                 label_menu,                             0),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("Add Sender to Address_book"), G_CALLBACK (addrbook_sender), SELECTION_SET | CAN_ADD_SENDER),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_ITEM (N_("Appl_y Filters"),             G_CALLBACK (apply_filters),      0),
	
	E_POPUP_SEPARATOR,
	
	E_POPUP_SUBMENU (N_("Crea_te Rule From Message"), filter_menu,                        SELECTION_SET),
	
	E_POPUP_TERMINATOR
};

/* Note: this must be kept in sync with the context_menu!!! */
static char *context_pixmaps[] = {
	NULL, /* Open */
	NULL, /* Edit */
	"save-as-16.png",
	"print.xpm",
	NULL,
	"reply.xpm",
	NULL, /* Reply to List */
	"reply_to_all.xpm",
	"forward.xpm",
	NULL,
	"flag-for-followup-16.png",
	NULL, /* Flag */
	NULL, /* Clear */
	"mail-read.xpm",
	"mail-new.xpm",
	"priority-high.xpm",
	NULL, /* Mark as Unimportant */
	NULL,
	"evolution-trash-mini.png",
	"undelete_message-16.png",
	NULL,
	"move_message.xpm",
	"copy_16_message.xpm",
	NULL,
	NULL, /* Label */
	NULL,
	NULL, /* Add Sender to Addressbook */
	NULL,
	NULL, /* Apply Filters */
	NULL,
	NULL, /* Create Rule from Message */
};

struct cmpf_data {
	ETree *tree;
	int row, col;
};

static void
context_menu_position_func (GtkMenu *menu, gint *x, gint *y,
			    gboolean *push_in, gpointer user_data)
{
	int tx, ty, tw, th;
	struct cmpf_data *closure = user_data;
	
	gdk_window_get_origin (GTK_WIDGET (closure->tree)->window, x, y);
	e_tree_get_cell_geometry (closure->tree, closure->row, closure->col,
				  &tx, &ty, &tw, &th);
	*x += tx + tw / 2;
	*y += ty + th / 2;
}

static void
setup_popup_icons (void)
{
	int i;
	
	for (i = 0; context_menu[i].name; i++) {
		if (context_pixmaps[i]) {
			char *filename;
			
			filename = g_strdup_printf ("%s/%s", EVOLUTION_IMAGES, context_pixmaps[i]);
			context_menu[i].pixmap_widget = gtk_image_new_from_file (filename);
			g_free (filename);
		}
	}
}

/* handle context menu over message-list */
static int
on_right_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, FolderBrowser *fb)
{
	struct _filter_data *fdata = NULL;
	GPtrArray *uids, *closures;
	CamelMessageInfo *info;
	GSList *labels;
	int enable_mask = 0;
	int hide_mask = 0;
	char *mlist = NULL;
	GtkMenu *menu;
	int i;
	
	if (!folder_browser_is_sent (fb)) {
		enable_mask |= CAN_RESEND;
		hide_mask |= CAN_RESEND;
	} else {
		enable_mask |= CAN_ADD_SENDER;
		hide_mask |= CAN_ADD_SENDER;
	}
	
	if (folder_browser_is_drafts (fb)) {
		enable_mask |= CAN_ADD_SENDER;
		hide_mask |= CAN_ADD_SENDER;
	}
	
	if (fb->folder == outbox_folder) {
		enable_mask |= CAN_ADD_SENDER;
		hide_mask |= CAN_ADD_SENDER;
	}
	
	enable_mask |= SELECTION_SET;
	
	/* get a list of uids */
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, enumerate_msg, uids);
	if (uids->len >= 1) {
		/* gray-out any items we don't need */
		gboolean have_deleted = FALSE;
		gboolean have_undeleted = FALSE;
		gboolean have_seen = FALSE;
		gboolean have_unseen = FALSE;
		gboolean have_important = FALSE;
		gboolean have_unimportant = FALSE;
		gboolean have_flag_for_followup = FALSE;
		gboolean have_flag_completed = FALSE;
		gboolean have_flag_incomplete = FALSE;
		gboolean have_unflagged = FALSE;
		const char *tag;
		
		for (i = 0; i < uids->len; i++) {
			info = camel_folder_get_message_info (fb->folder, uids->pdata[i]);
			if (info == NULL)
				continue;
			
			if (i == 0 && uids->len == 1) {
				const char *mname, *p;
				char c, *o;
				
				/* used by filter/vfolder from X callbacks */
				fdata = g_malloc0(sizeof(*fdata));
				fdata->uid = g_strdup(uids->pdata[i]);
				fdata->uri = g_strdup(fb->uri);
				fdata->folder = fb->folder;
				camel_object_ref((CamelObject *)fdata->folder);
				if (folder_browser_is_sent (fb) || folder_browser_is_outbox (fb))
					fdata->source = FILTER_SOURCE_OUTGOING;
				else
					fdata->source = FILTER_SOURCE_INCOMING;
				
				enable_mask &= ~SELECTION_SET;
				mname = camel_message_info_mlist(info);
				if (mname && mname[0]) {
					fdata->mlist = g_strdup(mname);
					
					/* Escape the mailing list name before showing it */
					mlist = g_alloca ((strlen (mname) * 2) + 1);
					p = mname;
					o = mlist;
					while ((c = *p++)) {
						if (c == '_')
							*o++ = '_';
						*o++ = c;
					}
					*o = 0;
				}
			}
			
			if (info->flags & CAMEL_MESSAGE_SEEN)
				have_seen = TRUE;
			else
				have_unseen = TRUE;
			
			if (info->flags & CAMEL_MESSAGE_DELETED)
				have_deleted = TRUE;
			else
				have_undeleted = TRUE;
			
			if (info->flags & CAMEL_MESSAGE_FLAGGED)
				have_important = TRUE;
			else
				have_unimportant = TRUE;
			
			tag = camel_tag_get (&info->user_tags, "follow-up");
			if (tag && *tag) {
				have_flag_for_followup = TRUE;
				tag = camel_tag_get (&info->user_tags, "completed-on");
				if (tag && *tag)
					have_flag_completed = TRUE;
				else
					have_flag_incomplete = TRUE;
			} else
				have_unflagged = TRUE;
			
			camel_folder_free_message_info (fb->folder, info);
			
			if (have_seen && have_unseen && have_deleted && have_undeleted)
				break;
		}
		
		if (!have_unseen)
			enable_mask |= CAN_MARK_READ;
		if (!have_seen)
			enable_mask |= CAN_MARK_UNREAD;
		
		if (!have_undeleted)
			enable_mask |= CAN_DELETE;
		if (!have_deleted)
			enable_mask |= CAN_UNDELETE;
		
		if (!have_unimportant)
			enable_mask |= CAN_MARK_IMPORTANT;
		if (!have_important)
			enable_mask |= CAN_MARK_UNIMPORTANT;
		
		if (!have_flag_for_followup)
			enable_mask |= CAN_CLEAR_FLAG;
		if (!have_unflagged)
			enable_mask |= CAN_FLAG_FOR_FOLLOWUP;
		if (!have_flag_incomplete)
			enable_mask |= CAN_FLAG_COMPLETED;
		
		/*
		 * Hide items that wont get used.
		 */
		if (!(have_unseen && have_seen)) {
			if (have_seen)
				hide_mask |= CAN_MARK_READ;
			else
				hide_mask |= CAN_MARK_UNREAD;
		}
		
		if (!(have_undeleted && have_deleted)) {
			if (have_deleted)
				hide_mask |= CAN_DELETE;
			else
				hide_mask |= CAN_UNDELETE;
		}
		
		if (!(have_important && have_unimportant)) {
			if (have_important)
				hide_mask |= CAN_MARK_IMPORTANT;
			else
				hide_mask |= CAN_MARK_UNIMPORTANT;
		}
		
		if (!have_flag_for_followup)
			hide_mask |= CAN_CLEAR_FLAG;
		if (!have_unflagged)
			hide_mask |= CAN_FLAG_FOR_FOLLOWUP;
		if (!have_flag_incomplete)
			hide_mask |= CAN_FLAG_COMPLETED;
	}
	
	/* free uids */
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
	
	/* generate the "Filter on Mailing List" menu item name */
	if (mlist == NULL) {
		enable_mask |= IS_MAILING_LIST;
		filter_menu[MLIST_FILTER].name = g_strdup (_("Filter on _Mailing List"));
		filter_menu[MLIST_VFOLDER].name = g_strdup (_("VFolder on M_ailing List"));
	} else {
		filter_menu[MLIST_FILTER].name = g_strdup_printf (_("Filter on _Mailing List (%s)"), mlist);
		filter_menu[MLIST_VFOLDER].name = g_strdup_printf (_("VFolder on M_ailing List (%s)"), mlist);
	}
	
	/* create the label/colour menu */
	closures = g_ptr_array_new ();
	label_menu[0].closure = g_new (struct _label_data, 1);
	g_ptr_array_add (closures, label_menu[0].closure);
	g_object_ref (fb);
	((struct _label_data *) label_menu[0].closure)->fb = fb;
	((struct _label_data *) label_menu[0].closure)->label = NULL;
	
	i = 0;
	labels = mail_config_get_labels ();
	while (labels != NULL && i < 5) {
		struct _label_data *closure;
		MailConfigLabel *label;
		GdkPixmap *pixmap;
		GdkColormap *map;
		GdkColor colour;
		GdkGC *gc;
		
		label = labels->data;
		gdk_color_parse (label->colour, &colour);
		map = gdk_colormap_get_system ();
		gdk_color_alloc (map, &colour);
		
		pixmap = gdk_pixmap_new (GTK_WIDGET (fb)->window, 16, 16, -1);
		gc = gdk_gc_new (GTK_WIDGET (fb)->window);
		gdk_gc_set_foreground (gc, &colour);
		gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, 16, 16);
		gdk_gc_unref (gc);
		
		closure = g_new (struct _label_data, 1);
		g_object_ref (fb);
		closure->fb = fb;
		closure->label = label->tag;
		
		g_ptr_array_add (closures, closure);
		
		label_menu[i + 2].name = label->name;
		label_menu[i + 2].pixmap_widget = gtk_image_new_from_pixmap (pixmap, NULL);
		label_menu[i + 2].closure = closure;
		
		i++;
		labels = labels->next;
	}
	
	setup_popup_icons ();
	
	for (i = 0; i < sizeof (filter_menu) / sizeof (filter_menu[0]); i++)
		filter_menu[i].closure = fdata;
	
	menu = e_popup_menu_create (context_menu, enable_mask, hide_mask, fb);
	e_auto_kill_popup_menu_on_selection_done (menu);
	
	g_object_set_data_full ((GObject *) menu, "label_closures", closures, (GtkDestroyNotify) label_closures_free);
	
	if (fdata)
		g_object_set_data_full ((GObject *) menu, "filter_data", fdata, (GtkDestroyNotify) filter_data_free);
	
	if (event->type == GDK_KEY_PRESS) {
		struct cmpf_data closure;
		
		closure.tree = tree;
		closure.row = row;
		closure.col = col;
		gtk_menu_popup (menu, NULL, NULL, context_menu_position_func,
				&closure, 0, event->key.time);
	} else {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
				event->button.button, event->button.time);
	}
	
	g_free (filter_menu[MLIST_FILTER].name);
	g_free (filter_menu[MLIST_VFOLDER].name);
	
	return TRUE;
}

static int
html_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	FolderBrowser *fb = data;
	HTMLEngine *engine;
	HTMLPoint *point;
	ETreePath *path;
	int row;
	
	if (event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;
	
	engine = GTK_HTML (widget)->engine;
	point = html_engine_get_point_at (engine, event->x, event->y, FALSE);
	
	if (point) {
		/* don't popup a menu if the mouse is hovering over a
                   url or a source image because those situations are
                   handled in mail-display.c's button_press_event
                   callback */
		const char *src, *url;
		
		url = html_object_get_url (point->object);
		src = html_object_get_src (point->object);
		
		if (url || src) {
			html_point_destroy (point);
			return FALSE;
		}
		
		html_point_destroy (point);
	}	
	
	path = e_tree_get_cursor (fb->message_list->tree);
	row = e_tree_row_of_node (fb->message_list->tree, path);
	
	on_right_click (fb->message_list->tree, row, path, 2,
			(GdkEvent *) event, fb);
	
	return TRUE;
}

static int
on_key_press (GtkWidget *widget, GdkEventKey *key, gpointer data)
{
	FolderBrowser *fb = data;
	ETreePath *path;
	int row;
	
	if (key->state & GDK_CONTROL_MASK)
		return FALSE;
	
	path = e_tree_get_cursor (fb->message_list->tree);
	row = e_tree_row_of_node (fb->message_list->tree, path);
	
	switch (key->keyval) {
	case GDK_Delete:
	case GDK_KP_Delete:
		delete_msg (NULL, fb);
		return TRUE;
		
	case GDK_Menu:
		on_right_click (fb->message_list->tree, row, path, 2,
				(GdkEvent *)key, fb);
		return TRUE;
	case '!':
		toggle_as_important (NULL, fb, NULL);
		return TRUE;
	}
	
	return FALSE;
}

static int
etree_key (ETree *tree, int row, ETreePath path, int col, GdkEvent *ev, FolderBrowser *fb)
{
	GtkAdjustment *vadj;
	gfloat page_size;
	
	if ((ev->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;
	
	vadj = gtk_scrolled_window_get_vadjustment (fb->mail_display->scroll);
	page_size = vadj->page_size - vadj->step_increment;
	
	switch (ev->key.keyval) {
	case GDK_space:
		/* Work around Ximian 4939 */
		if (vadj->upper < vadj->page_size)
			break;
		if (vadj->value < vadj->upper - vadj->page_size - page_size)
			vadj->value += page_size;
		else
			vadj->value = vadj->upper - vadj->page_size;
		gtk_adjustment_value_changed (vadj);
		break;
	case GDK_BackSpace:
		if (vadj->value > vadj->lower + page_size)
			vadj->value -= page_size;
		else
			vadj->value = vadj->lower;
		gtk_adjustment_value_changed (vadj);
		break;
	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_ISO_Enter:
       		open_msg (NULL, fb);
		break;
	default:
		return on_key_press ((GtkWidget *)tree, (GdkEventKey *)ev, fb);
	}
	
	return TRUE;
}

static void
on_double_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, FolderBrowser *fb)
{
	/* Ignore double-clicks on columns where single-click doesn't
	 * just select.
	 */
	if (MESSAGE_LIST_COLUMN_IS_ACTIVE (col))
		return;
	
	open_msg (NULL, fb);
}

static void
on_selection_changed (GtkObject *obj, gpointer user_data)
{
	FolderBrowser *fb = FOLDER_BROWSER (user_data);
	FolderBrowserSelectionState state;
	
	/* we can get this signal at strange times...
	 * if no uicomp, don't even bother */
	
	if (fb->uicomp == NULL)
		return;
	
	switch (e_selection_model_selected_count (E_SELECTION_MODEL (obj))) {
	case 0:
		state = FB_SELSTATE_NONE;
		break;
	case 1:
		state = FB_SELSTATE_SINGLE;
		break;
	default:
		state = FB_SELSTATE_MULTIPLE;
		break;
	}
	
	folder_browser_ui_set_selection_state (fb, state);
	
	update_status_bar_idle (fb);
}


static void
on_cursor_activated (ETree *tree, int row, ETreePath path, gpointer user_data)
{
	on_selection_changed ((GtkObject *)tree, user_data);
}

static gboolean
fb_resize_cb (GtkWidget *w, GdkEventButton *e, FolderBrowser *fb)
{
	GConfClient *gconf;
	
	if (GTK_WIDGET_REALIZED (w) && fb->preview_shown) {
		gconf = mail_config_get_gconf_client ();
		gconf_client_set_int (gconf, "/apps/evolution/mail/display/paned_size", gtk_paned_get_position (GTK_PANED (w)), NULL);
	}
	
	return FALSE;
}

/* hack to get around the fact setting the paned size doesn't work */
static void
paned_realised(GtkWidget *w, FolderBrowser *fb)
{
	GConfClient *gconf;
	int size;
	
	gconf = mail_config_get_gconf_client ();
	size = gconf_client_get_int (gconf, "/apps/evolution/mail/display/paned_size", NULL);
	gtk_paned_set_position (GTK_PANED (fb->vpaned), size);
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	extern RuleContext *search_context;
	ESelectionModel *esm;
	
	/* The panned container */
	fb->vpaned = gtk_vpaned_new ();
	g_signal_connect(fb->vpaned, "realize", G_CALLBACK(paned_realised), fb);
	gtk_widget_show (fb->vpaned);
	
	gtk_table_attach (GTK_TABLE (fb), fb->vpaned,
			  0, 1, 1, 3,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);
	
	/* quick-search bar */
	if (search_context) {
		const char *systemrules = g_object_get_data (G_OBJECT (search_context), "system");
		const char *userrules = g_object_get_data (G_OBJECT (search_context), "user");
		
		fb->search = e_filter_bar_new (search_context, systemrules, userrules,
					       folder_browser_config_search, fb);
		e_search_bar_set_menu ((ESearchBar *)fb->search, folder_browser_search_menu_items);
	}
	
	gtk_widget_show (GTK_WIDGET (fb->search));
	
	g_signal_connect (fb->search, "menu_activated",
			  G_CALLBACK (folder_browser_search_menu_activated), fb);
	g_signal_connect (fb->search, "search_activated",
			  G_CALLBACK (folder_browser_search_do_search), fb);
	g_signal_connect (fb->search, "query_changed",
			  G_CALLBACK (folder_browser_query_changed), fb);
	
	gtk_table_attach (GTK_TABLE (fb), GTK_WIDGET (fb->search),
			  0, 1, 0, 1,
			  GTK_FILL | GTK_EXPAND,
			  0,
			  0, 0);
	
	esm = e_tree_get_selection_model (E_TREE (fb->message_list->tree));
	g_signal_connect (esm, "selection_changed", G_CALLBACK (on_selection_changed), fb);
	g_signal_connect (esm, "cursor_activated", G_CALLBACK (on_cursor_activated), fb);
	fb->selection_state = FB_SELSTATE_NONE; /* default to none */

	gtk_paned_add1 (GTK_PANED (fb->vpaned), GTK_WIDGET (fb->message_list));
	gtk_widget_show (GTK_WIDGET (fb->message_list));
	
	fb->paned_resize_id = g_signal_connect (fb->vpaned, "button_release_event",
						G_CALLBACK (fb_resize_cb), fb);

	gtk_paned_add2 (GTK_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb));
}

/* mark the message seen if the current message still matches */
static gint 
do_mark_seen (gpointer data)
{
	FolderBrowser *fb = FOLDER_BROWSER (data);
	
	if (fb->new_uid && fb->loaded_uid && !strcmp (fb->new_uid, fb->loaded_uid)) {
		camel_folder_set_message_flags (fb->folder, fb->new_uid,
						CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	}
	
	return FALSE;
}

/* callback when we have the message to display, after async loading it (see below) */
/* if we have pending uid's, it means another was selected before we finished displaying
   the last one - so we cycle through and start loading the pending one immediately now */
static void
done_message_selected (CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *data)
{
	FolderBrowser *fb = data;
	CamelMessageInfo *info;
	GConfClient *gconf;
	int timeout;
	
	if (folder != fb->folder || fb->mail_display == NULL)
		return;
	
	gconf = mail_config_get_gconf_client ();
	timeout = gconf_client_get_int (gconf, "/apps/evolution/mail/display/mark_seen_timeout", NULL);
	
	info = camel_folder_get_message_info (fb->folder, uid);
	mail_display_set_message (fb->mail_display, (CamelMedium *) msg, fb->folder, info);
	if (info)
		camel_folder_free_message_info (fb->folder, info);
	
	/* FIXME: should this signal be emitted here?? */
	g_signal_emit (fb, folder_browser_signals[MESSAGE_LOADED], 0, uid);
	
	/* pain, if we have pending stuff, re-run */
	if (fb->pending_uid) {
		g_free (fb->loading_uid);
		fb->loading_uid = fb->pending_uid;
		fb->pending_uid = NULL;
		
		mail_get_message (fb->folder, fb->loading_uid, done_message_selected, fb, mail_thread_new);
		return;
	}
	
	g_free (fb->loaded_uid);
	fb->loaded_uid = fb->loading_uid;
	fb->loading_uid = NULL;
	
	folder_browser_ui_message_loaded (fb);
	
	/* if we are still on the same message, do the 'idle read' thing */
	if (fb->seen_id)
		gtk_timeout_remove (fb->seen_id);
	
	if (msg && gconf_client_get_bool (gconf, "/apps/evolution/mail/display/mark_seen", NULL)) {
		if (timeout > 0)
			fb->seen_id = gtk_timeout_add (timeout, do_mark_seen, fb);
		else
			do_mark_seen (fb);
	}
}

/* ok we waited enough, display it anyway (see below) */
static gboolean
do_message_selected (FolderBrowser *fb)
{
	d(printf ("%p: selecting uid %s (delayed)\n", fb, fb->new_uid ? fb->new_uid : "NONE"));
	
	fb->loading_id = 0;
	
	/* if we are loading, then set a pending, but leave the loading, coudl cancel here (?) */
	if (fb->loading_uid) {
		if (fb->new_uid == NULL || fb->pending_uid == NULL || strcmp(fb->pending_uid, fb->new_uid) != 0) {
			g_free (fb->pending_uid);
			fb->pending_uid = g_strdup (fb->new_uid);
		}
	} else {
		if (fb->new_uid) {
			if (fb->loaded_uid == NULL || strcmp(fb->new_uid, fb->loaded_uid) != 0) {
				fb->loading_uid = g_strdup (fb->new_uid);
				mail_get_message (fb->folder, fb->loading_uid, done_message_selected, fb, mail_thread_new);
			}
		} else {
			mail_display_set_message (fb->mail_display, NULL, NULL, NULL);
		}
	}
	
	return FALSE;
}

/* when a message is selected, wait a while before trying to display it */
static void
on_message_selected (MessageList *ml, const char *uid, FolderBrowser *fb)
{
	d(printf ("%p: selecting uid %s (direct)\n", fb, uid ? uid : "NONE"));
	
	if (fb->loading_id != 0)
		gtk_timeout_remove (fb->loading_id);
	
	g_free (fb->new_uid);
	fb->new_uid = g_strdup (uid);
	
	if (fb->preview_shown)
		fb->loading_id = gtk_timeout_add (100, (GtkFunction)do_message_selected, fb);
}

static gboolean
on_message_list_focus_in (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	FolderBrowser *fb = (FolderBrowser *) user_data;
	
	d(printf ("got focus!\n"));
	folder_browser_ui_message_list_focus (fb);
	
	return FALSE;
}

static gboolean
on_message_list_focus_out (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	FolderBrowser *fb = (FolderBrowser *) user_data;
	
	d(printf ("got unfocus!\n"));
	folder_browser_ui_message_list_unfocus (fb);
	
	return FALSE;
}

static void
folder_browser_init (FolderBrowser *fb)
{
	fb->async_event = mail_async_event_new ();
	fb->get_id = -1;
}

static void
my_folder_browser_init (FolderBrowser *fb)
{
	int i;
	
	fb->view_instance = NULL;
	fb->view_menus = NULL;
	
	fb->pref_master = FALSE;
	
	/*
	 * Setup parent class fields.
	 */ 
	GTK_TABLE (fb)->homogeneous = FALSE;
	gtk_table_resize (GTK_TABLE (fb), 1, 2);
	
	/*
	 * Our instance data
	 */
	fb->message_list = (MessageList *)message_list_new ();
	fb->mail_display = (MailDisplay *)mail_display_new ();
	
	fb->preview_shown = TRUE;
	
	g_signal_connect (fb->mail_display->html, "key_press_event",
			  G_CALLBACK (on_key_press), fb);
	g_signal_connect (fb->mail_display->html, "button_press_event",
			  G_CALLBACK (html_button_press_event), fb);
	
	g_signal_connect (fb->message_list->tree, "key_press",
			  G_CALLBACK (etree_key), fb);
	
	g_signal_connect (fb->message_list->tree, "right_click",
			  G_CALLBACK (on_right_click), fb);
	
	g_signal_connect (fb->message_list->tree, "double_click",
			  G_CALLBACK (on_double_click), fb);
	
	g_signal_connect (fb->message_list, "focus_in_event",
			  G_CALLBACK (on_message_list_focus_in), fb);
	
	g_signal_connect (fb->message_list, "focus_out_event",
			  G_CALLBACK (on_message_list_focus_out), fb);
	
	g_signal_connect (fb->message_list, "message_selected",
			  G_CALLBACK (on_message_selected), fb);
	
	/* drag & drop */
	e_tree_drag_source_set (fb->message_list->tree, GDK_BUTTON1_MASK,
				drag_types, num_drag_types, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	
	g_signal_connect (fb->message_list->tree, "tree_drag_data_get",
			  G_CALLBACK (message_list_drag_data_get), fb);
	
	e_tree_drag_dest_set (fb->message_list->tree, GTK_DEST_DEFAULT_ALL,
			      drag_types, num_drag_types, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	
	g_signal_connect (fb->message_list->tree, "tree_drag_data_received",
			  G_CALLBACK (message_list_drag_data_received), fb);
	
	/* cut, copy & paste */
	fb->invisible = gtk_invisible_new ();
	g_object_ref (fb->invisible);
	gtk_object_sink ((GtkObject *) fb->invisible);
	
	for (i = 0; i < num_paste_types; i++)
		gtk_selection_add_target (fb->invisible, clipboard_atom,
					  paste_types[i].target,
					  paste_types[i].info);
	
	g_signal_connect (fb->invisible, "selection_get",
			  G_CALLBACK (selection_get), fb);
	g_signal_connect (fb->invisible, "selection_clear_event",
			  G_CALLBACK (selection_clear_event), fb);
	g_signal_connect (fb->invisible, "selection_received",
			  G_CALLBACK (selection_received), fb);
	
	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (const GNOME_Evolution_Shell shell, const char *uri)
{
	CORBA_Environment ev;
	FolderBrowser *folder_browser;
	
	CORBA_exception_init (&ev);
	
	folder_browser = g_object_new (folder_browser_get_type (), NULL);
	
	my_folder_browser_init (folder_browser);
	
	folder_browser->shell = CORBA_Object_duplicate (shell, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		folder_browser->shell = CORBA_OBJECT_NIL;
		gtk_widget_destroy (GTK_WIDGET (folder_browser));
		CORBA_exception_free (&ev);
		return NULL;
	}
	
	CORBA_exception_free (&ev);
	
	if (uri) {
		folder_browser->uri = g_strdup (uri);
		folder_browser->meta = mail_tool_get_meta_data(uri);
		g_object_ref (folder_browser);
		folder_browser->get_id = mail_get_folder (folder_browser->uri, 0, got_folder,
							  folder_browser, mail_thread_new);
	}
	
	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);
