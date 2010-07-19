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

struct _EMailNotebookViewPrivate {
	GtkNotebook *book;
	EMailView *current_view;
	GHashTable *views;

#if HAVE_CLUTTER	
	EMailTabPicker *tab_picker;
	GtkWidget *embed;
	GtkWidget *actor;
	GtkWidget *stage;
#endif	
};

enum {
	PROP_0,
	PROP_GROUP_BY_THREADS,
};

#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")

static EMailViewClass *parent_class;
static GType mail_notebook_view_type;

#if HAVE_CLUTTER
struct _anim_data {
	EMailNotebookView *view;
	int page;
};

static void
start_tab_switch_cb (ClutterAnimation *animation,
                     struct _anim_data *data)
{
	gtk_notebook_set_current_page (data->view->priv->book, data->page);
	animation = clutter_actor_animate ((ClutterActor *)data->view->priv->actor, CLUTTER_EASE_IN_SINE, 75,
       	                 	 	   "opacity", 255,
       	                  		   NULL);
	
}


static void
mnv_set_current_tab (EMailNotebookView *view,
		     int page)
{
	ClutterAnimation *animation;
	struct _anim_data *data = g_new0 (struct _anim_data, 1);

	data->view = view;
	data->page = page;

	animation = clutter_actor_animate ((ClutterActor *)view->priv->actor, CLUTTER_EASE_OUT_SINE, 75,
       	                 	 	   "opacity", 0,
       	                  		   NULL);
	g_signal_connect_after (animation, "completed", G_CALLBACK(start_tab_switch_cb), data);
}
#endif

static void
mail_notebook_view_init (EMailNotebookView  *shell)
{
	shell->priv = g_new0(EMailNotebookViewPrivate, 1);

	shell->priv->views = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
e_mail_notebook_view_finalize (GObject *object)
{
	/* EMailNotebookView *shell = (EMailNotebookView *)object; */

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static int
emnv_get_page_num (EMailNotebookView *view,
		   GtkWidget *widget)
{
	EMailNotebookViewPrivate *priv = view->priv;
	int i, n;
	
	n = gtk_notebook_get_n_pages (priv->book);

	for (i=0; i<n; i++) {
		GtkWidget *curr = gtk_notebook_get_nth_page (priv->book, i);
		if (curr == widget)
			return i;
	}

	g_warn_if_reached ();

	return 0;
}

static void
mnv_page_changed (GtkNotebook *book, GtkNotebookPage *page,
		  guint page_num, EMailNotebookView *view)
{
	EMailView *mview = (EMailView *)gtk_notebook_get_nth_page (book, page_num);

	view->priv->current_view = mview;
	/* For EMailReader related changes to EShellView*/
	g_signal_emit_by_name (view, "changed");
	g_signal_emit_by_name (view, "folder-loaded");
	
	/* For EMailShellContent related changes */
	g_signal_emit_by_name (view, "view-changed");

}

#if HAVE_CLUTTER
static void
fix_tab_picker_width (GtkWidget *widget, GtkAllocation *allocation, ClutterActor *actor)
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
	gtk_widget_set_size_request (embed, -1, (int) clutter_actor_get_height(actor));
}

static void
chooser_clicked_cb (EMailTabPicker *picker, EMailNotebookView *view)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (view)->priv;

	gboolean preview_mode = !e_mail_tab_picker_get_preview_mode (priv->tab_picker);
	e_mail_tab_picker_set_preview_mode (priv->tab_picker , preview_mode);
}

