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
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "mail/e-mail-reader.h"
#include "mail/message-list.h"
#include "mail/em-folder-tree.h"
#include "e-mail-notebook-view.h"
#include "e-mail-folder-pane.h"
#include "e-mail-message-pane.h"

#include <shell/e-shell-window-actions.h>

#if HAVE_CLUTTER
#include <clutter/clutter.h>
#include <mx/mx.h>
#include <clutter-gtk/clutter-gtk.h>

#include "e-mail-tab-picker.h"
#endif

#define E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_NOTEBOOK_VIEW, EMailNotebookViewPrivate))

struct _EMailNotebookViewPrivate {
	GtkNotebook *book;
	EMailView *current_view;
	GHashTable *views;
	gboolean inited;

#if HAVE_CLUTTER
	EMailTabPicker *tab_picker;
	GtkWidget *embed;
	ClutterActor *actor;
	ClutterActor *stage;
#endif
};

enum {
	PROP_0,
	PROP_GROUP_BY_THREADS,
};

#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")

/* Forward Declarations */
static void e_mail_notebook_view_reader_init (EMailReaderInterface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EMailNotebookView, e_mail_notebook_view, E_TYPE_MAIL_VIEW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_READER, e_mail_notebook_view_reader_init))

#if HAVE_CLUTTER
static void
mnv_set_current_tab (EMailNotebookView *view,
                     gint page)
{
	ClutterAnimation *animation;

	clutter_actor_set_opacity (view->priv->actor, 0);
	gtk_notebook_set_current_page (view->priv->book, page);

	animation = clutter_actor_animate (
		(ClutterActor *)view->priv->actor,
		CLUTTER_EASE_IN_SINE, 500, "opacity", 255, NULL);
}
#endif

