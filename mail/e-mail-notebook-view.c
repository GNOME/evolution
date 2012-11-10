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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "mail/e-mail-reader.h"
#include "mail/message-list.h"
#include "mail/em-folder-tree.h"
#include "e-mail-notebook-view.h"
#include "e-mail-folder-pane.h"
#include "libemail-engine/e-mail-folder-utils.h"
#include "e-mail-message-pane.h"

#include <shell/e-shell-window-actions.h>

#define E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_NOTEBOOK_VIEW, EMailNotebookViewPrivate))

struct _EMailNotebookViewPrivate {
	GtkNotebook *book;
	EMailView *current_view;
	GHashTable *views;
	gboolean inited;
};

enum {
	PROP_0,
	PROP_FORWARD_STYLE,
	PROP_GROUP_BY_THREADS,
	PROP_REPLY_STYLE
};

#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")

/* Forward Declarations */
static void e_mail_notebook_view_reader_init (EMailReaderInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EMailNotebookView, e_mail_notebook_view, E_TYPE_MAIL_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_READER, e_mail_notebook_view_reader_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static gint
emnv_get_page_num (EMailNotebookView *view,
                   GtkWidget *widget)
{
	EMailNotebookViewPrivate *priv = view->priv;
	gint i, n;

	n = gtk_notebook_get_n_pages (priv->book);

	for (i = 0; i < n; i++) {
		GtkWidget *curr = gtk_notebook_get_nth_page (priv->book, i);
		if (curr == widget)
			return i;
	}

	return 0;
}

static void
mnv_page_changed (GtkNotebook *book,
                  GtkWidget *page,
                  guint page_num,
                  EMailView *view)
{
	EMailNotebookViewPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	EMailView *mview;
	CamelFolder *folder;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	shell_view = e_mail_view_get_shell_view (view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	page = gtk_notebook_get_nth_page (book, page_num);
	folder = e_mail_reader_get_folder (E_MAIL_READER (page));
	mview = E_MAIL_VIEW (page);

	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

	if (folder != NULL && E_IS_MAIL_FOLDER_PANE (mview)) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_from_folder (folder);
		em_folder_tree_set_selected (folder_tree, folder_uri, FALSE);
		g_free (folder_uri);
	}

	if (mview != priv->current_view) {
		e_mail_view_set_previous_view (mview, priv->current_view);
		priv->current_view = mview;
	}

	/* For EMailReader related changes to EShellView */
	g_signal_emit_by_name (view, "changed");
	g_signal_emit_by_name (view, "folder-loaded");

	/* For EMailShellContent related changes */
	g_signal_emit_by_name (view, "view-changed");

	g_object_unref (folder_tree);
}

static void
tab_remove_gtk_cb (GtkWidget *button,
                   EMailNotebookView *view)
{
	EMailView *page = g_object_get_data ((GObject *) button, "page");
	EMailView *prev;
	gint num;

	if (gtk_notebook_get_n_pages (view->priv->book) == 1)
		return;

	if (E_IS_MAIL_FOLDER_PANE (page)) {
		CamelFolder *folder;
		gchar *folder_uri;

		folder = e_mail_reader_get_folder (E_MAIL_READER (page));
		folder_uri = e_mail_folder_uri_from_folder (folder);
		g_hash_table_remove (view->priv->views, folder_uri);
		g_free (folder_uri);
	}

	prev = e_mail_view_get_previous_view (page);
	if (prev) {
		num = emnv_get_page_num (view, (GtkWidget *) prev);
		gtk_notebook_set_current_page (view->priv->book, num);
	}
	gtk_notebook_remove_page (
		view->priv->book,
		gtk_notebook_page_num (view->priv->book, (GtkWidget *) page));

}

static void
adjust_label_size_request (GtkWidget *view,
                           GtkAllocation *allocation,
                           GtkWidget *label)
{
	GtkRequisition requisition;
	gint max_width = allocation->width / 2;

	/* We make sure the label is not over-ellipisized, but doesn't
	 * get too big to cause the tab to not fit either. */
	gtk_widget_get_preferred_size (label, NULL, &requisition);
	if (requisition.width < max_width)
		gtk_widget_set_size_request (label, requisition.width, -1);
	else
		gtk_widget_set_size_request (label, max_width, -1);
}

static void
disconnect_label_adjusting (EMailNotebookView *view,
                            GtkWidget *label)
{
	g_signal_handlers_disconnect_by_func (
		view,
		adjust_label_size_request,
		label);
}

static GtkWidget *
create_tab_label (EMailNotebookView *view,
                  EMailView *page,
                  const gchar *str)
{
	GtkWidget *container, *widget;
	GtkAllocation allocation;

	widget = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (widget);
	container = widget;

	widget = gtk_label_new (str);
	gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, FALSE, 0);

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);
	adjust_label_size_request (GTK_WIDGET (view), &allocation, widget);

	g_signal_connect (
		view, "size-allocate",
		G_CALLBACK (adjust_label_size_request), widget);

	g_object_weak_ref (
		G_OBJECT (widget),
		(GWeakNotify) disconnect_label_adjusting,
		view);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		"gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show_all (widget);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	g_object_set_data ((GObject *) widget, "page", page);
	g_object_set_data ((GObject *) page, "close-button", widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (tab_remove_gtk_cb), view);

	return container;
}