static void
tab_picker_preview_mode_notify (EMailTabPicker *picker,
                                GParamSpec   *pspec,
                                EMailNotebookView *view)
{
	GList *tabs, *t;
	gboolean preview_mode = e_mail_tab_picker_get_preview_mode (picker);

	clutter_actor_set_name (CLUTTER_ACTOR (picker),
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
	const char *folder_uri = e_mail_reader_get_folder_uri (E_MAIL_READER(page));
	
	e_mail_tab_picker_remove_tab (data->view->priv->tab_picker, data->tab); 

	g_hash_table_remove (data->view->priv->views, folder_uri);
	gtk_notebook_remove_page (data->view->priv->book, 
			gtk_notebook_page_num (data->view->priv->book, (GtkWidget *)page));

}

static void
mnv_tab_closed (EMailTab *tab, EMailNotebookView *view)
{
	EMailNotebookViewPrivate *priv = view->priv;
	int page, cur;
	gboolean select = FALSE;
	ClutterTimeline *timeline;
	struct _tab_data *data = g_new0 (struct _tab_data, 1);

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
  	g_signal_connect (timeline, "new-frame",
                    G_CALLBACK (mnv_tab_anim_frame_cb), tab);
  	g_signal_connect (timeline, "completed",
                    G_CALLBACK (mnv_tab_closed_cb), data);
  	clutter_timeline_start (timeline);
	
}

static void
tab_activated_cb (EMailTabPicker *picker,
		  EMailTab	 *tab,
		  EMailNotebookView *view)
{
	EMailView *page = g_object_get_data ((GObject *)tab, "page");
	int num = emnv_get_page_num (view, (GtkWidget *)page);

	mnv_set_current_tab (view, num);
}
#endif

static void
tab_remove_gtk_called (GtkWidget *button,
		       EMailNotebookView *view)
{

}

static GtkWidget *
create_tab_label (EMailNotebookView *view, 
		  EMailView *page,
		  const char *str)
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
	gtk_button_set_image (GTK_BUTTON (widget), gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show_all (widget);
	gtk_box_pack_end (GTK_BOX(container), widget, FALSE, FALSE, 0);
	g_object_set_data ((GObject *)widget, "page", page);
	g_signal_connect (widget, "clicked", G_CALLBACK (tab_remove_gtk_called), view);

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
fix_clutter_embed_width (GtkWidget *widget, GtkAllocation *allocation, ClutterActor *actor)
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

	g_signal_connect (paned, "size-allocate", G_CALLBACK(fix_clutter_embed_width), actor);
	clutter_actor_show(stage);
	
	return embed;
}

#endif

static void
mail_notebook_view_constructed (GObject *object)
{
	GtkWidget *widget, *container;
	EMailNotebookViewPrivate *priv;
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
	g_signal_connect (priv->tab_picker, "tab-activated",
		    G_CALLBACK (tab_activated_cb), object);
  	g_signal_connect (priv->tab_picker, "chooser-clicked",
                    G_CALLBACK (chooser_clicked_cb), object);
	g_signal_connect (priv->tab_picker, "notify::preview-mode",
                    G_CALLBACK (tab_picker_preview_mode_notify), object);
	g_signal_connect (priv->tab_picker, "notify::height", 
		    G_CALLBACK(fix_height_cb), widget);

	clutter_container_add_actor ((ClutterContainer *)stage, (ClutterActor *)priv->tab_picker);

	g_object_set_data ((GObject *)priv->tab_picker, "embed", widget);
	g_object_set_data ((GObject *)priv->tab_picker, "stage", stage);

	g_signal_connect (object, "size-allocate", 
		    G_CALLBACK(fix_tab_picker_width), priv->tab_picker);

	clutter_actor_set_height (stage, clutter_actor_get_height((ClutterActor *)priv->tab_picker));
	gtk_widget_set_size_request (widget, -1, (int) clutter_actor_get_height((ClutterActor *)priv->tab_picker));

	tab = (EMailTab *) e_mail_tab_new_full ("", NULL, 1);
	clone = e_mail_tab_new_full ("", NULL, 200);
	
	e_mail_tab_set_can_close ((EMailTab *)clone, FALSE);
	clutter_actor_set_reactive (clone, FALSE);
	clutter_actor_show (clone);

	e_mail_tab_set_preview_actor ((EMailTab *)tab, clone);
	e_mail_tab_set_can_close (tab, TRUE);
	e_mail_tab_enable_drag (tab, TRUE);

	e_mail_tab_picker_add_tab (priv->tab_picker, tab, -1);
	clutter_actor_show((ClutterActor *)tab);
	e_mail_tab_picker_set_current_tab (priv->tab_picker, 0);

	g_object_ref (tab);
      	timeline = clutter_timeline_new (150);
	g_signal_connect (timeline, "new-frame",
       	            G_CALLBACK (mnv_tab_anim_frame_cb), tab);
      	g_signal_connect (timeline, "completed",
       	            G_CALLBACK (mnv_tab_anim_complete_cb), tab);
	clutter_timeline_start (timeline);
#else

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
	g_signal_connect (widget, "switch-page", 
		    G_CALLBACK(mnv_page_changed), object);

	priv->current_view = (EMailView *)e_mail_folder_pane_new (E_MAIL_VIEW(object)->content);
	e_mail_paned_view_set_preview_visible ((EMailPanedView *)priv->current_view, FALSE);
	gtk_widget_show ((GtkWidget *)priv->current_view);

	gtk_notebook_append_page (priv->book, (GtkWidget *)priv->current_view, 
			create_tab_label (E_MAIL_NOTEBOOK_VIEW(object),
				priv->current_view,
				_("Please select a folder")));
	
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
mail_notebook_view_class_init (EMailViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->constructed = mail_notebook_view_constructed;
	object_class->set_property = mail_notebook_view_set_property;
	object_class->get_property = mail_notebook_view_get_property;
	
	object_class->finalize = e_mail_notebook_view_finalize;

	klass->get_searchbar = e_mail_notebook_view_get_searchbar;
	klass->set_search_strings = e_mail_notebook_view_set_search_strings;
	klass->get_view_instance = e_mail_notebook_view_get_view_instance;
	klass->update_view_instance = e_mail_notebook_view_update_view_instance;
	klass->set_orientation = e_mail_notebook_view_set_orientation;
	klass->get_orientation = e_mail_notebook_view_get_orientation;
	klass->set_show_deleted = e_mail_notebook_view_set_show_deleted;
	klass->get_show_deleted = e_mail_notebook_view_get_show_deleted;
	klass->set_preview_visible = e_mail_notebook_view_set_preview_visible;
	klass->get_preview_visible = e_mail_notebook_view_get_preview_visible;

	/* Inherited from EMailReader */
	g_object_class_override_property (
		object_class,
		PROP_GROUP_BY_THREADS,
		"group-by-threads");
/*
	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			"Preview is Visible",
			"Whether the preview pane is visible",
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			"Show Deleted",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_override_property (
		object_class, PROP_ORIENTATION, "orientation"); */	
}

GtkWidget *
e_mail_notebook_view_new (EShellContent *content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (content), NULL);

	return g_object_new (
		E_MAIL_NOTEBOOK_VIEW_TYPE,
		"shell-content", content, NULL);
}

static GtkActionGroup *
mail_notebook_view_get_action_group (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_MAIL_VIEW (reader)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return E_SHELL_WINDOW_ACTION_GROUP_MAIL (shell_window);	
/*	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_action_group (E_MAIL_READER(priv->current_view));*/
}

static EMFormatHTML *
mail_notebook_view_get_formatter (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_formatter (E_MAIL_READER(priv->current_view));
}

static gboolean
mail_notebook_view_get_hide_deleted (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return FALSE;

	return e_mail_reader_get_hide_deleted (E_MAIL_READER(priv->current_view));
}

static GtkWidget *
mail_notebook_view_get_message_list (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_message_list (E_MAIL_READER(priv->current_view));	
}

static GtkMenu *
mail_notebook_view_get_popup_menu (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_popup_menu (E_MAIL_READER(priv->current_view));	
}

static EShellBackend *
mail_notebook_view_get_shell_backend (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellView *shell_view;

	shell_content = E_MAIL_VIEW (reader)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);

	return e_shell_view_get_shell_backend (shell_view);
}

