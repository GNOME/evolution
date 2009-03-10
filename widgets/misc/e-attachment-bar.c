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
 *		Ettore Perazzoli <ettore@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *	    Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glade/glade.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libgnome/libgnome.h>

#ifdef HAVE_LIBGNOMEUI_GNOME_THUMBNAIL_H
#include <libgnomeui/gnome-thumbnail.h>
#endif

#include "e-attachment.h"
#include "e-attachment-bar.h"
#include "e-mime-part-utils.h"

#include <libedataserver/e-data-server-util.h>

#include <camel/camel-data-wrapper.h>
#include <camel/camel-iconv.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-bestenc.h>
#include <camel/camel-mime-part.h>

#include "e-util/e-binding.h"
#include "e-util/e-error.h"
#include "e-util/e-gui-utils.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-util.h"
#include "e-util/gconf-bridge.h"

#define ICON_WIDTH 64
#define ICON_SEPARATORS " /-_"
#define ICON_SPACING 2
#define ICON_ROW_SPACING ICON_SPACING
#define ICON_COL_SPACING ICON_SPACING
#define ICON_BORDER 2
#define ICON_TEXT_SPACING 2

#define E_ATTACHMENT_BAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_BAR, EAttachmentBarPrivate))

struct _EAttachmentBarPrivate {
	gboolean batch_unref;
	GPtrArray *attachments;
	gchar *current_folder;
	char *path;

	GtkUIManager *ui_manager;
	GtkActionGroup *standard_actions;
	GtkActionGroup *editable_actions;
	GtkActionGroup *open_actions;
	guint merge_id;

	gchar *background_filename;
	gchar *background_options;

	guint editable : 1;
};

enum {
	PROP_0,
	PROP_BACKGROUND_FILENAME,
	PROP_BACKGROUND_OPTIONS,
	PROP_CURRENT_FOLDER,
	PROP_EDITABLE,
	PROP_UI_MANAGER
};

enum {
	CHANGED,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static const gchar *ui =
"<ui>"
"  <popup name='attachment-popup'>"
"    <menuitem action='save-as'/>"
"    <menuitem action='set-background'/>"
"    <menuitem action='remove'/>"
"    <menuitem action='properties'/>"
"    <placeholder name='custom-actions'/>"
"    <separator/>"
"    <menuitem action='add'/>"
"    <separator/>"
"    <placeholder name='open-actions'/>"
"  </popup>"
"</ui>";

static void
action_add_cb (GtkAction *action,
               EAttachmentBar *attachment_bar)
{
	GtkWidget *dialog;
	GtkWidget *option;
	GSList *uris, *iter;
	const gchar *disposition;
	gboolean active;
	gpointer parent;
	gint response;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (attachment_bar));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	dialog = gtk_file_chooser_dialog_new (
		_("Insert Attachment"), parent,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		_("A_ttach"), GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (
		GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_file_chooser_set_select_multiple (
		GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_window_set_icon_name (
		GTK_WINDOW (dialog), "mail-attachment");

	option = gtk_check_button_new_with_mnemonic (
		_("_Suggest automatic display of attachment"));
	gtk_file_chooser_set_extra_widget (
		GTK_FILE_CHOOSER (dialog), option);
	gtk_widget_show (option);

	response = e_attachment_bar_file_chooser_dialog_run (
		attachment_bar, dialog);

	if (response != GTK_RESPONSE_OK)
		goto exit;

	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dialog));
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (option));
	disposition = active ? "inline" : "attachment";

	for (iter = uris; iter != NULL; iter = iter->next) {
		CamelURL *url;

		url = camel_url_new (iter->data, NULL);
		if (url == NULL)
			continue;

		/* XXX Do we really need two different attach functions? */
		if (g_ascii_strcasecmp (url->protocol, "file") == 0)
			e_attachment_bar_attach (
				attachment_bar, url->path, disposition);
		else
			e_attachment_bar_attach_remote_file (
				attachment_bar, iter->data, disposition);

		camel_url_free (url);
	}

	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);

exit:
	gtk_widget_destroy (dialog);
}

static void
action_properties_cb (GtkAction *action,
                      EAttachmentBar *attachment_bar)
{
	GnomeIconList *icon_list;
	GPtrArray *array;
	GList *selection;
	gpointer parent;

	array = attachment_bar->priv->attachments;

	icon_list = GNOME_ICON_LIST (attachment_bar);
	selection = gnome_icon_list_get_selection (icon_list);
	g_return_if_fail (selection != NULL);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (icon_list));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	while (selection != NULL) {
		gint index = GPOINTER_TO_INT (selection->data);
		EAttachment *attachment;

		selection = g_list_next (selection);

		if (index >= array->len)
			continue;

		attachment = array->pdata[index];
		e_attachment_edit (attachment, parent);
	}
}

static void
action_recent_cb (GtkAction *action,
                  EAttachmentBar *attachment_bar)
{
	GtkRecentChooser *chooser;
	GFile *file;
	gchar *uri;

	chooser = GTK_RECENT_CHOOSER (action);

	/* Wish: gtk_recent_chooser_get_current_file() */
	uri = gtk_recent_chooser_get_current_uri (chooser);
	file = g_file_new_for_uri (uri);
	g_free (uri);

	if (g_file_is_native (file))
		e_attachment_bar_attach (
			E_ATTACHMENT_BAR (attachment_bar),
			g_file_get_path (file), "attachment");
	else
		e_attachment_bar_attach_remote_file (
			E_ATTACHMENT_BAR (attachment_bar),
			g_file_get_uri (file), "attachment");

	g_object_unref (file);
}

static void
action_remove_cb (GtkAction *action,
                  EAttachmentBar *attachment_bar)
{
	GnomeIconList *icon_list;
	GPtrArray *array;
	GList *selection;
	GList *trash = NULL;

	array = attachment_bar->priv->attachments;

	icon_list = GNOME_ICON_LIST (attachment_bar);
	selection = gnome_icon_list_get_selection (icon_list);
	g_return_if_fail (selection != NULL);

	while (selection != NULL) {
		gint index = GPOINTER_TO_INT (selection->data);

		selection = g_list_next (selection);

		if (index >= array->len)
			continue;

		/* We can't unref the attachment here because that may
		 * change the selection and invalidate the list we are
		 * iterating over.  So move it to a trash list instead. */
		trash = g_list_prepend (trash, array->pdata[index]);
		array->pdata[index] = NULL;
	}

	/* Compress the attachment array. */
	while (g_ptr_array_remove (array, NULL));

	/* Take out the trash. */
	g_list_foreach (trash, (GFunc) g_object_unref, NULL);
	g_list_free (trash);

	e_attachment_bar_refresh (attachment_bar);

	g_signal_emit (attachment_bar, signals[CHANGED], 0);
}

static void
action_save_as_cb (GtkAction *action,
                   EAttachmentBar *attachment_bar)
{
}

