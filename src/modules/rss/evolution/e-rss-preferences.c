/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "shell/e-shell.h"
#include "camel-rss-store-summary.h"
#include "e-rss-parser.h"

#include "e-rss-preferences.h"

enum {
	COLUMN_STRING_ID = 0,
	COLUMN_STRING_NAME,
	COLUMN_STRING_HREF,
	COLUMN_STRING_CONTENT_TYPE,
	COLUMN_STRING_DESCRIPTION,
	COLUMN_PIXBUF_ICON,
	N_COLUMNS
};

static const gchar *
e_rss_preferences_content_type_to_string (CamelRssContentType content_type)
{
	switch (content_type) {
	case CAMEL_RSS_CONTENT_TYPE_HTML:
		break;
	case CAMEL_RSS_CONTENT_TYPE_PLAIN_TEXT:
		return "text";
	case CAMEL_RSS_CONTENT_TYPE_MARKDOWN:
		return "markdown";
	}

	return "html";
}

static const gchar *
e_rss_preferences_content_type_to_locale_string (CamelRssContentType content_type)
{
	switch (content_type) {
	case CAMEL_RSS_CONTENT_TYPE_HTML:
		break;
	case CAMEL_RSS_CONTENT_TYPE_PLAIN_TEXT:
		return _("Plain Text");
	case CAMEL_RSS_CONTENT_TYPE_MARKDOWN:
		return _("Markdown");
	}

	return _("HTML");
}

static CamelRssContentType
e_rss_preferences_content_type_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "text") == 0)
		return CAMEL_RSS_CONTENT_TYPE_PLAIN_TEXT;

	if (g_strcmp0 (str, "markdown") == 0)
		return CAMEL_RSS_CONTENT_TYPE_MARKDOWN;

	return CAMEL_RSS_CONTENT_TYPE_HTML;
}

static CamelService *
e_rss_preferences_ref_store (EShell *shell)
{
	EShellBackend *shell_backend;
	CamelSession *session = NULL;
	CamelService *service;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	if (!shell_backend)
		return NULL;

	g_object_get (G_OBJECT (shell_backend), "session", &session, NULL);
	if (!session)
		return NULL;

	service = camel_session_ref_service (session, "rss");

	g_clear_object (&session);

	return service;
}

static gchar *
e_rss_preferences_describe_feed (const gchar *href,
				 const gchar *name)
{
	return g_markup_printf_escaped ("%s\n<small>%s</small>", name, href);
}

static GdkPixbuf *
e_rss_preferences_create_icon_pixbuf (const gchar *icon_filename)
{
	GdkPixbuf *pixbuf = NULL;

	if (icon_filename && *icon_filename) {
		GError *error = NULL;

		pixbuf = gdk_pixbuf_new_from_file (icon_filename, &error);

		if (!pixbuf)
			g_warning ("%s: Failed to load feed icon '%s': %s", G_STRFUNC, icon_filename, error ? error->message : "Unknown error");

		g_clear_error (&error);
	}

	if (!pixbuf)
		pixbuf = e_icon_factory_get_icon ("rss", GTK_ICON_SIZE_DIALOG);

	return pixbuf;
}

static void
e_rss_preferences_add_feed_to_list_store (GtkListStore *list_store,
					  CamelRssStoreSummary *store_summary,
					  const gchar *id)
{
	const gchar *href, *display_name, *icon_filename;
	CamelRssContentType content_type;
	gchar *description;
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;

	href = camel_rss_store_summary_get_href (store_summary, id);
	display_name = camel_rss_store_summary_get_display_name (store_summary, id);
	content_type = camel_rss_store_summary_get_content_type (store_summary, id);
	description = e_rss_preferences_describe_feed (href, display_name);
	icon_filename = camel_rss_store_summary_get_icon_filename (store_summary, id);
	pixbuf = e_rss_preferences_create_icon_pixbuf (icon_filename);

	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter,
		COLUMN_STRING_ID, id,
		COLUMN_STRING_NAME, display_name,
		COLUMN_STRING_HREF, href,
		COLUMN_STRING_CONTENT_TYPE, e_rss_preferences_content_type_to_locale_string (content_type),
		COLUMN_STRING_DESCRIPTION, description,
		COLUMN_PIXBUF_ICON, pixbuf,
		-1);

	g_clear_object (&pixbuf);
	g_free (description);
}

static void
e_rss_preferences_fill_list_store (GtkListStore *list_store,
				   CamelRssStoreSummary *store_summary)
{
	GSList *feeds, *link;

	gtk_list_store_clear (list_store);

	feeds = camel_rss_store_summary_dup_feeds (store_summary);

	for (link = feeds; link; link = g_slist_next (link)) {
		const gchar *id = link->data;

		e_rss_preferences_add_feed_to_list_store (list_store, store_summary, id);
	}

	g_slist_free_full (feeds, g_free);
}

static void
e_rss_preferences_source_written_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	GError *error = NULL;

	if (!e_source_write_finish (E_SOURCE (source_object), result, &error))
		g_warning ("%s: Failed to save RSS changes: %s", G_STRFUNC, error ? error->message : "Unknown error");

	g_clear_error (&error);
}

static void
e_rss_preferences_source_changed_cb (ESource *source)
{
	e_source_write (source, NULL, e_rss_preferences_source_written_cb, NULL);
}

static void
e_rss_preferences_three_state_toggled_cb (GtkToggleButton *widget,
					  gpointer user_data)
{
	gulong *phandler_id = user_data;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
	g_return_if_fail (phandler_id != NULL);

	g_signal_handler_block (widget, *phandler_id);

	if (gtk_toggle_button_get_inconsistent (widget) &&
	    gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_active (widget, FALSE);
		gtk_toggle_button_set_inconsistent (widget, FALSE);
	} else if (!gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_inconsistent (widget, TRUE);
		gtk_toggle_button_set_active (widget, FALSE);
	}

	g_signal_handler_unblock (widget, *phandler_id);
}

static GtkWidget *
e_rss_preferences_new_three_state_check (const gchar *label)
{
	GtkWidget *widget;
	gulong *phandler_id;

	widget = gtk_check_button_new_with_mnemonic (label);

	g_object_set (widget,
		"inconsistent", TRUE,
		"active", FALSE,
		"visible", TRUE,
		NULL);

	phandler_id = g_new (gulong, 1);

	*phandler_id = g_signal_connect_data (widget, "toggled",
		G_CALLBACK (e_rss_preferences_three_state_toggled_cb),
		phandler_id, (GClosureNotify) g_free, 0);

	return widget;
}

static CamelThreeState
e_rss_preferences_three_state_from_widget (GtkToggleButton *button)
{
	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (button), CAMEL_THREE_STATE_INCONSISTENT);

	if (gtk_toggle_button_get_inconsistent (button))
		return CAMEL_THREE_STATE_INCONSISTENT;

	if (gtk_toggle_button_get_active (button))
		return CAMEL_THREE_STATE_ON;

	return CAMEL_THREE_STATE_OFF;
}