static gint
emnv_get_page_num (EMailNotebookView *view,
                   GtkWidget *widget)
{
	EMailNotebookViewPrivate *priv = view->priv;
	gint i, n;

	n = gtk_notebook_get_n_pages (priv->book);

	for (i=0; i<n; i++) {
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
	const gchar *uri;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	shell_view = e_mail_view_get_shell_view (view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	page = gtk_notebook_get_nth_page (book, page_num);
	uri = e_mail_reader_get_folder_uri (E_MAIL_READER (page));
	mview = E_MAIL_VIEW (page);

	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

	if (uri && E_IS_MAIL_FOLDER_PANE (mview))
		em_folder_tree_set_selected (folder_tree, uri, FALSE);

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

#if HAVE_CLUTTER
static void
fix_tab_picker_width (GtkWidget *widget,
                      GtkAllocation *allocation,
                      ClutterActor *actor)
{
	ClutterActor *stage = g_object_get_data ((GObject *)actor, "stage");

	clutter_actor_set_size (actor, allocation->width-1, -1);
	clutter_actor_set_size (stage, allocation->width-1, -1);
}

static void
fix_height_cb (ClutterActor *actor,
               GParamSpec *pspec,
               ClutterActor *table)
{
	GtkWidget *embed = (GtkWidget *)g_object_get_data ((GObject *)actor, "embed");
	ClutterActor *stage = g_object_get_data ((GObject *)actor, "stage");

	clutter_actor_set_height (stage, clutter_actor_get_height(actor));
	gtk_widget_set_size_request (embed, -1, (gint) clutter_actor_get_height(actor));
}

static void
chooser_clicked_cb (EMailTabPicker *picker,
                    EMailNotebookView *view)
{
	EMailNotebookViewPrivate *priv;

	gboolean preview_mode;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);
	preview_mode = !e_mail_tab_picker_get_preview_mode (priv->tab_picker);

	e_mail_tab_picker_set_preview_mode (priv->tab_picker , preview_mode);
}

static void
tab_picker_preview_mode_notify (EMailTabPicker *picker,
                                GParamSpec *pspec,
                                EMailNotebookView *view)
{
	GList *tabs, *t;
	gboolean preview_mode = e_mail_tab_picker_get_preview_mode (picker);

	clutter_actor_set_name (
		CLUTTER_ACTOR (picker),
		preview_mode ? "tab-picker-preview" : NULL);

	tabs = e_mail_tab_picker_get_tabs (picker);
	for (t = tabs; t; t = t->next) {
		EMailTab *tab;
		ClutterActor *preview;
		tab = E_MAIL_TAB (t->data);

		preview = e_mail_tab_get_preview_actor (tab);

		if (!preview)
			continue;

		if (preview_mode) {
			/* Show all pages so that the preview clones work correctly */
			clutter_actor_set_opacity (preview, 255);
			clutter_actor_show (preview);
		} else {
			clutter_actor_hide (preview);
		}
	}
	g_list_free (tabs);
}

static void
mnv_tab_anim_frame_cb (ClutterTimeline *timeline,
                               gint             frame_num,
                               EMailTab          *tab)
{
	if (!clutter_actor_get_parent (CLUTTER_ACTOR (tab))) {
		clutter_timeline_stop (timeline);
		g_object_unref (timeline);
		g_object_unref (tab);

		return;
	}

	e_mail_tab_set_width (tab, 200 * clutter_timeline_get_progress (timeline));
}

static void
mnv_tab_anim_complete_cb (ClutterTimeline *timeline,
                                  EMailTab          *tab)
{
	e_mail_tab_set_width (tab, 200);
	g_object_unref (tab);
	g_object_unref (timeline);
}

struct _tab_data {
	gboolean select;
	EMailNotebookView *view;
	EMailTab *tab;
};

static void
mnv_tab_closed_cb (ClutterTimeline *timeline,
		struct _tab_data *data)
{
	EMailView *page = g_object_get_data ((GObject *)data->tab, "page");
	const gchar *folder_uri = e_mail_reader_get_folder_uri (E_MAIL_READER(page));
	EMailView *prev;
	gint num;

	if (E_IS_MAIL_FOLDER_PANE (page))
		g_hash_table_remove (data->view->priv->views, folder_uri);
	prev = e_mail_view_get_previous_view (page);
	if (prev) {
		num = emnv_get_page_num (data->view, (GtkWidget *)prev);
		mnv_set_current_tab (data->view, num);
		e_mail_tab_picker_set_current_tab (data->view->priv->tab_picker, num);
	}

	e_mail_tab_picker_remove_tab (data->view->priv->tab_picker, data->tab);
	gtk_notebook_remove_page (data->view->priv->book,
			gtk_notebook_page_num (data->view->priv->book, (GtkWidget *)page));

}

static void
mnv_tab_closed (EMailTab *tab, EMailNotebookView *view)
{
	EMailNotebookViewPrivate *priv = view->priv;
	gint page, cur;
	gboolean select = FALSE;
	ClutterTimeline *timeline;
	struct _tab_data *data = g_new0 (struct _tab_data, 1);

	if (e_mail_tab_picker_get_n_tabs (priv->tab_picker) == 1)
		return;

	page = e_mail_tab_picker_get_tab_no (priv->tab_picker,
					     tab);
	cur = e_mail_tab_picker_get_current_tab (priv->tab_picker);

	if (cur == page)
		select = TRUE;

	data->select  = select;
	data->tab = tab;
	data->view = view;

	clutter_actor_set_reactive (CLUTTER_ACTOR (tab), FALSE);
	timeline = clutter_timeline_new (150);
	clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_BACKWARD);
	g_signal_connect (
		timeline, "new-frame",
		G_CALLBACK (mnv_tab_anim_frame_cb), tab);
	g_signal_connect (
		timeline, "completed",
		G_CALLBACK (mnv_tab_closed_cb), data);
	clutter_timeline_start (timeline);
}

static void
tab_activated_cb (EMailTabPicker *picker,
		  EMailTab	 *tab,
		  EMailNotebookView *view)
{
	EMailView *page = g_object_get_data ((GObject *)tab, "page");
	gint num = emnv_get_page_num (view, (GtkWidget *)page);

	mnv_set_current_tab (view, num);
}
#endif

static void
tab_remove_gtk_cb (GtkWidget *button,
		   EMailNotebookView *view)
{
	EMailView *page = g_object_get_data ((GObject *)button, "page");
	const gchar *folder_uri = e_mail_reader_get_folder_uri (E_MAIL_READER(page));
	EMailView *prev;
	gint num;

	if (gtk_notebook_get_n_pages(view->priv->book) == 1)
		return;

	if (E_IS_MAIL_FOLDER_PANE (page))
		g_hash_table_remove (view->priv->views, folder_uri);

	prev = e_mail_view_get_previous_view (page);
	if (prev) {
		num = emnv_get_page_num (view, (GtkWidget *)prev);
		gtk_notebook_set_current_page (view->priv->book, num);
	}
	gtk_notebook_remove_page (view->priv->book,
			gtk_notebook_page_num (view->priv->book, (GtkWidget *)page));

}

static GtkWidget *
create_tab_label (EMailNotebookView *view,
		  EMailView *page,
		  const gchar *str)
{
	GtkWidget *container, *widget;

	widget = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (widget);
	container = widget;

	widget = gtk_label_new (str);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX(container), widget, TRUE, FALSE, 0);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		"gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show_all (widget);
	gtk_box_pack_end (GTK_BOX(container), widget, FALSE, FALSE, 0);
	g_object_set_data ((GObject *)widget, "page", page);
	g_object_set_data ((GObject *)page, "close-button", widget);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (tab_remove_gtk_cb), view);

	return container;
}