static void
action_set_background_cb (GtkAction *action,
                          EAttachmentBar *attachment_bar)
{
	GnomeIconList *icon_list;
	CamelContentType *content_type;
	CamelMimePart *mime_part;
	EAttachment *attachment;
	GPtrArray *array;
	GList *selection;
	gchar *basename;
	gchar *filename;
	gchar *dirname;
	GFile *file;
	gint index;
	GError *error = NULL;

	icon_list = GNOME_ICON_LIST (attachment_bar);
	selection = gnome_icon_list_get_selection (icon_list);
	g_return_if_fail (selection != NULL);

	array = attachment_bar->priv->attachments;
	index = GPOINTER_TO_INT (selection->data);
	attachment = E_ATTACHMENT (array->pdata[index]);
	mime_part = e_attachment_get_mime_part (attachment);
	g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));

	content_type = camel_mime_part_get_content_type (mime_part);
	basename = g_strdup (camel_mime_part_get_filename (mime_part));

	if (basename == NULL || basename == '\0') {
		g_free (basename);
		basename = g_strdup_printf (
			_("untitled_image.%s"),
			content_type->subtype);
	}

	dirname = g_build_filename (
		g_get_home_dir (), ".gnome2", "wallpapers", NULL);

	index = 0;
	filename = g_build_filename (dirname, basename, NULL);

	while (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gchar *temp;
		gchar *ext;

		ext = strrchr (filename, '.');
		if (ext != NULL)
			*ext++ = '\0';

		if (ext == NULL)
			temp = g_strdup_printf (
				"%s (%d)", basename, index++);
		else
			temp = g_strdup_printf (
				"%s (%d).%s", basename, index++, ext);

		g_free (basename);
		g_free (filename);
		basename = temp;

		filename = g_build_filename (dirname, basename, NULL);
	}

	g_free (basename);
	g_free (dirname);

	file = g_file_new_for_path (filename);

	if (e_mime_part_utils_save_to_file (mime_part, file, &error)) {
		const gchar *background_filename;
		const gchar *background_options;

		background_filename =
			e_attachment_bar_get_background_filename (
			attachment_bar);
		background_options =
			e_attachment_bar_get_background_options (
			attachment_bar);

		if (g_strcmp0 (background_filename, filename) == 0)
			e_attachment_bar_set_background_filename (
				attachment_bar, NULL);

		e_attachment_bar_set_background_filename (
			attachment_bar, filename);

		if (g_strcmp0 (background_options, "none") == 0)
			e_attachment_bar_set_background_options (
				attachment_bar, "wallpaper");
	}

	g_object_unref (file);
	g_free (filename);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static GtkActionEntry standard_entries[] = {

	{ "save-as",
	  GTK_STOCK_SAVE_AS,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_save_as_cb) },

	{ "set-background",
	  NULL,
	  N_("Set as _Background"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_set_background_cb) }
};