static void
e_rss_preferences_three_state_to_widget (GtkToggleButton *button,
					 CamelThreeState state)
{
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

	g_signal_handlers_block_matched (button, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, e_rss_preferences_three_state_toggled_cb, NULL);

	if (state == CAMEL_THREE_STATE_INCONSISTENT) {
		gtk_toggle_button_set_active (button, FALSE);
		gtk_toggle_button_set_inconsistent (button, TRUE);
	} else {
		gtk_toggle_button_set_inconsistent (button, FALSE);
		gtk_toggle_button_set_active (button, state == CAMEL_THREE_STATE_ON);
	}

	g_signal_handlers_unblock_matched (button, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, e_rss_preferences_three_state_toggled_cb, NULL);
}

typedef struct _FolderOpts {
	CamelThreeState complete_articles;
	CamelThreeState feed_enclosures;
} FolderOpts;

static void
e_rss_properties_got_folder_to_save_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	FolderOpts *fo = user_data;
	CamelFolder *folder;
	GError *error = NULL;

	folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), result, &error);

	if (folder) {
		g_object_set (folder,
			"complete-articles", fo->complete_articles,
			"feed-enclosures", fo->feed_enclosures,
			NULL);

		camel_folder_save_state (folder);
		g_object_unref (folder);
	} else {
		g_warning ("%s: Failed to get folder: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_slice_free (FolderOpts, fo);
}

typedef struct _PopoverData {
	gchar *id; /* can be NULL */
	GtkEntry *href;
	GtkWidget *fetch_button;
	GtkEntry *name;
	GtkButton *icon_button;
	GtkImage *icon_image;
	GtkComboBox *content_type;
	GtkToggleButton *complete_articles;
	GtkToggleButton *feed_enclosures;
	GtkWidget *save_button;
	gchar *icon_filename;
	EActivityBar *activity_bar;
	EActivity *activity;
} PopoverData;

static void
popover_data_cancel_activity (PopoverData *pd)
{
	GCancellable *cancellable;

	if (!pd || !pd->activity)
		return;

	cancellable = e_activity_get_cancellable (pd->activity);
	g_cancellable_cancel (cancellable);

	e_activity_set_state (pd->activity, E_ACTIVITY_CANCELLED);

	g_clear_object (&pd->activity);
}

static void
popover_data_free (gpointer ptr)
{
	PopoverData *pd = ptr;

	if (pd) {
		popover_data_cancel_activity (pd);

		g_free (pd->id);
		g_free (pd->icon_filename);
		g_free (pd);
	}
}

static void
e_rss_preferences_entry_changed_cb (GtkEntry *entry,
				    gpointer user_data)
{
	GObject *popover = user_data;
	PopoverData *pd;
	const gchar *text;
	gboolean sensitive;

	pd = g_object_get_data (popover, "e-rss-popover-data");

	text = gtk_entry_get_text (pd->href);
	sensitive = text && *text;
	gtk_widget_set_sensitive (pd->fetch_button, sensitive);

	if (sensitive) {
		text = gtk_entry_get_text (pd->name);
		sensitive = text && *text;
	}

	gtk_widget_set_sensitive (pd->save_button, sensitive);
}

static void
e_rss_preferences_maybe_scale_image (GtkImage *image)
{
	if (gtk_image_get_storage_type (image) == GTK_IMAGE_PIXBUF) {
		GdkPixbuf *pixbuf;

		pixbuf = gtk_image_get_pixbuf (image);
		if (pixbuf && (gdk_pixbuf_get_width (pixbuf) > 48 || gdk_pixbuf_get_height (pixbuf) > 48)) {
			gint width, height;

			width = gdk_pixbuf_get_width (pixbuf);
			height = gdk_pixbuf_get_height (pixbuf);

			if (width > height) {
				height = height * 48 / width;
				width = 48;
			} else {
				width = width * 48 / height;
				height = 48;
			}

			pixbuf = e_icon_factory_pixbuf_scale (pixbuf, width, height);
			gtk_image_set_from_pixbuf (image, pixbuf);
			g_object_unref (pixbuf);
		}
	}
}

static void
e_rss_preferences_feed_icon_ready_cb (GObject *source_object,
				      GAsyncResult *result,
				      gpointer user_data)
{
	GObject *popover = user_data;
	GBytes *bytes;
	GError *error = NULL;

	bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object), result, &error);

	if (bytes) {
		PopoverData *pd = g_object_get_data (popover, "e-rss-popover-data");
		SoupMessage *message = soup_session_get_async_result_message (SOUP_SESSION (source_object), result);
		gboolean success = !error && g_bytes_get_size (bytes) > 0 && message &&
			SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message));

		if (success) {
			gchar *tmp_file;

			tmp_file = e_mktemp ("rss-feed-XXXXXX.png");
			success = g_file_set_contents (tmp_file, (const gchar *) g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &error);

			if (success) {
				gtk_image_set_from_file (pd->icon_image, tmp_file);
				g_clear_pointer (&pd->icon_filename, g_free);
				pd->icon_filename = g_steal_pointer (&tmp_file);
				e_rss_preferences_maybe_scale_image (pd->icon_image);
			}

			g_free (tmp_file);
		}

		if (success) {
			e_activity_set_state (pd->activity, E_ACTIVITY_COMPLETED);
			g_clear_object (&pd->activity);
		}
	}

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		PopoverData *pd = g_object_get_data (popover, "e-rss-popover-data");
		gchar *message;

		message = g_strdup_printf (_("Failed to fetch feed icon: %s"), error->message);

		e_activity_set_state (pd->activity, E_ACTIVITY_WAITING);
		e_activity_set_text (pd->activity, message);

		g_free (message);
	}

	g_clear_pointer (&bytes, g_bytes_unref);
	g_clear_error (&error);
}