static void
mail_notebook_view_constructed (GObject *object)
{
	EMailNotebookViewPrivate *priv;
	EShellView *shell_view;
	GtkWidget *container;
	GtkWidget *widget;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (object);

	container = GTK_WIDGET (object);

	widget = gtk_notebook_new ();
	priv->book = (GtkNotebook *) widget;
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);

	gtk_notebook_set_scrollable ((GtkNotebook *) widget, TRUE);

	gtk_notebook_set_show_border ((GtkNotebook *) widget, FALSE);

	shell_view = e_mail_view_get_shell_view (E_MAIL_VIEW (object));
	priv->current_view = e_mail_folder_pane_new (shell_view);
	e_mail_view_set_preview_visible (priv->current_view, FALSE);
	gtk_widget_show (GTK_WIDGET (priv->current_view));

	gtk_notebook_append_page (
		priv->book, GTK_WIDGET (priv->current_view),
		create_tab_label (E_MAIL_NOTEBOOK_VIEW (object),
		priv->current_view, _("Please select a folder")));

	g_signal_connect (
		priv->book, "switch-page",
		G_CALLBACK (mnv_page_changed), object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_notebook_view_parent_class)->constructed (object);
}

static void
mail_notebook_view_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (object);
	current_view = notebook_view->priv->current_view;

	switch (property_id) {
		case PROP_FORWARD_STYLE:
			e_mail_reader_set_forward_style (
				E_MAIL_READER (current_view),
				g_value_get_enum (value));
			return;

		case PROP_GROUP_BY_THREADS:
			e_mail_reader_set_group_by_threads (
				E_MAIL_READER (current_view),
				g_value_get_boolean (value));
			return;

		case PROP_REPLY_STYLE:
			e_mail_reader_set_reply_style (
				E_MAIL_READER (current_view),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_notebook_view_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (object);
	current_view = notebook_view->priv->current_view;

	switch (property_id) {
		case PROP_FORWARD_STYLE:
			g_value_set_enum (
				value,
				e_mail_reader_get_forward_style (
				E_MAIL_READER (current_view)));
			return;

		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_group_by_threads (
				E_MAIL_READER (current_view)));
			return;

		case PROP_REPLY_STYLE:
			g_value_set_enum (
				value,
				e_mail_reader_get_reply_style (
				E_MAIL_READER (current_view)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_notebook_view_set_search_strings (EMailView *view,
                                       GSList *search_strings)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	e_mail_view_set_search_strings (current_view, search_strings);
}

static GalViewInstance *
mail_notebook_view_get_view_instance (EMailView *view)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return NULL;

	return e_mail_view_get_view_instance (current_view);
}

static void
mail_notebook_view_update_view_instance (EMailView *view)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return;

	e_mail_view_update_view_instance (current_view);
}

static void
mail_notebook_view_set_orientation (EMailView *view,
                                    GtkOrientation orientation)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return;

	e_mail_view_set_orientation (current_view, orientation);
}

static GtkOrientation
mail_notebook_view_get_orientation (EMailView *view)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return GTK_ORIENTATION_VERTICAL;

	return e_mail_view_get_orientation (current_view);
}

static gboolean
mail_notebook_view_get_show_deleted (EMailView *view)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return FALSE;

	return e_mail_view_get_show_deleted (current_view);
}