static GtkActionEntry editable_entries[] = {

	{ "add",
	  GTK_STOCK_ADD,
	  N_("A_dd Attachment..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_add_cb) },

	{ "properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_properties_cb) },

	{ "remove",
	  GTK_STOCK_REMOVE,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_remove_cb) }
};

static void
attachment_bar_show_popup_menu (EAttachmentBar *attachment_bar,
                                GdkEventButton *event)
{
	GtkUIManager *ui_manager;
	GtkWidget *menu;

	ui_manager = e_attachment_bar_get_ui_manager (attachment_bar);
	menu = gtk_ui_manager_get_widget (ui_manager, "/attachment-popup");
	g_return_if_fail (GTK_IS_MENU (menu));

	e_attachment_bar_update_actions (attachment_bar);

	if (event != NULL)
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
	else
		gtk_menu_popup (
			GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

/* Attachment handling functions.  */

static void
attachment_destroy (EAttachmentBar *bar,
                    EAttachment *attachment)
{
	if (bar->priv->batch_unref)
		return;

	if (g_ptr_array_remove (bar->priv->attachments, attachment)) {
		e_attachment_bar_refresh (bar);
		g_signal_emit (bar, signals[CHANGED], 0);
	}
}

/* get_system_thumbnail:
 * If filled store_uri, then creating thumbnail for it, otherwise, if is_available_local,
 * and attachment is not an application, then save to temp and create a thumbnail for the body.
 * Otherwise returns NULL (or if something goes wrong/library not available).
 */
static GdkPixbuf *
get_system_thumbnail (EAttachment *attachment, CamelContentType *content_type)
{
	GdkPixbuf *pixbuf = NULL;
#ifdef HAVE_LIBGNOMEUI_GNOME_THUMBNAIL_H
	CamelMimePart *mime_part;
	struct stat file_stat;
	char *file_uri = NULL;
	gboolean is_tmp = FALSE;

	if (!attachment || !attachment->is_available_local)
		return NULL;

	mime_part = e_attachment_get_mime_part (attachment);

	if (attachment->store_uri && g_str_has_prefix (attachment->store_uri, "file://"))
		file_uri = attachment->store_uri;
	else if (mime_part != NULL) {
		/* save part to the temp directory */
		char *tmp_file;

		is_tmp = TRUE;

		tmp_file = e_mktemp ("tmp-XXXXXX");
		if (tmp_file) {
			CamelStream *stream;
			char *mfilename = NULL;
			const char * filename;

			filename = camel_mime_part_get_filename (mime_part);
			if (filename == NULL)
				filename = "unknown";
			else {
				char *utf8_mfilename;

				utf8_mfilename = g_strdup (filename);
				e_filename_make_safe (utf8_mfilename);
				mfilename = g_filename_from_utf8 ((const char *) utf8_mfilename, -1, NULL, NULL, NULL);
				g_free (utf8_mfilename);

				filename = (const char *) mfilename;
			}

			file_uri = g_strjoin (NULL, "file://", tmp_file, "-", filename, NULL);

			stream = camel_stream_fs_new_with_name (file_uri + 7, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (stream) {
				CamelDataWrapper *content;

				content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

				if (camel_data_wrapper_decode_to_stream (content, stream) == -1
				    || camel_stream_flush (stream) == -1) {
					g_free (file_uri);
					file_uri = NULL;
				}

				camel_object_unref (stream);
			} else {
				g_free (file_uri);
				file_uri = NULL;
			}

			g_free (mfilename);
			g_free (tmp_file);
		}
	}

	if (!file_uri || !g_str_has_prefix (file_uri, "file://")) {
		if (is_tmp)
			g_free (file_uri);

		return NULL;
	}

	if (stat (file_uri + 7, &file_stat) != -1 && S_ISREG (file_stat.st_mode)) {
		GnomeThumbnailFactory *th_factory;
		char *th_file;

		th_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
		th_file = gnome_thumbnail_factory_lookup (th_factory, file_uri, file_stat.st_mtime);

		if (th_file) {
			pixbuf = gdk_pixbuf_new_from_file (th_file, NULL);
			g_free (th_file);
		} else if (content_type) {
			char *mime = camel_content_type_simple (content_type);

			if (gnome_thumbnail_factory_can_thumbnail (th_factory, file_uri, mime, file_stat.st_mtime)) {
				pixbuf = gnome_thumbnail_factory_generate_thumbnail (th_factory, file_uri, mime);
				
				if (pixbuf && !is_tmp)
					gnome_thumbnail_factory_save_thumbnail (th_factory, pixbuf, file_uri, file_stat.st_mtime);
			}

			g_free (mime);
		}

		g_object_unref (th_factory);
	}

	if (is_tmp) {
		/* clear the temp */
		g_remove (file_uri + 7);
		g_free (file_uri);
	}
#endif

	return pixbuf;
}

static GdkPixbuf *
scale_pixbuf (GdkPixbuf *pixbuf)
{
	int ratio, width, height;

	if (!pixbuf)
		return NULL;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	if (width >= height) {
		if (width > 48) {
			ratio = width / 48;
			width = 48;
			height = height / ratio;
			if (height == 0)
				height = 1;
		}
	} else {
		if (height > 48) {
			ratio = height / 48;
			height = 48;
			width = width / ratio;
			if (width == 0)
				width = 1;
		}
	}

	return e_icon_factory_pixbuf_scale (pixbuf, width, height);
}

/* Icon list contents handling.  */

static void
calculate_height_width (EAttachmentBar *bar,
                        gint *new_width,
                        gint *new_height)
{
        int width, height, icon_width;
        PangoFontMetrics *metrics;
        PangoContext *context;

        context = gtk_widget_get_pango_context (GTK_WIDGET (bar));
        metrics = pango_context_get_metrics (
		context, GTK_WIDGET (bar)->style->font_desc,
		pango_context_get_language (context));
        width = PANGO_PIXELS (
		pango_font_metrics_get_approximate_char_width (metrics)) * 15;
	/* This should be *2, but the icon list creates too much space above ... */
	height = PANGO_PIXELS (
		pango_font_metrics_get_ascent (metrics) +
		pango_font_metrics_get_descent (metrics)) * 3;
	pango_font_metrics_unref (metrics);
	icon_width = ICON_WIDTH + ICON_SPACING + ICON_BORDER + ICON_TEXT_SPACING;

	if (new_width)
		*new_width = MAX (icon_width, width);

	if (new_height)
		*new_height = ICON_WIDTH + ICON_SPACING +
			ICON_BORDER + ICON_TEXT_SPACING + height;
}

void
e_attachment_bar_create_attachment_cache (EAttachment *attachment)
{
	CamelContentType *content_type;
	CamelMimePart *mime_part;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part == NULL)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);

	if (camel_content_type_is(content_type, "image", "*")) {
		CamelDataWrapper *wrapper;
		CamelStreamMem *mstream;
		GdkPixbufLoader *loader;
		gboolean error = TRUE;
		GdkPixbuf *pixbuf;

		wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
		mstream = (CamelStreamMem *) camel_stream_mem_new ();

		camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) mstream);

		/* Stream image into pixbuf loader */
		loader = gdk_pixbuf_loader_new ();
		error = !gdk_pixbuf_loader_write (loader, mstream->buffer->data, mstream->buffer->len, NULL);
		gdk_pixbuf_loader_close (loader, NULL);

		if (!error) {
			/* The loader owns the reference. */
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

			/* This returns a new GdkPixbuf. */
			pixbuf = scale_pixbuf (pixbuf);
			e_attachment_set_thumbnail (attachment, pixbuf);
			g_object_unref (pixbuf);
		} else {
			e_attachment_set_thumbnail (attachment, NULL);
			g_warning ("GdkPixbufLoader Error");
		}

		/* Destroy everything */
		g_object_unref (loader);
		camel_object_unref (mstream);
	}
}

static void
update_remote_file (EAttachment *attachment, EAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	GnomeIconTextItem *item;
	const gchar *filename;
	char *msg, *base;

	if (attachment->percentage == -1) {
		e_attachment_bar_refresh (bar);
		return;
	}

	filename = e_attachment_get_filename (attachment);
	base = g_path_get_basename (filename);
	msg = g_strdup_printf ("%s (%d%%)", base, attachment->percentage);
	g_free (base);

	icon_list = GNOME_ICON_LIST (bar);

	gnome_icon_list_freeze (icon_list);

	item = gnome_icon_list_get_icon_text_item (
		icon_list, attachment->index);
	if (!item->is_text_allocated)
		g_free (item->text);

	gnome_icon_text_item_configure (
		item, item->x, item->y, item->width,
		item->fontname, msg, item->is_editable, TRUE);

	gnome_icon_list_thaw (icon_list);
}

void
e_attachment_bar_set_width(EAttachmentBar *bar, int bar_width)
{
	int per_col, rows, height, width;

	calculate_height_width(bar, &width, &height);
	per_col = bar_width / width;
	per_col = (per_col ? per_col : 1);
	rows = (bar->priv->attachments->len + per_col - 1) / per_col;
	gtk_widget_set_size_request ((GtkWidget *)bar, bar_width, rows * height);
}

/**
 * e_attachment_bar_get_selected:
 * @bar: an #EAttachmentBar object
 *
 * Returns a newly allocated #GSList of ref'd #EAttachment objects
 * representing the selected items in the #EAttachmentBar Icon List.
 **/
GSList *
e_attachment_bar_get_selected (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GSList *attachments = NULL;
	EAttachment *attachment;
	GList *items;
	int id;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	priv = bar->priv;

	items = gnome_icon_list_get_selection ((GnomeIconList *) bar);

	while (items != NULL) {
		if ((id = GPOINTER_TO_INT (items->data)) < priv->attachments->len) {
			attachment = priv->attachments->pdata[id];
			attachments = g_slist_prepend (attachments, attachment);
			g_object_ref (attachment);
		}

		items = items->next;
	}

	attachments = g_slist_reverse (attachments);

	return attachments;
}

/* FIXME: Cleanup this, since there is a api to get selected attachments */
/**
 * e_attachment_bar_get_attachment:
 * @bar: an #EAttachmentBar object
 * @id: Index of the desired attachment or -1 to request all selected attachments
 *
 * Returns a newly allocated #GSList of ref'd #EAttachment objects
 * representing the requested item(s) in the #EAttachmentBar Icon
 * List.
 **/
GSList *
e_attachment_bar_get_attachment (EAttachmentBar *bar, int id)
{
	EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	GSList *attachments;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	priv = bar->priv;

	if (id == -1 || id > priv->attachments->len)
		return e_attachment_bar_get_selected (bar);

	attachment = priv->attachments->pdata[id];
	attachments = g_slist_prepend (NULL, attachment);
	g_object_ref (attachment);

	return attachments;
}


/**
 * e_attachment_bar_get_all_attachments:
 * @bar: an #EAttachmentBar object
 *
 * Returns a newly allocated #GSList of ref'd #EAttachment objects.
 **/
GSList *
e_attachment_bar_get_all_attachments (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GSList *attachments = NULL;
	EAttachment *attachment;
	int i;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	priv = bar->priv;

	for (i = priv->attachments->len - 1; i >= 0; i--) {
		attachment = priv->attachments->pdata[i];
		if (attachment->is_available_local) {
			attachments = g_slist_prepend (attachments, attachment);
			g_object_ref (attachment);
		}
	}

	return attachments;
}

/* Just the GSList has to be freed by the caller */
GSList *
e_attachment_bar_get_parts (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	GSList *parts = NULL;
	int i;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	priv = bar->priv;

	for (i = 0; i < priv->attachments->len; i++) {
		CamelMimePart *mime_part;

		attachment = priv->attachments->pdata[i];
		mime_part = e_attachment_get_mime_part (attachment);

		if (attachment->is_available_local)
			parts = g_slist_prepend (parts, mime_part);
	}

        return parts;
}

static char *
temp_save_part (EAttachment *attachment, gboolean readonly)
{
	CamelMimePart *mime_part;
	const char *filename;
	char *tmpdir, *path, *mfilename = NULL, *utf8_mfilename = NULL;
	CamelStream *stream;
	CamelDataWrapper *wrapper;

	if (!(tmpdir = e_mkdtemp ("evolution-tmp-XXXXXX")))
		return NULL;

	mime_part = e_attachment_get_mime_part (attachment);

	if (!(filename = camel_mime_part_get_filename (mime_part))) {
		/* This is the default filename used for temporary file creation */
		filename = _("Unknown");
	} else {
		utf8_mfilename = g_strdup (filename);
		e_filename_make_safe (utf8_mfilename);
		mfilename = g_filename_from_utf8 ((const char *) utf8_mfilename, -1, NULL, NULL, NULL);
		g_free (utf8_mfilename);
		filename = (const char *) mfilename;
	}

	path = g_build_filename (tmpdir, filename, NULL);
	g_free (tmpdir);
	g_free (mfilename);

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	if (readonly)
		stream = camel_stream_fs_new_with_name (path, O_RDWR|O_CREAT|O_TRUNC, 0444);
	else
		stream = camel_stream_fs_new_with_name (path, O_RDWR|O_CREAT|O_TRUNC, 0644);

	if (!stream) {
		/* TODO handle error conditions */
		g_message ("DEBUG: could not open the file to write\n");
		g_free (path);
		return NULL;
	}

	if (camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) stream) == -1) {
		g_free (path);
		camel_stream_close (stream);
		camel_object_unref (stream);
		g_message ("DEBUG: could not write to file\n");
		return NULL;
	}

	camel_stream_close(stream);
	camel_object_unref(stream);

	return path;
}