static void
e_rss_preferences_feed_info_ready_cb (GObject *source_object,
				      GAsyncResult *result,
				      gpointer user_data)
{
	GObject *popover = user_data;
	GBytes *bytes;
	GError *error = NULL;

	bytes = soup_session_send_and_read_finish (SOUP_SESSION (source_object), result, &error);

	if (bytes) {
		PopoverData *pd = g_object_get_data (popover, "e-rss-popover-data");
		GCancellable *cancellable = e_activity_get_cancellable (pd->activity);
		SoupMessage *message = soup_session_get_async_result_message (SOUP_SESSION (source_object), result);
		gboolean success = !error && g_bytes_get_size (bytes) > 0 && message &&
			SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message));

		if (success) {
			gchar *link = NULL, *alt_link = NULL, *title = NULL, *icon = NULL;

			success = e_rss_parser_parse ((const gchar *) g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &link, &alt_link, &title, &icon, NULL);
			if (success) {
				if ((link && camel_strstrcase (link, "gitlab")) ||
				    (alt_link && camel_strstrcase (alt_link, "gitlab")))
					gtk_combo_box_set_active_id (pd->content_type, e_rss_preferences_content_type_to_string (CAMEL_RSS_CONTENT_TYPE_MARKDOWN));
				else
					gtk_combo_box_set_active_id (pd->content_type, e_rss_preferences_content_type_to_string (CAMEL_RSS_CONTENT_TYPE_HTML));

				if (title && *title)
					gtk_entry_set_text (pd->name, title);

				if (icon && *icon) {
					SoupMessage *message2;

					e_activity_set_text (pd->activity, _("Fetching feed icon…"));

					message2 = soup_message_new (SOUP_METHOD_GET, icon);
					if (message2) {
						soup_session_send_and_read_async (SOUP_SESSION (source_object), message2, G_PRIORITY_DEFAULT, cancellable,
							e_rss_preferences_feed_icon_ready_cb, popover);

						g_object_unref (message2);

						/* Not as a problem, but as a flag to not complete the activity */
						success = FALSE;
					}
				}
			} else {
				g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to read feed information."));
			}

			g_free (link);
			g_free (alt_link);
			g_free (title);
			g_free (icon);
		}

		if (success) {
			e_activity_set_state (pd->activity, E_ACTIVITY_COMPLETED);
			g_clear_object (&pd->activity);
		}
	}

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		PopoverData *pd = g_object_get_data (popover, "e-rss-popover-data");
		gchar *message;

		message = g_strdup_printf (_("Failed to fetch feed information: %s"), error->message);

		e_activity_set_state (pd->activity, E_ACTIVITY_WAITING);
		e_activity_set_text (pd->activity, message);

		g_free (message);
	}

	g_clear_pointer (&bytes, g_bytes_unref);
	g_clear_error (&error);
}

static void
e_rss_preferences_fetch_clicked_cb (GtkWidget *button,
				    gpointer user_data)
{
	GObject *popover = user_data;
	SoupSession *session;
	SoupMessage *message;
	GCancellable *cancellable;
	PopoverData *pd;

	pd = g_object_get_data (popover, "e-rss-popover-data");
	cancellable = g_cancellable_new ();

	popover_data_cancel_activity (pd);

	pd->activity = e_activity_new ();
	e_activity_set_cancellable (pd->activity, cancellable);
	e_activity_set_state (pd->activity, E_ACTIVITY_RUNNING);
	e_activity_set_text (pd->activity, _("Fetching feed information…"));
	e_activity_bar_set_activity (pd->activity_bar, pd->activity);

	message = soup_message_new (SOUP_METHOD_GET, gtk_entry_get_text (pd->href));
	if (!message) {
		e_activity_set_text (pd->activity, _("Invalid Feed URL"));
		e_activity_set_state (pd->activity, E_ACTIVITY_WAITING);
		g_clear_object (&cancellable);

		return;
	}

	session = soup_session_new_with_options (
		"timeout", 15,
		"user-agent", "Evolution/" VERSION,
		NULL);

	if (camel_debug ("rss")) {
		SoupLogger *logger;

		logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
		soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
		g_object_unref (logger);
	}

	soup_session_send_and_read_async (session, message, G_PRIORITY_DEFAULT, cancellable,
		e_rss_preferences_feed_info_ready_cb, popover);

	g_clear_object (&message);
	g_clear_object (&session);
	g_clear_object (&cancellable);
}

static void
e_rss_preferences_icon_clicked_cb (GtkWidget *button,
				   gpointer user_data)
{
	GObject *popover = user_data;
	PopoverData *pd;
	GtkWidget *dialog;
	GtkWindow *parent;
	GFile *file;

	pd = g_object_get_data (popover, "e-rss-popover-data");

	dialog = gtk_widget_get_toplevel (button);
	parent = GTK_IS_WINDOW (dialog) ? GTK_WINDOW (dialog) : NULL;

	dialog = e_image_chooser_dialog_new (_("Choose Feed Image"), parent);
	file = e_image_chooser_dialog_run (E_IMAGE_CHOOSER_DIALOG (dialog));

	g_clear_pointer (&pd->icon_filename, g_free);

	if (G_IS_FILE (file)) {
		pd->icon_filename = g_file_get_path (file);
		gtk_image_set_from_file (pd->icon_image, pd->icon_filename);
		e_rss_preferences_maybe_scale_image (pd->icon_image);
	} else {
		gtk_image_set_from_icon_name (pd->icon_image, "rss", GTK_ICON_SIZE_DIALOG);
	}

	gtk_widget_destroy (dialog);
}

/* Copy icon to the private directory */
static gchar *
e_rss_preferences_maybe_copy_icon (const gchar *feed_id,
				   const gchar *icon_filename,
				   const gchar *user_data_dir)
{
	gchar *basename, *filename;
	GFile *src, *des;
	GdkPixbuf *pixbuf;
	const gchar *ext;
	GError *error = NULL;

	if (!icon_filename || !*icon_filename || !user_data_dir || !*user_data_dir ||
	    g_str_has_prefix (icon_filename, user_data_dir))
		return NULL;

	basename = g_path_get_basename (icon_filename);
	if (basename && *basename && (*basename == G_DIR_SEPARATOR || *basename == '.')) {
		g_free (basename);
		return NULL;
	}

	ext = basename ? strrchr (basename, '.') : NULL;
	if (!ext || !ext[1])
		ext = ".png";

	filename = g_strconcat (user_data_dir, G_DIR_SEPARATOR_S, feed_id, ext, NULL);

	src = g_file_new_for_path (icon_filename);
	des = g_file_new_for_path (filename);

	/* ensure expected icon size, as some feeds can provide large images */
	pixbuf = gdk_pixbuf_new_from_file_at_size (icon_filename, 48, 48, NULL);
	if (pixbuf) {
		gchar *pixbuf_filename = NULL;

		/* requires .png extension, when it's a .png file */
		if (g_ascii_strcasecmp (ext, ".png") != 0)
			pixbuf_filename = g_strconcat (user_data_dir, G_DIR_SEPARATOR_S, feed_id, ".png", NULL);

		if (gdk_pixbuf_save (pixbuf, pixbuf_filename ? pixbuf_filename : filename, "png", NULL, NULL)) {
			if (pixbuf_filename) {
				g_free (filename);
				filename = pixbuf_filename;
			}
		} else {
			/* NULL indicates the save to file failed */
			g_clear_object (&pixbuf);
			g_free (pixbuf_filename);
		}
	}

	if (pixbuf)
		gtk_icon_theme_rescan_if_needed (gtk_icon_theme_get_default ());
	else if (g_file_copy (src, des, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error))
		gtk_icon_theme_rescan_if_needed (gtk_icon_theme_get_default ());
	else
		g_warning ("Failed to copy icon file '%s' to '%s': %s", icon_filename, filename, error ? error->message : "Unknown error");

	g_clear_error (&error);
	g_clear_object (&pixbuf);
	g_clear_object (&src);
	g_clear_object (&des);

	g_free (basename);

	return filename;
}

