/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser.c: Folder browser top level component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000, 2001 Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkinvisible.h>
#include <gal/e-paned/e-vpaned.h>
#include <gal/e-table/e-table.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-unicode.h>

#include <libgnomeui/gnome-dialog-util.h>

#include <gtkhtml/htmlengine.h>
#include <gtkhtml/htmlengine-edit-cut-and-paste.h>

#include "filter/vfolder-rule.h"
#include "filter/vfolder-context.h"
#include "filter/filter-option.h"
#include "filter/filter-input.h"

#include "mail-search-dialogue.h"
#include "e-util/e-sexp.h"
#include "e-util/e-mktemp.h"
#include "folder-browser.h"
#include "e-searching-tokenizer.h"
#include "mail.h"
#include "mail-callbacks.h"
#include "mail-tools.h"
#include "message-list.h"
#include "mail-ops.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "folder-browser-ui.h"

#include "mail-local.h"
#include "mail-config.h"

#include <camel/camel-vtrash-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-mem.h>

#define d(x)

#define PARENT_TYPE (gtk_table_get_type ())

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

static GtkObjectClass *folder_browser_parent_class;

enum {
	FOLDER_LOADED,
	MESSAGE_LOADED,
	LAST_SIGNAL
};

static guint folder_browser_signals [LAST_SIGNAL] = {0, };

static void
folder_browser_destroy (GtkObject *object)
{
	FolderBrowser *folder_browser;
	CORBA_Environment ev;
	
	folder_browser = FOLDER_BROWSER (object);
	
	CORBA_exception_init (&ev);
	
	if (folder_browser->search_full)
		gtk_object_unref (GTK_OBJECT (folder_browser->search_full));
	
	if (folder_browser->sensitize_timeout_id)
		g_source_remove (folder_browser->sensitize_timeout_id);

	if (folder_browser->shell != CORBA_OBJECT_NIL)
		CORBA_Object_release (folder_browser->shell, &ev);

	if (folder_browser->uicomp)
		bonobo_object_unref (BONOBO_OBJECT (folder_browser->uicomp));
	
	g_free (folder_browser->uri);
	
	if (folder_browser->folder) {
		mail_sync_folder (folder_browser->folder, NULL, NULL);
		camel_object_unref (CAMEL_OBJECT (folder_browser->folder));
	}
	
	if (folder_browser->message_list)
		gtk_widget_destroy (GTK_WIDGET (folder_browser->message_list));
	
	if (folder_browser->mail_display)
		gtk_widget_destroy (GTK_WIDGET (folder_browser->mail_display));
	
	CORBA_exception_free (&ev);
	
	if (folder_browser->view_collection) {
		gtk_object_unref (GTK_OBJECT (folder_browser->view_collection));
		folder_browser->view_collection = NULL;
	}
	
	if (folder_browser->view_menus) {
		gtk_object_unref (GTK_OBJECT (folder_browser->view_menus));
		folder_browser->view_menus = NULL;
	}
	
	gtk_object_unref (GTK_OBJECT (folder_browser->invisible));
	if (folder_browser->clipboard_selection)
		g_byte_array_free (folder_browser->clipboard_selection, TRUE);
	
	folder_browser_parent_class->destroy (object);
}