static void
attachment_bar_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKGROUND_FILENAME:
			e_attachment_bar_set_background_filename (
				E_ATTACHMENT_BAR (object),
				g_value_get_string (value));
			return;

		case PROP_BACKGROUND_OPTIONS:
			e_attachment_bar_set_background_options (
				E_ATTACHMENT_BAR (object),
				g_value_get_string (value));
			return;

		case PROP_CURRENT_FOLDER:
			e_attachment_bar_set_current_folder (
				E_ATTACHMENT_BAR (object),
				g_value_get_string (value));
			return;

		case PROP_EDITABLE:
			e_attachment_bar_set_editable (
				E_ATTACHMENT_BAR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_bar_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKGROUND_FILENAME:
			g_value_set_string (
				value,
				e_attachment_bar_get_background_filename (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_BACKGROUND_OPTIONS:
			g_value_set_string (
				value,
				e_attachment_bar_get_background_options (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_CURRENT_FOLDER:
			g_value_set_string (
				value,
				e_attachment_bar_get_current_folder (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_EDITABLE:
			g_value_set_boolean (
				value,
				e_attachment_bar_get_editable (
				E_ATTACHMENT_BAR (object)));
			return;

		case PROP_UI_MANAGER:
			g_value_set_object (
				value,
				e_attachment_bar_get_ui_manager (
				E_ATTACHMENT_BAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_bar_dispose (GObject *object)
{
	EAttachmentBarPrivate *priv;
	guint ii;

	priv = E_ATTACHMENT_BAR_GET_PRIVATE (object);

	priv->batch_unref = TRUE;

	for (ii = 0; ii < priv->attachments->len; ii++) {
		EAttachment *attachment;

		attachment = priv->attachments->pdata[ii];
		g_object_weak_unref (
			G_OBJECT (attachment), (GWeakNotify)
			attachment_destroy, object);
		g_object_unref (attachment);
	}
	g_ptr_array_set_size (priv->attachments, 0);

	if (priv->ui_manager != NULL) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->standard_actions != NULL) {
		g_object_unref (priv->standard_actions);
		priv->standard_actions = NULL;
	}

	if (priv->editable_actions != NULL) {
		g_object_unref (priv->editable_actions);
		priv->editable_actions = NULL;
	}

	if (priv->open_actions != NULL) {
		g_object_unref (priv->open_actions);
		priv->open_actions = NULL;
	}

	/* Chain up to parent's dipose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
attachment_bar_finalize (GObject *object)
{
	EAttachmentBarPrivate *priv;

	priv = E_ATTACHMENT_BAR_GET_PRIVATE (object);

	g_ptr_array_free (priv->attachments, TRUE);
	g_free (priv->current_folder);
	g_free (priv->path);

	g_free (priv->background_filename);
	g_free (priv->background_options);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
attachment_bar_constructed (GObject *object)
{
	EAttachmentBarPrivate *priv;
	GtkActionGroup *action_group;
	GConfBridge *bridge;
	const gchar *prop;
	const gchar *key;

	priv = E_ATTACHMENT_BAR_GET_PRIVATE (object);
	action_group = priv->editable_actions;
	bridge = gconf_bridge_get ();

	e_mutual_binding_new (
		G_OBJECT (object), "editable",
		G_OBJECT (action_group), "visible");

	prop = "background-filename";
	key = "/desktop/gnome/background/picture_filename";
	gconf_bridge_bind_property (bridge, key, object, prop);

	prop = "background-options";
	key = "/desktop/gnome/background/picture_options";
	gconf_bridge_bind_property (bridge, key, object, prop);
}

static gboolean
attachment_bar_event (GtkWidget *widget,
                      GdkEvent *event)
{
	EAttachment *attachment;
	gboolean ret = FALSE;
	gpointer parent;
	CamelURL *url;
	char *path;
	GSList *p;

	if (event->type != GDK_2BUTTON_PRESS)
		return FALSE;

	parent = gtk_widget_get_toplevel (widget);
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	p = e_attachment_bar_get_selected (E_ATTACHMENT_BAR (widget));
	/* check if has body already, remote files can take longer to fetch */
	if (p && p->next == NULL && e_attachment_get_mime_part (p->data) != NULL) {
		attachment = p->data;

		/* Check if the file is stored already */
		if (!attachment->store_uri) {
			path = temp_save_part (attachment, TRUE);
			url = camel_url_new ("file://", NULL);
			camel_url_set_path (url, path);
			attachment->store_uri = camel_url_to_string (url, 0);
			camel_url_free (url);
			g_free (path);
		}

		e_show_uri (parent, attachment->store_uri);

		ret = TRUE;
	}

	g_slist_foreach (p, (GFunc) g_object_unref, NULL);
	g_slist_free (p);

	return ret;
}

static gboolean
attachment_bar_button_press_event (GtkWidget *widget,
                                   GdkEventButton *event)
{
	GnomeIconList *icon_list;
	GList *selected, *tmp;
	int length, icon_number;
	gboolean take_selected = FALSE;

	GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, 0 },
	};

	icon_list = GNOME_ICON_LIST (widget);
	selected = gnome_icon_list_get_selection (icon_list);
	length = g_list_length (selected);

	icon_number = gnome_icon_list_get_icon_at (
		icon_list, event->x, event->y);
	if (icon_number < 0) {
		/* When nothing is selected, deselect all */
		gnome_icon_list_unselect_all (icon_list);
		length = 0;
		selected = NULL;
	}

	if (event->button == 1) {
		/* If something is selected, then allow drag or else help to select */
		if (length)
			gtk_drag_source_set (
				widget, GDK_BUTTON1_MASK, drag_types,
				G_N_ELEMENTS (drag_types), GDK_ACTION_COPY);
		else
			gtk_drag_source_unset (widget);
		goto exit;
	}

	/* If not r-click dont progress any more.*/
	if (event->button != 3)
		goto exit;

	/* When a r-click on something, if it is in the already selected list, consider a r-click of multiple things
	 * or deselect all and select only this for r-click
	 */
	if (icon_number >= 0) {
		for (tmp = selected; tmp; tmp = tmp->next) {
			if (GPOINTER_TO_INT (tmp->data) == icon_number)
				take_selected = TRUE;
		}

		if (!take_selected) {
			gnome_icon_list_unselect_all (icon_list);
			gnome_icon_list_select_icon (icon_list, icon_number);
		}
	}

	attachment_bar_show_popup_menu (E_ATTACHMENT_BAR (widget), event);

exit:
	/* Chain up to parent's button_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->
		button_press_event (widget, event);
}

static gboolean
attachment_bar_button_release_event (GtkWidget *widget,
                                     GdkEventButton *event)
{
	GnomeIconList *icon_list;
	GList *selected;

	GtkTargetEntry drag_types[] = {
		{ "text/uri-list", 0, 0 },
	};

	if (event->button != 1)
		goto exit;

	icon_list = GNOME_ICON_LIST (widget);
	selected = gnome_icon_list_get_selection (icon_list);

	if (selected != NULL)
		gtk_drag_source_set (
			widget, GDK_BUTTON1_MASK, drag_types,
			G_N_ELEMENTS (drag_types), GDK_ACTION_COPY);
	else
		gtk_drag_source_unset (widget);

exit:
	/* Chain up to parent's button_release_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->
		button_release_event (widget, event);
}

static gboolean
attachment_bar_key_press_event (GtkWidget *widget,
                                GdkEventKey *event)
{
	EAttachmentBar *attachment_bar;
	gboolean editable;

	attachment_bar = E_ATTACHMENT_BAR (widget);
	editable = e_attachment_bar_get_editable (attachment_bar);

	if (editable && event->keyval == GDK_Delete) {
		GtkAction *action;

		action = e_attachment_bar_get_action (attachment_bar, "remove");
		gtk_action_activate (action);
	}

	/* Chain up to parent's key_press_event() method. */
	return GTK_WIDGET_CLASS (parent_class)->
		key_press_event (widget, event);
}

static void
attachment_bar_drag_data_get (GtkWidget *widget,
                              GdkDragContext *drag,
                              GtkSelectionData *data,
                              guint info,
                              guint time)
{
	EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	char *path, **uris;
	int len, n, i = 0;
	CamelURL *url;
	GList *items;

	if (info)
		return;

	priv = E_ATTACHMENT_BAR_GET_PRIVATE (widget);
	items = gnome_icon_list_get_selection (GNOME_ICON_LIST (widget));
	len = g_list_length (items);

	uris = g_malloc0 (sizeof (char *) * (len + 1));

	for ( ; items != NULL; items = items->next) {
		if (!((n = GPOINTER_TO_INT (items->data)) < priv->attachments->len))
			continue;

		attachment = priv->attachments->pdata[n];

		if (!attachment->is_available_local)
			continue;

		if (attachment->store_uri) {
			uris[i++] = attachment->store_uri;
			continue;
		}

		/* If we are not able to save, ignore it */
		if (!(path = temp_save_part (attachment, FALSE)))
			continue;

		url = camel_url_new ("file://", NULL);
		camel_url_set_path (url, path);
		attachment->store_uri = camel_url_to_string (url, 0);
		camel_url_free (url);
		g_free (path);

		uris[i++] = attachment->store_uri;
	}

	uris[i] = NULL;

	gtk_selection_data_set_uris (data, uris);

	g_free (uris);
}

static gboolean
attachment_bar_popup_menu (GtkWidget *widget)
{
	attachment_bar_show_popup_menu (E_ATTACHMENT_BAR (widget), NULL);

	return TRUE;
}

static void
attachment_bar_update_actions (EAttachmentBar *attachment_bar)
{
	GnomeIconList *icon_list;
	CamelMimePart *mime_part;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *selection;
	guint n_selected;
	gboolean is_image;
	gpointer parent;
	guint merge_id;

	icon_list = GNOME_ICON_LIST (attachment_bar);
	selection = gnome_icon_list_get_selection (icon_list);
	n_selected = g_list_length (selection);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (attachment_bar));
	parent = GTK_WIDGET_TOPLEVEL (parent) ? parent : NULL;

	is_image = FALSE;
	mime_part = NULL;

	if (n_selected == 1) {
		GPtrArray *array;
		EAttachment *attachment;
		gint index;

		array = attachment_bar->priv->attachments;
		index = GPOINTER_TO_INT (selection->data);
		attachment = E_ATTACHMENT (array->pdata[index]);
		mime_part = e_attachment_get_mime_part (attachment);
		is_image = e_attachment_is_image (attachment);
	}

	action = e_attachment_bar_get_action (attachment_bar, "properties");
	gtk_action_set_visible (action, n_selected == 1);

	action = e_attachment_bar_get_action (attachment_bar, "remove");
	gtk_action_set_visible (action, n_selected > 0);

	action = e_attachment_bar_get_action (attachment_bar, "save-as");
	gtk_action_set_visible (action, n_selected > 0);

	action = e_attachment_bar_get_action (attachment_bar, "set-background");
	gtk_action_set_visible (action, is_image);

	/* Clear out the "open" action group. */
	merge_id = attachment_bar->priv->merge_id;
	action_group = attachment_bar->priv->open_actions;
	ui_manager = e_attachment_bar_get_ui_manager (attachment_bar);
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);

	if (mime_part == NULL)
		return;

	e_mime_part_utils_add_open_actions (
		mime_part, ui_manager, action_group,
		"/attachment-popup/open-actions", parent, merge_id);
}

static void
attachment_bar_class_init (EAttachmentBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentBarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_bar_set_property;
	object_class->get_property = attachment_bar_get_property;
	object_class->dispose = attachment_bar_dispose;
	object_class->finalize = attachment_bar_finalize;
	object_class->constructed = attachment_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->event = attachment_bar_event;
	widget_class->button_press_event = attachment_bar_button_press_event;
	widget_class->button_release_event = attachment_bar_button_release_event;
	widget_class->key_press_event = attachment_bar_key_press_event;
	widget_class->drag_data_get = attachment_bar_drag_data_get;
	widget_class->popup_menu = attachment_bar_popup_menu;

	class->update_actions = attachment_bar_update_actions;

	g_object_class_install_property (
		object_class,
		PROP_BACKGROUND_FILENAME,
		g_param_spec_string (
			"background-filename",
			"Background Filename",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_BACKGROUND_OPTIONS,
		g_param_spec_string (
			"background-options",
			"Background Options",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_FOLDER,
		g_param_spec_string (
			"current-folder",
			"Current Folder",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_UI_MANAGER,
		g_param_spec_object (
			"ui-manager",
			"UI Manager",
			NULL,
			GTK_TYPE_UI_MANAGER,
			G_PARAM_READABLE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentBarClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EAttachmentBarClass, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
attachment_bar_init (EAttachmentBar *bar)
{
	GnomeIconList *icon_list;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	gint icon_width, window_height;
	const gchar *domain = GETTEXT_PACKAGE;
	GError *error = NULL;

	bar->priv = E_ATTACHMENT_BAR_GET_PRIVATE (bar);
	bar->priv->attachments = g_ptr_array_new ();

	GTK_WIDGET_SET_FLAGS (bar, GTK_CAN_FOCUS);

	icon_list = GNOME_ICON_LIST (bar);

	calculate_height_width (bar, &icon_width, &window_height);
	gnome_icon_list_construct (icon_list, icon_width, NULL, 0);

	gtk_widget_set_size_request (
		GTK_WIDGET (bar), icon_width * 4, window_height);

	atk_object_set_name (
		gtk_widget_get_accessible (GTK_WIDGET (bar)),
		_("Attachment Bar"));

	gnome_icon_list_set_separators (icon_list, ICON_SEPARATORS);
	gnome_icon_list_set_row_spacing (icon_list, ICON_ROW_SPACING);
	gnome_icon_list_set_col_spacing (icon_list, ICON_COL_SPACING);
	gnome_icon_list_set_icon_border (icon_list, ICON_BORDER);
	gnome_icon_list_set_text_spacing (icon_list, ICON_TEXT_SPACING);
	gnome_icon_list_set_selection_mode (icon_list, GTK_SELECTION_MULTIPLE);

	ui_manager = gtk_ui_manager_new ();
	bar->priv->ui_manager = ui_manager;
	bar->priv->merge_id = gtk_ui_manager_new_merge_id (ui_manager);

	action_group = gtk_action_group_new ("standard");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, standard_entries,
		G_N_ELEMENTS (standard_entries), bar);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	bar->priv->standard_actions = action_group;

	action_group = gtk_action_group_new ("editable");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, editable_entries,
		G_N_ELEMENTS (editable_entries), bar);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	bar->priv->editable_actions = action_group;

	action_group = gtk_action_group_new ("open");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	bar->priv->open_actions = action_group;

	/* Because we are loading from a hard-coded string, there is
	 * no chance of I/O errors.  Failure here imples a malformed
	 * UI definition.  Full stop. */
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	if (error != NULL)
		g_error ("%s", error->message);
}

GType
e_attachment_bar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentBarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_bar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAttachmentBar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) attachment_bar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GNOME_TYPE_ICON_LIST, "EAttachmentBar", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_attachment_bar_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT_BAR, NULL);
}

static char *
get_default_charset (void)
{
	GConfClient *gconf;
	const char *locale;
	char *charset;

	gconf = gconf_client_get_default ();
	charset = gconf_client_get_string (gconf, "/apps/evolution/mail/composer/charset", NULL);

	if (!charset || charset[0] == '\0') {
		g_free (charset);
		charset = gconf_client_get_string (gconf, "/apps/evolution/mail/format/charset", NULL);
		if (charset && charset[0] == '\0') {
			g_free (charset);
			charset = NULL;
		}
	}

	g_object_unref (gconf);

	if (!charset && (locale = camel_iconv_locale_charset ()))
		charset = g_strdup (locale);

	return charset ? charset : g_strdup ("us-ascii");
}

static void
attach_to_multipart (CamelMultipart *multipart,
		     EAttachment *attachment,
		     const char *default_charset)
{
	CamelContentType *content_type;
	CamelDataWrapper *content;
	CamelMimePart *mime_part;

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part == NULL)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);
	content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	if (!CAMEL_IS_MULTIPART (content)) {
		if (camel_content_type_is (content_type, "text", "*")) {
			CamelTransferEncoding encoding;
			CamelStreamFilter *filter_stream;
			CamelMimeFilterBestenc *bestenc;
			CamelStream *stream;
			const char *charset;
			char *buf = NULL;
			char *type;

			charset = camel_content_type_param (content_type, "charset");

			stream = camel_stream_null_new ();
			filter_stream = camel_stream_filter_new_with_stream (stream);
			bestenc = camel_mime_filter_bestenc_new (CAMEL_BESTENC_GET_ENCODING);
			camel_stream_filter_add (filter_stream, CAMEL_MIME_FILTER (bestenc));
			camel_object_unref (stream);

			camel_data_wrapper_decode_to_stream (content, CAMEL_STREAM (filter_stream));
			camel_object_unref (filter_stream);

			encoding = camel_mime_filter_bestenc_get_best_encoding (bestenc, CAMEL_BESTENC_8BIT);
			camel_mime_part_set_encoding (mime_part, encoding);

			if (encoding == CAMEL_TRANSFER_ENCODING_7BIT) {
				/* the text fits within us-ascii so this is safe */
				/* FIXME: check that this isn't iso-2022-jp? */
				default_charset = "us-ascii";
			} else if (!charset) {
				if (!default_charset)
					default_charset = buf = get_default_charset ();

				/* FIXME: We should really check that this fits within the
                                   default_charset and if not find one that does and/or
				   allow the user to specify? */
			}

			if (!charset) {
				/* looks kinda nasty, but this is how ya have to do it */
				camel_content_type_set_param (content_type, "charset", default_charset);
				type = camel_content_type_format (content_type);
				camel_mime_part_set_content_type (mime_part, type);
				g_free (type);
				g_free (buf);
			}

			camel_object_unref (bestenc);
		} else if (!CAMEL_IS_MIME_MESSAGE (content)) {
			camel_mime_part_set_encoding (mime_part, CAMEL_TRANSFER_ENCODING_BASE64);
		}
	}

	camel_multipart_add_part (multipart, mime_part);
}

void
e_attachment_bar_to_multipart (EAttachmentBar *bar,
                               CamelMultipart *multipart,
                               const gchar *default_charset)
{
	EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	int i;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));

	priv = bar->priv;

	for (i = 0; i < priv->attachments->len; i++) {
		attachment = priv->attachments->pdata[i];
		if (attachment->is_available_local)
			attach_to_multipart (multipart, attachment, default_charset);
	}
}

guint
e_attachment_bar_get_num_attachments (EAttachmentBar *bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);

	return bar->priv->attachments->len;
}