static GtkWindow *
mail_notebook_view_get_window (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_MAIL_VIEW (reader)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return GTK_WINDOW (shell_window);
}

static void
reconnect_changed_event (EMailReader *child, EMailReader *parent)
{
	g_signal_emit_by_name (parent, "changed");
}

static void
reconnect_folder_loaded_event (EMailReader *child, EMailReader *parent)
{
	g_signal_emit_by_name (parent, "folder-loaded");
}

static void
mail_netbook_view_open_mail (EMailView *view, const char *uid, EMailNotebookView *nview)
{
	const gchar *folder_uri;
	CamelFolder *folder;	
	GtkWidget *pane;
	int page;
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (nview)->priv;
	CamelMessageInfo *info;
#if HAVE_CLUTTER
	EMailTab *tab;
	ClutterActor *clone;
	ClutterTimeline *timeline;

	e_mail_tab_set_active (e_mail_tab_picker_get_tab (priv->tab_picker, 
						e_mail_tab_picker_get_current_tab (priv->tab_picker)),
				FALSE);
#endif	
	pane = e_mail_message_pane_new (E_MAIL_VIEW(nview)->content);
	gtk_widget_show (pane);

	folder = e_mail_reader_get_folder (E_MAIL_READER(view));
	folder_uri = e_mail_reader_get_folder_uri (E_MAIL_READER(view));

	page = gtk_notebook_append_page (priv->book, pane, 
				create_tab_label (nview,
				priv->current_view,
				_("Mail")));

#if HAVE_CLUTTER
	mnv_set_current_tab (nview, page);
#else	
	gtk_notebook_set_current_page (priv->book, page);
#endif
	info = camel_folder_get_message_info (folder, uid);

#if HAVE_CLUTTER			
	tab = (EMailTab *)e_mail_tab_new_full (camel_message_info_subject(info), NULL, 1);
	g_object_set_data ((GObject *)tab, "page", pane);
	clutter_actor_show((ClutterActor *)tab);

	clone = e_mail_tab_new_full (camel_message_info_subject(info), NULL, 200);
	clutter_actor_set_reactive (clone, FALSE);		
	clutter_actor_show (clone);

	e_mail_tab_set_preview_actor (tab, clone);
	e_mail_tab_set_can_close (tab, TRUE);
	e_mail_tab_enable_drag (tab, TRUE);
	e_mail_tab_picker_add_tab (priv->tab_picker, tab, -1);
	
	page = e_mail_tab_picker_get_tab_no (priv->tab_picker, tab);
	e_mail_tab_picker_set_current_tab (priv->tab_picker, page);

	g_signal_connect (tab , "closed", 
			  G_CALLBACK (mnv_tab_closed), nview);

	g_object_ref (tab);
	timeline = clutter_timeline_new (150);
	g_signal_connect (timeline, "new-frame",
       	            G_CALLBACK (mnv_tab_anim_frame_cb), tab);
	g_signal_connect (timeline, "completed",
       	            G_CALLBACK (mnv_tab_anim_complete_cb), tab);
	clutter_timeline_start (timeline);
#endif

	g_signal_connect ( E_MAIL_READER(pane), "changed",
			   G_CALLBACK (reconnect_changed_event),
			   nview);
	g_signal_connect ( E_MAIL_READER (pane), "folder-loaded",
			   G_CALLBACK (reconnect_folder_loaded_event),
			   nview);
	e_mail_reader_set_folder (
		E_MAIL_READER (pane), folder, folder_uri);
	e_mail_reader_set_group_by_threads (
		E_MAIL_READER (pane),
		e_mail_reader_get_group_by_threads (E_MAIL_READER(view))); 

	e_mail_reader_set_message (E_MAIL_READER (pane), uid);
	camel_message_info_free (info);
}