static void
mail_notebook_view_set_show_deleted (EMailView *view,
                                     gboolean show_deleted)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return;

	e_mail_view_set_show_deleted (current_view, show_deleted);
}

static gboolean
mail_notebook_view_get_preview_visible (EMailView *view)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return FALSE;

	return e_mail_view_get_preview_visible (current_view);
}

static void
mail_notebook_view_set_preview_visible (EMailView *view,
                                        gboolean preview_visible)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (view);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return;

	e_mail_view_set_preview_visible (current_view, preview_visible);
}

static GtkActionGroup *
mail_notebook_view_get_action_group (EMailReader *reader,
                                     EMailReaderActionGroup group)
{
	EMailView *view;
	EShellView *shell_view;
	EShellWindow *shell_window;
	const gchar *group_name;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	switch (group) {
		case E_MAIL_READER_ACTION_GROUP_STANDARD:
			group_name = "mail";
			break;
		case E_MAIL_READER_ACTION_GROUP_SEARCH_FOLDERS:
			group_name = "search-folders";
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return e_shell_window_get_action_group (shell_window, group_name);
}

static EAlertSink *
mail_notebook_view_get_alert_sink (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;
	EShellContent *shell_content;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	return E_ALERT_SINK (shell_content);
}

static EMailBackend *
mail_notebook_view_get_backend (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	return E_MAIL_BACKEND (shell_backend);
}

static EMailDisplay *
mail_notebook_view_get_mail_display (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;

	if (priv->current_view == NULL)
		return NULL;

	return e_mail_reader_get_mail_display (E_MAIL_READER (priv->current_view));
}

static gboolean
mail_notebook_view_get_hide_deleted (EMailReader *reader)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (reader);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return FALSE;

	reader = E_MAIL_READER (current_view);

	return e_mail_reader_get_hide_deleted (reader);
}

static GtkWidget *
mail_notebook_view_get_message_list (EMailReader *reader)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (reader);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return NULL;

	reader = E_MAIL_READER (current_view);

	return e_mail_reader_get_message_list (reader);
}

static GtkMenu *
mail_notebook_view_get_popup_menu (EMailReader *reader)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (reader);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return NULL;

	reader = E_MAIL_READER (current_view);

	return e_mail_reader_get_popup_menu (reader);
}

static EPreviewPane *
mail_notebook_view_get_preview_pane (EMailReader *reader)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (reader);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return NULL;

	reader = E_MAIL_READER (current_view);

	return e_mail_reader_get_preview_pane (reader);
}

static GtkWindow *
mail_notebook_view_get_window (EMailReader *reader)
{
	EMailView *view;
	EShellWindow *shell_window;
	EShellView *shell_view;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return GTK_WINDOW (shell_window);
}

static void
reconnect_changed_event (EMailReader *child,
                         EMailReader *parent)
{
	g_signal_emit_by_name (parent, "changed");
}

static void
reconnect_folder_loaded_event (EMailReader *child,
                               EMailReader *parent)
{
	g_signal_emit_by_name (parent, "folder-loaded");
}

static void
mail_netbook_view_open_mail (EMailView *view,
                             const gchar *uid,
                             EMailNotebookView *nview)
{
	EMailNotebookViewPrivate *priv;
	EShellView *shell_view;
	CamelFolder *folder;
	EMailView *pane;
	gint page;
	CamelMessageInfo *info;
	gint pos;

	priv = nview->priv;

	shell_view = e_mail_view_get_shell_view (E_MAIL_VIEW (nview));
	pos = emnv_get_page_num (nview, GTK_WIDGET (priv->current_view));
	pane = e_mail_message_pane_new (shell_view);
	e_mail_view_set_previous_view (pane, priv->current_view);
	E_MAIL_MESSAGE_PANE (pane)->parent_folder_view = priv->current_view;
	priv->current_view = pane;

	gtk_widget_show (GTK_WIDGET (pane));

	folder = e_mail_reader_get_folder (E_MAIL_READER (view));

	info = camel_folder_get_message_info (folder, uid);

	page = gtk_notebook_insert_page (
		priv->book, GTK_WIDGET (pane),
		create_tab_label (nview, priv->current_view,
		camel_message_info_subject (info)), pos + 1);

	gtk_notebook_set_current_page (priv->book, page);

	g_signal_connect (
		E_MAIL_READER (pane), "changed",
		G_CALLBACK (reconnect_changed_event), nview);
	g_signal_connect (
		E_MAIL_READER (pane), "folder-loaded",
		G_CALLBACK (reconnect_folder_loaded_event), nview);
	e_mail_reader_set_folder (
		E_MAIL_READER (pane), folder);
	e_mail_reader_set_group_by_threads (
		E_MAIL_READER (pane),
		e_mail_reader_get_group_by_threads (E_MAIL_READER (view)));

	e_mail_reader_enable_show_folder (E_MAIL_READER (pane));
	e_mail_reader_set_message (E_MAIL_READER (pane), uid);
	camel_message_info_free (info);
}