void
e_attachment_bar_attach (EAttachmentBar *bar,
                         const gchar *filename,
                         const gchar *disposition)
{
	EAttachment *attachment;
	CamelException ex;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (filename != NULL);
	g_return_if_fail (disposition != NULL);

	camel_exception_init (&ex);

	attachment = e_attachment_new (filename, disposition, &ex);

	if (attachment != NULL)
		e_attachment_bar_add_attachment (bar, attachment);
	else {
		GtkWidget *toplevel;

		/* FIXME: Avoid using error from mailer */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (bar));
		e_error_run (
			GTK_WINDOW (toplevel), "mail-composer:no-attach",
			filename, camel_exception_get_description (&ex), NULL);
		camel_exception_clear (&ex);
	}
}

void
e_attachment_bar_add_attachment (EAttachmentBar *bar,
                                 EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_ptr_array_add (bar->priv->attachments, attachment);

	g_object_weak_ref (
		G_OBJECT (attachment), (GWeakNotify)
		attachment_destroy, bar);

	g_signal_connect_swapped (
		attachment, "changed",
		G_CALLBACK (e_attachment_bar_refresh), bar);

	e_attachment_bar_refresh (bar);

	g_signal_emit (bar, signals[CHANGED], 0);
}

void
e_attachment_bar_add_attachment_silent (EAttachmentBar *bar,
                                        EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_ptr_array_add (bar->priv->attachments, attachment);

	g_object_weak_ref (
		G_OBJECT (attachment), (GWeakNotify)
		attachment_destroy, bar);

	g_signal_connect_swapped (
		attachment, "changed",
		G_CALLBACK (e_attachment_bar_refresh), bar);

	g_signal_emit (bar, signals[CHANGED], 0);
}