static void
mail_notebook_view_set_folder (EMailReader *reader,
                               CamelFolder *folder,
                               const gchar *folder_uri)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	GtkWidget *new_view;
#if HAVE_CLUTTER	
	EMailTab *tab;
	ClutterActor *clone;
	ClutterTimeline *timeline;
#endif

	if (!folder_uri)
		return;

	new_view = g_hash_table_lookup (priv->views, folder_uri);
	if (new_view) {
		int curr = emnv_get_page_num (E_MAIL_NOTEBOOK_VIEW (reader), new_view);
#if HAVE_CLUTTER		
		EMailTab *tab;

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
		int page;
		
		if (g_hash_table_size (priv->views) != 0) {
			priv->current_view = (EMailView *)e_mail_folder_pane_new (E_MAIL_VIEW(reader)->content);
			gtk_widget_show ((GtkWidget *)priv->current_view);
			page = gtk_notebook_append_page (priv->book, (GtkWidget *)priv->current_view, 
						create_tab_label (E_MAIL_NOTEBOOK_VIEW(reader),
							    	  priv->current_view,
								  camel_folder_get_full_name (folder)));
#if HAVE_CLUTTER		
			mnv_set_current_tab (E_MAIL_NOTEBOOK_VIEW(reader), page);
#else
				gtk_notebook_set_current_page (priv->book, page);
#endif

#if HAVE_CLUTTER
			e_mail_tab_set_active (e_mail_tab_picker_get_tab (priv->tab_picker, 
						e_mail_tab_picker_get_current_tab (priv->tab_picker)),
						FALSE);
			
			tab = (EMailTab *)e_mail_tab_new_full (camel_folder_get_full_name(folder), NULL, 1);
			g_object_set_data ((GObject *)tab, "page", priv->current_view);
			g_object_set_data ((GObject *)priv->current_view, "page", tab);

			clone = e_mail_tab_new_full (camel_folder_get_full_name(folder), NULL, 1);
			clutter_actor_set_reactive (clone, FALSE);
			e_mail_tab_set_can_close ((EMailTab *)clone, FALSE);
			clutter_actor_show (clone);

			e_mail_tab_set_can_close (tab, TRUE);
			e_mail_tab_picker_add_tab (priv->tab_picker, tab, -1);
			e_mail_tab_set_preview_actor (tab, clone);
			e_mail_tab_enable_drag (tab, TRUE);
			clutter_actor_show((ClutterActor *)tab);

			page = e_mail_tab_picker_get_tab_no (priv->tab_picker, tab);
			e_mail_tab_picker_set_current_tab (priv->tab_picker, page);

			g_object_ref (tab);
      			timeline = clutter_timeline_new (150);
	      		g_signal_connect (timeline, "new-frame",
       	       	        	    G_CALLBACK (mnv_tab_anim_frame_cb), tab);
      			g_signal_connect (timeline, "completed",
       		        	    G_CALLBACK (mnv_tab_anim_complete_cb), tab);
      			clutter_timeline_start (timeline);
#endif		
		} else {
			gtk_notebook_set_tab_label (priv->book, (GtkWidget *)priv->current_view, 
						create_tab_label (E_MAIL_NOTEBOOK_VIEW(reader),
							    	  priv->current_view,
								  camel_folder_get_full_name (folder)));

#if HAVE_CLUTTER			
			tab = e_mail_tab_picker_get_tab(priv->tab_picker, 
						e_mail_tab_picker_get_current_tab (priv->tab_picker));
			g_object_set_data ((GObject *)tab, "page", priv->current_view);
			g_object_set_data ((GObject *)priv->current_view, "page", tab);

			e_mail_tab_set_text (tab, camel_folder_get_full_name(folder));
			clone = e_mail_tab_get_preview_actor (tab);
			e_mail_tab_set_text ((EMailTab *)clone, camel_folder_get_full_name(folder));
#endif			
		}

#if HAVE_CLUTTER		
		g_signal_connect (tab , "closed", 
				   G_CALLBACK (mnv_tab_closed), reader);
#endif		
		e_mail_reader_set_folder (E_MAIL_READER(priv->current_view), folder, folder_uri);
		g_hash_table_insert (priv->views, g_strdup(folder_uri), priv->current_view);
		g_signal_connect ( E_MAIL_READER(priv->current_view), "changed",
				   G_CALLBACK (reconnect_changed_event),
				   reader);
		g_signal_connect ( E_MAIL_READER (priv->current_view), "folder-loaded",
				   G_CALLBACK (reconnect_folder_loaded_event),
				   reader);
		g_signal_connect ( priv->current_view, "open-mail",
				   G_CALLBACK (mail_netbook_view_open_mail), reader);
	}
}