static void
mail_notebook_view_set_folder (EMailReader *reader,
                               CamelFolder *folder)
{
	EMailNotebookViewPrivate *priv;
	GtkWidget *new_view;
	gchar *folder_uri;

	if (folder == NULL)
		return;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	folder_uri = e_mail_folder_uri_from_folder (folder);
	new_view = g_hash_table_lookup (priv->views, folder_uri);
	g_free (folder_uri);

	if (new_view) {
		gint curr = emnv_get_page_num (E_MAIL_NOTEBOOK_VIEW (reader), new_view);

		priv->current_view = (EMailView *) new_view;
		gtk_notebook_set_current_page (priv->book, curr);
		return;
	}

	/* FIXME Redundant NULL check. */
	if (folder != NULL) {
		gint page;

		if (priv->inited) {
			EMailView *old_view = priv->current_view;
			EShellView *shell_view;

			shell_view = e_mail_view_get_shell_view (E_MAIL_VIEW (reader));
			priv->current_view = e_mail_folder_pane_new (shell_view);
			gtk_widget_show ((GtkWidget *) priv->current_view);
			e_mail_view_set_previous_view (priv->current_view, old_view);
			page = gtk_notebook_append_page (
				priv->book, (GtkWidget *) priv->current_view,
				create_tab_label (
					E_MAIL_NOTEBOOK_VIEW (reader),
					priv->current_view,
					camel_folder_get_full_name (folder)));
			gtk_notebook_set_current_page (priv->book, page);

		} else {
			priv->inited = TRUE;
			gtk_notebook_set_tab_label (
				priv->book,
				GTK_WIDGET (priv->current_view),
				create_tab_label (
					E_MAIL_NOTEBOOK_VIEW (reader),
					priv->current_view,
					camel_folder_get_full_name (folder)));
		}

		e_mail_reader_set_folder (E_MAIL_READER (priv->current_view), folder);

		folder_uri = e_mail_folder_uri_from_folder (folder);
		g_hash_table_insert (priv->views, folder_uri, priv->current_view);
		g_signal_connect (
			priv->current_view, "changed",
			G_CALLBACK (reconnect_changed_event), reader);
		g_signal_connect (
			priv->current_view, "folder-loaded",
			G_CALLBACK (reconnect_folder_loaded_event), reader);
		g_signal_connect (
			priv->current_view, "open-mail",
			G_CALLBACK (mail_netbook_view_open_mail), reader);
	}
}

static gboolean
mail_notebook_view_enable_show_folder (EMailReader *reader)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (reader);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return FALSE;

	reader = E_MAIL_READER (current_view);

	return e_mail_reader_get_enable_show_folder (reader);
}

static guint
mail_notebook_view_open_selected_mail (EMailReader *reader)
{
	EMailNotebookView *notebook_view;
	EMailView *current_view;

	notebook_view = E_MAIL_NOTEBOOK_VIEW (reader);
	current_view = notebook_view->priv->current_view;

	if (current_view == NULL)
		return 0;

	reader = E_MAIL_READER (current_view);

	return e_mail_reader_open_selected_mail (reader);
}