#if HAVE_CLUTTER

static ClutterActor *
create_gtk_actor (GtkWidget *vbox)
{
  GtkWidget       *bin;
  ClutterActor    *gtk_actor;

  gtk_actor = gtk_clutter_actor_new ();
  bin = gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (gtk_actor));

  gtk_container_add (GTK_CONTAINER (bin), vbox);

  gtk_widget_show (bin);
  gtk_widget_show(vbox);
  return gtk_actor;
}

static void
fix_clutter_embed_width (GtkWidget *widget,
                         GtkAllocation *allocation,
                         ClutterActor *actor)
{
	GtkWidget *embed = (GtkWidget *)g_object_get_data ((GObject *)actor, "embed");
	GtkAllocation galoc;

	gtk_widget_get_allocation (embed, &galoc);
	clutter_actor_set_size (actor, allocation->width-1, galoc.height);
}

static GtkWidget *
create_under_clutter (GtkWidget *widget, GtkWidget *paned)
{
	GtkWidget *embed;
	ClutterActor *stage, *actor;

	embed = gtk_clutter_embed_new ();
	gtk_widget_show (embed);

	actor = create_gtk_actor (widget);
	clutter_actor_show (actor);
	stage = gtk_clutter_embed_get_stage ((GtkClutterEmbed *)embed);
	clutter_container_add_actor ((ClutterContainer *)stage, actor);

	g_object_set_data ((GObject *)actor, "embed", embed);
	g_object_set_data ((GObject *)actor, "stage", stage);
	g_object_set_data ((GObject *)actor, "widget", widget);
	g_object_set_data ((GObject *)widget, "actor", actor);
	g_object_set_data ((GObject *)embed, "actor", actor);

	g_signal_connect (
		paned, "size-allocate",
		G_CALLBACK (fix_clutter_embed_width), actor);
	clutter_actor_show(stage);

	return embed;
}

#endif

static void
mail_notebook_view_constructed (GObject *object)
{
	EMailNotebookViewPrivate *priv;
	EShellView *shell_view;
	GtkWidget *container;
	GtkWidget *widget;
#if HAVE_CLUTTER
	EMailTab *tab;
	ClutterActor *stage, *clone;
	ClutterTimeline *timeline;
#endif

	priv = E_MAIL_NOTEBOOK_VIEW (object)->priv;

	container = GTK_WIDGET(object);

#if HAVE_CLUTTER
	widget = gtk_clutter_embed_new ();
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX(container), widget, FALSE, FALSE, 0);

	stage = gtk_clutter_embed_get_stage ((GtkClutterEmbed *)widget);
	clutter_actor_show(stage);
	clutter_actor_set_reactive (stage, TRUE);

	priv->tab_picker = (EMailTabPicker *)e_mail_tab_picker_new ();
	clutter_actor_show ((ClutterActor *)priv->tab_picker);
	g_signal_connect (
		priv->tab_picker, "tab-activated",
		G_CALLBACK (tab_activated_cb), object);
	g_signal_connect (
		priv->tab_picker, "chooser-clicked",
		G_CALLBACK (chooser_clicked_cb), object);
	g_signal_connect (
		priv->tab_picker, "notify::preview-mode",
		G_CALLBACK (tab_picker_preview_mode_notify), object);
	g_signal_connect (
		priv->tab_picker, "notify::height",
		G_CALLBACK(fix_height_cb), widget);

	clutter_container_add_actor (
		(ClutterContainer *) stage,
		(ClutterActor *) priv->tab_picker);

	e_mail_tab_picker_enable_drop (priv->tab_picker, TRUE);

	g_object_set_data ((GObject *)priv->tab_picker, "embed", widget);
	g_object_set_data ((GObject *)priv->tab_picker, "stage", stage);

	g_signal_connect (
		object, "size-allocate",
		G_CALLBACK(fix_tab_picker_width), priv->tab_picker);

	clutter_actor_set_height (
		stage, clutter_actor_get_height (
		(ClutterActor *) priv->tab_picker));
	gtk_widget_set_size_request (
		widget, -1, (gint) clutter_actor_get_height (
		(ClutterActor *) priv->tab_picker));

	tab = (EMailTab *) e_mail_tab_new_full ("", NULL, 1);
	clone = e_mail_tab_new_full ("", NULL, 200);

	e_mail_tab_set_can_close ((EMailTab *)clone, FALSE);
	clutter_actor_set_reactive (clone, FALSE);
	clutter_actor_show (clone);

	e_mail_tab_set_preview_actor ((EMailTab *)tab, clone);
	e_mail_tab_set_can_close (tab, TRUE);

	e_mail_tab_picker_add_tab (priv->tab_picker, tab, -1);
	clutter_actor_show((ClutterActor *)tab);
	e_mail_tab_picker_set_current_tab (priv->tab_picker, 0);
	e_mail_tab_enable_drag (tab, TRUE);

	g_object_ref (tab);
	timeline = clutter_timeline_new (150);
	g_signal_connect (
		timeline, "new-frame",
		G_CALLBACK (mnv_tab_anim_frame_cb), tab);
	g_signal_connect (
		timeline, "completed",
		G_CALLBACK (mnv_tab_anim_complete_cb), tab);
	clutter_timeline_start (timeline);