static void
e_rss_preferences_create_feed (CamelService *service,
			       CamelRssStoreSummary *store_summary,
			       const gchar *href,
			       const gchar *display_name,
			       const gchar *icon_filename,
			       CamelRssContentType content_type,
			       const gchar *user_data_dir,
			       gchar **out_new_id,
			       GError **error)
{
	const gchar *new_id;

	new_id = camel_rss_store_summary_add (store_summary,
		href,
		display_name,
		icon_filename,
		content_type);

	if (new_id) {
		gchar *new_id_copy = g_strdup (new_id);
		gchar *real_icon_filename;

		real_icon_filename = e_rss_preferences_maybe_copy_icon (new_id_copy, icon_filename, user_data_dir);
		if (real_icon_filename) {
			camel_rss_store_summary_set_icon_filename (store_summary, new_id_copy, real_icon_filename);
			g_free (real_icon_filename);
		}

		if (camel_rss_store_summary_save (store_summary, error)) {
			CamelFolderInfo *fi;

			fi = camel_rss_store_summary_dup_folder_info (store_summary, new_id_copy);

			camel_store_folder_created (CAMEL_STORE (service), fi);

			camel_folder_info_free (fi);
		}

		if (out_new_id)
			*out_new_id = new_id_copy;
		else
			g_free (new_id_copy);
	}
}

static void
e_rss_preferences_save_clicked_cb (GtkWidget *button,
				   gpointer user_data)
{
	GObject *popover = user_data;
	CamelService *service;
	CamelRssStoreSummary *store_summary = NULL;
	CamelRssContentType content_type;
	FolderOpts *fo;
	gchar *icon_filename;
	const gchar *user_data_dir;
	PopoverData *pd;
	GError *error = NULL;

	pd = g_object_get_data (popover, "e-rss-popover-data");

	service = e_rss_preferences_ref_store (e_shell_get_default ());
	if (!service) {
		g_warn_if_reached ();
		return;
	}

	g_object_get (service, "summary", &store_summary, NULL);

	if (!store_summary) {
		g_clear_object (&service);
		g_warn_if_reached ();
		return;
	}

	user_data_dir = camel_service_get_user_data_dir (service);
	icon_filename = pd->icon_filename;
	content_type = e_rss_preferences_content_type_from_string (gtk_combo_box_get_active_id (pd->content_type));

	if (pd->id) {
		const gchar *display_name;
		gchar *old_display_name;
		gchar *real_icon_filename;

		old_display_name = g_strdup (camel_rss_store_summary_get_display_name (store_summary, pd->id));
		display_name = gtk_entry_get_text (pd->name);

		real_icon_filename = e_rss_preferences_maybe_copy_icon (pd->id, icon_filename, user_data_dir);

		camel_rss_store_summary_set_display_name (store_summary, pd->id, display_name);
		camel_rss_store_summary_set_icon_filename (store_summary, pd->id, real_icon_filename ? real_icon_filename : icon_filename);
		camel_rss_store_summary_set_content_type (store_summary, pd->id, content_type);

		if (camel_rss_store_summary_save (store_summary, &error) &&
		    g_strcmp0 (old_display_name, display_name) != 0) {
			CamelFolderInfo *fi;

			fi = camel_rss_store_summary_dup_folder_info (store_summary, pd->id);

			camel_store_folder_renamed (CAMEL_STORE (service), pd->id, fi);

			camel_folder_info_free (fi);
		}

		g_free (real_icon_filename);
		g_free (old_display_name);
	} else {
		e_rss_preferences_create_feed (service, store_summary,
			gtk_entry_get_text (pd->href),
			gtk_entry_get_text (pd->name),
			icon_filename,
			content_type,
			user_data_dir,
			&pd->id,
			&error);
	}

	fo = g_slice_new0 (FolderOpts);
	fo->complete_articles = e_rss_preferences_three_state_from_widget (pd->complete_articles);
	fo->feed_enclosures = e_rss_preferences_three_state_from_widget (pd->feed_enclosures);

	camel_store_get_folder (CAMEL_STORE (service), pd->id, CAMEL_STORE_FOLDER_NONE, G_PRIORITY_DEFAULT, NULL,
		e_rss_properties_got_folder_to_save_cb, fo);

	if (error) {
		g_warning ("Failed to store RSS settings: %s", error->message);
		g_clear_error (&error);
	}

	g_clear_object (&store_summary);
	g_clear_object (&service);

	gtk_widget_hide (GTK_WIDGET (popover));
}

static GtkPopover *
e_rss_preferences_get_popover (GtkWidget *parent,
			       GtkTreeView *tree_view,
			       const gchar *id,
			       PopoverData **out_pd)
{
	GtkPopover *popover;
	PopoverData *pd;
	GtkGrid *grid;
	GtkWidget *widget, *label;

	popover = g_object_get_data (G_OBJECT (tree_view), "e-rss-popover");

	if (popover) {
		pd = g_object_get_data (G_OBJECT (popover), "e-rss-popover-data");
		gtk_popover_set_relative_to (popover, parent);
		g_clear_pointer (&pd->id, g_free);
		g_clear_pointer (&pd->icon_filename, g_free);
		pd->id = g_strdup (id);

		*out_pd = pd;

		return popover;
	}

	pd = g_new0 (PopoverData, 1);
	pd->id = g_strdup (id);

	popover = GTK_POPOVER (gtk_popover_new (parent));

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 6);
	gtk_grid_set_row_spacing (grid, 6);

	widget = gtk_button_new ();
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_grid_attach (grid, widget, 0, 0, 1, 3);
	pd->icon_button = GTK_BUTTON (widget);

	widget = gtk_image_new_from_icon_name ("rss", GTK_ICON_SIZE_DIALOG);
	gtk_container_add (GTK_CONTAINER (pd->icon_button), widget);
	pd->icon_image = GTK_IMAGE (widget);

	widget = gtk_label_new_with_mnemonic (_("Feed _URL:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	label = widget;

	widget = gtk_entry_new ();
	gtk_widget_set_size_request (widget, 250, -1);
	gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 2, 0, 1, 1);
	pd->href = GTK_ENTRY (widget);

	widget = gtk_button_new_with_mnemonic (_("_Fetch"));
	gtk_grid_attach (grid, widget, 3, 0, 1, 1);
	pd->fetch_button = widget;

	widget = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	label = widget;

	widget = gtk_entry_new ();
	gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 2, 1, 2, 1);
	pd->name = GTK_ENTRY (widget);

	widget = gtk_label_new_with_mnemonic (_("C_ontent:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);
	label = widget;

	widget = gtk_combo_box_text_new ();
	gtk_widget_set_size_request (widget, 250, -1);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "html", _("HTML"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "text", _("Plain Text"));
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), "markdown", _("Markdown"));
	gtk_grid_attach (grid, widget, 2, 2, 2, 1);
	pd->content_type = GTK_COMBO_BOX (widget);

	widget = e_rss_preferences_new_three_state_check (_("_Download complete articles"));
	gtk_grid_attach (grid, widget, 2, 3, 2, 1);
	pd->complete_articles = GTK_TOGGLE_BUTTON (widget);

	widget = e_rss_preferences_new_three_state_check (_("Download feed _enclosures"));
	gtk_grid_attach (grid, widget, 2, 4, 2, 1);
	pd->feed_enclosures = GTK_TOGGLE_BUTTON (widget);

	widget = gtk_button_new_with_mnemonic (_("_Save"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 1, 5, 3, 1);
	pd->save_button = widget;

	gtk_widget_show_all (GTK_WIDGET (grid));

	widget = e_activity_bar_new ();
	gtk_grid_attach (grid, widget, 0, 6, 4, 1);
	pd->activity_bar = E_ACTIVITY_BAR (widget);

	gtk_popover_set_position (popover, GTK_POS_BOTTOM);
	gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (grid));
	gtk_container_set_border_width (GTK_CONTAINER (popover), 6);

	g_object_set_data_full (G_OBJECT (popover), "e-rss-popover-data", pd, popover_data_free);
	g_object_set_data_full (G_OBJECT (tree_view), "e-rss-popover", g_object_ref_sink (popover), g_object_unref);

	g_signal_connect_object (pd->href, "changed",
		G_CALLBACK (e_rss_preferences_entry_changed_cb), popover, 0);

	g_signal_connect_object (pd->name, "changed",
		G_CALLBACK (e_rss_preferences_entry_changed_cb), popover, 0);

	g_signal_connect_object (pd->fetch_button, "clicked",
		G_CALLBACK (e_rss_preferences_fetch_clicked_cb), popover, 0);

	g_signal_connect_object (pd->icon_button, "clicked",
		G_CALLBACK (e_rss_preferences_icon_clicked_cb), popover, 0);

	g_signal_connect_object (pd->save_button, "clicked",
		G_CALLBACK (e_rss_preferences_save_clicked_cb), popover, 0);

	e_rss_preferences_entry_changed_cb (pd->href, popover);

	*out_pd = pd;

	return popover;
}

