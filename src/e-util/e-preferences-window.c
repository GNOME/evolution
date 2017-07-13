/*
 * e-preferences-window.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-dialog-widgets.h"
#include "e-icon-factory.h"
#include "e-misc-utils.h"

#include "e-preferences-window.h"

#define SWITCH_PAGE_INTERVAL 250

#define E_PREFERENCES_WINDOW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PREFERENCES_WINDOW, EPreferencesWindowPrivate))

struct _EPreferencesWindowPrivate {
	gboolean   setup;
	gpointer   shell;

	GtkWidget *icon_view;
	GtkWidget *scroll;
	GtkWidget *notebook;
	GHashTable *index;

	GtkListStore *store;
	GtkTreeModelFilter *filter;
	const gchar *filter_view;
};

enum {
	COLUMN_ID,	/* G_TYPE_STRING */
	COLUMN_TEXT,	/* G_TYPE_STRING */
	COLUMN_HELP,	/* G_TYPE_STRING */
	COLUMN_PIXBUF,	/* GDK_TYPE_PIXBUF */
	COLUMN_PAGE,	/* G_TYPE_INT */
	COLUMN_SORT	/* G_TYPE_INT */
};

G_DEFINE_TYPE (
	EPreferencesWindow,
	e_preferences_window,
	GTK_TYPE_WINDOW)

static gboolean
preferences_window_filter_view (GtkTreeModel *model,
                                GtkTreeIter *iter,
                                EPreferencesWindow *window)
{
	gchar *str;
	gboolean visible = FALSE;

	if (!window->priv->filter_view)
		return TRUE;

	gtk_tree_model_get (model, iter, COLUMN_ID, &str, -1);
	if (strncmp (window->priv->filter_view, "mail", 4) == 0) {
		/* Show everything except calendar */
		if (str && (strncmp (str, "cal", 3) == 0))
			visible = FALSE;
		else
			visible = TRUE;
	} else if (strncmp (window->priv->filter_view, "cal", 3) == 0) {
		/* Show only calendar and nothing else */
		if (str && (strncmp (str, "cal", 3) != 0))
			visible = FALSE;
		else
			visible = TRUE;

	} else  /* In any other case, show everything */
		visible = TRUE;

	g_free (str);

	return visible;
}

static GdkPixbuf *
preferences_window_load_pixbuf (const gchar *icon_name)
{
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;
	GdkPixbuf *pixbuf;
	const gchar *filename;
	gint size;
	GError *error = NULL;

	icon_theme = gtk_icon_theme_get_default ();

	if (!gtk_icon_size_lookup (GTK_ICON_SIZE_DIALOG, &size, 0))
		return NULL;

	icon_info = gtk_icon_theme_lookup_icon (
		icon_theme, icon_name, size, 0);

	if (icon_info == NULL)
		return NULL;

	filename = gtk_icon_info_get_filename (icon_info);

	pixbuf = gdk_pixbuf_new_from_file (filename, &error);

	gtk_icon_info_free (icon_info);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	if (pixbuf && (gdk_pixbuf_get_width (pixbuf) != size || gdk_pixbuf_get_height (pixbuf) != size)) {
		GdkPixbuf *scaled;

		scaled = e_icon_factory_pixbuf_scale (pixbuf, size, size);
		g_object_unref (pixbuf);

		pixbuf = scaled;
	}

	return pixbuf;
}

