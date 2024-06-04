/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <camel/camel.h>
#include <errno.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "e-mail-backend.h"
#include "e-mail-display.h"
#include "e-mail-ui-session.h"
#include "em-format/e-mail-parser.h"
#include "em-format/e-mail-part-utils.h"
#include "em-composer-utils.h"
#include "em-folder-selector.h"
#include "em-utils.h"
#include "shell/e-shell.h"

#include "e-mail-viewer.h"

struct _EMailViewerPrivate {
	GtkWidget *statusbar;
	GtkMenuBar *real_menu_bar;
	EMenuBar *e_menu_bar;
	GtkWidget *webview_preview;
	EAlertBar *alert_bar;
	EActivityBar *activity_bar;

	EMailBackend *backend;
	GFile *file;
	GCancellable *cancellable;
	GtkBuilder *builder;
	EMailDisplay *mail_display;
	GtkWidget *preview_pane;
	GActionMap *action_map;
	GtkAccelGroup *accel_group;
	GMenuItem *file_import;
	gboolean has_menumultiple;
	gboolean scan_from;
};

enum {
	PROP_0,
	PROP_BACKEND
};

static void mail_viewer_alert_sink_init (EAlertSinkInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EMailViewer, e_mail_viewer, GTK_TYPE_WINDOW,
	G_ADD_PRIVATE (EMailViewer)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, mail_viewer_alert_sink_init))

enum {
	COL_SUBJECT = 0,
	COL_FROM,
	COL_OFFSET,
	N_COLUMNS
};

static void
mail_viewer_report_error (EMailViewer *self,
			  const gchar *msg)
{
	e_alert_submit (E_ALERT_SINK (self), "system:simple-error", msg, NULL);
}

typedef struct _ClaimActivityData {
	GWeakRef *weakref_self;
	GWeakRef *weakref_activity;
} ClaimActivityData;

static void
claim_activity_data_free (gpointer ptr)
{
	ClaimActivityData *cad = ptr;

	if (cad) {
		e_weak_ref_free (cad->weakref_self);
		e_weak_ref_free (cad->weakref_activity);
		g_free (cad);
	}
}

static gboolean
mail_viewer_claim_activity_cb (gpointer user_data)
{
	ClaimActivityData *cad = user_data;
	EActivity *activity;
	EMailViewer *self = NULL;

	activity = g_weak_ref_get (cad->weakref_activity);

	if (activity) {
		self = g_weak_ref_get (cad->weakref_self);

		if (self && self->priv->cancellable == e_activity_get_cancellable (activity))
			e_activity_bar_set_activity (self->priv->activity_bar, activity);
	}

	g_clear_object (&activity);
	g_clear_object (&self);

	claim_activity_data_free (cad);

	return G_SOURCE_REMOVE;
}

static void
mail_viewer_handle_new_activity (EMailViewer *self,
				 EActivity *activity)
{
	ClaimActivityData *cad;

	if (!activity)
		return;

	self->priv->cancellable = e_activity_get_cancellable (activity);

	if (self->priv->cancellable)
		g_object_ref (self->priv->cancellable);

	cad = g_new0 (ClaimActivityData, 1);
	cad->weakref_self = e_weak_ref_new (self);
	cad->weakref_activity = e_weak_ref_new (activity);

	/* These might be almost instant in most cases, thus no need to clutter the GUI
	   with such quick activities (plus the delay to hide the activity bar on completion). */
	g_timeout_add (500, mail_viewer_claim_activity_cb, cad);
}

static guint64
mail_viewer_get_file_size (GFile *file)
{
	GStatBuf buf;

	if (!file ||
	    !g_file_peek_path (file))
		return 0;

	if (g_stat (g_file_peek_path (file), &buf) == -1)
		return 0;

	return buf.st_size;
}

static CamelMimeMessage *
mail_viewer_get_current_message (EMailViewer *self)
{
	EMailPartList *part_list;

	part_list = e_mail_display_get_part_list (self->priv->mail_display);
	if (!part_list)
		return NULL;

	return e_mail_part_list_get_message (part_list);
}

static CamelMimeParser *
mail_viewer_create_mime_parser (GFile *file,
				goffset offset,
				gboolean scan_from_line,
				GError **error)
{
	CamelMimeParser *mime_parser;
	gint fd;

	if (!file) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "File to open is not set");
		return NULL;
	}

	fd = g_open (g_file_peek_path (file), O_RDONLY | O_BINARY, 0);
	if (fd == -1) {
		gint errn = errno;

		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errn),
			_("Failed to open file “%s”: %s"), g_file_peek_path (file), g_strerror (errn));
		return NULL;
	}

	mime_parser = camel_mime_parser_new ();

	camel_mime_parser_scan_from (mime_parser, scan_from_line);

	if (camel_mime_parser_init_with_fd (mime_parser, fd) == -1) {
		/* this may not happen; do not localize the string, it's here only for just-in-case */
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to initialize message parser");
		g_clear_object (&mime_parser);
		return NULL;
	}

	if (offset > 0 && camel_mime_parser_seek (mime_parser, offset, SEEK_SET) != offset) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to seek to offset in file"));
		g_clear_object (&mime_parser);
		return NULL;
	}

	return mime_parser;
}

static gchar *
mail_viewer_select_folder_uri (EMailViewer *self)
{
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	GtkWidget *dialog;
	gchar *folder_uri = NULL;

	model = em_folder_tree_model_get_default ();
	dialog = em_folder_selector_new (GTK_WINDOW (self), model);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Import to Folder"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);
	em_folder_selector_set_default_button_label (selector, _("_Import"));

	folder_tree = em_folder_selector_get_folder_tree (selector);
	em_folder_tree_set_excluded (folder_tree, EMFT_EXCLUDE_NOSELECT | EMFT_EXCLUDE_VIRTUAL | EMFT_EXCLUDE_VTRASH);

	em_folder_selector_maybe_collapse_archive_folders (selector);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
		folder_uri = g_strdup (em_folder_selector_get_selected_uri (selector));

	gtk_widget_destroy (dialog);

	return folder_uri;
}

typedef struct _ImportData {
	GWeakRef *weakref; /* EMailViewer */
	CamelMimeMessage *msg; /* NULL for all */
	GFile *file; /* NULL for one */
	gboolean scan_from;
	gchar *folder_uri;
	guint n_imported;
} ImportData;

static void
import_data_free (gpointer ptr)
{
	ImportData *id = ptr;

	if (id) {
		g_clear_pointer (&id->weakref, e_weak_ref_free);
		g_clear_object (&id->msg);
		g_clear_object (&id->file);
		g_free (id->folder_uri);
		g_free (id);
	}
}

static gboolean
mail_viewer_import_message_idle_cb (gpointer user_data)
{
	ImportData *id = user_data;
	EMailViewer *self;

	self = g_weak_ref_get (id->weakref);

	if (self) {
		if (!id->n_imported) {
			mail_viewer_report_error (self, _("Could not import any message"));
		} else {
			gchar *msg;

			msg = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Imported one message.", "Imported %u messages.", id->n_imported), id->n_imported);
			e_alert_submit (E_ALERT_SINK (self), "system:simple-info", msg, NULL);
			g_free (msg);
		}
	}

	g_clear_object (&self);
	import_data_free (id);

	return G_SOURCE_REMOVE;
}

/* Defines copied from nsMsgMessageFlags.h in Mozilla source. */
#define MSG_FLAG_READ 0x0001
#define MSG_FLAG_REPLIED 0x0002
#define MSG_FLAG_MARKED 0x0004
#define MSG_FLAG_EXPUNGED 0x0008

static struct {
	gchar tag;
	guint32 mozflag;
	guint32 flag;
} status_flags[] = {
	{ 'F', MSG_FLAG_MARKED, CAMEL_MESSAGE_FLAGGED },
	{ 'A', MSG_FLAG_REPLIED, CAMEL_MESSAGE_ANSWERED },
	{ 'D', MSG_FLAG_EXPUNGED, CAMEL_MESSAGE_DELETED },
	{ 'R', MSG_FLAG_READ, CAMEL_MESSAGE_SEEN },
};

static guint32
decode_status (const gchar *status)
{
	const gchar *p;
	guint32 flags = 0;
	gint ii;

	p = status;
	while ((*p++)) {
		for (ii = 0; ii < G_N_ELEMENTS (status_flags); ii++)
			if (status_flags[ii].tag == *p)
				flags |= status_flags[ii].flag;
	}

	return flags;
}