static void
e_rss_preferences_add_clicked_cb (GtkWidget *button,
				  GtkTreeView *tree_view)
{
	GtkPopover *popover;
	PopoverData *pd = NULL;

	popover = e_rss_preferences_get_popover (button, tree_view, NULL, &pd);

	gtk_entry_set_text (pd->href, "");
	gtk_entry_set_text (pd->name, "");
	gtk_image_set_from_icon_name (pd->icon_image, "rss", GTK_ICON_SIZE_DIALOG);
	gtk_combo_box_set_active_id (pd->content_type, "html");
	e_rss_preferences_three_state_to_widget (pd->complete_articles, CAMEL_THREE_STATE_INCONSISTENT);
	e_rss_preferences_three_state_to_widget (pd->feed_enclosures, CAMEL_THREE_STATE_INCONSISTENT);
	g_clear_pointer (&pd->icon_filename, g_free);
	g_clear_pointer (&pd->id, g_free);

	gtk_widget_show (GTK_WIDGET (popover));
}

static gchar *
e_rss_preferences_dup_selected_id (GtkTreeView *tree_view,
				   CamelStore **out_store)
{
	CamelService *service;
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *id = NULL;

	if (out_store)
		*out_store = NULL;

	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter,
		COLUMN_STRING_ID, &id,
		-1);

	if (!id)
		return NULL;

	service = e_rss_preferences_ref_store (e_shell_get_default ());
	if (!service) {
		g_warn_if_reached ();
		g_free (id);
		return NULL;
	}

	if (out_store)
		*out_store = CAMEL_STORE (service);
	else
		g_object_unref (service);

	return id;
}

static void
e_rss_properties_got_folder_to_edit_cb (GObject *source_object,
					GAsyncResult *result,
					gpointer user_data)
{
	GtkTreeView *tree_view = user_data;
	CamelFolder *folder;
	GError *error = NULL;

	folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), result, &error);

	if (folder) {
		CamelRssStoreSummary *store_summary = NULL;
		CamelThreeState state = CAMEL_THREE_STATE_INCONSISTENT;
		GtkPopover *popover;
		PopoverData *pd = NULL;
		const gchar *icon_filename, *id;

		id = camel_folder_get_full_name (folder);
		g_object_get (source_object, "summary", &store_summary, NULL);
		popover = g_object_get_data (G_OBJECT (tree_view), "e-rss-popover");
		g_warn_if_fail (popover != NULL);
		pd = g_object_get_data (G_OBJECT (popover), "e-rss-popover-data");
		g_warn_if_fail (pd != NULL);
		g_warn_if_fail (g_strcmp0 (id, pd->id) == 0);

		icon_filename = camel_rss_store_summary_get_icon_filename (store_summary, id);

		gtk_entry_set_text (pd->href, camel_rss_store_summary_get_href (store_summary, id));
		gtk_entry_set_text (pd->name, camel_rss_store_summary_get_display_name (store_summary, id));

		if (icon_filename && g_file_test (icon_filename, G_FILE_TEST_IS_REGULAR)) {
			gtk_image_set_from_file (pd->icon_image, icon_filename);
			e_rss_preferences_maybe_scale_image (pd->icon_image);
		} else {
			gtk_image_set_from_icon_name (pd->icon_image, "rss", GTK_ICON_SIZE_DIALOG);
		}

		gtk_combo_box_set_active_id (pd->content_type, e_rss_preferences_content_type_to_string (
			camel_rss_store_summary_get_content_type (store_summary, id)));

		g_clear_pointer (&pd->icon_filename, g_free);
		pd->icon_filename = g_strdup (icon_filename);

		g_object_get (folder, "complete-articles", &state, NULL);
		e_rss_preferences_three_state_to_widget (pd->complete_articles, state);

		g_object_get (folder, "feed-enclosures", &state, NULL);
		e_rss_preferences_three_state_to_widget (pd->feed_enclosures, state);

		gtk_widget_show (GTK_WIDGET (popover));

		g_clear_object (&store_summary);
		g_object_unref (folder);
	} else {
		g_warning ("%s: Failed to get folder: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_object (&tree_view);
}

static void
e_rss_preferences_edit_clicked_cb (GtkWidget *button,
				   GtkTreeView *tree_view)
{
	CamelStore *store = NULL;
	gchar *id;

	id = e_rss_preferences_dup_selected_id (tree_view, &store);
	if (id) {
		PopoverData *pd = NULL;

		/* prepare the popover */
		g_warn_if_fail (e_rss_preferences_get_popover (button, tree_view, id, &pd) != NULL);

		camel_store_get_folder (store, id, CAMEL_STORE_FOLDER_NONE, G_PRIORITY_DEFAULT, NULL,
			e_rss_properties_got_folder_to_edit_cb, g_object_ref (tree_view));
	}

	g_clear_object (&store);
	g_free (id);
}

static void
e_rss_preferences_delete_done_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	GError *error = NULL;

	if (!camel_store_delete_folder_finish (CAMEL_STORE (source_object), result, &error))
		g_warning ("%s: Failed to delete folder: %s", G_STRFUNC, error ? error->message : "Unknown error");

	g_clear_error (&error);
}

