/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ShortcutBar displays a vertical bar with a number of Groups, each of which
 * contains any number of icons. It is used on the left of the main application
 * window so users can easily access items such as folders and files.
 */

#include <string.h>
#include <gnome.h>

#include "e-shortcut-bar.h"
#include "e-clipped-label.h"
#include "e-vscrolled-bar.h"

/* Drag and Drop stuff. */
enum {
	TARGET_SHORTCUT
};
static GtkTargetEntry target_table[] = {
	{ "E-SHORTCUT",     0, TARGET_SHORTCUT }
};
static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

gboolean e_shortcut_bar_default_icon_loaded = FALSE;
GdkPixbuf *e_shortcut_bar_default_icon = NULL;
gchar *e_shortcut_bar_default_icon_filename = "gnome-folder.png";

static void e_shortcut_bar_class_init (EShortcutBarClass *class);
static void e_shortcut_bar_init (EShortcutBar *shortcut_bar);
static void e_shortcut_bar_destroy (GtkObject *object);
static void e_shortcut_bar_set_canvas_style (EShortcutBar *shortcut_bar,
					     GtkWidget *canvas);
static void e_shortcut_bar_item_selected (EIconBar *icon_bar,
					  GdkEvent *event,
					  gint item_num,
					  EShortcutBar *shortcut_bar);
static void e_shortcut_bar_item_dragged (EIconBar *icon_bar,
					 GdkEvent *event,
					 gint item_num,
					 EShortcutBar *shortcut_bar);
static void e_shortcut_bar_on_drag_data_get (GtkWidget          *widget,
					     GdkDragContext     *context,
					     GtkSelectionData   *selection_data,
					     guint               info,
					     guint               time,
					     EShortcutBar	*shortcut_bar);
static void e_shortcut_bar_on_drag_data_received  (GtkWidget          *widget,
						   GdkDragContext     *context,
						   gint                x,
						   gint                y,
						   GtkSelectionData   *data,
						   guint               info,
						   guint               time,
						   EShortcutBar	  *shortcut_bar);
static void e_shortcut_bar_on_drag_data_delete (GtkWidget          *widget,
						GdkDragContext     *context,
						EShortcutBar       *shortcut_bar);
static void e_shortcut_bar_on_drag_end (GtkWidget      *widget,
					GdkDragContext *context,
					EShortcutBar   *shortcut_bar);
static void e_shortcut_bar_stop_editing (GtkWidget *button,
					 EShortcutBar *shortcut_bar);
static GdkPixbuf* e_shortcut_bar_get_image_from_url (EShortcutBar *shortcut_bar,
						     const gchar *item_url);
static GdkPixbuf* e_shortcut_bar_load_image (const gchar *filename);


enum
{
  ITEM_SELECTED,
  ITEM_ADDED,
  ITEM_REMOVED,
  GROUP_ADDED,
  GROUP_REMOVED,
  LAST_SIGNAL
};

static guint e_shortcut_bar_signals[LAST_SIGNAL] = {0};

static EGroupBarClass *parent_class;


GtkType
e_shortcut_bar_get_type (void)
{
	static GtkType e_shortcut_bar_type = 0;

	if (!e_shortcut_bar_type){
		GtkTypeInfo e_shortcut_bar_info = {
			"EShortcutBar",
			sizeof (EShortcutBar),
			sizeof (EShortcutBarClass),
			(GtkClassInitFunc) e_shortcut_bar_class_init,
			(GtkObjectInitFunc) e_shortcut_bar_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		parent_class = gtk_type_class (e_group_bar_get_type ());
		e_shortcut_bar_type = gtk_type_unique (e_group_bar_get_type (),
						       &e_shortcut_bar_info);
	}

	return e_shortcut_bar_type;
}


static void
e_shortcut_bar_class_init (EShortcutBarClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	e_shortcut_bar_signals[ITEM_SELECTED] =
		gtk_signal_new ("item_selected",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutBarClass,
						   selected_item),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_GDK_EVENT,
				GTK_TYPE_INT, GTK_TYPE_INT);

	e_shortcut_bar_signals[ITEM_ADDED] =
		gtk_signal_new ("item_added",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutBarClass,
						   added_item),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	/* This is emitted just before the item is actually removed. */
	e_shortcut_bar_signals[ITEM_REMOVED] =
		gtk_signal_new ("item_removed",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutBarClass,
						   removed_item),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

	e_shortcut_bar_signals[GROUP_ADDED] =
		gtk_signal_new ("group_added",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutBarClass,
						   added_group),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	/* This is emitted just before the group is actually removed. */
	e_shortcut_bar_signals[GROUP_REMOVED] =
		gtk_signal_new ("group_removed",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShortcutBarClass,
						   removed_group),
				gtk_marshal_NONE__INT,
				GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, e_shortcut_bar_signals,
				      LAST_SIGNAL);

	/* Method override */
	object_class->destroy		= e_shortcut_bar_destroy;
}