void
e_attachment_bar_refresh (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	GnomeIconList *icon_list;
	int bar_width, bar_height;
	int i;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));

	priv = bar->priv;
	icon_list = GNOME_ICON_LIST (bar);

	gnome_icon_list_freeze (icon_list);

	gnome_icon_list_clear (icon_list);

	/* FIXME could be faster, but we don't care.  */
	for (i = 0; i < priv->attachments->len; i++) {
		EAttachment *attachment;
		CamelContentType *content_type;
		CamelMimePart *mime_part;
		char *size_string, *label;
		GdkPixbuf *pixbuf = NULL;
		const char *desc;

		attachment = priv->attachments->pdata[i];
		mime_part = e_attachment_get_mime_part (attachment);

		if (!attachment->is_available_local || mime_part == NULL) {
			if ((pixbuf = e_icon_factory_get_icon("mail-attachment", E_ICON_SIZE_DIALOG))) {
				attachment->index = gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, "");
				g_object_unref (pixbuf);
			}
			continue;
		}

		content_type = camel_mime_part_get_content_type (mime_part);
		/* Get the image out of the attachment
		   and create a thumbnail for it */
		pixbuf = e_attachment_get_thumbnail (attachment);
		if (pixbuf != NULL)
			g_object_ref (pixbuf);
		else if (camel_content_type_is(content_type, "image", "*")) {
			CamelDataWrapper *wrapper;
			CamelStreamMem *mstream;
			GdkPixbufLoader *loader;
			gboolean error = TRUE;

			wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
			mstream = (CamelStreamMem *) camel_stream_mem_new ();

			camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) mstream);

			/* Stream image into pixbuf loader */
			loader = gdk_pixbuf_loader_new ();
			error = !gdk_pixbuf_loader_write (loader, mstream->buffer->data, mstream->buffer->len, NULL);
			gdk_pixbuf_loader_close (loader, NULL);

			if (!error) {
				/* The loader owns the reference. */
				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

				/* This returns a new GdkPixbuf. */
				pixbuf = scale_pixbuf (pixbuf);
				e_attachment_set_thumbnail (attachment, pixbuf);
			} else {
				pixbuf = NULL;
				g_warning ("GdkPixbufLoader Error");
			}

			/* Destroy everything */
			g_object_unref (loader);
			camel_object_unref (mstream);
		} else if (!bar->expand && (pixbuf = get_system_thumbnail (attachment, content_type))) {
			/* This returns a new GdkPixbuf. */
			pixbuf = scale_pixbuf (pixbuf);
			e_attachment_set_thumbnail (attachment, pixbuf);
		}

		desc = camel_mime_part_get_description (mime_part);
		if (desc == NULL || *desc == '\0')
			desc = e_attachment_get_filename (attachment);
		if (desc == NULL || *desc == '\0')
			desc = camel_mime_part_get_filename (mime_part);

		if (!desc)
			desc = _("attachment");

		if (attachment->size && (size_string = g_format_size_for_display (attachment->size))) {
			label = g_strdup_printf ("%s (%s)", desc, size_string);
			g_free (size_string);
		} else
			label = g_strdup (desc);

		if (pixbuf == NULL) {
			char *mime_type;

			mime_type = camel_content_type_simple (content_type);
			pixbuf = e_icon_for_mime_type (mime_type, 48);
			if (pixbuf == NULL) {
				g_warning("cannot find icon for mime type %s (installation problem?)", mime_type);
				pixbuf = e_icon_factory_get_icon("mail-attachment", E_ICON_SIZE_DIALOG);
			}
			g_free (mime_type);

			/* remember this picture and use it later again */
			if (pixbuf)
				e_attachment_set_thumbnail (attachment, pixbuf);
		}

		if (pixbuf) {
			GdkPixbuf *pixbuf_orig = pixbuf;
			pixbuf = gdk_pixbuf_add_alpha (pixbuf_orig, TRUE, 255, 255, 255);

			/* gdk_pixbuf_add_alpha returns a newly allocated pixbuf,
			   free the original one.
			*/
			g_object_unref (pixbuf_orig);

			/* In case of a attachment bar, in a signed/encrypted part, display the status as a emblem*/
			if (attachment->sign) {
				/* Show the signature status at the right-bottom.*/
				GdkPixbuf *sign = NULL;
				int x, y;

				if (attachment->sign == CAMEL_CIPHER_VALIDITY_SIGN_BAD)
					sign = e_icon_factory_get_icon ("stock_signature-bad", E_ICON_SIZE_MENU);
				else if (attachment->sign == CAMEL_CIPHER_VALIDITY_SIGN_GOOD)
					sign = e_icon_factory_get_icon ("stock_signature-ok", E_ICON_SIZE_MENU);
				else
					sign = e_icon_factory_get_icon ("stock_signature", E_ICON_SIZE_MENU);

				x = gdk_pixbuf_get_width (pixbuf) - 17;
				y = gdk_pixbuf_get_height (pixbuf) - 17;

				gdk_pixbuf_copy_area (sign, 0, 0, 16, 16, pixbuf, x, y);
				g_object_unref (sign);
			}

			if (attachment->encrypt) {
				/* Show the encryption status at the top left.*/
				GdkPixbuf *encrypt = e_icon_factory_get_icon ("stock_lock-ok", E_ICON_SIZE_MENU);

				gdk_pixbuf_copy_area (encrypt, 0, 0, 16, 16, pixbuf, 1, 1);
				g_object_unref (encrypt);
			}

			gnome_icon_list_append_pixbuf (icon_list, pixbuf, NULL, label);
			g_object_unref (pixbuf);
		}

		g_free (label);
	}

	gnome_icon_list_thaw (icon_list);

	/* Resize */
	if (bar->expand) {
		gtk_widget_get_size_request ((GtkWidget *) bar, &bar_width, &bar_height);

		if (bar->priv->attachments->len) {
			int per_col, rows, height, width;

			calculate_height_width(bar, &width, &height);
			per_col = bar_width / width;
			per_col = (per_col ? per_col : 1);
			rows = (bar->priv->attachments->len + per_col -1) / per_col;
			gtk_widget_set_size_request ((GtkWidget *) bar, bar_width, rows * height);
		}
	}
}