static void
folder_browser_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = folder_browser_destroy;
	
	folder_browser_parent_class = gtk_type_class (PARENT_TYPE);
	
	folder_browser_signals[FOLDER_LOADED] =
		gtk_signal_new ("folder_loaded",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FolderBrowserClass, folder_loaded),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	
	folder_browser_signals[MESSAGE_LOADED] =
		gtk_signal_new ("message_loaded",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FolderBrowserClass, message_loaded),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	
	gtk_object_class_add_signals (object_class, folder_browser_signals, LAST_SIGNAL);
	
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
			return;
		}
		
		message = camel_folder_get_message (fb->folder, uids->pdata[0], NULL);
		g_free (uids->pdata[0]);
		
		if (uids->len == 1) {
			filename = camel_mime_message_get_subject (message);
			if (!filename)
				filename = "Unknown";
		} else
			filename = "mbox";
		
		uri_list = g_strdup_printf ("file://%s/%s", tmpdir, filename);
		
		fd = open (uri_list + 7, O_WRONLY | O_CREAT);
		if (fd == -1) {
			/* cleanup and abort */
			camel_object_unref (CAMEL_OBJECT (message));
			for (i = 1; i < uids->len; i++)
				g_free (uids->pdata[i]);
			g_ptr_array_free (uids, TRUE);
			g_free (uri_list);
			return;
		}
		
		stream = camel_stream_fs_new_with_fd (fd);
		
		camel_stream_write (stream, "From - \n", 8);
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
		camel_object_unref (CAMEL_OBJECT (message));
		for (i = 1; i < uids->len; i++) {
			message = camel_folder_get_message (fb->folder, uids->pdata[i], NULL);
			camel_stream_write (stream, "From - \n", 8);
			camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
			camel_object_unref (CAMEL_OBJECT (message));
			g_free (uids->pdata[i]);
		}
		
		camel_object_unref (CAMEL_OBJECT (stream));
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					uri_list, strlen (uri_list));
		g_free (uri_list);
	}
	break;
	case DND_TARGET_TYPE_MESSAGE_RFC822:
	{
		/* FIXME: this'll be fucking slow for the user... pthread this? */
		CamelStream *stream;
		GByteArray *bytes;
		
		bytes = g_byte_array_new ();
		stream = camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), bytes);
		
		for (i = 0; i < uids->len; i++) {
			CamelMimeMessage *message;
			
			message = camel_folder_get_message (fb->folder, uids->pdata[i], NULL);
			g_free (uids->pdata[i]);
			
			if (message) {			
				camel_stream_write (stream, "From - \n", 8);
				camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
				camel_object_unref (CAMEL_OBJECT (message));
			}
		}
		
		g_ptr_array_free (uids, TRUE);
		camel_object_unref (CAMEL_OBJECT (stream));
		
		gtk_selection_data_set (selection_data, selection_data->target, 8,
					bytes->data, bytes->len);
		
		g_byte_array_free (bytes, FALSE);
	}
	break;
	case DND_TARGET_TYPE_X_EVOLUTION_MESSAGE:
	{
		GByteArray *array;
		
		/* format: "uri uid1\0uid2\0uid3\0...\0uidn" */
		
		/* write the uri portion */
		array = g_byte_array_new ();
		g_byte_array_append (array, fb->uri, strlen (fb->uri));
		g_byte_array_append (array, " ", 1);
		
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
		
		g_byte_array_free (array, FALSE);
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
		camel_folder_append_message (dest, msg, info, ex);
		camel_object_unref (CAMEL_OBJECT (msg));
		
		if (camel_exception_is_set (ex))
			break;
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (CAMEL_OBJECT (mp));
}

static CamelFolder *
x_evolution_message_parse (char *in, unsigned int inlen, GPtrArray **uids)
{
	/* format: "uri uid1\0uid2\0uid3\0...\0uidn" */
	char *inptr, *inend, *uri;
	CamelFolder *folder;
	
	if (in == NULL)
		return NULL;
	
	inend = in + inlen;
	
	inptr = strchr (in, ' ');
	uri = g_strndup (in, inptr - in);
	
	folder = mail_tool_uri_to_folder (uri, NULL);
	g_free (uri);
	
	if (!folder)
		return NULL;
	
	/* split the uids */
	inptr++;
	*uids = g_ptr_array_new ();
	while (inptr < inend) {
		char *start = inptr;
		
		while (inptr < inend && *inptr)
			inptr++;
		
		g_ptr_array_add (*uids, g_strndup (start, inptr - start));
		inptr++;
	}
	
	return folder;
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
		folder = x_evolution_message_parse (selection_data->data, selection_data->length, &uids);
		if (folder == NULL)
			goto fail;
		
		if (uids == NULL) {
			camel_object_unref (CAMEL_OBJECT (folder));
			goto fail;
		}
		
		mail_transfer_messages (folder, uids, context->action == GDK_ACTION_MOVE,
					fb->uri, NULL, NULL);
		
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
		source = x_evolution_message_parse (bytes->data, bytes->len, &uids);
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
	
	source = x_evolution_message_parse (selection_data->data, selection_data->length, &uids);
	if (source == NULL)
		return;
	
	if (uids == NULL) {
		camel_object_unref (CAMEL_OBJECT (source));
		return;
	}
	
	mail_transfer_messages (source, uids, FALSE, fb->uri, NULL, NULL);
	
	camel_object_unref (CAMEL_OBJECT (source));
}