#endif

	widget = gtk_notebook_new ();
	priv->book = (GtkNotebook *)widget;
	gtk_widget_show (widget);
#if HAVE_CLUTTER
	priv->embed = create_under_clutter (widget, container);
	gtk_box_pack_start (GTK_BOX (container), priv->embed, TRUE, TRUE, 0);
	priv->actor = g_object_get_data((GObject *)priv->embed, "actor");
	priv->stage = g_object_get_data((GObject *)priv->actor, "stage");
#else
	gtk_box_pack_start (GTK_BOX(container), widget, TRUE, TRUE, 0);
#endif

#if HAVE_CLUTTER
	gtk_notebook_set_show_tabs ((GtkNotebook *)widget, FALSE);
#else
	gtk_notebook_set_scrollable ((GtkNotebook *)widget, TRUE);
#endif

	gtk_notebook_set_show_border ((GtkNotebook *)widget, FALSE);

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
}

static void
mail_notebook_view_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_GROUP_BY_THREADS:
			e_mail_reader_set_group_by_threads (
				E_MAIL_READER(E_MAIL_NOTEBOOK_VIEW(object)->priv->current_view),
				g_value_get_boolean (value));
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
	switch (property_id) {
		case PROP_GROUP_BY_THREADS:
			g_value_set_boolean (
				value,
				e_mail_reader_get_group_by_threads (
				E_MAIL_READER(E_MAIL_NOTEBOOK_VIEW(object)->priv->current_view)));
			return;

	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_notebook_view_set_search_strings (EMailView *view,
                                       GSList *search_strings)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	e_mail_view_set_search_strings (priv->current_view, search_strings);
}

static GalViewInstance *
mail_notebook_view_get_view_instance (EMailView *view)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return NULL;

	return e_mail_view_get_view_instance (priv->current_view);
}

static void
mail_notebook_view_update_view_instance (EMailView *view)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return;

	e_mail_view_update_view_instance (priv->current_view);
}

static void
mail_notebook_view_set_orientation (EMailView *view,
                                    GtkOrientation orientation)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return;

	e_mail_view_set_orientation (priv->current_view, orientation);
}

static GtkOrientation
mail_notebook_view_get_orientation (EMailView *view)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return GTK_ORIENTATION_VERTICAL;

	return e_mail_view_get_orientation (priv->current_view);
}

static gboolean
mail_notebook_view_get_show_deleted (EMailView *view)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return FALSE;

	return e_mail_view_get_show_deleted (priv->current_view);
}

static void
mail_notebook_view_set_show_deleted (EMailView *view,
                                     gboolean show_deleted)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return;

	e_mail_view_set_show_deleted (priv->current_view, show_deleted);
}

static gboolean
mail_notebook_view_get_preview_visible (EMailView *view)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return FALSE;

	return e_mail_view_get_preview_visible (priv->current_view);
}

static void
mail_notebook_view_set_preview_visible (EMailView *view,
                                        gboolean preview_visible)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	if (priv->current_view == NULL)
		return;

	e_mail_view_set_preview_visible (priv->current_view, preview_visible);
}

static GtkActionGroup *
mail_notebook_view_get_action_group (EMailReader *reader)
{
	EMailView *view;
	EShellWindow *shell_window;
	EShellView *shell_view;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return E_SHELL_WINDOW_ACTION_GROUP_MAIL (shell_window);
}

static EMFormatHTML *
mail_notebook_view_get_formatter (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	if (priv->current_view == NULL)
		return NULL;

	return e_mail_reader_get_formatter (E_MAIL_READER(priv->current_view));
}

static gboolean
mail_notebook_view_get_hide_deleted (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	if (priv->current_view == NULL)
		return FALSE;

	reader = E_MAIL_READER (priv->current_view);

	return e_mail_reader_get_hide_deleted (reader);
}