static void
preferences_window_help_clicked_cb (EPreferencesWindow *window)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *list;
	gchar *help = NULL;

	g_return_if_fail (window != NULL);

	model = GTK_TREE_MODEL (window->priv->filter);
	list = gtk_icon_view_get_selected_items (
		GTK_ICON_VIEW (window->priv->icon_view));

	if (list != NULL) {
		gtk_tree_model_get_iter (model, &iter, list->data);
		gtk_tree_model_get (model, &iter, COLUMN_HELP, &help, -1);

	} else if (gtk_tree_model_get_iter_first (model, &iter)) {
		gint page_index, current_index;

		current_index = gtk_notebook_get_current_page (
			GTK_NOTEBOOK (window->priv->notebook));
		do {
			gtk_tree_model_get (
				model, &iter, COLUMN_PAGE, &page_index, -1);

			if (page_index == current_index) {
				gtk_tree_model_get (
					model, &iter, COLUMN_HELP, &help, -1);
				break;
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	e_display_help (GTK_WINDOW (window), help ? help : "index");

	g_free (help);
}

static void
preferences_window_selection_changed_cb (EPreferencesWindow *window)
{
	GtkIconView *icon_view;
	GtkNotebook *notebook;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *list;
	gint page;

	icon_view = GTK_ICON_VIEW (window->priv->icon_view);
	list = gtk_icon_view_get_selected_items (icon_view);
	if (list == NULL)
		return;

	model = GTK_TREE_MODEL (window->priv->filter);
	gtk_tree_model_get_iter (model, &iter, list->data);
	gtk_tree_model_get (model, &iter, COLUMN_PAGE, &page, -1);

	notebook = GTK_NOTEBOOK (window->priv->notebook);
	gtk_notebook_set_current_page (notebook, page);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);

	gtk_widget_grab_focus (GTK_WIDGET (icon_view));
}

static void
preferences_window_dispose (GObject *object)
{
	EPreferencesWindowPrivate *priv;

	priv = E_PREFERENCES_WINDOW_GET_PRIVATE (object);

	if (priv->icon_view != NULL) {
		g_signal_handlers_disconnect_by_func (priv->icon_view,
			G_CALLBACK (preferences_window_selection_changed_cb), object);

		g_object_unref (priv->icon_view);
		priv->icon_view = NULL;
	}

	if (priv->notebook != NULL) {
		g_object_unref (priv->notebook);
		priv->notebook = NULL;
	}

	if (priv->shell) {
		g_object_remove_weak_pointer (priv->shell, &priv->shell);
		priv->shell = NULL;
	}

	g_hash_table_remove_all (priv->index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_preferences_window_parent_class)->dispose (object);
}

static void
preferences_window_finalize (GObject *object)
{
	EPreferencesWindowPrivate *priv;

	priv = E_PREFERENCES_WINDOW_GET_PRIVATE (object);

	g_hash_table_destroy (priv->index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_preferences_window_parent_class)->finalize (object);
}

static void
preferences_window_show (GtkWidget *widget)
{
	EPreferencesWindowPrivate *priv;
	GtkIconView *icon_view;
	GtkTreePath *path;

	priv = E_PREFERENCES_WINDOW_GET_PRIVATE (widget);
	if (!priv->setup)
		g_warning ("Preferences window has not been setup correctly");

	icon_view = GTK_ICON_VIEW (priv->icon_view);

	path = gtk_tree_path_new_first ();
	gtk_icon_view_select_path (icon_view, path);
	gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.0, 0.0);
	gtk_tree_path_free (path);

	gtk_widget_grab_focus (priv->icon_view);

	/* Chain up to parent's show() method. */
	GTK_WIDGET_CLASS (e_preferences_window_parent_class)->show (widget);
}

static void
e_preferences_window_class_init (EPreferencesWindowClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (EPreferencesWindowPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = preferences_window_dispose;
	object_class->finalize = preferences_window_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->show = preferences_window_show;
}

static void
e_preferences_window_init (EPreferencesWindow *window)
{
	GtkListStore *store;
	GtkWidget *container;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *widget;
	GHashTable *index;
	const gchar *title;
	GtkAccelGroup *accel_group;

	index = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);

	window->priv = E_PREFERENCES_WINDOW_GET_PRIVATE (window);
	window->priv->index = index;
	window->priv->filter_view = NULL;

	store = gtk_list_store_new (
		6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
		GDK_TYPE_PIXBUF, G_TYPE_INT, G_TYPE_INT);
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (store), COLUMN_SORT, GTK_SORT_ASCENDING);
	window->priv->store = store;

	window->priv->filter = (GtkTreeModelFilter *)
		gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
	gtk_tree_model_filter_set_visible_func (
		window->priv->filter, (GtkTreeModelFilterVisibleFunc)
		preferences_window_filter_view, window, NULL);

	title = _("Evolution Preferences");
	gtk_window_set_title (GTK_WINDOW (window), title);
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	vbox = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	hbox = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, TRUE, 0);
	window->priv->scroll = widget;
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_icon_view_new_with_model (
		GTK_TREE_MODEL (window->priv->filter));
	gtk_icon_view_set_columns (GTK_ICON_VIEW (widget), 1);
	gtk_icon_view_set_text_column (GTK_ICON_VIEW (widget), COLUMN_TEXT);
	gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (widget), COLUMN_PIXBUF);
	g_signal_connect_swapped (
		widget, "selection-changed",
		G_CALLBACK (preferences_window_selection_changed_cb), window);
	gtk_container_add (GTK_CONTAINER (container), widget);
	window->priv->icon_view = g_object_ref (widget);
	gtk_widget_show (widget);
	g_object_unref (store);

	widget = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
	window->priv->notebook = g_object_ref (widget);
	gtk_widget_show (widget);

	widget = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_dialog_button_new_with_icon ("help-browser", _("_Help"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (preferences_window_help_clicked_cb), window);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_button_box_set_child_secondary (
		GTK_BUTTON_BOX (container), widget, TRUE);
	gtk_widget_show (widget);

	widget = e_dialog_button_new_with_icon ("window-close", _("_Close"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_hide), window);
	gtk_widget_set_can_default (widget, TRUE);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	accel_group = gtk_accel_group_new ();
	gtk_widget_add_accelerator (
		widget, "activate", accel_group,
		GDK_KEY_Escape, (GdkModifierType) 0,
		GTK_ACCEL_VISIBLE);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	gtk_widget_grab_default (widget);
	gtk_widget_show (widget);
}

GtkWidget *
e_preferences_window_new (gpointer shell)
{
	EPreferencesWindow *window;

	window = g_object_new (E_TYPE_PREFERENCES_WINDOW, NULL);

	/* ideally should be an object property */
	window->priv->shell = shell;
	if (shell)
		g_object_add_weak_pointer (shell, &window->priv->shell);

	return GTK_WIDGET (window);
}