void
folder_browser_copy (GtkWidget *menuitem, FolderBrowser *fb)
{
	GPtrArray *uids = NULL;
	GByteArray *bytes;
	gboolean cut;
	int i;
	
	cut = menuitem == NULL;
	
	if (!GTK_WIDGET_HAS_FOCUS (fb->message_list)) {
		/* Copy text from the HTML Engine */
		html_engine_copy (fb->mail_display->html->engine);
		return;
	}
	
	if (fb->clipboard_selection) {
		g_byte_array_free (fb->clipboard_selection, TRUE);
		fb->clipboard_selection = NULL;
	}
	
	uids = g_ptr_array_new ();
	message_list_foreach (fb->message_list, add_uid, uids);
	
	/* format: "uri uid1\0uid2\0uid3\0...\0uidn" */
	
	/* write the uri portion */
	bytes = g_byte_array_new ();
	g_byte_array_append (bytes, fb->uri, strlen (fb->uri));
	g_byte_array_append (bytes, " ", 1);
	
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

static void
got_folder(char *uri, CamelFolder *folder, void *data)
{
	FolderBrowser *fb = data;
	
	d(printf ("got folder '%s' = %p\n", uri, folder));
	
	if (fb->folder == folder)
		goto done;
	
	if (fb->folder)
		camel_object_unref (CAMEL_OBJECT (fb->folder));
	g_free (fb->uri);
	fb->uri = g_strdup (uri);
	fb->folder = folder;
	
	if (folder == NULL)
		goto done;
	
	camel_object_ref (CAMEL_OBJECT (folder));
	
	gtk_widget_set_sensitive (GTK_WIDGET (fb->search), camel_folder_has_search_capability (folder));
	message_list_set_threaded (fb->message_list, mail_config_get_thread_list (fb->uri));
	message_list_set_folder (fb->message_list, folder,
				 folder_browser_is_drafts (fb) ||
				 folder_browser_is_sent (fb) ||
				 folder_browser_is_outbox (fb));
	vfolder_register_source (folder);

	mail_folder_cache_note_folder (fb->uri, folder);
	mail_folder_cache_note_fb (fb->uri, fb);

	/* when loading a new folder, nothing is selected initially */

	if (fb->uicomp)
		folder_browser_ui_set_selection_state (fb, FB_SELSTATE_NONE);

 done:
	gtk_object_unref (GTK_OBJECT (fb));
	
	gtk_signal_emit (GTK_OBJECT (fb), folder_browser_signals [FOLDER_LOADED], fb->uri);
}

gboolean
folder_browser_set_uri (FolderBrowser *folder_browser, const char *uri)
{
	if (uri && *uri) {
		gtk_object_ref((GtkObject *)folder_browser);
		mail_get_folder(uri, got_folder, folder_browser);
	}

	return TRUE;
}

void
folder_browser_set_ui_component (FolderBrowser *fb, BonoboUIComponent *uicomp)
{
	g_return_if_fail (IS_FOLDER_BROWSER (fb));
	
	if (fb->uicomp)
		bonobo_object_unref (BONOBO_OBJECT (fb->uicomp));

	if (uicomp)
		bonobo_object_ref (BONOBO_OBJECT (uicomp));

	fb->uicomp = uicomp;
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
	const GSList *accounts;
	MailConfigAccount *account;
	
	g_return_val_if_fail (IS_FOLDER_BROWSER (fb) && fb->uri, FALSE);
	
	if (fb->folder == drafts_folder)
		return TRUE;
	
	accounts = mail_config_get_accounts ();
	while (accounts) {
		account = accounts->data;
		if (account->drafts_folder_uri &&
		    !strcmp (account->drafts_folder_uri, fb->uri))
			return TRUE;
		accounts = accounts->next;
	}
	
	return FALSE;
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
	const GSList *accounts;
	MailConfigAccount *account;
	
	g_return_val_if_fail (IS_FOLDER_BROWSER (fb) && fb->uri, FALSE);
	
	if (fb->folder == sent_folder)
		return TRUE;
	
	accounts = mail_config_get_accounts ();
	while (accounts) {
		account = accounts->data;
		if (account->sent_folder_uri &&
		    !strcmp (account->sent_folder_uri, fb->uri))
			return TRUE;
		accounts = accounts->next;
	}
	
	return FALSE;
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
	int row, y, height;

	node = e_tree_get_cursor (fb->message_list->tree);
	if (!node)
		return -1;

	row = e_tree_row_of_node (fb->message_list->tree, node);

	if (row == -1)
		return 0;

	e_tree_get_cell_geometry (fb->message_list->tree, row, 0,
				  NULL, &y, NULL, &height);

	adj = e_scroll_frame_get_vadjustment (E_SCROLL_FRAME (fb->message_list));
	y += adj->value - ((mail_config_get_paned_size () - height) / 2);
	
	return y;
}

static void
set_cursor_pos (FolderBrowser *fb, int y)
{
	GtkAdjustment *adj;

	if (y == -1)
		return;

	adj = e_scroll_frame_get_vadjustment (E_SCROLL_FRAME (fb->message_list));
	gtk_adjustment_set_value (adj, (gfloat)y);
}

static gboolean do_message_selected(FolderBrowser *fb);

void
folder_browser_set_message_preview (FolderBrowser *folder_browser, gboolean show_message_preview)
{
	if (folder_browser->preview_shown == show_message_preview)
		return;

	folder_browser->preview_shown = show_message_preview;

	if (show_message_preview) {
		int y;
		y = save_cursor_pos (folder_browser);
		e_paned_set_position (E_PANED (folder_browser->vpaned),
				      mail_config_get_paned_size ());
		gtk_widget_show (GTK_WIDGET (folder_browser->mail_display));
		do_message_selected (folder_browser);
		set_cursor_pos (folder_browser, y);
	} else {
		e_paned_set_position (E_PANED (folder_browser->vpaned),
				      10000);
		gtk_widget_hide (GTK_WIDGET (folder_browser->mail_display));
		mail_display_set_message(folder_browser->mail_display, NULL);
	}
}

enum {
	ESB_SAVE,
};

static ESearchBarItem folder_browser_search_menu_items[] = {
	E_FILTERBAR_RESET,
	E_FILTERBAR_SAVE,
	{ N_("Create vFolder from Search"),  ESB_SAVE, NULL  },
	E_FILTERBAR_EDIT,
	{ NULL,                           -1, NULL           }
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
			e_searching_tokenizer_set_secondary_search_string (st, query);
		} else if(!strcmp(part->name, "sender")) {
			FilterInput *input = (FilterInput *)filter_part_find_element(part, "sender");
			if (input)
				filter_input_set_value(input, query);
		}
		
		partl = partl->next;
	}
	
	d(printf("configuring search for search string '%s', rule is '%s'\n", query, rule->name));
	
	mail_display_redisplay (fb->mail_display, FALSE);
}