static guint32
decode_mozilla_status (const gchar *tmp)
{
	gulong status = strtoul (tmp, NULL, 16);
	guint32 flags = 0;
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (status_flags); ii++) {
		if (status_flags[ii].mozflag & status)
			flags |= status_flags[ii].flag;
	}
	return flags;
}

static gboolean
mail_viewer_import_message_sync (CamelMimeMessage *msg,
				 CamelFolder *folder,
				 GCancellable *cancellable,
				 GError **error)
{
	CamelMessageInfo *info;
	CamelMedium *medium;
	guint32 flags = 0;
	const gchar *tmp;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (msg), FALSE);

	medium = CAMEL_MEDIUM (msg);

	tmp = camel_medium_get_header (medium, "X-Mozilla-Status");
	if (tmp)
		flags |= decode_mozilla_status (tmp);
	tmp = camel_medium_get_header (medium, "Status");
	if (tmp)
		flags |= decode_status (tmp);
	tmp = camel_medium_get_header (medium, "X-Status");
	if (tmp)
		flags |= decode_status (tmp);

	info = camel_message_info_new (NULL);

	camel_message_info_set_flags (info, flags, ~0);
	success = camel_folder_append_message_sync (folder, msg, info, NULL, cancellable, error);
	g_clear_object (&info);

	return success;
}

static void
mail_viewer_import_thread (EAlertSinkThreadJobData *job_data,
			   gpointer user_data,
			   GCancellable *cancellable,
			   GError **error)
{
	ImportData *id = user_data;
	CamelFolder *folder = NULL;
	EMailViewer *self;
	gboolean success = TRUE;

	g_return_if_fail (id != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	self = g_weak_ref_get (id->weakref);
	if (self) {
		EMailSession *session;

		session = e_mail_backend_get_session (self->priv->backend);
		folder = e_mail_session_uri_to_folder_sync (session, id->folder_uri, CAMEL_STORE_FOLDER_CREATE, cancellable, error);
		g_clear_object (&self);
	}

	if (folder) {
		if (id->msg) {
			success = mail_viewer_import_message_sync (id->msg, folder, cancellable, error);
			if (success)
				id->n_imported++;
		} else if (id->file) {
			guint64 file_size = mail_viewer_get_file_size (id->file);
			CamelMimeParser *mime_parser;

			mime_parser = mail_viewer_create_mime_parser (id->file, 0, id->scan_from, error);
			if (mime_parser) {
				if (id->scan_from) {
					gint percent = 0;

					camel_folder_freeze (folder);

					while (camel_mime_parser_step (mime_parser, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM &&
					       !g_cancellable_is_cancelled (cancellable)) {
						CamelMimeMessage *msg;

						msg = camel_mime_message_new ();

						success = camel_mime_part_construct_from_parser_sync (CAMEL_MIME_PART (msg), mime_parser, cancellable, error) &&
							  mail_viewer_import_message_sync (msg, folder, cancellable, error);

						g_object_unref (msg);

						if (success)
							id->n_imported++;
						else
							break;

						camel_mime_parser_step (mime_parser, NULL, NULL);

						if (file_size > 0) {
							gint new_percent;

							new_percent = 100 * camel_mime_parser_tell (mime_parser) / file_size;
							if (new_percent > percent && new_percent <= 100) {
								percent = new_percent;
								camel_operation_progress (cancellable, percent);
							}
						}
					}

					if (file_size > 0 && !g_cancellable_is_cancelled (cancellable))
						camel_operation_progress (cancellable, 100);

					camel_folder_thaw (folder);
				} else {
					CamelMimeMessage *msg;

					msg = camel_mime_message_new ();

					success = camel_mime_part_construct_from_parser_sync (CAMEL_MIME_PART (msg), mime_parser, cancellable, error) &&
						mail_viewer_import_message_sync (msg, folder, cancellable, error);
					if (success)
						id->n_imported++;

					g_object_unref (msg);
				}

				g_clear_object (&mime_parser);
			}
		}

		camel_folder_synchronize_sync (folder, FALSE, cancellable, NULL);
	}

	g_clear_object (&folder);

	if (success && !g_cancellable_is_cancelled (cancellable)) {
		ImportData *id2;

		id2 = g_new0 (ImportData, 1);
		id2->weakref = g_steal_pointer (&id->weakref);
		id2->n_imported = id->n_imported;

		g_idle_add (mail_viewer_import_message_idle_cb, id2);
	}
}

static void
mail_viewer_import (EMailViewer *self,
		    CamelMimeMessage *msg,
		    gchar *folder_uri) /* (transfer full) */
{
	EActivity *activity;
	ImportData *id;

	if (self->priv->cancellable) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	id = g_new0 (ImportData, 1);
	id->weakref = e_weak_ref_new (self);
	if (msg) {
		id->msg = g_object_ref (msg);
	} else {
		id->file = g_object_ref (self->priv->file);
		id->scan_from = self->priv->scan_from;
	}
	id->folder_uri = folder_uri;

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (self),
		_("Importing…"),
		"system:generic-error",
		_("Failed to import message"),
		mail_viewer_import_thread, id,
		import_data_free);

	mail_viewer_handle_new_activity (self, activity);

	g_clear_object (&activity);
}

static void
import_one_activated_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;
	CamelMimeMessage *msg;
	gchar *folder_uri;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	folder_uri = mail_viewer_select_folder_uri (self);
	if (!folder_uri)
		return;

	mail_viewer_import (self, msg, folder_uri);
}

static void
import_all_activated_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;
	gchar *folder_uri;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	folder_uri = mail_viewer_select_folder_uri (self);
	if (!folder_uri)
		return;

	mail_viewer_import (self, NULL, folder_uri);
}

static void
mail_viewer_print_done_cb (GObject *source_object,
			   GAsyncResult *result,
			   gpointer user_data)
{
	GWeakRef *weakref = user_data;
	EMailViewer *self = g_weak_ref_get (weakref);
	GError *local_error = NULL;

	if (!em_utils_print_part_list_finish (source_object, result, &local_error) && self &&
	    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		gchar *msg;

		msg = g_strdup_printf (_("Failed to print message: %s"), local_error ? local_error->message : _("Unknown error"));
		mail_viewer_report_error (self, msg);
		g_free (msg);
	}

	g_clear_object (&self);
	g_clear_error (&local_error);
	e_weak_ref_free (weakref);
}

static void
open_activated_cb (GSimpleAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	EMailViewer *self = user_data;
	GtkFileChooser *file_chooser;
	GtkFileChooserNative *native;
	GtkFileFilter *filter;
	GFile *chosen_file = NULL;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	native = gtk_file_chooser_native_new (
		_("Open Message File"), GTK_WINDOW (self),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);

	gtk_file_chooser_set_local_only (file_chooser, FALSE);

	e_util_load_file_chooser_folder (file_chooser);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.mbox");
	gtk_file_filter_add_pattern (filter, "*.eml");
	gtk_file_filter_set_name (filter, _("Mail Messages"));
	gtk_file_filter_add_mime_type (filter, "application/mbox");
	gtk_file_filter_add_mime_type (filter, "message/rfc822");
	gtk_file_chooser_add_filter (file_chooser, filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_filter_set_name (filter, _("All Files (*)"));
	gtk_file_chooser_add_filter (file_chooser, filter);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (file_chooser);

		chosen_file = gtk_file_chooser_get_file (file_chooser);
	}

	g_object_unref (native);

	if (chosen_file) {
		e_mail_viewer_assign_file (self, chosen_file);
		g_clear_object (&chosen_file);
	}
}

static void
print_activated_cb (GSimpleAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	em_utils_print_part_list (
		e_mail_display_get_part_list (self->priv->mail_display),
		self->priv->mail_display,
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
		self->priv->cancellable,
		mail_viewer_print_done_cb,
		e_weak_ref_new (self));
}

static void
close_activated_cb (GSimpleAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	gtk_window_close (GTK_WINDOW (self));
}

static void
cut_activated_cb (GSimpleAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_selectable_cut_clipboard (E_SELECTABLE (self->priv->mail_display));
}

static void
copy_activated_cb (GSimpleAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_selectable_copy_clipboard (E_SELECTABLE (self->priv->mail_display));
}