static void
e_rss_preferences_remove_clicked_cb (GtkButton *button,
				     GtkTreeView *tree_view)
{
	CamelStore *store = NULL;
	gchar *id;

	id = e_rss_preferences_dup_selected_id (tree_view, &store);
	if (id)
		camel_store_delete_folder (store, id, G_PRIORITY_DEFAULT, NULL, e_rss_preferences_delete_done_cb, NULL);

	g_clear_object (&store);
	g_free (id);
}

static void
e_rss_pereferences_selection_changed_cb (GtkTreeSelection *selection,
					 GtkWidget *button)
{
	gtk_widget_set_sensitive (button, gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static GFile *
e_rss_choose_file (gpointer parent,
		   gboolean is_import)
{
	GtkFileChooserNative *native;
	GtkFileFilter *filter;
	GFile *file = NULL;

	native = gtk_file_chooser_native_new (
		is_import ? _("Import RSS Feeds") : _("Export RSS Feeds"),
		GTK_IS_WINDOW (parent) ? GTK_WINDOW (parent) : NULL,
		is_import ? GTK_FILE_CHOOSER_ACTION_OPEN : GTK_FILE_CHOOSER_ACTION_SAVE,
		is_import ? _("_Import") : _("Export"),
		_("_Cancel"));

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("OPML files"));
	gtk_file_filter_add_mime_type (filter, "text/x-opml+xml");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), filter);

	e_util_load_file_chooser_folder (GTK_FILE_CHOOSER (native));

	if (!is_import) {
		/* Translators: This is a default file name for exported RSS feeds.
		   Keep the extension (".opml") as is, translate only the "feeds" word, if needed. */
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (native), _("feeds.opml"));
	}

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		e_util_save_file_chooser_folder (GTK_FILE_CHOOSER (native));
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
	}

	g_object_unref (native);

	return file;
}

static void
e_rss_report_text (GtkWindow *parent,
		   const gchar *text)
{
	g_return_if_fail (text != NULL);

	e_notice (parent, GTK_MESSAGE_ERROR, "%s", text);
}

static void
e_rss_report_error (GtkWindow *parent,
		    const GError *local_error)
{
	g_return_if_fail (local_error != NULL);

	e_rss_report_text (parent, local_error->message);
}

#define EVO_RSS_NS_HREF PACKAGE_URL

static void
e_rss_export_to_file (GtkWindow *parent,
		      CamelRssStoreSummary *store_summary,
		      GFile *file)
{
	EXmlDocument *xml;
	GSList *feeds, *link;
	gchar *content;
	GError *local_error = NULL;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (store_summary));
	g_return_if_fail (G_IS_FILE (file));

	xml = e_xml_document_new (NULL, "opml");
	e_xml_document_add_attribute (xml, NULL, "version", "2.0");
	e_xml_document_add_namespaces (xml, "e", EVO_RSS_NS_HREF, NULL);

	e_xml_document_start_element (xml, NULL, "head");
	e_xml_document_start_text_element (xml, NULL, "title");
	if (strlen (VERSION_COMMENT) > 0)
		e_xml_document_write_string (xml, "Evolution RSS Feeds (" VERSION VERSION_SUBSTRING " " VERSION_COMMENT ")");
	else
		e_xml_document_write_string (xml, "Evolution RSS Feeds (" VERSION VERSION_SUBSTRING ")");
	e_xml_document_end_element (xml); /* title */
	e_xml_document_start_text_element (xml, NULL, "dateCreated");
	e_xml_document_write_time (xml, time (NULL));
	e_xml_document_end_element (xml); /* dateCreated */
	e_xml_document_end_element (xml); /* head */

	e_xml_document_start_element (xml, NULL, "body");

	feeds = camel_rss_store_summary_dup_feeds (store_summary);
	for (link = feeds; link; link = g_slist_next (link)) {
		const gchar *id = link->data;
		const gchar *href;
		const gchar *display_name;
		CamelRssContentType content_type;

		href = camel_rss_store_summary_get_href (store_summary, id);
		display_name = camel_rss_store_summary_get_display_name (store_summary, id);
		content_type = camel_rss_store_summary_get_content_type (store_summary, id);

		e_xml_document_start_element (xml, NULL, "outline");
		e_xml_document_add_attribute (xml, NULL, "type", "rss");
		e_xml_document_add_attribute (xml, NULL, "text", display_name);
		e_xml_document_add_attribute (xml, NULL, "xmlUrl", href);
		e_xml_document_add_attribute (xml, EVO_RSS_NS_HREF, "contentType", e_rss_preferences_content_type_to_string (content_type));
		e_xml_document_end_element (xml); /* outline */
	}

	e_xml_document_end_element (xml); /* body */

	content = e_xml_document_get_content (xml, NULL);

	if (!g_file_set_contents (g_file_peek_path (file), content, -1, &local_error)) {
		g_prefix_error_literal (&local_error, _("Failed to export RSS feeds: "));
		e_rss_report_error (parent, local_error);
		g_clear_error (&local_error);
	}

	g_slist_free_full (feeds, g_free);
	g_clear_object (&xml);
	g_free (content);
}

