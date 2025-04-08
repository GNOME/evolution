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

enum {
	CLOSE,
	LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL];

struct _EPreferencesWindowPrivate {
	gboolean   setup;
	gpointer   shell;

	GtkWidget *stack;
	GtkWidget *listbox;
};

G_DEFINE_TYPE_WITH_PRIVATE (EPreferencesWindow, e_preferences_window, GTK_TYPE_WINDOW)

#define E_TYPE_PREFERENCES_WINDOW_ROW e_preferences_window_row_get_type ()
G_DECLARE_FINAL_TYPE (EPreferencesWindowRow, e_preferences_window_row, E, PREFERENCES_WINDOW_ROW, GtkListBoxRow)

struct _EPreferencesWindowRow {
	GtkListBoxRow parent_instance;

	gchar *page_name;
	gchar *caption;
	gchar *help_target;
	EPreferencesWindowCreatePageFn create_fn;
	gint sort_order;
	GtkWidget *page;
};

G_DEFINE_TYPE (EPreferencesWindowRow, e_preferences_window_row, GTK_TYPE_LIST_BOX_ROW)

static void
e_preferences_window_row_finalize (GObject *gobject)
{
	EPreferencesWindowRow *row = E_PREFERENCES_WINDOW_ROW (gobject);

	g_clear_pointer (&row->page_name, g_free);
	g_clear_pointer (&row->caption, g_free);
	g_clear_pointer (&row->help_target, g_free);

	G_OBJECT_CLASS (e_preferences_window_row_parent_class)->finalize (gobject);
}

static void
e_preferences_window_row_class_init (EPreferencesWindowRowClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = e_preferences_window_row_finalize;
}

static void
e_preferences_window_row_init (EPreferencesWindowRow *row)
{
}

static GtkWidget *
e_preferences_window_row_new (const gchar *page_name,
                              const gchar *icon_name,
                              const gchar *caption,
                              const gchar *help_target,
                              EPreferencesWindowCreatePageFn create_fn,
                              gint sort_order)
{
	GtkWidget *hbox, *image, *label;
	EPreferencesWindowRow *row;

	row = g_object_new (E_TYPE_PREFERENCES_WINDOW_ROW, NULL);
	row->page_name = g_strdup (page_name);
	row->caption = g_strdup (caption);
	row->help_target = g_strdup (help_target);
	row->create_fn = create_fn;
	row->sort_order = sort_order;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
	g_object_set (G_OBJECT (image),
		"pixel-size", 24,
		"use-fallback", TRUE,
		NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (image), "sidebar-icon");
	label = gtk_label_new (caption);
	gtk_container_add (GTK_CONTAINER (hbox), image);
	gtk_container_add (GTK_CONTAINER (hbox), label);
	gtk_container_add (GTK_CONTAINER (row), hbox);
	return GTK_WIDGET (row);
}

static GtkWidget *
e_preferences_window_row_create_page (EPreferencesWindowRow *self,
                                      EPreferencesWindow *window)
{
	g_return_val_if_fail (E_IS_PREFERENCES_WINDOW_ROW (self), NULL);
	g_return_val_if_fail (E_IS_PREFERENCES_WINDOW (window), NULL);
	g_return_val_if_fail (self->create_fn != NULL, NULL);
	g_return_val_if_fail (self->page == NULL, NULL);

	self->page = self->create_fn (window);
	return self->page;
}

static void
preferences_window_help_clicked_cb (EPreferencesWindow *window)
{
	gchar *help = NULL;
	GtkListBoxRow *child;

	g_return_if_fail (window != NULL);

	child = gtk_list_box_get_selected_row (GTK_LIST_BOX (window->priv->listbox));
	if (child && E_IS_PREFERENCES_WINDOW_ROW (child)) {
		EPreferencesWindowRow *row = E_PREFERENCES_WINDOW_ROW (child);
		help = row->help_target;
	}

	e_display_help (GTK_WINDOW (window), help ? help : "index");
}

static void
preferences_window_row_selected (EPreferencesWindow *window,
                                 GtkListBoxRow *row,
                                 GtkListBox *box)
{
	g_signal_emit_by_name (row, "activate", NULL);
}

static void
preferences_window_row_activated (EPreferencesWindow *window,
                                  GtkListBoxRow *row,
                                  GtkListBox *box)
{
	EPreferencesWindowRow *pref_row = E_PREFERENCES_WINDOW_ROW (row);

	g_return_if_fail (window != NULL);
	g_return_if_fail (E_IS_PREFERENCES_WINDOW_ROW (row));

	gtk_stack_set_visible_child_name (GTK_STACK (window->priv->stack), pref_row->page_name);
}