static GtkWidget *
mail_notebook_view_get_message_list (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	if (priv->current_view == NULL)
		return NULL;

	reader = E_MAIL_READER (priv->current_view);

	return e_mail_reader_get_message_list (reader);
}

static GtkMenu *
mail_notebook_view_get_popup_menu (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	if (priv->current_view == NULL)
		return NULL;

	reader = E_MAIL_READER (priv->current_view);

	return e_mail_reader_get_popup_menu (reader);
}

static EShellBackend *
mail_notebook_view_get_shell_backend (EMailReader *reader)
{
	EMailView *view;
	EShellView *shell_view;

	view = E_MAIL_VIEW (reader);
	shell_view = e_mail_view_get_shell_view (view);

	return e_shell_view_get_shell_backend (shell_view);
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
reconnect_changed_event (EMailReader *child, EMailReader *parent)
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
	const gchar *folder_uri;
	CamelFolder *folder;
	EMailView *pane;
	gint page;
	CamelMessageInfo *info;
	GtkWidget *preview;
	gint pos;

#if HAVE_CLUTTER
	EMailTab *tab;
	ClutterActor *clone;
	ClutterTimeline *timeline;
	GtkWidget *mlist;
#endif

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (nview);

#if HAVE_CLUTTER
	e_mail_tab_set_active (
		e_mail_tab_picker_get_tab (priv->tab_picker,
		e_mail_tab_picker_get_current_tab (priv->tab_picker)), FALSE);
#endif

	shell_view = e_mail_view_get_shell_view (E_MAIL_VIEW (nview));
	pos = emnv_get_page_num (nview, GTK_WIDGET (priv->current_view));
	pane = e_mail_message_pane_new (shell_view);
	e_mail_view_set_previous_view (pane, priv->current_view);
	E_MAIL_MESSAGE_PANE(pane)->parent_folder_view = priv->current_view;
	priv->current_view = pane;

	gtk_widget_show (GTK_WIDGET (pane));

	preview = e_mail_paned_view_get_preview (E_MAIL_PANED_VIEW(pane));

	folder = e_mail_reader_get_folder (E_MAIL_READER(view));
	folder_uri = e_mail_reader_get_folder_uri (E_MAIL_READER(view));

	info = camel_folder_get_message_info (folder, uid);

	page = gtk_notebook_insert_page (
		priv->book, GTK_WIDGET (pane),
		create_tab_label (nview, priv->current_view,
		camel_message_info_subject (info)), pos + 1);

#if HAVE_CLUTTER
	mlist = e_mail_reader_get_message_list (E_MAIL_READER(pane));
	mnv_set_current_tab (nview, page);
	g_object_set_data ((GObject *)priv->current_view, "stage", priv->stage);
	g_object_set_data ((GObject *)mlist, "stage", priv->stage);
	g_object_set_data ((GObject *)mlist, "preview-actor", priv->actor);
#else
	gtk_notebook_set_current_page (priv->book, page);
#endif

#if HAVE_CLUTTER
	tab = (EMailTab *)e_mail_tab_new_full (camel_message_info_subject(info), NULL, 1);
	g_object_set_data ((GObject *)tab, "page", pane);
	g_object_set_data ((GObject *)pane, "tab", tab);

	clutter_actor_show((ClutterActor *)tab);

	clone = e_mail_tab_new_full (camel_message_info_subject(info), NULL, 200);
	clutter_actor_set_reactive (clone, FALSE);
	clutter_actor_show (clone);

	e_mail_tab_set_preview_actor (tab, clone);
	e_mail_tab_set_can_close (tab, TRUE);
	e_mail_tab_picker_add_tab (priv->tab_picker, tab, pos+1);
	e_mail_tab_enable_drag (tab, TRUE);

	page = e_mail_tab_picker_get_tab_no (priv->tab_picker, tab);
	e_mail_tab_picker_set_current_tab (priv->tab_picker, page);

	g_signal_connect (tab , "closed",
			  G_CALLBACK (mnv_tab_closed), nview);

	g_object_ref (tab);
	timeline = clutter_timeline_new (150);
	g_signal_connect (
		timeline, "new-frame",
		G_CALLBACK (mnv_tab_anim_frame_cb), tab);
	g_signal_connect (
		timeline, "completed",
		G_CALLBACK (mnv_tab_anim_complete_cb), tab);
	clutter_timeline_start (timeline);
#endif

	g_signal_connect (
		E_MAIL_READER(pane), "changed",
		G_CALLBACK (reconnect_changed_event), nview);
	g_signal_connect (
		E_MAIL_READER (pane), "folder-loaded",
		G_CALLBACK (reconnect_folder_loaded_event), nview);
	e_mail_reader_set_folder (
		E_MAIL_READER (pane), folder, folder_uri);
	e_mail_reader_set_group_by_threads (
		E_MAIL_READER (pane),
		e_mail_reader_get_group_by_threads (E_MAIL_READER(view)));

	e_mail_reader_enable_show_folder (E_MAIL_READER(pane));
	e_mail_reader_set_message (E_MAIL_READER (pane), uid);
	camel_message_info_free (info);
}