static void
paste_activated_cb (GSimpleAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_selectable_paste_clipboard (E_SELECTABLE (self->priv->mail_display));
}

static void
select_all_activated_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_selectable_select_all (E_SELECTABLE (self->priv->mail_display));
}

static void
find_activated_cb (GSimpleAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_preview_pane_show_search_bar (E_PREVIEW_PANE (self->priv->preview_pane));
}

static void
load_images_activated_cb (GSimpleAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_mail_display_load_images (self->priv->mail_display);
}

static void
activate_toggle_cb (GSimpleAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
	g_variant_unref (state);
}

static void
activate_radio_cb (GSimpleAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	g_action_change_state (G_ACTION (action), parameter);
}

static void
all_headers_change_state_cb (GSimpleAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailViewer *self = user_data;
	EMailFormatterMode mode;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	g_simple_action_set_state (action, parameter);

	mode = e_mail_display_get_mode (self->priv->mail_display);

	if (mode != E_MAIL_FORMATTER_MODE_SOURCE) {
		if (g_variant_get_boolean (parameter))
			e_mail_display_set_mode (self->priv->mail_display, E_MAIL_FORMATTER_MODE_ALL_HEADERS);
		else
			e_mail_display_set_mode (self->priv->mail_display, E_MAIL_FORMATTER_MODE_NORMAL);
	}
}

static void
msg_source_change_state_cb (GSimpleAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	g_simple_action_set_state (action, parameter);

	if (g_variant_get_boolean (parameter)) {
		e_mail_display_set_mode (self->priv->mail_display, E_MAIL_FORMATTER_MODE_SOURCE);
	} else {
		GAction *all_headers_action;
		GVariant *all_headers_value;

		all_headers_action = g_action_map_lookup_action (self->priv->action_map, "all-headers");
		all_headers_value = g_action_get_state (all_headers_action);

		if (all_headers_value && g_variant_get_boolean (all_headers_value))
			e_mail_display_set_mode (self->priv->mail_display, E_MAIL_FORMATTER_MODE_ALL_HEADERS);
		else
			e_mail_display_set_mode (self->priv->mail_display, E_MAIL_FORMATTER_MODE_NORMAL);

		g_clear_pointer (&all_headers_value, g_variant_unref);
	}
}

static void
caret_mode_change_state_cb (GSimpleAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	g_simple_action_set_state (action, parameter);

	e_web_view_set_caret_mode (E_WEB_VIEW (self->priv->mail_display), g_variant_get_boolean (parameter));
}

static void
zoom_in_activated_cb (GSimpleAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_web_view_zoom_in (E_WEB_VIEW (self->priv->mail_display));
}

static void
zoom_out_activated_cb (GSimpleAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_web_view_zoom_out (E_WEB_VIEW (self->priv->mail_display));
}

static void
zoom_zero_activated_cb (GSimpleAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	e_web_view_zoom_100 (E_WEB_VIEW (self->priv->mail_display));
}

static void
charset_change_state_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;
	EMailFormatter *formatter;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	g_simple_action_set_state (action, parameter);

	formatter = e_mail_display_get_formatter (self->priv->mail_display);

	if (formatter) {
		const gchar *charset;

		charset = g_variant_get_string (parameter, NULL);

		/* Default value it an empty string in the GMenu, but a NULL for the formatter */
		if (charset && !*charset)
			charset = NULL;

		e_mail_formatter_set_charset (formatter, charset);
	}
}

static void
mail_viewer_edit_as_new_composer_created_cb (GObject *source_object,
					     GAsyncResult *result,
					     gpointer user_data)
{
	CamelMimeMessage *msg = user_data;
	EMsgComposer *composer;
	GError *local_error = NULL;

	g_return_if_fail (msg != NULL);

	composer = e_msg_composer_new_finish (result, &local_error);
	if (local_error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	} else {
		camel_medium_remove_header (CAMEL_MEDIUM (msg), "User-Agent");
		camel_medium_remove_header (CAMEL_MEDIUM (msg), "X-Mailer");
		camel_medium_remove_header (CAMEL_MEDIUM (msg), "X-Newsreader");
		camel_medium_remove_header (CAMEL_MEDIUM (msg), "X-MimeOLE");

		em_utils_edit_message (composer, NULL, msg, NULL, FALSE, FALSE);
	}

	g_clear_object (&msg);
}

static void
edit_as_new_activated_cb (GSimpleAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailViewer *self = user_data;
	EShell *shell;
	CamelMimeMessage *msg;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	e_msg_composer_new (shell, mail_viewer_edit_as_new_composer_created_cb, g_object_ref (msg));
}

static void
add_sender_activated_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;
	EShell *shell;
	CamelMimeMessage *msg;
	CamelInternetAddress *from;
	const gchar *email_only = NULL;
	gchar *address;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	from = camel_mime_message_get_from (msg);
	if (!from)
		return;

	address = camel_address_format (CAMEL_ADDRESS (from));

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));
	e_shell_event (shell, "contact-quick-add-email", (gpointer) address);

	g_free (address);

	if (camel_internet_address_get (from, 0, NULL, &email_only)) {
		EMailSession *session;
		EPhotoCache *photo_cache;

		session = e_mail_backend_get_session (self->priv->backend);
		photo_cache = e_mail_ui_session_get_photo_cache (E_MAIL_UI_SESSION (session));
		e_photo_cache_remove_photo (photo_cache, email_only);
	}
}

static void
mail_viewer_goto (EMailViewer *self,
		  GdkScrollDirection direction)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean has_iter;

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (self->priv->webview_preview));
	selection = gtk_tree_view_get_selection (tree_view);

	has_iter = gtk_tree_selection_get_selected (selection, &model, &iter);
	if (!has_iter) {
		if (!model)
			model = gtk_tree_view_get_model (tree_view);
		if (model)
			has_iter = gtk_tree_model_get_iter_first (model, &iter);
	}

	if (has_iter) {
		if ((direction == GDK_SCROLL_UP && gtk_tree_model_iter_previous (model, &iter)) ||
		    (direction == GDK_SCROLL_DOWN && gtk_tree_model_iter_next (model, &iter))) {
			gtk_tree_selection_select_iter (selection, &iter);
		}
	}
}

static void
go_to_next_activated_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_goto (self, GDK_SCROLL_DOWN);
}

static void
go_to_previous_activated_cb (GSimpleAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_goto (self, GDK_SCROLL_UP);
}

typedef struct _ReplyForwardData {
	GWeakRef *weakref; /* EMailViewer * */
	CamelMimeMessage *msg;
	EMailReplyType reply_type;
	EMailForwardStyle forward_style;
	gboolean skip_insecure_parts;
} ReplyForwardData;

static void
reply_forward_data_free (gpointer ptr)
{
	ReplyForwardData *rfd = ptr;

	if (rfd) {
		e_weak_ref_free (rfd->weakref);
		g_clear_object (&rfd->msg);
		g_free (rfd);
	}
}

static void
mail_viewer_reply_message_composer_created_cb (GObject *source_object,
					       GAsyncResult *result,
					       gpointer user_data)
{
	ReplyForwardData *rfd = user_data;
	EMsgComposer *composer;
	GError *local_error = NULL;

	composer = e_msg_composer_new_finish (result, &local_error);
	if (local_error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	} else {
		EMailReplyStyle reply_style;
		EMailReplyFlags reply_flags = 0;
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		reply_style = g_settings_get_enum (settings, "reply-style-name");
		g_clear_object (&settings);

		if (rfd->skip_insecure_parts)
			reply_flags |= E_MAIL_REPLY_FLAG_SKIP_INSECURE_PARTS;

		em_utils_reply_to_message (composer, rfd->msg, NULL, NULL, rfd->reply_type,
			reply_style, NULL, NULL, reply_flags);
	}

	reply_forward_data_free (rfd);
}

static void
mail_viewer_reply_message (EMailViewer *self,
			   EMailReplyType reply_type)
{
	EShell *shell;
	CamelMimeMessage *msg;
	ReplyForwardData *rfd;

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	rfd = g_new0 (ReplyForwardData, 1);
	rfd->weakref = e_weak_ref_new (self);
	rfd->msg = g_object_ref (msg);
	rfd->reply_type = reply_type;
	rfd->skip_insecure_parts = e_mail_display_get_skip_insecure_parts (self->priv->mail_display);

	e_msg_composer_new (shell, mail_viewer_reply_message_composer_created_cb, rfd);
}