static gint
on_list_box_sort (GtkListBoxRow *row1,
                  GtkListBoxRow *row2,
                  gpointer user_data)
{
	EPreferencesWindowRow *pref_row1 = E_PREFERENCES_WINDOW_ROW (row1);
	EPreferencesWindowRow *pref_row2 = E_PREFERENCES_WINDOW_ROW (row2);

	if (pref_row1->sort_order != pref_row2->sort_order)
		return pref_row1->sort_order - pref_row2->sort_order;

	return g_utf8_collate (pref_row1->caption, pref_row2->caption);
}

static void
e_preferences_window_close (EPreferencesWindow *window)
{
	gtk_window_close (GTK_WINDOW (window));
}

static void
preferences_window_dispose (GObject *object)
{
	EPreferencesWindow *self = E_PREFERENCES_WINDOW (object);

	if (self->priv->shell) {
		g_object_remove_weak_pointer (self->priv->shell, &self->priv->shell);
		self->priv->shell = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_preferences_window_parent_class)->dispose (object);
}

static void
e_preferences_window_class_init (EPreferencesWindowClass *class)
{
	GObjectClass *object_class;
	GtkBindingSet *binding_set;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = preferences_window_dispose;

	class->close = e_preferences_window_close;

	/**
	 * EPreferencesWindow::close:
	 *
	 * GtkBindingSignal which gets emitted when the user uses a
	 * keybinding to close the dialog.
	 *
	 * The default binding for this signal is the Escape key.
	 */
	dialog_signals[CLOSE] =
		g_signal_new ("close",
			G_OBJECT_CLASS_TYPE (class),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			G_STRUCT_OFFSET (EPreferencesWindowClass, close),
			NULL, NULL, NULL,
			G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);
}

static void
e_preferences_window_init (EPreferencesWindow *window)
{
	GtkWidget *header = NULL;
	GtkWidget *widget;
	GtkWidget *hbox;
	GtkWidget *vbox;

	window->priv = e_preferences_window_get_instance_private (window);

	if (e_util_get_use_header_bar ()) {
		widget = gtk_header_bar_new ();
		g_object_set (G_OBJECT (widget),
			"show-close-button", TRUE,
			"visible", TRUE,
			NULL);
		gtk_window_set_titlebar (GTK_WINDOW (window), widget);
		header = widget;
	}

	widget = gtk_stack_new ();
	gtk_widget_show (widget);
	window->priv->stack = widget;

	widget = g_object_new (GTK_TYPE_LIST_BOX,
		"selection-mode", GTK_SELECTION_BROWSE,
		"visible", TRUE,
		NULL);
	g_signal_connect_swapped (
		widget, "row-selected",
		G_CALLBACK (preferences_window_row_selected), window);
	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (preferences_window_row_activated), window);
	gtk_list_box_set_sort_func (GTK_LIST_BOX (widget),
		on_list_box_sort,
		NULL, NULL);
	window->priv->listbox = widget;
	widget = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
		"hscrollbar-policy", GTK_POLICY_NEVER,
		"visible", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (widget), window->priv->listbox);

	vbox = g_object_new (GTK_TYPE_BOX,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"visible", TRUE,
		NULL);
	hbox = g_object_new (GTK_TYPE_BOX,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"visible", TRUE,
		NULL);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_container_add (GTK_CONTAINER (hbox), widget);
	gtk_container_add (GTK_CONTAINER (hbox), window->priv->stack);

	widget = gtk_button_new_from_icon_name ("help-browser", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text (widget, _("Help"));
	gtk_widget_show (widget);
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (preferences_window_help_clicked_cb), window);
	if (header) {
		gtk_header_bar_pack_end (GTK_HEADER_BAR (header), widget);
	} else {
		GtkAccelGroup *accel_group;
		GtkWidget *bbox;

		bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
		g_object_set (bbox,
			"layout-style", GTK_BUTTONBOX_END,
			"visible", TRUE,
			"margin-start", 6,
			"margin-end", 6,
			"margin-top", 6,
			"margin-bottom", 6,
			NULL);
		gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
		gtk_container_add (GTK_CONTAINER (vbox), bbox);

		gtk_box_pack_start (GTK_BOX (bbox), widget, FALSE, FALSE, 0);
		gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (bbox), widget, TRUE);

		widget = e_dialog_button_new_with_icon ("window-close", _("_Close"));
		g_signal_connect_swapped (
			widget, "clicked",
			G_CALLBACK (gtk_widget_hide), window);
		gtk_widget_set_can_default (widget, TRUE);
		gtk_box_pack_start (GTK_BOX (bbox), widget, FALSE, FALSE, 0);
		accel_group = gtk_accel_group_new ();
		gtk_widget_add_accelerator (
			widget, "activate", accel_group,
			GDK_KEY_Escape, (GdkModifierType) 0,
			GTK_ACCEL_VISIBLE);
		gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
		gtk_widget_grab_default (widget);
		gtk_widget_show (widget);
	}

	gtk_window_set_title (GTK_WINDOW (window), _("Evolution Preferences"));
	gtk_window_set_resizable (GTK_WINDOW (window), TRUE);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (gtk_widget_hide_on_delete), NULL);
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