#if HAVE_CLUTTER
static ClutterActor *
build_histogram (GtkWidget *widget, CamelFolder *folder)
{
	gint week_time = 60 * 60 * 24 * 7;
	gint weeks[54];
	gint i;
	GPtrArray *uids;
	gint max = 1;
	ClutterActor *texture;
	cairo_t *cr;
	gfloat ratio;
	gint x = 0;
	time_t now = time(NULL);
	GtkStyle *style;
	GdkColor *color;

	for (i=0; i<54; i++)
		weeks[i] = 0;

	style = gtk_widget_get_style (GTK_WIDGET (widget));
	color = &style->mid[GTK_STATE_NORMAL];

	uids = camel_folder_get_uids (folder);
	camel_folder_summary_prepare_fetch_all (folder->summary, NULL);
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info = camel_folder_get_message_info (folder, uids->pdata[i]);
		if (info) {
			time_t dreceived = now - camel_message_info_date_received (info);
			gint week;

			week = (dreceived / week_time) - 1;
			if (week > 52)
				weeks[53]++;
			else
				weeks[week]++;

			camel_message_info_free (info);
		}
	}

	for (i=0; i< 53; i++) {
		if (weeks[i] > max)
			max = weeks[i];
	}

	ratio = 50.0/max;

	camel_folder_free_uids (folder, uids);

	texture = clutter_cairo_texture_new (200, 50);
	clutter_actor_set_size (texture, 200, 50);
	cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (texture));

	clutter_actor_show_all (texture);

	cairo_save (cr);
	cairo_new_path (cr);
	cairo_move_to (cr, x, 50 - (weeks[52] * ratio));

	cairo_set_source_rgba (cr, 0.3, 0.2, 0.4, 1.0);

	for (i=51; i>=0; i--) {
		x+=3;
		cairo_line_to (cr, x, 50 - (weeks[i]*ratio));

	}

	cairo_stroke (cr);
	cairo_restore (cr);

	cairo_save (cr);

	cairo_set_source_rgba (cr, 0.8, 0.5, 0.3, 1.0);
	cairo_arc (cr, x,  50 - (weeks[0] * ratio), 3, 0, 2*M_PI);

	cairo_fill (cr);
	cairo_restore (cr);

	cairo_destroy(cr);

	return texture;
}
#endif