static void
reply_sender_activated_cb (GSimpleAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_reply_message (self, E_MAIL_REPLY_TO_SENDER);
}

static void
reply_list_activated_cb (GSimpleAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_reply_message (self, E_MAIL_REPLY_TO_LIST);
}

static void
reply_all_activated_cb (GSimpleAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_reply_message (self, E_MAIL_REPLY_TO_ALL);
}

static void
reply_alt_activated_cb (GSimpleAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailViewer *self = user_data;
	EMailReplyStyle reply_style;
	EShell *shell;
	CamelMimeMessage *msg;
	GSettings *settings;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	reply_style = g_settings_get_enum (settings, "reply-style-name");
	g_clear_object (&settings);

	em_utils_reply_alternative (GTK_WINDOW (self), shell, NULL, msg, NULL, NULL,
		reply_style, e_mail_display_get_part_list (self->priv->mail_display),
		0, 0, e_mail_display_get_skip_insecure_parts (self->priv->mail_display));
}

static void
mail_viewer_forward_message_composer_created_cb (GObject *source_object,
						 GAsyncResult *result,
						 gpointer user_data)
{
	ReplyForwardData *rfd = user_data;
	EMsgComposer *composer;
	GError *local_error = NULL;

	composer = e_msg_composer_new_finish (result, &local_error);
	if (local_error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	} else {
		em_utils_forward_message (composer, rfd->msg, rfd->forward_style, NULL, NULL, rfd->skip_insecure_parts);
	}

	reply_forward_data_free (rfd);
}

static void
mail_viewer_forward_message (EMailViewer *self,
			     EMailForwardStyle forward_style)
{
	EShell *shell;
	CamelMimeMessage *msg;
	ReplyForwardData *rfd;

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	rfd = g_new0 (ReplyForwardData, 1);
	rfd->weakref = e_weak_ref_new (self);
	rfd->msg = g_object_ref (msg);
	rfd->forward_style = forward_style;
	rfd->skip_insecure_parts = e_mail_display_get_skip_insecure_parts (self->priv->mail_display);

	e_msg_composer_new (shell, mail_viewer_forward_message_composer_created_cb, rfd);
}

static void
forward_activated_cb (GSimpleAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EMailViewer *self = user_data;
	EMailForwardStyle forward_style;
	GSettings *settings;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	forward_style = g_settings_get_enum (settings, "forward-style-name");
	g_clear_object (&settings);

	mail_viewer_forward_message (self, forward_style);
}

static void
forward_attached_activated_cb (GSimpleAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_forward_message (self, E_MAIL_FORWARD_STYLE_ATTACHED);
}

static void
forward_inline_activated_cb (GSimpleAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_forward_message (self, E_MAIL_FORWARD_STYLE_INLINE);
}

static void
forward_quoted_activated_cb (GSimpleAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailViewer *self = user_data;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	mail_viewer_forward_message (self, E_MAIL_FORWARD_STYLE_QUOTED);
}

static void
mail_viewer_redirect_composer_created_cb (GObject *source_object,
					  GAsyncResult *result,
					  gpointer user_data)
{
	CamelMimeMessage *msg = user_data;
	EMsgComposer *composer;
	GError *local_error = NULL;

	g_return_if_fail (msg != NULL);

	composer = e_msg_composer_new_finish (result, &local_error);
	if (local_error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	} else {
		em_utils_redirect_message (composer, msg);
	}

	g_clear_object (&msg);
}

static void
redirect_activated_cb (GSimpleAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EMailViewer *self = user_data;
	EShell *shell;
	CamelMimeMessage *msg;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	msg = mail_viewer_get_current_message (self);
	if (!msg)
		return;

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	e_msg_composer_new (shell, mail_viewer_redirect_composer_created_cb, g_object_ref (msg));
}

static void
mail_viewer_init_actions (EMailViewer *self)
{
	GActionEntry actions[] = {
		{ "import-one", import_one_activated_cb, NULL, NULL, NULL },
		{ "import-all", import_all_activated_cb, NULL, NULL, NULL },
		{ "open", open_activated_cb, NULL, NULL, NULL },
		{ "print", print_activated_cb, NULL, NULL, NULL },
		{ "close", close_activated_cb, NULL, NULL, NULL },
		{ "cut", cut_activated_cb, NULL, NULL, NULL },
		{ "copy", copy_activated_cb, NULL, NULL, NULL },
		{ "paste", paste_activated_cb, NULL, NULL, NULL },
		{ "select-all", select_all_activated_cb, NULL, NULL, NULL },
		{ "find", find_activated_cb, NULL, NULL, NULL },
		{ "load-images", load_images_activated_cb, NULL, NULL, NULL },
		{ "all-headers", activate_toggle_cb, NULL, "false", all_headers_change_state_cb },
		{ "msg-source", activate_toggle_cb, NULL, "false", msg_source_change_state_cb },
		{ "caret-mode", activate_toggle_cb, NULL, "false", caret_mode_change_state_cb  },
		{ "zoom-in", zoom_in_activated_cb, NULL, NULL, NULL },
		{ "zoom-out", zoom_out_activated_cb, NULL, NULL, NULL },
		{ "zoom-zero", zoom_zero_activated_cb, NULL, NULL, NULL },
		{ "charset", activate_radio_cb, "s", "''", charset_change_state_cb },
		{ "edit-as-new", edit_as_new_activated_cb, NULL, NULL, NULL },
		{ "add-sender", add_sender_activated_cb, NULL, NULL, NULL },
		{ "goto-next", go_to_next_activated_cb, NULL, NULL, NULL },
		{ "goto-previous", go_to_previous_activated_cb, NULL, NULL, NULL },
		{ "reply-sender", reply_sender_activated_cb, NULL, NULL, NULL },
		{ "reply-list", reply_list_activated_cb, NULL, NULL, NULL },
		{ "reply-all", reply_all_activated_cb, NULL, NULL, NULL },
		{ "reply-alt", reply_alt_activated_cb, NULL, NULL, NULL },
		{ "forward", forward_activated_cb, NULL, NULL, NULL },
		{ "forward-attached", forward_attached_activated_cb, NULL, NULL, NULL },
		{ "forward-inline", forward_inline_activated_cb, NULL, NULL, NULL },
		{ "forward-quoted", forward_quoted_activated_cb, NULL, NULL, NULL },
		{ "redirect", redirect_activated_cb, NULL, NULL, NULL }
	};
	GSimpleActionGroup *group;

	group = g_simple_action_group_new ();

	g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
	gtk_widget_insert_action_group (GTK_WIDGET (self), "mail-viewer", G_ACTION_GROUP (group));

	self->priv->action_map = G_ACTION_MAP (group);
}

static void
mail_viewer_closure_accel_activate (GClosure *closure,
				    GValue *return_value,
				    guint n_param_values,
				    const GValue *param_values,
				    gpointer invocation_hint,
				    gpointer marshal_data)
{
	GAction *action = G_ACTION (closure->data);

	if (g_action_get_enabled (action)) {
		const GVariantType *param_type;

		param_type = g_action_get_parameter_type (action);
		if (!param_type) {
			g_action_activate (action, NULL);
		} else if (param_type == G_VARIANT_TYPE_BOOLEAN) {
			GVariant *current_value = g_action_get_state (action);
			GVariant *new_value;

			new_value = g_variant_new_boolean (current_value ? !g_variant_get_boolean (current_value) : TRUE);
			g_variant_ref_sink (new_value);

			g_action_activate (action, new_value);

			g_clear_pointer (&current_value, g_variant_unref);
			g_clear_pointer (&new_value, g_variant_unref);
		} else {
			g_warn_if_reached ();
		}

		/* accelerator was handled */
		g_value_set_boolean (return_value, TRUE);
	}
}