int
e_attachment_bar_get_download_count (EAttachmentBar *bar)
{
	EAttachmentBarPrivate *priv;
	EAttachment *attachment;
	int i, n = 0;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), 0);

	priv = bar->priv;

	for (i = 0; i < priv->attachments->len; i++) {
		attachment = priv->attachments->pdata[i];
		if (!attachment->is_available_local)
			n++;
	}

	return n;
}

void
e_attachment_bar_attach_remote_file (EAttachmentBar *bar,
                                     const gchar *url,
                                     const gchar *disposition)
{
	EAttachment *attachment;
	CamelException ex;
	GtkWidget *parent;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));

	if (bar->priv->path == NULL)
		bar->priv->path = e_mkdtemp ("attach-XXXXXX");

	parent = gtk_widget_get_toplevel (GTK_WIDGET (bar));
	camel_exception_init (&ex);

	attachment = e_attachment_new_remote_file (
		GTK_WINDOW (parent), url, disposition, bar->priv->path, &ex);

	if (attachment != NULL) {
		e_attachment_bar_add_attachment (bar, attachment);
		g_signal_connect (
			attachment, "update",
			G_CALLBACK (update_remote_file), bar);
	} else {
		e_error_run (
			GTK_WINDOW (parent), "mail-composer:no-attach",
			url, camel_exception_get_description (&ex), NULL);
		camel_exception_clear (&ex);
	}
}