static void
e_shortcut_bar_init (EShortcutBar *shortcut_bar)
{
	shortcut_bar->groups = g_array_new (FALSE, FALSE,
					    sizeof (EShortcutBarGroup));

	shortcut_bar->dragged_url = NULL;
	shortcut_bar->dragged_name = NULL;
}


GtkWidget *
e_shortcut_bar_new (void)
{
	GtkWidget *shortcut_bar;

	shortcut_bar = GTK_WIDGET (gtk_type_new (e_shortcut_bar_get_type ()));

	return shortcut_bar;
}


static void
e_shortcut_bar_destroy (GtkObject *object)
{
	EShortcutBar *shortcut_bar;

	shortcut_bar = E_SHORTCUT_BAR (object);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);

	g_array_free (shortcut_bar->groups, TRUE);
}


gint
e_shortcut_bar_add_group (EShortcutBar *shortcut_bar, const gchar *group_name)
{
	EShortcutBarGroup *group, tmp_group;
	gint group_num;
	GtkWidget *button, *label;

	g_return_val_if_fail (E_IS_SHORTCUT_BAR (shortcut_bar), -1);
	g_return_val_if_fail (group_name != NULL, -1);

	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	gtk_widget_push_visual (gdk_rgb_get_visual ());

	group_num = shortcut_bar->groups->len;
	g_array_append_val (shortcut_bar->groups, tmp_group);

	group = &g_array_index (shortcut_bar->groups,
				EShortcutBarGroup, group_num);

	group->vscrolled_bar = e_vscrolled_bar_new (NULL);
	gtk_widget_show (group->vscrolled_bar);
	gtk_signal_connect (
		GTK_OBJECT (E_VSCROLLED_BAR (group->vscrolled_bar)->up_button),
		"pressed", GTK_SIGNAL_FUNC (e_shortcut_bar_stop_editing), shortcut_bar);
	gtk_signal_connect (
		GTK_OBJECT (E_VSCROLLED_BAR (group->vscrolled_bar)->down_button),
		"pressed", GTK_SIGNAL_FUNC (e_shortcut_bar_stop_editing), shortcut_bar);

	group->icon_bar = e_icon_bar_new ();
	gtk_widget_show (group->icon_bar);
	gtk_container_add (GTK_CONTAINER (group->vscrolled_bar),
			   group->icon_bar);
	gtk_signal_connect (GTK_OBJECT (group->icon_bar), "item_selected",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_item_selected),
			    shortcut_bar);
	gtk_signal_connect (GTK_OBJECT (group->icon_bar), "item_dragged",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_item_dragged),
			    shortcut_bar);
	gtk_signal_connect (GTK_OBJECT (group->icon_bar), "drag_data_get",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_on_drag_data_get),
			    shortcut_bar);
	gtk_signal_connect (GTK_OBJECT (group->icon_bar), "drag_data_received",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_on_drag_data_received),
			    shortcut_bar);
	gtk_signal_connect (GTK_OBJECT (group->icon_bar), "drag_data_delete",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_on_drag_data_delete),
			    shortcut_bar);
	gtk_signal_connect (GTK_OBJECT (group->icon_bar), "drag_end",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_on_drag_end),
			    shortcut_bar);

	e_shortcut_bar_set_canvas_style (shortcut_bar, group->icon_bar);

	button = gtk_button_new ();
	label = e_clipped_label_new (group_name);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_widget_show (button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (e_shortcut_bar_stop_editing),
			    shortcut_bar);

	gtk_drag_dest_set (GTK_WIDGET (group->icon_bar),
			   GTK_DEST_DEFAULT_ALL,
			   target_table, n_targets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set (GTK_WIDGET (button),
			   GTK_DEST_DEFAULT_ALL,
			   target_table, n_targets,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_group_bar_add_group (E_GROUP_BAR (shortcut_bar),
			       group->vscrolled_bar, button, -1);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	gtk_signal_emit (GTK_OBJECT (shortcut_bar),
			 e_shortcut_bar_signals[GROUP_ADDED],
			 group_num);

	return group_num;
}


void
e_shortcut_bar_remove_group	(EShortcutBar	 *shortcut_bar,
				 gint		  group_num)
{
	gtk_signal_emit (GTK_OBJECT (shortcut_bar),
			 e_shortcut_bar_signals[GROUP_REMOVED],
			 group_num);

	e_group_bar_remove_group (E_GROUP_BAR (shortcut_bar), group_num);
	g_array_remove_index (shortcut_bar->groups, group_num);
}


gint
e_shortcut_bar_add_item (EShortcutBar *shortcut_bar, gint group_num,
			 const gchar *item_url, const gchar *item_name)
{
	EShortcutBarGroup *group;
	GdkPixbuf *image;
	gint item_num;

	g_return_val_if_fail (E_IS_SHORTCUT_BAR (shortcut_bar), -1);
	g_return_val_if_fail (group_num >= 0, -1);
	g_return_val_if_fail (group_num < shortcut_bar->groups->len, -1);
	g_return_val_if_fail (item_url != NULL, -1);
	g_return_val_if_fail (item_name != NULL, -1);

	image = e_shortcut_bar_get_image_from_url (shortcut_bar, item_url);
	group = &g_array_index (shortcut_bar->groups,
				EShortcutBarGroup, group_num);

	item_num = e_icon_bar_add_item (E_ICON_BAR (group->icon_bar),
					image, item_name, -1);
	gdk_pixbuf_unref (image);
	e_icon_bar_set_item_data_full (E_ICON_BAR (group->icon_bar), item_num,
				       g_strdup (item_url), g_free);

	gtk_signal_emit (GTK_OBJECT (shortcut_bar),
			 e_shortcut_bar_signals[ITEM_ADDED],
			 group_num, item_num);

	return item_num;
}


void
e_shortcut_bar_remove_item	(EShortcutBar	 *shortcut_bar,
				 gint		  group_num,
				 gint		  item_num)
{
	EShortcutBarGroup *group;

	g_return_if_fail (E_IS_SHORTCUT_BAR (shortcut_bar));
	g_return_if_fail (group_num >= 0);
	g_return_if_fail (group_num < shortcut_bar->groups->len);

	group = &g_array_index (shortcut_bar->groups,
				EShortcutBarGroup, group_num);

	gtk_signal_emit (GTK_OBJECT (shortcut_bar),
			 e_shortcut_bar_signals[ITEM_REMOVED],
			 group_num, item_num);

	e_icon_bar_remove_item (E_ICON_BAR (group->icon_bar), item_num);
}


static void
e_shortcut_bar_set_canvas_style (EShortcutBar *shortcut_bar,
				 GtkWidget *canvas)
{
        GtkRcStyle *rc_style;

        rc_style = gtk_rc_style_new ();

        rc_style->color_flags[GTK_STATE_NORMAL] = GTK_RC_FG | GTK_RC_BG;
        rc_style->fg[GTK_STATE_NORMAL].red   = 65535;
        rc_style->fg[GTK_STATE_NORMAL].green = 65535;
        rc_style->fg[GTK_STATE_NORMAL].blue  = 65535;

        rc_style->bg[GTK_STATE_NORMAL].red   = 32512;
        rc_style->bg[GTK_STATE_NORMAL].green = 32512;
        rc_style->bg[GTK_STATE_NORMAL].blue  = 32512;

        gtk_widget_modify_style (GTK_WIDGET (canvas), rc_style);
        gtk_rc_style_unref (rc_style);
}


void
e_shortcut_bar_set_view_type (EShortcutBar *shortcut_bar,
			      gint group_num,
			      EIconBarViewType view_type)
{
	EShortcutBarGroup *group;

	g_return_if_fail (E_IS_SHORTCUT_BAR (shortcut_bar));
	g_return_if_fail (group_num >= 0);
	g_return_if_fail (group_num < shortcut_bar->groups->len);

	group = &g_array_index (shortcut_bar->groups,
				EShortcutBarGroup, group_num);

	e_icon_bar_set_view_type (E_ICON_BAR (group->icon_bar), view_type);
}


static void
e_shortcut_bar_item_selected (EIconBar *icon_bar,
			      GdkEvent *event,
			      gint item_num,
			      EShortcutBar *shortcut_bar)
{
	gint group_num;

	group_num = e_group_bar_get_group_num (E_GROUP_BAR (shortcut_bar),
					       GTK_WIDGET (icon_bar)->parent);

	gtk_signal_emit (GTK_OBJECT (shortcut_bar),
			 e_shortcut_bar_signals[ITEM_SELECTED],
			 event, group_num, item_num);
}


static void
e_shortcut_bar_item_dragged (EIconBar *icon_bar,
			     GdkEvent *event,
			     gint item_num,
			     EShortcutBar *shortcut_bar)
{
	GtkTargetList *target_list;
	gint group_num;

	group_num = e_group_bar_get_group_num (E_GROUP_BAR (shortcut_bar),
					       GTK_WIDGET (icon_bar)->parent);

	shortcut_bar->dragged_url = g_strdup (e_icon_bar_get_item_data (icon_bar, item_num));
	shortcut_bar->dragged_name = e_icon_bar_get_item_text (icon_bar, item_num);

	target_list = gtk_target_list_new (target_table, n_targets);
	gtk_drag_begin (GTK_WIDGET (icon_bar), target_list,
			GDK_ACTION_COPY | GDK_ACTION_MOVE,
			1, event);
	gtk_target_list_unref (target_list);
}


static void
e_shortcut_bar_on_drag_data_get (GtkWidget          *widget,
				 GdkDragContext     *context,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time,
				 EShortcutBar	    *shortcut_bar)
{
	gchar *data;

	if (info == TARGET_SHORTCUT) {
		data = g_strdup_printf ("%s%c%s", shortcut_bar->dragged_name,
					'\0', shortcut_bar->dragged_url);
		gtk_selection_data_set (selection_data,	selection_data->target,
					8, data,
					strlen (shortcut_bar->dragged_name)
					+ strlen (shortcut_bar->dragged_url)
					+ 2);
		g_free (data);
	}
}


static void  
e_shortcut_bar_on_drag_data_received  (GtkWidget          *widget,
				       GdkDragContext     *context,
				       gint                x,
				       gint                y,
				       GtkSelectionData   *data,
				       guint               info,
				       guint               time,
				       EShortcutBar	  *shortcut_bar)
{
	EShortcutBarGroup *group;
	gchar *item_name, *item_url;
	EIconBar *icon_bar;
	GdkPixbuf *image;
	gint group_num, item_num;

	icon_bar = E_ICON_BAR (widget);

	if ((data->length >= 0) && (data->format == 8)
	    && icon_bar->dragging_before_item_num != -1) {
		item_name = data->data;
		item_url = item_name + strlen (item_name) + 1;

		image = e_shortcut_bar_get_image_from_url (shortcut_bar,
							   item_url);

		group_num = e_group_bar_get_group_num (E_GROUP_BAR (shortcut_bar),
						       GTK_WIDGET (icon_bar)->parent);
		group = &g_array_index (shortcut_bar->groups,
					EShortcutBarGroup, group_num);

		item_num = e_icon_bar_add_item (E_ICON_BAR (group->icon_bar), image, item_name, icon_bar->dragging_before_item_num);
		e_icon_bar_set_item_data_full (E_ICON_BAR (group->icon_bar),
					       item_num, g_strdup (item_url),
					       g_free);

		gtk_signal_emit (GTK_OBJECT (shortcut_bar),
				 e_shortcut_bar_signals[ITEM_ADDED],
				 group_num, item_num);

		gtk_drag_finish (context, TRUE, TRUE, time);
		return;
	}
  
	gtk_drag_finish (context, FALSE, FALSE, time);
}


static void  
e_shortcut_bar_on_drag_data_delete (GtkWidget          *widget,
				    GdkDragContext     *context,
				    EShortcutBar       *shortcut_bar)
{
	EIconBar *icon_bar;
	gint group_num;

	icon_bar = E_ICON_BAR (widget);

	group_num = e_group_bar_get_group_num (E_GROUP_BAR (shortcut_bar),
					       widget->parent);

	e_shortcut_bar_remove_item (shortcut_bar, group_num,
				    icon_bar->dragged_item_num);
}


static void
e_shortcut_bar_on_drag_end (GtkWidget      *widget,
			    GdkDragContext *context,
			    EShortcutBar   *shortcut_bar)
{
	g_free (shortcut_bar->dragged_name);
	shortcut_bar->dragged_name = NULL;

	g_free (shortcut_bar->dragged_url);
	shortcut_bar->dragged_url = NULL;
}


void
e_shortcut_bar_start_editing_item (EShortcutBar *shortcut_bar,
				   gint group_num,
				   gint item_num)
{
	EShortcutBarGroup *group;

	g_return_if_fail (E_IS_SHORTCUT_BAR (shortcut_bar));
	g_return_if_fail (group_num >= 0);
	g_return_if_fail (group_num < shortcut_bar->groups->len);

	group = &g_array_index (shortcut_bar->groups,
				EShortcutBarGroup, group_num);

	e_icon_bar_start_editing_item (E_ICON_BAR (group->icon_bar), item_num);
}


/* We stop editing any item when a scroll button is pressed. */
static void
e_shortcut_bar_stop_editing (GtkWidget *button,
			     EShortcutBar *shortcut_bar)
{
	EShortcutBarGroup *group;
	gint group_num;

	for (group_num = 0;
	     group_num < shortcut_bar->groups->len;
	     group_num++) {
		group = &g_array_index (shortcut_bar->groups,
					EShortcutBarGroup, group_num);
		e_icon_bar_stop_editing_item (E_ICON_BAR (group->icon_bar),
					      TRUE);
	}
}


/* Sets the callback which is called to return the icon to use for a particular
   URL. */
void
e_shortcut_bar_set_icon_callback (EShortcutBar *shortcut_bar,
				  EShortcutBarIconCallback cb,
				  gpointer data)
{
	shortcut_bar->icon_callback = cb;
	shortcut_bar->icon_callback_data = data;
}


static GdkPixbuf *
e_shortcut_bar_get_image_from_url (EShortcutBar *shortcut_bar,
				   const gchar *item_url)
{
	GdkPixbuf *icon = NULL;

	if (shortcut_bar->icon_callback)
		icon = (*shortcut_bar->icon_callback) (shortcut_bar,
						       item_url,
						       shortcut_bar->icon_callback_data);

	if (!icon) {
		if (!e_shortcut_bar_default_icon_loaded) {
			e_shortcut_bar_default_icon_loaded = TRUE;
			e_shortcut_bar_default_icon = e_shortcut_bar_load_image (e_shortcut_bar_default_icon_filename);
		}
		icon = e_shortcut_bar_default_icon;
		/* ref the default icon each time we return it */
		gdk_pixbuf_ref (icon);
	}

	return icon;
}


static GdkPixbuf *
e_shortcut_bar_load_image (const gchar *filename)
{
	gchar *pathname;
	GdkPixbuf *image = NULL;

	pathname = gnome_pixmap_file (filename);
	if (pathname)
		image = gdk_pixbuf_new_from_file (pathname);
	else
		g_warning ("Couldn't find pixmap: %s", filename);

	return image;
}