static void
e_rss_import_from_file (GtkWindow *parent,
			CamelService *service,
			CamelRssStoreSummary *store_summary,
			GFile *file)
{
	gchar *content = NULL;
	gsize length = 0;
	xmlDoc *doc;
	GError *local_error = NULL;

	g_return_if_fail (CAMEL_IS_STORE (service));
	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (store_summary));
	g_return_if_fail (G_IS_FILE (file));

	if (!g_file_get_contents (g_file_peek_path (file), &content, &length, &local_error)) {
		g_prefix_error_literal (&local_error, _("Failed to read file content: "));
		e_rss_report_error (parent, local_error);
		g_clear_error (&local_error);
		return;
	}

	doc = e_xml_parse_data (content, length);
	if (doc) {
		xmlNode *root;

		root = xmlDocGetRootElement (doc);
		if (root && e_xml_is_element_name (root, NULL, "opml")) {
			GHashTable *known_feeds;
			GSList *feeds, *link;
			xmlNode *node, *next;
			gsize n_found = 0, n_imported = 0;

			known_feeds = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
			feeds = camel_rss_store_summary_dup_feeds (store_summary);
			for (link = feeds; link; link = g_slist_next (link)) {
				const gchar *id = link->data;
				const gchar *href;

				href = camel_rss_store_summary_get_href (store_summary, id);
				if (href && *href)
					g_hash_table_add (known_feeds, g_strdup (href));
			}

			g_slist_free_full (feeds, g_free);

			/* Find the 'body' element */
			node = root->children;
			if (node) {
				node = e_xml_find_sibling (node, NULL, "body");
				if (node) {
					root = node;
					node = node->children;
				}
			}

			while (node != NULL && node != root) {
				if (e_xml_is_element_name (node, NULL, "outline")) {
					xmlChar *value;

					value = xmlGetNsProp (node, (const xmlChar *) "type", NULL);
					if (g_strcmp0 ((const gchar *) value, "rss") == 0) {
						xmlChar *text, *xml_url, *content_type_str;

						n_found++;

						text = xmlGetNsProp (node, (const xmlChar *) "text", NULL);
						xml_url = xmlGetNsProp (node, (const xmlChar *) "xmlUrl", NULL);
						content_type_str = xmlGetNsProp (node, (const xmlChar *) "contentType", (const xmlChar *) EVO_RSS_NS_HREF);

						if (text && *text && xml_url && *xml_url &&
						    !g_hash_table_contains (known_feeds, xml_url)) {
							CamelRssContentType content_type;

							content_type = e_rss_preferences_content_type_from_string ((const gchar *) content_type_str);

							g_hash_table_add (known_feeds, g_strdup ((const gchar *) xml_url));

							e_rss_preferences_create_feed (service, store_summary,
								(const gchar *) xml_url, (const gchar *) text,
								NULL, content_type, NULL, NULL, &local_error);

							if (local_error) {
								g_prefix_error_literal (&local_error, _("Failed to add feed: "));
								break;
							}

							n_imported++;
						}

						g_clear_pointer (&text, xmlFree);
						g_clear_pointer (&xml_url, xmlFree);
						g_clear_pointer (&content_type_str, xmlFree);
					}

					g_clear_pointer (&value, xmlFree);
				}

				/* traverse the XML structure */
				next = node->children;
				if (!next)
					next = node->next;
				if (!next) {
					next = node->parent;
					if (next == root)
						next = NULL;

					while (next) {
						xmlNode *sibl = next->next;
						if (sibl) {
							next = sibl;
							break;
						} else {
							next = next->parent;
							if (next == root)
								next = NULL;
						}
					}
				}
				node = next;
			}

			g_hash_table_destroy (known_feeds);

			if (local_error) {
				e_rss_report_error (parent, local_error);
				g_clear_error (&local_error);
			} else if (!n_found) {
				e_notice (parent, GTK_MESSAGE_ERROR, "%s", _("No RSS feeds found"));
			} else if (!n_imported) {
				e_notice (parent, GTK_MESSAGE_INFO, "%s", _("No new RSS feeds imported"));
			} else {
				e_notice (parent, GTK_MESSAGE_INFO, g_dngettext (GETTEXT_PACKAGE,
					"Imported %d feed",
					"Imported %d feeds", n_imported), (gint) n_imported);
			}
		} else {
			e_rss_report_text (parent, _("Failed to import data, the file does not contain valid OPML data."));
		}

		xmlFreeDoc (doc);
	} else {
		e_rss_report_text (parent, _("Failed to parse file content. Expected is an OPML file."));
	}

	g_free (content);
}

static void
e_rss_preferences_export_import (GtkWidget *button,
				 gboolean is_import)
{
	CamelService *service;
	CamelRssStoreSummary *store_summary = NULL;
	gpointer toplevel;
	GFile *file;

	service = e_rss_preferences_ref_store (e_shell_get_default ());
	if (!service) {
		g_warn_if_reached ();
		return;
	}

	g_object_get (service, "summary", &store_summary, NULL);

	if (!store_summary) {
		g_clear_object (&service);
		g_warn_if_reached ();
		return;
	}

	toplevel = gtk_widget_get_toplevel (button);
	if (!GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	file = e_rss_choose_file (toplevel, is_import);
	if (file) {
		if (is_import)
			e_rss_import_from_file (toplevel, service, store_summary, file);
		else
			e_rss_export_to_file (toplevel, store_summary, file);
	}

	g_clear_object (&store_summary);
	g_clear_object (&service);
	g_clear_object (&file);
}

static void
e_rss_preferences_export_clicked_cb (GtkWidget *button)
{
	e_rss_preferences_export_import (button, FALSE);
}

static void
e_rss_preferences_import_clicked_cb (GtkWidget *button)
{
	e_rss_preferences_export_import (button, TRUE);
}

static void
e_rss_preferences_map_cb (GtkTreeView *tree_view,
			  gpointer user_data)
{
	CamelRssStoreSummary *store_summary = user_data;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (tree_view);

	e_rss_preferences_fill_list_store (GTK_LIST_STORE (model), store_summary);
}

static void
e_rss_preferences_feed_changed_cb (CamelRssStoreSummary *store_summary,
				   const gchar *id,
				   gpointer user_data)
{
	GtkTreeView *tree_view = user_data;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkListStore *list_store;
	gboolean found;

	if (!gtk_widget_get_mapped (GTK_WIDGET (tree_view)))
		return;

	model = gtk_tree_view_get_model (tree_view);
	list_store = GTK_LIST_STORE (model);

	found = gtk_tree_model_get_iter_first (model, &iter);
	while (found) {
		gchar *stored_id = NULL;

		gtk_tree_model_get (model, &iter,
			COLUMN_STRING_ID, &stored_id,
			-1);

		found = g_strcmp0 (id, stored_id) == 0;

		g_free (stored_id);

		if (found)
			break;

		found = gtk_tree_model_iter_next (model, &iter);
	}

	if (found) {
		if (camel_rss_store_summary_contains (store_summary, id)) {
			const gchar *href, *display_name, *icon_filename;
			CamelRssContentType content_type;
			gchar *description;
			GdkPixbuf *pixbuf;

			href = camel_rss_store_summary_get_href (store_summary, id);
			display_name = camel_rss_store_summary_get_display_name (store_summary, id);
			content_type = camel_rss_store_summary_get_content_type (store_summary, id);
			description = e_rss_preferences_describe_feed (href, display_name);
			icon_filename = camel_rss_store_summary_get_icon_filename (store_summary, id);
			pixbuf = e_rss_preferences_create_icon_pixbuf (icon_filename);

			gtk_list_store_set (list_store, &iter,
				COLUMN_STRING_NAME, display_name,
				COLUMN_STRING_HREF, href,
				COLUMN_STRING_CONTENT_TYPE, e_rss_preferences_content_type_to_locale_string (content_type),
				COLUMN_STRING_DESCRIPTION, description,
				COLUMN_PIXBUF_ICON, pixbuf,
				-1);

			g_clear_object (&pixbuf);
			g_free (description);
		} else {
			gtk_list_store_remove (list_store, &iter);
		}
	} else if (camel_rss_store_summary_contains (store_summary, id)) {
		e_rss_preferences_add_feed_to_list_store (list_store, store_summary, id);
	}
}

static void
e_rss_preferences_row_activated_cb (GtkTreeView *tree_view,
				    GtkTreePath *path,
				    GtkTreeViewColumn *column,
				    gpointer user_data)
{
	GtkWidget *button = user_data;

	e_rss_preferences_edit_clicked_cb (button, tree_view);
}

static void
e_rss_preferences_row_deleted_cb (GtkTreeModel *model,
				  GtkTreePath *path,
				  gpointer user_data)
{
	GtkWidget *button = user_data;
	GtkTreeIter iter;

	gtk_widget_set_sensitive (button, gtk_tree_model_get_iter_first (model, &iter));
}

static void
e_rss_preferences_row_inserted_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   GtkTreeIter *iter,
				   gpointer user_data)
{
	GtkWidget *button = user_data;

	gtk_widget_set_sensitive (button, TRUE);
}