static void
mail_viewer_traverse_menu_model (EMailViewer *self,
				 GMenuModel *menu_model,
				 gint item_index)
{
	GMenuLinkIter *iter;
	gchar *accel_str = NULL;

	if (g_menu_model_get_item_attribute (menu_model, item_index, "accel", "s", &accel_str)) {
		gchar *action_name = NULL;

		if (g_menu_model_get_item_attribute (menu_model, item_index, "action", "s", &action_name) && action_name) {
			if (g_str_has_prefix (action_name, "mail-viewer.")) {
				const gchar *after_dot = action_name + strlen ("mail-viewer.");
				GAction *action = g_action_map_lookup_action (self->priv->action_map, after_dot);

				if (action) {
					guint key = 0;
					GdkModifierType mods = 0;

					gtk_accelerator_parse (accel_str, &key, &mods);

					if (key != 0) {
						GClosure *closure;

						closure = g_closure_new_object (sizeof (GClosure), G_OBJECT (action));
						g_closure_set_marshal (closure, mail_viewer_closure_accel_activate);

						gtk_accel_group_connect (self->priv->accel_group, key, mods, GTK_ACCEL_LOCKED, closure);
					}
				}
			}

			g_free (action_name);
		}

		g_free (accel_str);
	}

	iter = g_menu_model_iterate_item_links (menu_model, item_index);
	if (iter) {
		while (g_menu_link_iter_next (iter)) {
			GMenuModel *submenu_model = g_menu_link_iter_get_value (iter);
			if (submenu_model) {
				gint ii, n_items;

				n_items = g_menu_model_get_n_items (submenu_model);

				for (ii = 0; ii < n_items; ii++) {
					mail_viewer_traverse_menu_model (self, submenu_model, ii);
				}
			}
			g_clear_object (&submenu_model);
		}

		g_clear_object (&iter);
	}
}

static void
mail_viewer_init_accel_group (EMailViewer *self)
{
	GMenuModel *menu_model;
	gint n_items, ii;

	g_return_if_fail (self->priv->accel_group == NULL);

	self->priv->accel_group = gtk_accel_group_new ();

	menu_model = G_MENU_MODEL (gtk_builder_get_object (self->priv->builder, "menu"));
	n_items = g_menu_model_get_n_items (menu_model);

	for (ii = 0; ii < n_items; ii++) {
		mail_viewer_traverse_menu_model (self, menu_model, ii);
	}

	menu_model = G_MENU_MODEL (gtk_builder_get_object (self->priv->builder, "goto-menu"));
	n_items = g_menu_model_get_n_items (menu_model);

	for (ii = 0; ii < n_items; ii++) {
		mail_viewer_traverse_menu_model (self, menu_model, ii);
	}

	gtk_window_add_accel_group (GTK_WINDOW (self), self->priv->accel_group);
}

static void
mail_viewer_update_actions (EMailViewer *self)
{
	const gchar *actions[] = {
		"import-one",
		"import-all",
		"print",
		"select-all",
		"find",
		"load-images",
		"all-headers",
		"msg-source",
		"caret-mode",
		"zoom-in",
		"zoom-out",
		"zoom-zero",
		"charset",
		"edit-as-new",
		"add-sender",
		"reply-sender",
		"reply-all",
		"reply-alt",
		"forward",
		"forward-attached",
		"forward-inline",
		"forward-quoted",
		"redirect"
	};
	GActionMap *action_map;
	GAction *action;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	CamelMimeMessage *msg;
	gboolean has_message = FALSE;
	gboolean has_list_post_header = FALSE;
	gboolean can_goto_next = FALSE;
	gboolean can_goto_previous = FALSE;
	guint ii;

	action_map = self->priv->action_map;
	if (!action_map)
		return;

	msg = mail_viewer_get_current_message (self);
	has_message = msg != NULL;

	if (msg)
		has_list_post_header = camel_medium_get_header (CAMEL_MEDIUM (msg), "List-Post") != NULL;

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (self->priv->webview_preview));
	model = gtk_tree_view_get_model (tree_view);
	if (model && gtk_tree_model_iter_n_children (model, NULL) > 1) {
		GtkTreeSelection *selection;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection (tree_view);
		if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
			GtkTreeIter iter2 = iter;

			can_goto_next = gtk_tree_model_iter_next (model, &iter);
			can_goto_previous = gtk_tree_model_iter_previous (model, &iter2);
		} else {
			/* like if the first is selected */
			can_goto_next = TRUE;
		}
	}

	for (ii = 0; ii < G_N_ELEMENTS (actions); ii++) {
		action = g_action_map_lookup_action (action_map, actions[ii]);
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), has_message);
	}

	action = g_action_map_lookup_action (action_map, "reply-list");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), has_message && has_list_post_header);

	action = g_action_map_lookup_action (action_map, "goto-next");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_goto_next);

	action = g_action_map_lookup_action (action_map, "goto-previous");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_goto_previous);
}

static void
mail_viewer_status_message_cb (EMailViewer *self,
			       const gchar *status_message)
{
	GtkStatusbar *statusbar;
	guint context_id;

	statusbar = GTK_STATUSBAR (self->priv->statusbar);
	context_id = gtk_statusbar_get_context_id (statusbar, G_STRFUNC);

	/* Always pop first.  This prevents messages from piling up. */
	gtk_statusbar_pop (statusbar, context_id);

	if (status_message && *status_message)
		gtk_statusbar_push (statusbar, context_id, status_message);
}

static void
mail_viewer_set_window_title (EMailViewer *self,
			      const gchar *text)
{
	gchar *title;

	/* Intentionally the "Mail Viewer" first, because it's more important than the message subject */
	title = g_strdup_printf (_("Mail Viewer — %s"), text);
	gtk_window_set_title (GTK_WINDOW (self), title);
	g_free (title);
}

typedef struct _PreviewData {
	GWeakRef *weakref; /* EMailViewer */
	CamelMimeParser *mime_parser;
	EMailPartList *part_list; /* (out) */
} PreviewData;

static void
preview_data_free (gpointer ptr)
{
	PreviewData *pd = ptr;

	if (pd) {
		g_clear_pointer (&pd->weakref, e_weak_ref_free);
		g_clear_object (&pd->mime_parser);
		g_clear_object (&pd->part_list);
		g_free (pd);
	}
}

static gboolean
mail_viewer_message_parsed_idle_cb (gpointer user_data)
{
	PreviewData *pd = user_data;
	EMailViewer *self = g_weak_ref_get (pd->weakref);

	if (self) {
		const gchar *title = NULL;

		if (pd->part_list) {
			CamelObjectBag *parts_registry;
			CamelMimeMessage *msg;
			EMailPartList *reserved_parts_list;
			const gchar *message_uid;
			gchar *mail_uri;
			gpointer existing;

			message_uid = e_mail_part_list_get_message_uid (pd->part_list);
			mail_uri = e_mail_part_build_uri (NULL, message_uid, NULL, NULL);
			parts_registry = e_mail_part_list_get_registry ();

			/* in case one file contains two messages with the same Message-ID header */
			existing = camel_object_bag_peek (parts_registry, mail_uri);
			if (existing)
				camel_object_bag_remove (parts_registry, existing);

			reserved_parts_list = camel_object_bag_reserve (parts_registry, mail_uri);
			g_clear_object (&reserved_parts_list);

			camel_object_bag_add (parts_registry, mail_uri, pd->part_list);

			g_free (mail_uri);

			e_mail_display_set_part_list (self->priv->mail_display, pd->part_list);
			e_mail_display_load (self->priv->mail_display, NULL);

			msg = e_mail_part_list_get_message (pd->part_list);
			if (msg)
				title = camel_mime_message_get_subject (msg);
		}

		if (title == NULL || *title == '\0')
			title = _("(No Subject)");

		mail_viewer_set_window_title (self, title);
		mail_viewer_update_actions (self);
	}

	g_clear_object (&self);
	preview_data_free (pd);

	return G_SOURCE_REMOVE;
}

static void
mail_viewer_preview_thread (EAlertSinkThreadJobData *job_data,
			    gpointer user_data,
			    GCancellable *cancellable,
			    GError **error)
{
	PreviewData *pd = user_data;
	CamelMimeMessage *msg;

	g_return_if_fail (pd != NULL);

	msg = camel_mime_message_new ();

	if (camel_mime_part_construct_from_parser_sync (CAMEL_MIME_PART (msg), pd->mime_parser, cancellable, error)) {
		EMailViewer *self = g_weak_ref_get (pd->weakref);

		if (self) {
			EMailParser *parser;

			if (!camel_mime_message_get_message_id (msg))
				camel_mime_message_set_message_id (msg, NULL);

			parser = e_mail_parser_new (CAMEL_SESSION (e_mail_backend_get_session (self->priv->backend)));
			pd->part_list = e_mail_parser_parse_sync (parser, NULL, camel_mime_message_get_message_id (msg), msg, cancellable);
			g_clear_object (&parser);

			if (pd->part_list) {
				PreviewData *pd2;

				pd2 = g_new0 (PreviewData, 1);
				pd2->weakref = g_steal_pointer (&pd->weakref);
				pd2->part_list = g_steal_pointer (&pd->part_list);

				g_idle_add (mail_viewer_message_parsed_idle_cb, pd2);
			}
		}

		g_clear_object (&self);
	}

	g_object_unref (msg);
}