static void
folder_browser_search_query_changed (ESearchBar *esb, FolderBrowser *fb)
{
	char *search_word;
	
	d(printf("query changed\n"));
	
	gtk_object_get (GTK_OBJECT (esb),
			"query", &search_word,
			NULL);

	message_list_set_search (fb->message_list, search_word);
	
	d(printf("query is %s\n", search_word));
	g_free(search_word);
	return;
}

void
folder_browser_toggle_preview (BonoboUIComponent           *component,
			       const char                  *path,
			       Bonobo_UIComponent_EventType type,
			       const char                  *state,
			       gpointer                     user_data)
{
	FolderBrowser *fb = user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	mail_config_set_show_preview (fb->uri, atoi (state));
	folder_browser_set_message_preview (fb, atoi (state));
}

void
folder_browser_toggle_threads (BonoboUIComponent           *component,
			       const char                  *path,
			       Bonobo_UIComponent_EventType type,
			       const char                  *state,
			       gpointer                     user_data)
{
	FolderBrowser *fb = user_data;
	
	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;
	
	mail_config_set_thread_list (fb->uri, atoi (state));
	message_list_set_threaded (fb->message_list, atoi (state));
}

void
folder_browser_toggle_hide_deleted (BonoboUIComponent           *component,
				    const char                  *path,
				    Bonobo_UIComponent_EventType type,
				    const char                  *state,
				    gpointer                     user_data)
{
	FolderBrowser *fb = user_data;

	if (type != Bonobo_UIComponent_STATE_CHANGED)
		return;

	if (!(fb->folder && CAMEL_IS_VTRASH_FOLDER(fb->folder)))
		mail_config_set_hide_deleted (atoi (state));
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
	int i;

	if (type != Bonobo_UIComponent_STATE_CHANGED || atoi(state) == 0)
		return;

	for (i = 0; i < MAIL_CONFIG_DISPLAY_MAX; i++) {
		if (strstr (message_display_styles[i], path)) {
			fb->mail_display->display_style = i;
			mail_display_redisplay (fb->mail_display, TRUE);

			if (fb->pref_master)
				mail_config_set_message_display_style (i);
			return;
		}
	}
}

void
vfolder_subject (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_SUBJECT, fb->uri);
}

void
vfolder_sender (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_FROM, fb->uri);
}

void
vfolder_recipient (GtkWidget *w, FolderBrowser *fb)
{
	vfolder_gui_add_from_message (fb->mail_display->current_message, AUTO_TO, fb->uri);
}

void
vfolder_mlist (GtkWidget *w, FolderBrowser *fb)
{
	char *name;

	g_return_if_fail (fb->mail_display->current_message != NULL);

	name = header_raw_check_mailing_list(&((CamelMimePart *)fb->mail_display->current_message)->headers);
	if (name) {
		g_strstrip (name);
		vfolder_gui_add_from_mlist(fb->mail_display->current_message, name, fb->uri);
		g_free(name);
	}
}

void
filter_subject (GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message (fb->mail_display->current_message, AUTO_SUBJECT);
}

void
filter_sender (GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message (fb->mail_display->current_message, AUTO_FROM);
}

void
filter_recipient (GtkWidget *w, FolderBrowser *fb)
{
	filter_gui_add_from_message (fb->mail_display->current_message, AUTO_TO);
}

void
filter_mlist (GtkWidget *w, FolderBrowser *fb)
{
	char *name;

	g_return_if_fail (fb->mail_display->current_message != NULL);

	name = header_raw_check_mailing_list(&((CamelMimePart *)fb->mail_display->current_message)->headers);
	if (name) {
		filter_gui_add_from_mlist(name);
		g_free(name);
	}
}

void
hide_none(GtkWidget *w, FolderBrowser *fb)
{
	message_list_hide_clear(fb->message_list);
}