gpointer
e_preferences_window_get_shell (EPreferencesWindow *window)
{
	g_return_val_if_fail (E_IS_PREFERENCES_WINDOW (window), NULL);

	return window->priv->shell;
}

void
e_preferences_window_add_page (EPreferencesWindow *window,
                               const gchar *page_name,
                               const gchar *icon_name,
                               const gchar *caption,
                               const gchar *help_target,
                               EPreferencesWindowCreatePageFn create_fn,
                               gint sort_order)
{
	GtkTreeRowReference *reference;
	GtkIconView *icon_view;
	GtkNotebook *notebook;
	GtkTreeModel *model;
	GtkTreePath *path;
	GHashTable *index;
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;
	GtkWidget *align;
	gint page;

	g_return_if_fail (E_IS_PREFERENCES_WINDOW (window));
	g_return_if_fail (create_fn != NULL);
	g_return_if_fail (page_name != NULL);
	g_return_if_fail (icon_name != NULL);
	g_return_if_fail (caption != NULL);

	icon_view = GTK_ICON_VIEW (window->priv->icon_view);
	notebook = GTK_NOTEBOOK (window->priv->notebook);

	page = gtk_notebook_get_n_pages (notebook);
	model = GTK_TREE_MODEL (window->priv->store);
	pixbuf = preferences_window_load_pixbuf (icon_name);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		COLUMN_ID, page_name,
		COLUMN_TEXT, caption,
		COLUMN_HELP, help_target,
		COLUMN_PIXBUF, pixbuf,
		COLUMN_PAGE, page,
		COLUMN_SORT, sort_order,
		-1);

	index = window->priv->index;
	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	g_hash_table_insert (index, g_strdup (page_name), reference);
	gtk_tree_path_free (path);

	align = g_object_new (GTK_TYPE_ALIGNMENT, NULL);
	gtk_widget_show (GTK_WIDGET (align));
	g_object_set_data (G_OBJECT (align), "create_fn", create_fn);
	gtk_notebook_append_page (notebook, align, NULL);
	gtk_container_child_set (
		GTK_CONTAINER (notebook), align,
		"tab-fill", FALSE, "tab-expand", FALSE, NULL);

	/* Force GtkIconView to recalculate the text wrap width,
	 * otherwise we get a really narrow icon list on the left
	 * side of the preferences window. */
	gtk_icon_view_set_item_width (icon_view, -1);
	gtk_widget_queue_resize (GTK_WIDGET (window));
}

void
e_preferences_window_show_page (EPreferencesWindow *window,
                                const gchar *page_name)
{
	GtkTreeRowReference *reference;
	GtkIconView *icon_view;
	GtkTreePath *path;

	g_return_if_fail (E_IS_PREFERENCES_WINDOW (window));
	g_return_if_fail (page_name != NULL);
	g_return_if_fail (window->priv->setup);

	icon_view = GTK_ICON_VIEW (window->priv->icon_view);
	reference = g_hash_table_lookup (window->priv->index, page_name);
	g_return_if_fail (reference != NULL);

	path = gtk_tree_row_reference_get_path (reference);
	gtk_icon_view_select_path (icon_view, path);
	gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.0, 0.0);
	gtk_tree_path_free (path);
}

/*
 * Create all the deferred configuration pages.
 */
void
e_preferences_window_setup (EPreferencesWindow *window)
{
	gint i, num;
	GtkNotebook *notebook;
	EPreferencesWindowPrivate *priv;
	GSList *children = NULL;

	g_return_if_fail (E_IS_PREFERENCES_WINDOW (window));

	priv = E_PREFERENCES_WINDOW_GET_PRIVATE (window);

	if (priv->setup)
		return;

	notebook = GTK_NOTEBOOK (priv->notebook);
	num = gtk_notebook_get_n_pages (notebook);

	for (i = 0; i < num; i++) {
		GtkBin *align;
		GtkWidget *content;
		EPreferencesWindowCreatePageFn create_fn;

		align = GTK_BIN (gtk_notebook_get_nth_page (notebook, i));
		create_fn = g_object_get_data (G_OBJECT (align), "create_fn");

		if (!create_fn || gtk_bin_get_child (align))
			continue;

		content = create_fn (window);
		if (content) {
			GtkScrolledWindow *scrolled;

			children = g_slist_prepend (children, content);

			scrolled = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
			gtk_scrolled_window_add_with_viewport (scrolled, content);
			gtk_scrolled_window_set_min_content_width (scrolled, 320);
			gtk_scrolled_window_set_min_content_height (scrolled, 240);
			gtk_scrolled_window_set_policy (scrolled, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			gtk_scrolled_window_set_shadow_type (scrolled, GTK_SHADOW_NONE);

			gtk_viewport_set_shadow_type (
				GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolled))),
				GTK_SHADOW_NONE);

			gtk_widget_show (content);
			gtk_widget_show (GTK_WIDGET (scrolled));

			gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (scrolled));
		}
	}

	e_util_resize_window_for_screen (GTK_WINDOW (window), -1, -1, children);

	g_slist_free (children);

	priv->setup = TRUE;
}