static void
mail_notebook_view_show_search_bar (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
		
	e_mail_reader_show_search_bar (E_MAIL_READER(priv->current_view));	
}

EShellSearchbar *
e_mail_notebook_view_get_searchbar (EMailView *view)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *widget;

	g_return_val_if_fail (
		E_IS_MAIL_NOTEBOOK_VIEW (view), NULL);

	shell_content = E_MAIL_VIEW (view)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);
	widget = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (widget);	
/*	
	if (!E_MAIL_NOTEBOOK_VIEW(view)->priv->current_view)
		return NULL;
	return e_mail_view_get_searchbar (E_MAIL_NOTEBOOK_VIEW(view)->priv->current_view); */
}

static void
mail_notebook_view_open_selected_mail (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return ;

	return e_mail_reader_open_selected_mail (E_MAIL_READER(priv->current_view));	
}

void
e_mail_notebook_view_set_search_strings (EMailView *view,
					 GSList *search_strings)
{
	e_mail_view_set_search_strings (E_MAIL_NOTEBOOK_VIEW (view)->priv->current_view, search_strings);
}

GalViewInstance *
e_mail_notebook_view_get_view_instance (EMailView *view)
{
	if (!E_MAIL_NOTEBOOK_VIEW(view)->priv->current_view)
		return NULL;

	return e_mail_view_get_view_instance (E_MAIL_NOTEBOOK_VIEW (view)->priv->current_view);
}