void
hide_selected(GtkWidget *w, FolderBrowser *fb)
{
	GPtrArray *uids;
	int i;

	uids = g_ptr_array_new();
	message_list_foreach(fb->message_list, enumerate_msg, uids);
	message_list_hide_uids(fb->message_list, uids);
	for (i=0; i<uids->len; i++)
		g_free(uids->pdata[i]);
	g_ptr_array_free(uids, TRUE);
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
				expr = g_string_new("(match-all (header-contains \"subject\" ");
				e_sexp_encode_string(expr, subject);
				g_string_append(expr, "))");
				message_list_hide_add(fb->message_list, expr->str, ML_HIDE_SAME, ML_HIDE_SAME);
				g_string_free(expr, TRUE);
				return;
			}
		}
	}
}

void
hide_sender(GtkWidget *w, FolderBrowser *fb)
{
	const CamelInternetAddress *from;
	const char *real, *addr;
	GString *expr;

	if (fb->mail_display->current_message) {
		from = camel_mime_message_get_from(fb->mail_display->current_message);
		if (camel_internet_address_get(from, 0, &real, &addr)) {
			expr = g_string_new("(match-all (header-contains \"from\" ");
			e_sexp_encode_string(expr, addr);
			g_string_append(expr, "))");
			message_list_hide_add(fb->message_list, expr->str, ML_HIDE_SAME, ML_HIDE_SAME);
			g_string_free(expr, TRUE);
			return;
		}
	}
}

enum {
	SELECTION_SET   = 2,
	CAN_MARK_READ   = 4,
	CAN_MARK_UNREAD = 8,
	CAN_DELETE      = 16,
	CAN_UNDELETE    = 32,
	IS_MAILING_LIST = 64,
	CAN_RESEND      = 128,
	CAN_MARK_IMPORTANT = 256,
	CAN_MARK_UNIMPORTANT = 512
};

#define MLIST_VFOLDER (3)
#define MLIST_FILTER (8)

static EPopupMenu filter_menu[] = {
	{ N_("VFolder on _Subject"),           NULL,
	  GTK_SIGNAL_FUNC (vfolder_subject),   NULL,
	  SELECTION_SET },
	{ N_("VFolder on Se_nder"),            NULL,
	  GTK_SIGNAL_FUNC (vfolder_sender),    NULL,
	  SELECTION_SET },
	{ N_("VFolder on _Recipients"),        NULL,
	  GTK_SIGNAL_FUNC (vfolder_recipient), NULL,
	  SELECTION_SET },
	{ N_("VFolder on Mailing _List"),      NULL,
	  GTK_SIGNAL_FUNC (vfolder_mlist),     NULL,
	  SELECTION_SET | IS_MAILING_LIST },
	
	E_POPUP_SEPARATOR,
	
	{ N_("Filter on Sub_ject"),            NULL,
	  GTK_SIGNAL_FUNC (filter_subject),    NULL,
	  SELECTION_SET },
	{ N_("Filter on Sen_der"),             NULL,
	  GTK_SIGNAL_FUNC (filter_sender),     NULL,
	  SELECTION_SET },
	{ N_("Filter on Re_cipients"),         NULL,
	  GTK_SIGNAL_FUNC (filter_recipient),  NULL,
	  SELECTION_SET },
	{ N_("Filter on _Mailing List"),       NULL,
	  GTK_SIGNAL_FUNC (filter_mlist),      NULL,
	  SELECTION_SET | IS_MAILING_LIST },
	
	E_POPUP_TERMINATOR
};