static void
e_mail_notebook_view_class_init (EMailNotebookViewClass *class)
{
	GObjectClass *object_class;
	EMailViewClass *mail_view_class;

	g_type_class_add_private (class, sizeof (EMailNotebookViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_notebook_view_constructed;
	object_class->set_property = mail_notebook_view_set_property;
	object_class->get_property = mail_notebook_view_get_property;

	mail_view_class = E_MAIL_VIEW_CLASS (class);
	mail_view_class->set_search_strings = mail_notebook_view_set_search_strings;
	mail_view_class->get_view_instance = mail_notebook_view_get_view_instance;
	mail_view_class->update_view_instance = mail_notebook_view_update_view_instance;
	mail_view_class->set_orientation = mail_notebook_view_set_orientation;
	mail_view_class->get_orientation = mail_notebook_view_get_orientation;
	mail_view_class->get_show_deleted = mail_notebook_view_get_show_deleted;
	mail_view_class->set_show_deleted = mail_notebook_view_set_show_deleted;
	mail_view_class->get_preview_visible = mail_notebook_view_get_preview_visible;
	mail_view_class->set_preview_visible = mail_notebook_view_set_preview_visible;

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_FORWARD_STYLE,
		"forward-style");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_GROUP_BY_THREADS,
		"group-by-threads");

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_REPLY_STYLE,
		"reply-style");
}

static void
e_mail_notebook_view_reader_init (EMailReaderInterface *interface)
{
	interface->get_action_group = mail_notebook_view_get_action_group;
	interface->get_alert_sink = mail_notebook_view_get_alert_sink;
	interface->get_backend = mail_notebook_view_get_backend;
	interface->get_mail_display = mail_notebook_view_get_mail_display;
	interface->get_hide_deleted = mail_notebook_view_get_hide_deleted;
	interface->get_message_list = mail_notebook_view_get_message_list;
	interface->get_popup_menu = mail_notebook_view_get_popup_menu;
	interface->get_preview_pane = mail_notebook_view_get_preview_pane;
	interface->get_window = mail_notebook_view_get_window;
	interface->set_folder = mail_notebook_view_set_folder;
	interface->open_selected_mail = mail_notebook_view_open_selected_mail;
	interface->enable_show_folder = mail_notebook_view_enable_show_folder;
}

static void
e_mail_notebook_view_init (EMailNotebookView *view)
{
	view->priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);
	view->priv->inited = FALSE;
	view->priv->views = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
emnv_show_folder (EMailNotebookView *view,
                  gpointer not_used)
{
	gint pos;
	EMailNotebookViewPrivate *priv = view->priv;

	pos = emnv_get_page_num (
		view, (GtkWidget *) E_MAIL_MESSAGE_PANE (
		priv->current_view)->parent_folder_view);

	gtk_notebook_set_current_page (priv->book, pos);

}

static void
emnv_show_prevtab (EMailNotebookView *view,
                   gpointer not_used)
{
	gint pos;
	EMailNotebookViewPrivate *priv = view->priv;

	pos = emnv_get_page_num (
		view, (GtkWidget *) E_MAIL_MESSAGE_PANE (
		priv->current_view)->parent_folder_view);

	pos = gtk_notebook_get_current_page (priv->book);
	if (pos > 0)
		gtk_notebook_set_current_page (priv->book, pos - 1);

}

static void
emnv_show_nexttab (EMailNotebookView *view,
                   gpointer not_used)
{
	gint pos;
	EMailNotebookViewPrivate *priv = view->priv;

	pos = gtk_notebook_get_current_page (priv->book);
	if (pos < (gtk_notebook_get_n_pages (priv->book) - 1))
		gtk_notebook_set_current_page (priv->book, pos + 1);

}

static void
emnv_close_tab (EMailNotebookView *view,
                gpointer not_used)
{
	EMailNotebookViewPrivate *priv = view->priv;

	tab_remove_gtk_cb (
		g_object_get_data (
			G_OBJECT (priv->current_view), "close-button"),
		view);
}

GtkWidget *
e_mail_notebook_view_new (EShellView *shell_view)
{
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	widget = g_object_new (
		E_TYPE_MAIL_NOTEBOOK_VIEW,
		"shell-view", shell_view, NULL);
	g_signal_connect (
		widget, "show-folder",
		G_CALLBACK (emnv_show_folder), widget);
	g_signal_connect (
		widget, "show-next-tab",
		G_CALLBACK (emnv_show_nexttab), widget);
	g_signal_connect (
		widget, "show-previous-tab",
		G_CALLBACK (emnv_show_prevtab), widget);
	g_signal_connect (
		widget, "close-tab",
		G_CALLBACK (emnv_close_tab), widget);

	return widget;
}