void
e_mail_notebook_view_update_view_instance (EMailView *view)
{
	e_mail_view_update_view_instance (E_MAIL_NOTEBOOK_VIEW (view)->priv->current_view);
}

static void
mail_notebook_view_reader_init (EMailReaderIface *iface)
{
	iface->get_action_group = mail_notebook_view_get_action_group;
	iface->get_formatter = mail_notebook_view_get_formatter;
	iface->get_hide_deleted = mail_notebook_view_get_hide_deleted;
	iface->get_message_list = mail_notebook_view_get_message_list;
	iface->get_popup_menu = mail_notebook_view_get_popup_menu;
	iface->get_shell_backend = mail_notebook_view_get_shell_backend;
	iface->get_window = mail_notebook_view_get_window;
	iface->set_folder = mail_notebook_view_set_folder;
	iface->show_search_bar = mail_notebook_view_show_search_bar;
	iface->open_selected_mail = mail_notebook_view_open_selected_mail;
}

GType
e_mail_notebook_view_get_type (void)
{
	return mail_notebook_view_type;
}

void
e_mail_notebook_view_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMailNotebookViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_notebook_view_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailNotebookView),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_notebook_view_init,
		NULL   /* value_table */
	};

	static const GInterfaceInfo reader_info = {
		(GInterfaceInitFunc) mail_notebook_view_reader_init,
		(GInterfaceFinalizeFunc) NULL,
		NULL  /* interface_data */
	};

	mail_notebook_view_type = g_type_module_register_type (
		type_module, E_MAIL_VIEW_TYPE,
		"EMailNotebookView", &type_info, 0);

	g_type_module_add_interface (
		type_module, mail_notebook_view_type,
		E_TYPE_MAIL_READER, &reader_info);
}

void
e_mail_notebook_view_set_show_deleted (EMailNotebookView *view,
                                       gboolean show_deleted)
{
	if (!view->priv->current_view)
		return;
	
	e_mail_view_set_show_deleted (view->priv->current_view, show_deleted);
}
gboolean
e_mail_notebook_view_get_show_deleted (EMailNotebookView *view)
{
	if (!view->priv->current_view)
		return FALSE;
	
	return e_mail_view_get_show_deleted (view->priv->current_view);
}
void
e_mail_notebook_view_set_preview_visible (EMailNotebookView *view,
                                          gboolean preview_visible)
{
	if (!view->priv->current_view)
		return ;

	e_mail_view_set_preview_visible (view->priv->current_view, preview_visible);
}
gboolean
e_mail_notebook_view_get_preview_visible (EMailNotebookView *view)
{
	if (!view->priv->current_view)
		return FALSE;
	
	return e_mail_view_get_preview_visible (view->priv->current_view);
}
void
e_mail_notebook_view_set_orientation (EMailNotebookView *view,
				   GtkOrientation orientation)
{
	if (!view->priv->current_view)
		return;
	
	e_mail_view_set_orientation (view->priv->current_view, orientation);
}
GtkOrientation 
e_mail_notebook_view_get_orientation (EMailNotebookView *view)
{
	if (!view->priv->current_view)
		return GTK_ORIENTATION_VERTICAL;
	
	return e_mail_view_get_orientation (view->priv->current_view);
}