static EPopupMenu context_menu[] = {
	{ N_("_Open"),                        NULL,
	  GTK_SIGNAL_FUNC (open_msg),         NULL,  0 },
	{ N_("_Edit as New Message..."),      NULL,
	  GTK_SIGNAL_FUNC (resend_msg),       NULL,  CAN_RESEND },
	{ N_("_Save As..."),                  NULL,
	  GTK_SIGNAL_FUNC (save_msg),         NULL,  0 },
	{ N_("_Print"),                       NULL,
	  GTK_SIGNAL_FUNC (print_msg),        NULL,  0 },
	
	E_POPUP_SEPARATOR,
	
	{ N_("_Reply to Sender"),             NULL,
	  GTK_SIGNAL_FUNC (reply_to_sender),  NULL,  0 },
	{ N_("Reply to _List"),        	      NULL,
	  GTK_SIGNAL_FUNC (reply_to_list),    NULL,  0 },
	{ N_("Reply to _All"),                NULL,
	  GTK_SIGNAL_FUNC (reply_to_all),     NULL,  0 },
	{ N_("_Forward"),                     NULL,
	  GTK_SIGNAL_FUNC (forward),          NULL,  0 },
	{ "", NULL, (NULL), NULL,  0 },
	{ N_("Mar_k as Read"),                NULL,
	  GTK_SIGNAL_FUNC (mark_as_seen),     NULL,  CAN_MARK_READ },
	{ N_("Mark as U_nread"),              NULL,
	  GTK_SIGNAL_FUNC (mark_as_unseen),   NULL,  CAN_MARK_UNREAD },
	{ N_("Mark as _Important"),            NULL,
	  GTK_SIGNAL_FUNC (mark_as_important), NULL, CAN_MARK_IMPORTANT },
	{ N_("Mark as Unim_portant"),            NULL,
	  GTK_SIGNAL_FUNC (mark_as_unimportant), NULL, CAN_MARK_UNIMPORTANT },
	
	E_POPUP_SEPARATOR,
	
	{ N_("_Move to Folder..."),           NULL,
	  GTK_SIGNAL_FUNC (move_msg),         NULL,  0 },
	{ N_("_Copy to Folder..."),           NULL,
	  GTK_SIGNAL_FUNC (copy_msg),         NULL,  0 },
	{ N_("_Delete"),                      NULL,
	  GTK_SIGNAL_FUNC (delete_msg),       NULL, CAN_DELETE },
	{ N_("_Undelete"),                    NULL,
	  GTK_SIGNAL_FUNC (undelete_msg),     NULL, CAN_UNDELETE },
	
	E_POPUP_SEPARATOR,
	
	{ N_("Add Sender to Address Book"),   NULL,
	  GTK_SIGNAL_FUNC (addrbook_sender),  NULL,  0 },
	  { "",                               NULL,
	  GTK_SIGNAL_FUNC (NULL),             NULL,  0 },
	
	{ N_("Apply Filters"),                NULL,
	  GTK_SIGNAL_FUNC (apply_filters),    NULL,  0 },
	{ "",                                 NULL,
	  GTK_SIGNAL_FUNC (NULL),             NULL,  0 },
	{ N_("Create Ru_le From Message"),    NULL,
	  GTK_SIGNAL_FUNC (NULL), filter_menu,  SELECTION_SET },
	
	E_POPUP_TERMINATOR
};


struct cmpf_data {
	ETree *tree;
	int row, col;
};

static void
context_menu_position_func (GtkMenu *menu, gint *x, gint *y,
			    gpointer user_data)
{
	int tx, ty, tw, th;
	struct cmpf_data *closure = user_data;

	gdk_window_get_origin (GTK_WIDGET (closure->tree)->window, x, y);
	e_tree_get_cell_geometry (closure->tree, closure->row, closure->col,
				  &tx, &ty, &tw, &th);
	*x += tx + tw / 2;
	*y += ty + th / 2;
}