static GtkWidget *
e_rss_preferences_new (EPreferencesWindow *window)
{
	CamelService *service;
	CamelSettings *settings;
	CamelRssStoreSummary *store_summary = NULL;
	EShell *shell;
	ESource *source;
	PangoAttrList *bold;
	GtkGrid *grid;
	GtkWidget *widget, *hbox, *spin, *scrolled_window, *button_box;
	GtkListStore *list_store;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	gint row = 0;

	shell = e_preferences_window_get_shell (window);
	service = e_rss_preferences_ref_store (shell);
	if (!service)
		return NULL;

	g_object_get (service, "summary", &store_summary, NULL);

	if (!store_summary) {
		g_clear_object (&service);
		g_warn_if_reached ();
		return NULL;
	}

	source = e_source_registry_ref_source (e_shell_get_registry (shell), "rss");
	if (source) {
		/* Auto-save changes */
		g_signal_connect (source, "changed",
			G_CALLBACK (e_rss_preferences_source_changed_cb), NULL);
		g_clear_object (&source);
	} else {
		g_warn_if_reached ();
	}

	settings = camel_service_ref_settings (service);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (grid),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"border-width", 12,
		NULL);

	widget = gtk_label_new (_("General"));
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_check_button_new_with_mnemonic (_("_Download complete articles"));
	g_object_set (G_OBJECT (widget),
		"margin-start", 12,
		NULL);

	e_binding_bind_property (
		settings, "complete-articles",
		widget, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	widget = gtk_check_button_new_with_mnemonic (_("Download _feed enclosures"));
	g_object_set (G_OBJECT (widget),
		"margin-start", 12,
		NULL);

	e_binding_bind_property (
		settings, "feed-enclosures",
		widget, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (hbox),
		"margin-start", 12,
		NULL);

	/* Translators: This is part of "Do not download enclosures larger than [ nnn ] KB" */
	widget = gtk_check_button_new_with_mnemonic (_("Do not download e_nclosures larger than"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	spin = gtk_spin_button_new_with_range (1, 999999, 100);
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 0);

	e_binding_bind_property (
		widget, "active",
		spin, "sensitive",
		G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "limit-feed-enclosure-size",
		widget, "active",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		settings, "max-feed-enclosure-size",
		spin, "value",
		G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);


	/* Translators: This is part of "Do not download enclosures larger than [ nnn ] KB" */
	widget = gtk_label_new (_("KB"));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

	gtk_grid_attach (grid, hbox, 0, row, 2, 1);
	row++;

	widget = gtk_label_new (_("Feeds"));
	g_object_set (G_OBJECT (widget),
		"halign", GTK_ALIGN_START,
		"hexpand", FALSE,
		"attributes", bold,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 2, 1);
	row++;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (scrolled_window),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"margin-start", 12,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		NULL);

	list_store = gtk_list_store_new (N_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_STRING_ID */
		G_TYPE_STRING,		/* COLUMN_STRING_NAME */
		G_TYPE_STRING,		/* COLUMN_STRING_HREF */
		G_TYPE_STRING,		/* COLUMN_STRING_CONTENT_TYPE */
		G_TYPE_STRING,		/* COLUMN_STRING_DESCRIPTION */
		GDK_TYPE_PIXBUF);	/* COLUMN_PIXBUF_ICON */

	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);
	g_object_unref (list_store);
	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	tree_view = GTK_TREE_VIEW (widget);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_column_set_expand (column, TRUE);

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell_renderer,
		"width", 48,
		"height", 48,
		NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"pixbuf", COLUMN_PIXBUF_ICON,
		NULL);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"markup", COLUMN_STRING_DESCRIPTION,
		NULL);

	gtk_tree_view_append_column (tree_view, column);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Content"));
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, 120);
	gtk_tree_view_column_set_expand (column, FALSE);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_set_attributes (column, cell_renderer,
		"text", COLUMN_STRING_CONTENT_TYPE,
		NULL);

	gtk_tree_view_append_column (tree_view, column);

	g_signal_connect_object (tree_view, "map",
		G_CALLBACK (e_rss_preferences_map_cb), store_summary, 0);

	g_signal_connect_object (store_summary, "feed-changed",
		G_CALLBACK (e_rss_preferences_feed_changed_cb), tree_view, 0);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	button_box = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (G_OBJECT (button_box),
		"layout-style", GTK_BUTTONBOX_START,
		"margin-start", 6,
		"spacing", 4,
		NULL);

	widget = e_dialog_button_new_with_icon ("list-add", _("_Add"));
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_rss_preferences_add_clicked_cb), tree_view, 0);

	widget = e_dialog_button_new_with_icon (NULL, _("_Edit"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_rss_preferences_edit_clicked_cb), tree_view, 0);

	g_signal_connect_object (selection, "changed",
		G_CALLBACK (e_rss_pereferences_selection_changed_cb), widget, 0);

	g_signal_connect_object (tree_view, "row-activated",
		G_CALLBACK (e_rss_preferences_row_activated_cb), widget, 0);

	widget = e_dialog_button_new_with_icon ("edit-delete", _("_Remove"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (e_rss_preferences_remove_clicked_cb), tree_view, 0);

	g_signal_connect_object (selection, "changed",
		G_CALLBACK (e_rss_pereferences_selection_changed_cb), widget, 0);

	widget = e_dialog_button_new_with_icon (NULL, _("E_xport"));
	gtk_widget_set_sensitive (widget, FALSE);
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect_object (list_store, "row-deleted",
		G_CALLBACK (e_rss_preferences_row_deleted_cb), widget, 0);

	g_signal_connect_object (list_store, "row-inserted",
		G_CALLBACK (e_rss_preferences_row_inserted_cb), widget, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (e_rss_preferences_export_clicked_cb), NULL);

	widget = e_dialog_button_new_with_icon (NULL, _("_Import"));
	gtk_container_add (GTK_CONTAINER (button_box), widget);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (e_rss_preferences_import_clicked_cb), NULL);

	gtk_grid_attach (grid, scrolled_window, 0, row, 1, 1);
	gtk_grid_attach (grid, button_box, 1, row, 1, 1);
	row++;

	pango_attr_list_unref (bold);

	widget = GTK_WIDGET (grid);
	gtk_widget_show_all (widget);

	g_clear_object (&store_summary);
	g_clear_object (&service);
	g_clear_object (&settings);

	return widget;
}

void
e_rss_preferences_init (EShell *shell)
{
	GtkWidget *preferences_window;
	CamelService *service;

	g_return_if_fail (E_IS_SHELL (shell));

	service = e_rss_preferences_ref_store (shell);
	if (!service)
		return;

	g_clear_object (&service);

	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"e-rss-page",
		"rss",
		_("News and Blogs"),
		NULL,
		e_rss_preferences_new,
		800);
}