static void
mail_notebook_view_set_folder (EMailReader *reader,
                               CamelFolder *folder,
                               const gchar *folder_uri)
{
	EMailNotebookViewPrivate *priv;
	GtkWidget *new_view;
#if HAVE_CLUTTER
	EMailTab *tab;
	ClutterActor *clone;
	ClutterTimeline *timeline;
#endif

	if (!folder_uri)
		return;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	new_view = g_hash_table_lookup (priv->views, folder_uri);
	if (new_view) {
		gint curr = emnv_get_page_num (E_MAIL_NOTEBOOK_VIEW (reader), new_view);
#if HAVE_CLUTTER
		EMailTab *tab;

		if (curr == e_mail_tab_picker_get_current_tab (priv->tab_picker))
			return;

		e_mail_tab_set_active (e_mail_tab_picker_get_tab (priv->tab_picker,
						e_mail_tab_picker_get_current_tab (priv->tab_picker)),
					FALSE);
#endif

		priv->current_view = (EMailView *)new_view;
#if HAVE_CLUTTER
		mnv_set_current_tab (E_MAIL_NOTEBOOK_VIEW(reader), curr);
#else
		gtk_notebook_set_current_page (priv->book, curr);
#endif

#if HAVE_CLUTTER
		tab = (EMailTab *)g_object_get_data ((GObject *)priv->current_view, "page");
		curr = e_mail_tab_picker_get_tab_no (priv->tab_picker, tab);
		e_mail_tab_picker_set_current_tab (priv->tab_picker, curr);
#endif
		return;
	}

	if (folder || folder_uri) {
		gint page;
		GtkWidget *list;

		if (priv->inited) {
			EMailView *old_view = priv->current_view;
			EShellView *shell_view;

			shell_view = e_mail_view_get_shell_view (E_MAIL_VIEW (reader));
			priv->current_view = e_mail_folder_pane_new (shell_view);
			gtk_widget_show ((GtkWidget *)priv->current_view);
			e_mail_view_set_previous_view (priv->current_view, old_view);
			page = gtk_notebook_append_page (
				priv->book, (GtkWidget *)priv->current_view,
				create_tab_label (
					E_MAIL_NOTEBOOK_VIEW (reader),
					priv->current_view,
					camel_folder_get_full_name (folder)));
#if HAVE_CLUTTER
			mnv_set_current_tab (E_MAIL_NOTEBOOK_VIEW(reader), page);
#else
			gtk_notebook_set_current_page (priv->book, page);
#endif

#if HAVE_CLUTTER
			e_mail_tab_set_active (
				e_mail_tab_picker_get_tab (
					priv->tab_picker,
					e_mail_tab_picker_get_current_tab (
						priv->tab_picker)),
				FALSE);

			tab = (EMailTab *)e_mail_tab_new_full (camel_folder_get_full_name(folder), NULL, 1);
			g_object_set_data ((GObject *)tab, "page", priv->current_view);
			g_object_set_data ((GObject *)priv->current_view, "page", tab);
			g_object_set_data ((GObject *)priv->current_view, "tab", tab);

			clutter_actor_show((ClutterActor *)tab);

			clone = build_histogram ((GtkWidget *)reader, folder);
			clutter_actor_set_reactive (clone, FALSE);
			clutter_actor_show (clone);

			e_mail_tab_set_preview_actor (tab, clone);
			e_mail_tab_set_can_close (tab, TRUE);
			e_mail_tab_set_preview_mode (
				tab, e_mail_tab_picker_get_preview_mode (
				priv->tab_picker));

			e_mail_tab_picker_add_tab (priv->tab_picker, tab, -1);
			page = e_mail_tab_picker_get_tab_no (priv->tab_picker, tab);
			e_mail_tab_picker_set_current_tab (priv->tab_picker, page);

			e_mail_tab_enable_drag (tab, TRUE);
			g_object_ref (tab);
			timeline = clutter_timeline_new (150);
			g_signal_connect (
				timeline, "new-frame",
				G_CALLBACK (mnv_tab_anim_frame_cb), tab);
			g_signal_connect (
				timeline, "completed",
				G_CALLBACK (mnv_tab_anim_complete_cb), tab);
			clutter_timeline_start (timeline);
#endif
		} else {
			priv->inited = TRUE;
			gtk_notebook_set_tab_label (priv->book, (GtkWidget *)priv->current_view,
						create_tab_label (E_MAIL_NOTEBOOK_VIEW(reader),
								  priv->current_view,
								  camel_folder_get_full_name (folder)));

#if HAVE_CLUTTER
			tab = e_mail_tab_picker_get_tab(priv->tab_picker,
						e_mail_tab_picker_get_current_tab (priv->tab_picker));
			g_object_set_data ((GObject *)tab, "page", priv->current_view);
			g_object_set_data ((GObject *)priv->current_view, "page", tab);
			g_object_set_data ((GObject *)priv->current_view, "tab", tab);

			e_mail_tab_set_text (tab, camel_folder_get_full_name(folder));
			clone = build_histogram ((GtkWidget *)reader, folder);
			clutter_actor_set_reactive (clone, FALSE);
			clutter_actor_show (clone);
			e_mail_tab_set_preview_actor (tab, clone);
#endif
		}

		list = e_mail_reader_get_message_list (E_MAIL_READER(priv->current_view));
#if HAVE_CLUTTER
		g_signal_connect (tab , "closed",
				   G_CALLBACK (mnv_tab_closed), reader);
		g_object_set_data ((GObject *)priv->current_view, "stage", priv->stage);
		g_object_set_data ((GObject *)list, "stage", priv->stage);
		g_object_set_data ((GObject *)list, "actor", priv->actor);

#endif
		e_mail_reader_set_folder (E_MAIL_READER(priv->current_view), folder, folder_uri);
		g_hash_table_insert (priv->views, g_strdup(folder_uri), priv->current_view);
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

static void
mail_notebook_view_show_search_bar (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	if (priv->current_view == NULL)
		return;

	reader = E_MAIL_READER (priv->current_view);

	e_mail_reader_show_search_bar (reader);
}

static gboolean
mail_notebook_view_enable_show_folder (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;

	if (!priv->current_view)
		return FALSE;

	return e_mail_reader_get_enable_show_folder (E_MAIL_READER(priv->current_view));
}

static guint
mail_notebook_view_open_selected_mail (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (reader);

	if (priv->current_view == NULL)
		return 0;

	reader = E_MAIL_READER (priv->current_view);

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
		PROP_GROUP_BY_THREADS,
		"group-by-threads");
}

static void
e_mail_notebook_view_reader_init (EMailReaderInterface *interface)
{
	interface->get_action_group = mail_notebook_view_get_action_group;
	interface->get_formatter = mail_notebook_view_get_formatter;
	interface->get_hide_deleted = mail_notebook_view_get_hide_deleted;
	interface->get_message_list = mail_notebook_view_get_message_list;
	interface->get_popup_menu = mail_notebook_view_get_popup_menu;
	interface->get_shell_backend = mail_notebook_view_get_shell_backend;
	interface->get_window = mail_notebook_view_get_window;
	interface->set_folder = mail_notebook_view_set_folder;
	interface->show_search_bar = mail_notebook_view_show_search_bar;
	interface->open_selected_mail = mail_notebook_view_open_selected_mail;
	interface->enable_show_folder = mail_notebook_view_enable_show_folder;
}

static void
e_mail_notebook_view_init (EMailNotebookView  *view)
{
	view->priv = E_MAIL_NOTEBOOK_VIEW_GET_PRIVATE (view);

	view->priv->inited = FALSE;
	view->priv->views = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
emnv_show_folder (EMailNotebookView *view, gpointer not_used)
{
	gint pos;
	EMailNotebookViewPrivate *priv = view->priv;

	pos = emnv_get_page_num (
		view, (GtkWidget *) E_MAIL_MESSAGE_PANE (
		priv->current_view)->parent_folder_view);

#if HAVE_CLUTTER
	e_mail_tab_picker_set_current_tab (priv->tab_picker, pos);
	mnv_set_current_tab (E_MAIL_NOTEBOOK_VIEW(view), pos);
#else
	gtk_notebook_set_current_page (priv->book, pos);
#endif

}

static void
emnv_show_prevtab (EMailNotebookView *view, gpointer not_used)
{
	gint pos;
	EMailNotebookViewPrivate *priv = view->priv;

	pos = emnv_get_page_num (
		view, (GtkWidget *) E_MAIL_MESSAGE_PANE (
		priv->current_view)->parent_folder_view);

#if HAVE_CLUTTER
	pos = e_mail_tab_picker_get_current_tab (priv->tab_picker);
	if (pos > 0) {
		e_mail_tab_picker_set_current_tab (priv->tab_picker, pos-1);
		mnv_set_current_tab (E_MAIL_NOTEBOOK_VIEW(view), pos-1);
	}
#else
	pos = gtk_notebook_get_current_page (priv->book);
	if (pos > 0 )
		gtk_notebook_set_current_page (priv->book, pos-1);
#endif

}

static void
emnv_show_nexttab (EMailNotebookView *view, gpointer not_used)
{
	gint pos;
	EMailNotebookViewPrivate *priv = view->priv;

#if HAVE_CLUTTER
	pos = e_mail_tab_picker_get_current_tab (priv->tab_picker);

	if (pos < (gtk_notebook_get_n_pages (priv->book)-1)) {
		e_mail_tab_picker_set_current_tab (priv->tab_picker, pos+1);
		mnv_set_current_tab (E_MAIL_NOTEBOOK_VIEW(view), pos+1);
	}
#else
	pos = gtk_notebook_get_current_page (priv->book);
	if (pos < (gtk_notebook_get_n_pages (priv->book)-1))
		gtk_notebook_set_current_page (priv->book, pos+1);
#endif

}

static void
emnv_close_tab (EMailNotebookView *view, gpointer not_used)
{
	EMailNotebookViewPrivate *priv = view->priv;

#if HAVE_CLUTTER
	mnv_tab_closed (g_object_get_data((GObject *)priv->current_view, "tab"),
			view);
#else
	tab_remove_gtk_cb (g_object_get_data((GObject *)priv->current_view, "close-button"),
				view);
#endif

}

GtkWidget *
e_mail_notebook_view_new (EShellView *shell_view)
{
	GtkWidget *widget;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	widget = g_object_new (
		E_TYPE_MAIL_NOTEBOOK_VIEW,
		"shell-view", shell_view, NULL);
	g_signal_connect (widget, "show-folder",
			G_CALLBACK (emnv_show_folder), widget);
	g_signal_connect (widget, "show-next-tab",
			G_CALLBACK (emnv_show_nexttab), widget);
	g_signal_connect (widget, "show-previous-tab",
			G_CALLBACK (emnv_show_prevtab), widget);
	g_signal_connect (widget, "close-tab",
			G_CALLBACK (emnv_close_tab), widget);

	return widget;
}