/* handle context menu over message-list */
static gint
on_right_click (ETree *tree, gint row, ETreePath path, gint col, GdkEvent *event, FolderBrowser *fb)
{
	extern CamelFolder *sent_folder;
	CamelMessageInfo *info;
	GPtrArray *uids;
	int enable_mask = 0;
	int hide_mask = 0;
	int i;
	char *mailing_list_name = NULL;
	char *subject_match = NULL, *from_match = NULL;
	GtkMenu *menu;

	if (fb->folder != sent_folder) {
		enable_mask |= CAN_RESEND;
		hide_mask |= CAN_RESEND;
	}
	
	if (fb->mail_display->current_message == NULL) {
		enable_mask |= SELECTION_SET;
		mailing_list_name = NULL;
	} else {
		/* FIXME: we are leaking subject_match and from_match...what do we use them for anyway??? */
		const char *subject, *real, *addr;
		const CamelInternetAddress *from;
		
		mailing_list_name = header_raw_check_mailing_list(
			&((CamelMimePart *)fb->mail_display->current_message)->headers);
		
		subject = camel_mime_message_get_subject (fb->mail_display->current_message);
		if (subject && (subject = strip_re (subject)) && subject[0])
			subject_match = g_strdup (subject);
		
		from = camel_mime_message_get_from (fb->mail_display->current_message);
		if (from && camel_internet_address_get (from, 0, &real, &addr) && addr && addr[0])
			from_match = g_strdup (addr);
	}
	
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
		
		for (i = 0; i < uids->len; i++) {
			info = camel_folder_get_message_info (fb->folder, uids->pdata[i]);
			if (info == NULL)
				continue;
			
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
		
		/*
		 * Hide items that wont get used.
		 */
		if (!(have_unseen && have_seen)){
			if (have_seen)
				hide_mask |= CAN_MARK_READ;
			else
				hide_mask |= CAN_MARK_UNREAD;
		}
		
		if (!(have_undeleted && have_deleted)){
			if (have_deleted)
				hide_mask |= CAN_DELETE;
			else
				hide_mask |= CAN_UNDELETE;
		}
		
		if (!(have_important && have_unimportant)){
			if (have_important)
				hide_mask |= CAN_MARK_IMPORTANT;
			else
				hide_mask |= CAN_MARK_UNIMPORTANT;
		}
	}
	
	/* free uids */
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
	
display_menu:
	
	/* generate the "Filter on Mailing List menu item name */
	if (mailing_list_name == NULL) {
		enable_mask |= IS_MAILING_LIST;
		filter_menu[MLIST_FILTER].name = g_strdup (_("Filter on Mailing List"));
		filter_menu[MLIST_VFOLDER].name = g_strdup (_("VFolder on Mailing List"));
	} else {
		filter_menu[MLIST_FILTER].name = g_strdup_printf (_("Filter on Mailing List (%s)"), mailing_list_name);
		filter_menu[MLIST_VFOLDER].name = g_strdup_printf (_("VFolder on Mailing List (%s)"), mailing_list_name);
		g_free(mailing_list_name);
	}
	
	menu = e_popup_menu_create (context_menu, enable_mask, hide_mask, fb);
	e_auto_kill_popup_menu_on_hide (menu);
	
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

static gint
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

	vadj = e_scroll_frame_get_vadjustment (fb->mail_display->scroll);
	page_size = vadj->page_size - vadj->step_increment;

	switch (ev->key.keyval) {
	case GDK_space:
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
}

static void
fb_resize_cb (GtkWidget *w, GtkAllocation *a, FolderBrowser *fb)
{	
	if (fb->preview_shown)
		mail_config_set_paned_size (a->height);
}

static void
folder_browser_gui_init (FolderBrowser *fb)
{
	ESelectionModel *esm;

	/* The panned container */
	fb->vpaned = e_vpaned_new ();
	gtk_widget_show (fb->vpaned);
	
	gtk_table_attach (GTK_TABLE (fb), fb->vpaned,
			  0, 1, 1, 3,
			  GTK_FILL | GTK_EXPAND,
			  GTK_FILL | GTK_EXPAND,
			  0, 0);
	
	/* quick-search bar */
	{
		RuleContext *rc = (RuleContext *)rule_context_new ();
		char *user = g_strdup_printf ("%s/searches.xml", evolution_dir);
		/* we reuse the vfolder types here, they should match */
		char *system = EVOLUTION_DATADIR "/evolution/vfoldertypes.xml";
		
		rule_context_add_part_set ((RuleContext *)rc, "partset", filter_part_get_type (),
					   rule_context_add_part, rule_context_next_part);
		
		rule_context_add_rule_set ((RuleContext *)rc, "ruleset", filter_rule_get_type (),
					   rule_context_add_rule, rule_context_next_rule);
		
		fb->search = e_filter_bar_new (rc, system, user, folder_browser_config_search, fb);
		e_search_bar_set_menu ((ESearchBar *)fb->search, folder_browser_search_menu_items);
		/*e_search_bar_set_option((ESearchBar *)fb->search, folder_browser_search_option_items);*/
		g_free (user);
		gtk_object_unref (GTK_OBJECT (rc));
	}
	
	gtk_widget_show (GTK_WIDGET (fb->search));
	
	gtk_signal_connect (GTK_OBJECT (fb->search), "query_changed",
			    GTK_SIGNAL_FUNC (folder_browser_search_query_changed), fb);
	gtk_signal_connect (GTK_OBJECT (fb->search), "menu_activated",
			    GTK_SIGNAL_FUNC (folder_browser_search_menu_activated), fb);
	
	
	gtk_table_attach (GTK_TABLE (fb), GTK_WIDGET (fb->search),
			  0, 1, 0, 1,
			  GTK_FILL | GTK_EXPAND,
			  0,
			  0, 0);

	esm = e_tree_get_selection_model (E_TREE (fb->message_list->tree));
	gtk_signal_connect (GTK_OBJECT (esm), "selection_changed", on_selection_changed, fb);
	fb->selection_state = FB_SELSTATE_NONE; /* default to none */

	e_paned_add1 (E_PANED (fb->vpaned), GTK_WIDGET (fb->message_list));
	gtk_widget_show (GTK_WIDGET (fb->message_list));
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list), "size_allocate",
	                    GTK_SIGNAL_FUNC (fb_resize_cb), fb);
	
	e_paned_add2 (E_PANED (fb->vpaned), GTK_WIDGET (fb->mail_display));
	e_paned_set_position (E_PANED (fb->vpaned), mail_config_get_paned_size ());
	gtk_widget_show (GTK_WIDGET (fb->mail_display));
	gtk_widget_show (GTK_WIDGET (fb));
}