static gboolean
e_preferences_window_has_page (EPreferencesWindow *window,
                               const gchar *page_name,
                               const gchar *caption,
                               const gchar *help_target,
                               EPreferencesWindowCreatePageFn create_fn,
                               gint sort_order)
{
	GList *children, *list;
	gboolean exists = FALSE;

	children = gtk_container_get_children (GTK_CONTAINER (window->priv->listbox));
	for (list = children; list != NULL; list = list->next) {
		EPreferencesWindowRow *row = list->data;

		if (g_strcmp0 (row->page_name, page_name) == 0 &&
		    g_strcmp0 (row->caption, caption) == 0 &&
		    g_strcmp0 (row->help_target, help_target) == 0 &&
		    row->create_fn == create_fn &&
		    row->sort_order == sort_order) {
			exists = TRUE;
			break;
		}
	}

	g_list_free (children);

	return exists;
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
	GtkWidget *row;

	g_return_if_fail (E_IS_PREFERENCES_WINDOW (window));
	g_return_if_fail (create_fn != NULL);
	g_return_if_fail (page_name != NULL);
	g_return_if_fail (icon_name != NULL);
	g_return_if_fail (caption != NULL);

	/* avoid duplicates */
	if (e_preferences_window_has_page (window, page_name, caption, help_target, create_fn, sort_order))
		return;

	row = e_preferences_window_row_new (page_name, icon_name, caption, help_target, create_fn, sort_order);
	gtk_widget_show_all (row);
	gtk_container_add (GTK_CONTAINER (window->priv->listbox), row);
}

void
e_preferences_window_show_page (EPreferencesWindow *window,
                                const gchar *page_name)
{
	GList *children, *list;

	g_return_if_fail (E_IS_PREFERENCES_WINDOW (window));
	g_return_if_fail (page_name != NULL);
	g_return_if_fail (window->priv->listbox);

	children = gtk_container_get_children (GTK_CONTAINER (window->priv->listbox));
	for (list = children; list != NULL; list = list->next) {
		EPreferencesWindowRow *child = list->data;
		if (!g_strcmp0 (page_name, child->page_name)) {
			gtk_list_box_select_row (GTK_LIST_BOX (window->priv->listbox), GTK_LIST_BOX_ROW (child));
			break;
		}
	}

	g_list_free (children);
}

/*
 * Create all the deferred configuration pages.
 */
void
e_preferences_window_setup (EPreferencesWindow *window)
{
	GList *children, *list;
	GSList *slist_children = NULL;

	g_return_if_fail (E_IS_PREFERENCES_WINDOW (window));

	if (window->priv->setup)
		return;

	children = gtk_container_get_children (GTK_CONTAINER (window->priv->listbox));
	for (list = children; list != NULL; list = list->next) {
		EPreferencesWindowRow *child = list->data;
		GtkWidget *content, *scrolled;

		content = e_preferences_window_row_create_page (child, window);
		if (content) {
			scrolled = gtk_scrolled_window_new (NULL, NULL);
			g_object_set (G_OBJECT (scrolled),
				"min-content-width", 320,
				"min-content-height", 240,
				"hscrollbar-policy", GTK_POLICY_NEVER,
				"visible", TRUE,
				NULL);
			gtk_container_add (GTK_CONTAINER (scrolled), content);
			gtk_widget_show (content);
			gtk_stack_add_named (GTK_STACK (window->priv->stack), scrolled, child->page_name);
			slist_children = g_slist_prepend (slist_children, scrolled);
		}
	}

	e_util_resize_window_for_screen (GTK_WINDOW (window), -1, -1, slist_children);
	g_slist_free (slist_children);
	g_list_free (children);
	window->priv->setup = TRUE;
}