static void
mail_viewer_selection_changed_cb (GtkTreeSelection *selection,
				  gpointer user_data)
{
	EMailViewer *self = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	gboolean found = FALSE;

	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	if (gtk_tree_selection_get_selected (selection, &model, &iter) && model) {
		gint64 offset = -1;

		gtk_tree_model_get (model, &iter, COL_OFFSET, &offset, -1);

		if (offset >= 0) {
			PreviewData *pd;
			CamelMimeParser *mime_parser;
			GError *local_error = NULL;

			found = TRUE;

			mime_parser = mail_viewer_create_mime_parser (self->priv->file, (goffset) offset, self->priv->scan_from, &local_error);
			if (mime_parser) {
				EActivity *activity;

				if (self->priv->scan_from)
					camel_mime_parser_step (mime_parser, NULL, NULL);

				if (self->priv->cancellable) {
					g_cancellable_cancel (self->priv->cancellable);
					g_clear_object (&self->priv->cancellable);
				}

				pd = g_new0 (PreviewData, 1);
				pd->weakref = e_weak_ref_new (self);
				pd->mime_parser = mime_parser; /* takes ownership */

				activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (self),
					_("Loading…"),
					"system:generic-error",
					_("Failed to load message"),
					mail_viewer_preview_thread, pd,
					preview_data_free);

				mail_viewer_handle_new_activity (self, activity);

				g_clear_object (&activity);
			} else {
				mail_viewer_report_error (self, local_error ? local_error->message : _("Unknown error"));
				g_clear_error (&local_error);
			}
		}
	}

	if (!found) {
		e_web_view_preview_begin_update (E_WEB_VIEW_PREVIEW (self->priv->webview_preview));
		e_web_view_preview_end_update (E_WEB_VIEW_PREVIEW (self->priv->webview_preview));
	}

	mail_viewer_update_actions (self);
}

static void
mail_viewer_can_execute_editing_command_cb (GObject *source_object,
					    GAsyncResult *result,
					    gpointer user_data)
{
	GSimpleAction *action = user_data;
	gboolean can_do_command;

	can_do_command = webkit_web_view_can_execute_editing_command_finish (WEBKIT_WEB_VIEW (source_object), result, NULL);

	g_simple_action_set_enabled (action, can_do_command);
	g_object_unref (action);
}

static void
mail_viewer_update_clipboard_actions (EMailViewer *self)
{
	g_return_if_fail (E_IS_MAIL_VIEWER (self));

	if (self->priv->mail_display) {
		WebKitWebView *web_view = WEBKIT_WEB_VIEW (self->priv->mail_display);
		GAction *action;

		action = g_action_map_lookup_action (self->priv->action_map, "copy");
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), e_web_view_has_selection (E_WEB_VIEW (web_view)));

		action = g_action_map_lookup_action (self->priv->action_map, "cut");
		webkit_web_view_can_execute_editing_command (web_view, WEBKIT_EDITING_COMMAND_CUT, NULL,
			mail_viewer_can_execute_editing_command_cb, g_object_ref (action));

		action = g_action_map_lookup_action (self->priv->action_map, "paste");
		webkit_web_view_can_execute_editing_command (web_view, WEBKIT_EDITING_COMMAND_PASTE, NULL,
			mail_viewer_can_execute_editing_command_cb, g_object_ref (action));
	}
}