/* mark the message seen if the current message still matches */
static gint 
do_mark_seen (gpointer data)
{
	FolderBrowser *fb = data;
	
	if (fb->new_uid && fb->loaded_uid && !strcmp (fb->new_uid, fb->loaded_uid)) {
		camel_folder_set_message_flags (fb->folder, fb->new_uid, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	}
	
	return FALSE;
}

/* callback when we have the message to display, after async loading it (see below) */
/* if we have pending uid's, it means another was selected before we finished displaying
   the last one - so we cycle through and start loading the pending one immediately now */
static void
done_message_selected (CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data)
{
	FolderBrowser *fb = data;
	int timeout = mail_config_get_mark_as_seen_timeout ();
	
	if (folder != fb->folder)
		return;
	
	mail_display_set_message (fb->mail_display, (CamelMedium *)msg);
	folder_browser_ui_message_loaded (fb);

	/* FIXME: should this signal be emitted here?? */
	gtk_signal_emit (GTK_OBJECT (fb), folder_browser_signals [MESSAGE_LOADED], uid);
	
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
	
	/* if we are still on the same message, do the 'idle read' thing */
	if (fb->seen_id)
		gtk_timeout_remove (fb->seen_id);
	
	if (mail_config_get_do_seen_timeout () && msg) {
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
	d(printf ("selecting uid %s (delayed)\n", fb->new_uid ? fb->new_uid : "NONE"));
	
	fb->loading_id = 0;
	
	/* if we are loading, then set a pending, but leave the loading, coudl cancel here (?) */
	if (fb->loading_uid) {
		g_free (fb->pending_uid);
		fb->pending_uid = g_strdup (fb->new_uid);
	} else {
		if (fb->new_uid) {
			fb->loading_uid = g_strdup (fb->new_uid);
			mail_get_message (fb->folder, fb->loading_uid, done_message_selected, fb, mail_thread_new);
		} else {
			mail_display_set_message (fb->mail_display, NULL);
		}
	}
	
	return FALSE;
}

/* when a message is selected, wait a while before trying to display it */
static void
on_message_selected (MessageList *ml, const char *uid, FolderBrowser *fb)
{
	d(printf ("selecting uid %s (direct)\n", uid ? uid : "NONE"));
	
	if (fb->loading_id != 0)
		gtk_timeout_remove (fb->loading_id);
	
	g_free (fb->new_uid);
	fb->new_uid = g_strdup (uid);
	
	if (fb->preview_shown)
		fb->loading_id = gtk_timeout_add (100, (GtkFunction)do_message_selected, fb);
}

static void
folder_browser_init (GtkObject *object)
{
}

static void
my_folder_browser_init (GtkObject *object)
{
	FolderBrowser *fb = FOLDER_BROWSER (object);
	int i;
	
	fb->view_collection = NULL;
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
	
	gtk_signal_connect (GTK_OBJECT (fb->mail_display->html),
			    "key_press_event", GTK_SIGNAL_FUNC (on_key_press), fb);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree),
			    "key_press", GTK_SIGNAL_FUNC (etree_key), fb);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree),
			    "right_click", GTK_SIGNAL_FUNC (on_right_click), fb);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree),
			    "double_click", GTK_SIGNAL_FUNC (on_double_click), fb);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list), "message_selected",
			    on_message_selected, fb);
	
	/* drag & drop */
	e_tree_drag_source_set (fb->message_list->tree, GDK_BUTTON1_MASK,
				drag_types, num_drag_types, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree), "tree_drag_data_get",
			    GTK_SIGNAL_FUNC (message_list_drag_data_get), fb);
	
	e_tree_drag_dest_set (fb->message_list->tree, GTK_DEST_DEFAULT_ALL,
			      drag_types, num_drag_types, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	
	gtk_signal_connect (GTK_OBJECT (fb->message_list->tree), "tree_drag_data_received",
			    GTK_SIGNAL_FUNC (message_list_drag_data_received), fb);
	
	/* cut, copy & paste */
	fb->invisible = gtk_invisible_new ();
	
	for (i = 0; i < num_paste_types; i++)
		gtk_selection_add_target (fb->invisible, clipboard_atom,
					  paste_types[i].target,
					  paste_types[i].info);
	
	gtk_signal_connect (GTK_OBJECT (fb->invisible),
			    "selection_get",
			    GTK_SIGNAL_FUNC (selection_get),
			    (gpointer) fb);
	gtk_signal_connect (GTK_OBJECT (fb->invisible),
			    "selection_clear_event",
			    GTK_SIGNAL_FUNC (selection_clear_event),
			    (gpointer) fb);
	gtk_signal_connect (GTK_OBJECT (fb->invisible),
			    "selection_received",
			    GTK_SIGNAL_FUNC (selection_received),
			    (gpointer) fb);
	
	folder_browser_gui_init (fb);
}

GtkWidget *
folder_browser_new (const GNOME_Evolution_Shell shell)
{
	CORBA_Environment ev;
	FolderBrowser *folder_browser;
	
	CORBA_exception_init (&ev);

	folder_browser = gtk_type_new (folder_browser_get_type ());

	my_folder_browser_init (GTK_OBJECT (folder_browser));
	folder_browser->uri = NULL;

	folder_browser->shell = CORBA_Object_duplicate (shell, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		folder_browser->shell = CORBA_OBJECT_NIL;
		gtk_widget_destroy (GTK_WIDGET (folder_browser));
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return GTK_WIDGET (folder_browser);
}


E_MAKE_TYPE (folder_browser, "FolderBrowser", FolderBrowser, folder_browser_class_init, folder_browser_init, PARENT_TYPE);