void
e_attachment_bar_attach_mime_part (EAttachmentBar *bar,
                                   CamelMimePart *part)
{
	EAttachment *attachment;

	/* XXX Is this function really worth keeping? */

	g_return_if_fail (E_IS_ATTACHMENT_BAR (bar));
	g_return_if_fail (CAMEL_IS_MIME_PART (part));

	attachment = e_attachment_new_from_mime_part (part);
	e_attachment_bar_add_attachment (bar, attachment);
}

GtkAction *
e_attachment_bar_recent_action_new (EAttachmentBar *bar,
                                    const gchar *action_name,
                                    const gchar *action_label)
{
	GtkAction *action;
	GtkRecentChooser *chooser;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (bar), NULL);

	action = gtk_recent_action_new (
		action_name, action_label, NULL, NULL);
	gtk_recent_action_set_show_numbers (GTK_RECENT_ACTION (action), TRUE);

	chooser = GTK_RECENT_CHOOSER (action);
	gtk_recent_chooser_set_show_icons (chooser, TRUE);
	gtk_recent_chooser_set_show_not_found (chooser, FALSE);
	gtk_recent_chooser_set_show_private (chooser, FALSE);
	gtk_recent_chooser_set_show_tips (chooser, TRUE);
	gtk_recent_chooser_set_sort_type (chooser, GTK_RECENT_SORT_MRU);

	g_signal_connect (
		action, "item-activated",
		G_CALLBACK (action_recent_cb), bar);

	return action;
}

gint
e_attachment_bar_file_chooser_dialog_run (EAttachmentBar *attachment_bar,
                                          GtkWidget *dialog)
{
	GtkFileChooser *file_chooser;
	gint response = GTK_RESPONSE_NONE;
	const gchar *current_folder;
	gboolean save_folder;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), response);
	g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), response);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	current_folder = e_attachment_bar_get_current_folder (attachment_bar);
	gtk_file_chooser_set_current_folder (file_chooser, current_folder);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	save_folder =
		(response == GTK_RESPONSE_ACCEPT) ||
		(response == GTK_RESPONSE_OK) ||
		(response == GTK_RESPONSE_YES) ||
		(response == GTK_RESPONSE_APPLY);

	if (save_folder) {
		gchar *folder;

		folder = gtk_file_chooser_get_current_folder (file_chooser);
		e_attachment_bar_set_current_folder (attachment_bar, folder);
		g_free (folder);
	}

	return response;
}

void
e_attachment_bar_update_actions (EAttachmentBar *attachment_bar)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar));

	g_signal_emit (attachment_bar, signals[UPDATE_ACTIONS], 0);
}

const gchar *
e_attachment_bar_get_background_filename (EAttachmentBar *attachment_bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), NULL);

	return attachment_bar->priv->background_filename;
}

void
e_attachment_bar_set_background_filename (EAttachmentBar *attachment_bar,
                                          const gchar *background_filename)
{
	EAttachmentBarPrivate *priv;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar));

	if (background_filename == NULL)
		background_filename = "";

	priv = attachment_bar->priv;
	g_free (priv->background_filename);
	priv->background_filename = g_strdup (background_filename);

	g_object_notify (G_OBJECT (attachment_bar), "background-filename");
}

const gchar *
e_attachment_bar_get_background_options (EAttachmentBar *attachment_bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), NULL);

	return attachment_bar->priv->background_options;
}

void
e_attachment_bar_set_background_options (EAttachmentBar *attachment_bar,
                                         const gchar *background_options)
{
	EAttachmentBarPrivate *priv;

	g_return_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar));

	if (background_options == NULL)
		background_options = "none";

	priv = attachment_bar->priv;
	g_free (priv->background_options);
	priv->background_options = g_strdup (background_options);

	g_object_notify (G_OBJECT (attachment_bar), "background-options");
}

const gchar *
e_attachment_bar_get_current_folder (EAttachmentBar *attachment_bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), NULL);

	return attachment_bar->priv->current_folder;
}

void
e_attachment_bar_set_current_folder (EAttachmentBar *attachment_bar,
                                     const gchar *current_folder)
{

	g_return_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar));

	if (current_folder == NULL)
		current_folder = g_get_home_dir ();

	g_free (attachment_bar->priv->current_folder);
	attachment_bar->priv->current_folder = g_strdup (current_folder);

	g_object_notify (G_OBJECT (attachment_bar), "current-folder");
}

gboolean
e_attachment_bar_get_editable (EAttachmentBar *attachment_bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), FALSE);

	return attachment_bar->priv->editable;
}

void
e_attachment_bar_set_editable (EAttachmentBar *attachment_bar,
                               gboolean editable)
{
	g_return_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar));

	attachment_bar->priv->editable = editable;

	g_object_notify (G_OBJECT (attachment_bar), "editable");
}

GtkUIManager *
e_attachment_bar_get_ui_manager (EAttachmentBar *attachment_bar)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), NULL);

	return attachment_bar->priv->ui_manager;
}

GtkAction *
e_attachment_bar_get_action (EAttachmentBar *attachment_bar,
                             const gchar *action_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_attachment_bar_get_ui_manager (attachment_bar);

	return e_lookup_action (ui_manager, action_name);
}

GtkActionGroup *
e_attachment_bar_get_action_group (EAttachmentBar *attachment_bar,
                                   const gchar *group_name)
{
	GtkUIManager *ui_manager;

	g_return_val_if_fail (E_IS_ATTACHMENT_BAR (attachment_bar), NULL);
	g_return_val_if_fail (group_name != NULL, NULL);

	ui_manager = e_attachment_bar_get_ui_manager (attachment_bar);

	return e_lookup_action_group (ui_manager, group_name);
}