static gboolean
mail_viewer_key_press_event_cb (GtkWidget *widget,
				GdkEventKey *event)
{
	EMailViewer *self = E_MAIL_VIEWER (widget);
	GtkWidget *focused;

	if (!event || (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0 ||
	    event->keyval == GDK_KEY_Tab ||
	    event->keyval == GDK_KEY_Return ||
	    event->keyval == GDK_KEY_KP_Tab ||
	    event->keyval == GDK_KEY_KP_Enter)
		return event && e_mail_display_need_key_event (self->priv->mail_display, event);

	focused = gtk_window_get_focus (GTK_WINDOW (self));

	if (focused && (GTK_IS_ENTRY (focused) || GTK_IS_EDITABLE (focused) ||
	    (GTK_IS_TREE_VIEW (focused) && gtk_tree_view_get_search_column (GTK_TREE_VIEW (focused)) >= 0))) {
		gtk_widget_event (focused, (GdkEvent *) event);
		return event->keyval != GDK_KEY_Escape;
	}

	if (e_web_view_get_need_input (E_WEB_VIEW (self->priv->mail_display)) &&
	    gtk_widget_has_focus (GTK_WIDGET (self->priv->mail_display))) {
		gtk_widget_event (GTK_WIDGET (self->priv->mail_display), (GdkEvent *) event);
		return TRUE;
	}

	if (e_mail_display_need_key_event (self->priv->mail_display, event))
		return TRUE;

	if (event->keyval == GDK_KEY_Escape) {
		GAction *action;

		action = g_action_map_lookup_action (self->priv->action_map, "close");
		g_action_activate (action, NULL);

		return TRUE;
	}

	return FALSE;
}

static void
mail_viewer_submit_alert (EAlertSink *alert_sink,
			  EAlert *alert)
{
	EMailViewer *self;

	g_return_if_fail (E_IS_MAIL_VIEWER (alert_sink));

	self = E_MAIL_VIEWER (alert_sink);

	e_alert_bar_submit_alert (self->priv->alert_bar, alert);
}

static void
mail_viewer_set_property (GObject *object,
			  guint property_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	EMailViewer *self = E_MAIL_VIEWER (object);

	switch (property_id) {
		case PROP_BACKEND:
			g_return_if_fail (self->priv->backend == NULL);
			self->priv->backend = g_value_dup_object (value);
			g_return_if_fail (self->priv->backend != NULL);
			return;
		default:
			break;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_viewer_get_property (GObject *object,
			  guint property_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	EMailViewer *self = E_MAIL_VIEWER (object);

	switch (property_id) {
		case PROP_BACKEND:
			g_value_set_object (value, e_mail_viewer_get_backend (self));
			return;
		default:
			break;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_viewer_dispose (GObject *object)
{
	EMailViewer *self = E_MAIL_VIEWER (object);

	if (self->priv->cancellable) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	g_clear_object (&self->priv->e_menu_bar);
	g_clear_object (&self->priv->action_map);
	g_clear_object (&self->priv->accel_group);
	g_clear_object (&self->priv->file_import);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_viewer_parent_class)->dispose (object);
}

static void
mail_viewer_finalize (GObject *object)
{
	EMailViewer *self = E_MAIL_VIEWER (object);

	g_clear_object (&self->priv->backend);
	g_clear_object (&self->priv->builder);
	g_clear_object (&self->priv->file);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_viewer_parent_class)->finalize (object);
}

static void
mail_viewer_constructed (GObject *object)
{
	EMailViewer *self = E_MAIL_VIEWER (object);
	EShell *shell;
	EAttachmentStore *attachment_store;
	GObject *menu_object;
	GtkWidget *widget;
	GtkWidget *content_box;
	GtkWidget *mail_display;
	GtkWidget *menu_button = NULL;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_mail_viewer_parent_class)->constructed (object);

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (self->priv->backend));

	gtk_application_add_window (GTK_APPLICATION (shell), GTK_WINDOW (object));

	content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_container_add (GTK_CONTAINER (self), content_box);
	gtk_widget_show (content_box);

	self->priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (self->priv->builder, "evolution-mail-viewer.ui");

	widget = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (content_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	self->priv->statusbar = widget;

	menu_object = gtk_builder_get_object (self->priv->builder, "menu");
	widget = gtk_menu_bar_new_from_model (G_MENU_MODEL (menu_object));
	gtk_box_pack_start (GTK_BOX (content_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	self->priv->real_menu_bar = GTK_MENU_BAR (widget);
	self->priv->e_menu_bar = e_menu_bar_new (self->priv->real_menu_bar, GTK_WINDOW (self), &menu_button);
	self->priv->has_menumultiple = FALSE;

	self->priv->file_import = g_menu_item_new (_("Import…"), "mail-viewer.import-all");
	menu_object = gtk_builder_get_object (self->priv->builder, "filesection");
	g_menu_insert_item (G_MENU (menu_object), 0, self->priv->file_import);

	menu_object = gtk_builder_get_object (self->priv->builder, "charset-submenu");
	e_charset_add_to_g_menu (G_MENU (menu_object), "mail-viewer.charset");

	widget = e_alert_bar_new ();
	gtk_widget_set_margin_bottom (widget, 6);
	gtk_box_pack_start (GTK_BOX (content_box), widget, FALSE, FALSE, 0);
	self->priv->alert_bar = E_ALERT_BAR (widget);

	widget = e_activity_bar_new ();
	gtk_widget_set_margin_bottom (widget, 6);
	gtk_box_pack_start (GTK_BOX (content_box), widget, FALSE, FALSE, 0);
	self->priv->activity_bar = E_ACTIVITY_BAR (widget);

	if (e_util_get_use_header_bar ()) {
		GtkHeaderBar *header_bar;

		widget = e_header_bar_new ();
		header_bar = GTK_HEADER_BAR (widget);

		if (menu_button)
			e_header_bar_pack_end (E_HEADER_BAR (header_bar), menu_button, G_MAXUINT);

		gtk_window_set_titlebar (GTK_WINDOW (self), widget);
		gtk_widget_show (widget);
	} else if (menu_button) {
		g_object_ref_sink (menu_button);
		gtk_widget_destroy (menu_button);
		menu_button = NULL;
	}

	widget = e_web_view_preview_new ();
	self->priv->webview_preview = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (self->priv->webview_preview));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_insert_column_with_attributes (
		/* Translators: Column header for a message subject */
		tree_view, -1, C_("mboxImp", "Subject"), renderer, "text", COL_SUBJECT, NULL);
	g_object_set (G_OBJECT (gtk_tree_view_get_column (tree_view, 0)),
		"expand", TRUE,
		"resizable", TRUE,
		NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_tree_view_insert_column_with_attributes (
		/* Translators: Column header for a message From address */
		tree_view, -1, C_("mboxImp", "From"), renderer, "text", COL_FROM, NULL);
	g_object_set (G_OBJECT (gtk_tree_view_get_column (tree_view, 1)),
		"min-width", 120,
		NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (mail_viewer_selection_changed_cb), self);

	mail_display = e_mail_display_new (e_mail_backend_get_remote_content (self->priv->backend));

	g_signal_connect_swapped (
		mail_display, "status-message",
		G_CALLBACK (mail_viewer_status_message_cb), self);

	self->priv->preview_pane = e_preview_pane_new (E_WEB_VIEW (mail_display));
	gtk_widget_show (self->priv->preview_pane);

	e_web_view_preview_set_preview (E_WEB_VIEW_PREVIEW (self->priv->webview_preview), self->priv->preview_pane);
	gtk_widget_show (mail_display);
	self->priv->mail_display = E_MAIL_DISPLAY (mail_display);

	attachment_store = e_mail_display_get_attachment_store (self->priv->mail_display);
	widget = GTK_WIDGET (e_mail_display_get_attachment_view (self->priv->mail_display));
	gtk_box_pack_start (GTK_BOX (content_box), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	gtk_box_pack_start (GTK_BOX (e_attachment_bar_get_content_area (E_ATTACHMENT_BAR (widget))), self->priv->webview_preview, TRUE, TRUE, 0);

	e_binding_bind_property_full (
		attachment_store, "num-attachments",
		widget, "attachments-visible",
		G_BINDING_SYNC_CREATE,
		e_attachment_store_transform_num_attachments_to_visible_boolean,
		NULL, NULL, NULL);

	g_signal_connect_object (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY), "owner-change",
		G_CALLBACK (mail_viewer_update_clipboard_actions), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner-change",
		G_CALLBACK (mail_viewer_update_clipboard_actions), self, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		self->priv->mail_display, "notify::has-selection",
		G_CALLBACK (mail_viewer_update_clipboard_actions), self, G_CONNECT_SWAPPED);

	g_signal_connect (
		self, "key-press-event",
		G_CALLBACK (mail_viewer_key_press_event_cb), NULL);

	mail_viewer_init_accel_group (self);
	mail_viewer_update_actions (self);
	mail_viewer_update_clipboard_actions (self);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_mail_viewer_class_init (EMailViewerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = mail_viewer_set_property;
	object_class->get_property = mail_viewer_get_property;
	object_class->dispose = mail_viewer_dispose;
	object_class->finalize = mail_viewer_finalize;
	object_class->constructed = mail_viewer_constructed;

	g_object_class_install_property (
		object_class,
		PROP_BACKEND,
		g_param_spec_object (
			"backend", NULL, NULL,
			E_TYPE_MAIL_BACKEND,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
mail_viewer_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = mail_viewer_submit_alert;
}

static void
e_mail_viewer_init (EMailViewer *self)
{
	self->priv = e_mail_viewer_get_instance_private (self);

	gtk_window_set_default_size (GTK_WINDOW (self), 600, 400);

	e_restore_window (
		GTK_WINDOW (self),
		"/org/gnome/evolution/mail/viewer-window/",
		E_RESTORE_WINDOW_SIZE);

	mail_viewer_init_actions (self);
}

EMailViewer *
e_mail_viewer_new (EMailBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return g_object_new (E_TYPE_MAIL_VIEWER,
		"backend", backend,
		NULL);
}

EMailBackend *
e_mail_viewer_get_backend (EMailViewer *self)
{
	g_return_val_if_fail (E_IS_MAIL_VIEWER (self), NULL);

	return self->priv->backend;
}

typedef struct _ReadFileData {
	GWeakRef *weakref; /* EMailViewer */
	CamelMimeParser *mime_parser;
	GtkListStore *store; /* out value */
	guint64 file_size;
} ReadFileData;

static void
read_file_data_free (gpointer ptr)
{
	ReadFileData *rfd = ptr;

	if (rfd) {
		g_clear_pointer (&rfd->weakref, e_weak_ref_free);
		g_clear_object (&rfd->mime_parser);
		g_clear_object (&rfd->store);
		g_free (rfd);
	}
}

static gboolean
mail_viewer_read_file_data_idle_cb (gpointer user_data)
{
	ReadFileData *rfd = user_data;
	EMailViewer *self = g_weak_ref_get (rfd->weakref);

	if (self) {
		gboolean needs_menumultiple = FALSE;

		if (rfd->store) {
			EWebViewPreview *webview_preview;
			GtkTreeView *tree_view;
			GtkTreeSelection *selection;
			GtkTreeIter iter;

			webview_preview = E_WEB_VIEW_PREVIEW (self->priv->webview_preview);
			tree_view = e_web_view_preview_get_tree_view (webview_preview);

			needs_menumultiple = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (rfd->store), NULL) > 1;
			if (needs_menumultiple)
				e_web_view_preview_show_tree_view (webview_preview);
			else
				e_web_view_preview_hide_tree_view (webview_preview);

			gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (rfd->store));

			selection = gtk_tree_view_get_selection (tree_view);
			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (rfd->store), &iter)) {
				gtk_tree_selection_select_iter (selection, &iter);
			} else {
				g_warn_if_reached ();
				mail_viewer_selection_changed_cb (selection, self);
			}
		}

		if ((!needs_menumultiple) != (!self->priv->has_menumultiple)) {
			GMenu *msgsubmenu, *filesection;

			self->priv->has_menumultiple = needs_menumultiple;

			msgsubmenu = G_MENU (gtk_builder_get_object (self->priv->builder, "msgsubmenu"));

			if (needs_menumultiple) {
				GObject *goto_menu = gtk_builder_get_object (self->priv->builder, "goto-menu");

				g_menu_item_set_label (self->priv->file_import, _("_Import All…"));
				g_menu_insert_section (msgsubmenu, 1, NULL, G_MENU_MODEL (goto_menu));
			} else {
				g_menu_item_set_label (self->priv->file_import, _("_Import…"));
				g_menu_remove (msgsubmenu, 1);
			}

			filesection = G_MENU (gtk_builder_get_object (self->priv->builder, "filesection"));
			g_menu_remove (filesection, 0);
			g_menu_insert_item (filesection, 0, self->priv->file_import);
		}
	}

	g_clear_object (&self);
	read_file_data_free (rfd);

	return G_SOURCE_REMOVE;
}

static void
mail_viewer_blame_message_with_headers (GtkListStore *store,
					CamelMimeMessage *msg,
					CamelNameValueArray *headers,
					goffset start_offset)
{
	const gchar *copy_headers[] = {
		"Content-Type", /* to know charset for headers; set it first */
		"From",
		"Subject"
	};
	CamelMedium *medium = CAMEL_MEDIUM (msg);
	GtkTreeIter iter;
	const gchar *tmp;
	gchar *from = NULL;
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (copy_headers); ii++) {
		const gchar *name = copy_headers[ii];

		tmp = camel_name_value_array_get_named (headers, CAMEL_COMPARE_CASE_INSENSITIVE, name);
		camel_medium_set_header (medium, name, tmp);
	}

	tmp = camel_mime_message_get_subject (msg);

	if (camel_mime_message_get_from (msg))
		from = camel_address_encode (CAMEL_ADDRESS (camel_mime_message_get_from (msg)));

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
		COL_FROM, from ? from : "",
		COL_SUBJECT, tmp ? tmp : "",
		COL_OFFSET, (gint64) start_offset,
		-1);

	g_free (from);
}

static void
mail_viewer_read_file_data_thread (EAlertSinkThreadJobData *job_data,
				   gpointer user_data,
				   GCancellable *cancellable,
				   GError **error)
{
	ReadFileData *rfd = user_data;

	if (!g_cancellable_is_cancelled (cancellable)) {
		CamelMimeParserState state;
		CamelMimeMessage *msg; /* only for easier parsing of the headers */
		CamelNameValueArray *headers;

		rfd->store = gtk_list_store_new (N_COLUMNS,
			G_TYPE_STRING, /* COL_SUBJECT */
			G_TYPE_STRING, /* COL_FROM */
			G_TYPE_INT64); /* COL_OFFSET */

		msg = camel_mime_message_new ();

		state = camel_mime_parser_state (rfd->mime_parser);
		if (state == CAMEL_MIME_PARSER_STATE_HEADER ||
		    state == CAMEL_MIME_PARSER_STATE_MULTIPART) {
			headers = camel_mime_parser_dup_headers (rfd->mime_parser);
			if (headers) {
				mail_viewer_blame_message_with_headers (rfd->store, msg, headers, 0);
				camel_name_value_array_free (headers);
			}
		} else {
			gint percent = 0;
			goffset start_offset = camel_mime_parser_tell_start_from (rfd->mime_parser);

			while (state == CAMEL_MIME_PARSER_STATE_FROM && !g_cancellable_is_cancelled (cancellable)) {
				state = camel_mime_parser_step (rfd->mime_parser, NULL, NULL);
				if (state == CAMEL_MIME_PARSER_STATE_HEADER ||
				    state == CAMEL_MIME_PARSER_STATE_MULTIPART) {
					headers = camel_mime_parser_dup_headers (rfd->mime_parser);
					if (headers) {
						mail_viewer_blame_message_with_headers (rfd->store, msg, headers, start_offset);
						camel_name_value_array_free (headers);
					}
				}

				while (state = camel_mime_parser_step (rfd->mime_parser, NULL, NULL),
				       state != CAMEL_MIME_PARSER_STATE_FROM &&
				       state != CAMEL_MIME_PARSER_STATE_END &&
				       state != CAMEL_MIME_PARSER_STATE_EOF &&
				       !g_cancellable_is_cancelled (cancellable)) {
					/* skip until end of file or the start of the next message */
					if (rfd->file_size > 0) {
						gint new_percent;

						new_percent = 100 * camel_mime_parser_tell (rfd->mime_parser) / rfd->file_size;
						if (new_percent > percent && new_percent <= 100) {
							percent = new_percent;
							camel_operation_progress (cancellable, percent);
						}
					}
				}

				start_offset = camel_mime_parser_tell_start_from (rfd->mime_parser);

				if (rfd->file_size > 0) {
					gint new_percent;

					new_percent = 100 * camel_mime_parser_tell (rfd->mime_parser) / rfd->file_size;
					if (new_percent > percent && new_percent <= 100) {
						percent = new_percent;
						camel_operation_progress (cancellable, percent);
					}
				}
			}

			if (rfd->file_size > 0 && !g_cancellable_is_cancelled (cancellable))
				camel_operation_progress (cancellable, 100);
		}

		g_clear_object (&msg);
	}

	if (!g_cancellable_set_error_if_cancelled (cancellable, error)) {
		ReadFileData *rfd2;

		rfd2 = g_new0 (ReadFileData, 1);
		rfd2->weakref = g_steal_pointer (&rfd->weakref);
		rfd2->store = g_steal_pointer (&rfd->store);

		g_idle_add (mail_viewer_read_file_data_idle_cb, rfd2);
	}
}

static gboolean
mail_viewer_file_is_mbox (EMailViewer *self)
{
	GFileInfo *info;
	const gchar *content_type;
	gboolean is_mbox;

	info = 	g_file_query_info (self->priv->file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (!info)
		return TRUE;

	content_type = g_file_info_get_content_type (info);
	is_mbox = content_type && g_content_type_is_mime_type (content_type, "application/mbox");

	g_clear_object (&info);

	return is_mbox;
}

gboolean
e_mail_viewer_assign_file (EMailViewer *self,
			   GFile *file)
{
	CamelMimeParserState state;
	CamelMimeParser *mime_parser;
	EActivity *activity;
	GtkTreeView *tree_view;
	ReadFileData *rfd;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_MAIL_VIEWER (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	if (self->priv->cancellable) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	if (self->priv->file != file) {
		g_clear_object (&self->priv->file);
		self->priv->file = g_object_ref (file);
	}

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (self->priv->webview_preview));
	gtk_tree_view_set_model (tree_view, NULL);
	e_web_view_clear (E_WEB_VIEW (self->priv->mail_display));

	self->priv->scan_from = mail_viewer_file_is_mbox (self);
	mime_parser = mail_viewer_create_mime_parser (self->priv->file, 0, self->priv->scan_from, &local_error);
	if (!mime_parser) {
		mail_viewer_report_error (self, local_error ? local_error->message : _("Unknown error"));
		g_clear_error (&local_error);
		return FALSE;
	}

	state = camel_mime_parser_step (mime_parser, NULL, NULL);
	if (state != CAMEL_MIME_PARSER_STATE_FROM && self->priv->scan_from) {
		/* re-try without reading the "From " lines */
		g_clear_object (&mime_parser);

		self->priv->scan_from = FALSE;
		mime_parser = mail_viewer_create_mime_parser (self->priv->file, 0, self->priv->scan_from, &local_error);
		if (!mime_parser) {
			mail_viewer_report_error (self, local_error ? local_error->message : _("Unknown error"));
			g_clear_error (&local_error);
			return FALSE;
		}

		state = camel_mime_parser_step (mime_parser, NULL, NULL);
	}

	if (state != CAMEL_MIME_PARSER_STATE_FROM &&
	    state != CAMEL_MIME_PARSER_STATE_HEADER &&
	    state != CAMEL_MIME_PARSER_STATE_MULTIPART) {
		gchar *msg;

		g_clear_object (&mime_parser);

		msg = g_strdup_printf (_("File “%s” does not contain MIME message"), g_file_peek_path (file));
		mail_viewer_report_error (self, msg);
		g_free (msg);

		return FALSE;
	}

	mail_viewer_set_window_title (self, _("Loading…"));

	mail_viewer_update_actions (self);

	rfd = g_new0 (ReadFileData, 1);
	rfd->weakref = e_weak_ref_new (self);
	rfd->mime_parser = mime_parser;
	rfd->file_size = mail_viewer_get_file_size (self->priv->file);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (self),
		_("Reading file…"),
		"system:generic-error",
		_("Failed to read file content"),
		mail_viewer_read_file_data_thread, rfd,
		read_file_data_free);

	mail_viewer_handle_new_activity (self, activity);

	g_clear_object (&activity);

	return TRUE;
}

GFile *
e_mail_viewer_get_file (EMailViewer *self)
{
	g_return_val_if_fail (E_IS_MAIL_VIEWER (self), NULL);

	return self->priv->file;
}
