/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <config.h>
#include <gnome.h>
#include <gdk/gdkx.h>

#include "e-util/e-canvas.h"
#include "e-util/e-util.h"
#include "widgets/e-text/e-text.h"

#include "e-note.h"
#include "e-bevel-button.h"
#include "e-bevel-button-util.h"

#define PARENT_TYPE GTK_TYPE_WINDOW

enum {
	E_NOTE_TEXT_CHANGED,
	E_NOTE_LAST_SIGNAL
};

static guint e_note_signals [E_NOTE_LAST_SIGNAL] = { 0 };

static GtkWindowClass *parent_class = NULL;

struct _ENotePrivate {
	GtkWidget *canvas;
	
	GnomeCanvasItem *frame;
	GnomeCanvasItem *rect;

	GnomeCanvasItem *text_item;
	
	GnomeCanvasItem *move_button;
	GnomeCanvasItem *close_button;
	GnomeCanvasItem *resize_button;
	
	/* Used for moving and resizing */
	gint press_x, press_y;
	gint resize_width, resize_height;
	gboolean in_drag;
};

static void
e_note_text_changed (ETextModel *model, gpointer data)
{
	gtk_signal_emit (GTK_OBJECT (data),
			 e_note_signals [E_NOTE_TEXT_CHANGED]);
}

static gint
e_note_resize_button_changed (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	ENote *note = E_NOTE (data);

	if (event->type == GDK_BUTTON_PRESS) {
		note->priv->press_x = event->x_root;
		note->priv->press_y = event->y_root;

		gdk_window_get_geometry (GTK_WIDGET (note)->window, NULL, NULL,
					 &note->priv->resize_width, &note->priv->resize_height, NULL);
		
		gdk_pointer_grab (widget->window,
				  FALSE,
				  (GDK_BUTTON1_MOTION_MASK |
				   GDK_POINTER_MOTION_HINT_MASK |
				   GDK_BUTTON_RELEASE_MASK),
				  NULL,
				  NULL,
				  GDK_CURRENT_TIME);
		note->priv->in_drag = TRUE;
		
	}
	else {
		if (note->priv->in_drag) {
			if (event->window != widget->window)
				return FALSE;
			
			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			note->priv->in_drag = FALSE;
		}
	}

	return TRUE;
}

static gint
e_note_resize_motion_event (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	GtkWidget *window = GTK_WIDGET (data);
	ENote *note = E_NOTE (data);
	gint new_x, new_y;
	gint width, height;

	if (note->priv->in_drag) {
		gdk_window_get_pointer (GDK_ROOT_PARENT (), &new_x, &new_y, NULL);

		width = note->priv->resize_width + new_x - note->priv->press_x;
		if (width < 60)
			width = 60;
		
		height = note->priv->resize_height + new_y - note->priv->press_y;
		if (height < 60)
			height = 60;

		gdk_window_resize (window->window, width, height);

		return TRUE;
	}

	return FALSE;
}

static gint
e_note_move_button_changed (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	ENote *note = E_NOTE (data);
	
	if (event->button != 1)
		return FALSE;

	if (event->type == GDK_BUTTON_PRESS) {
		gint root_x, root_y;

		gdk_window_get_origin (widget->window, &root_x, &root_y);
		note->priv->press_x = root_x - event->x_root;
		note->priv->press_y = root_y - event->y_root;
		
		gdk_pointer_grab (widget->window,
				  FALSE,
				  (GDK_BUTTON1_MOTION_MASK |
				   GDK_POINTER_MOTION_HINT_MASK |
				   GDK_BUTTON_RELEASE_MASK),
				  NULL,
				  NULL,
				  GDK_CURRENT_TIME);

		note->priv->in_drag = TRUE;
	}
	else {
		if (note->priv->in_drag) {
			if (event->window != widget->window)
				return FALSE;

			gdk_pointer_ungrab (GDK_CURRENT_TIME);
			note->priv->in_drag = FALSE;
		}
	}
	
	return TRUE;
}

static gint
e_note_move_motion_event (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
	gint new_x, new_y;
	ENote *note = E_NOTE (data);
	GtkWidget *window = GTK_WIDGET (data);

	if (note->priv->in_drag) {
		gdk_window_get_pointer (GDK_ROOT_PARENT (), &new_x, &new_y, NULL);

		new_x += note->priv->press_x;
		new_y += note->priv->press_y;

		gdk_window_move (window->window, new_x, new_y);
	}

	return TRUE;
}

static void
e_note_canvas_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	ENote *note;
	gdouble height;
	
	note = E_NOTE (data);

	gnome_canvas_item_set (note->priv->text_item,
			       "width", (gdouble) allocation->width - 10,
			       NULL);
	gtk_object_get (GTK_OBJECT (note->priv->text_item),
			"height", &height,
			NULL);
	height = MAX (height, allocation->height);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (note->priv->canvas), 0, 0,
					allocation->width, height);
	gnome_canvas_item_set (note->priv->frame,
			       "x2", (gdouble) allocation->width - 1,
			       "y2", (gdouble) allocation->height - 1,
			       NULL);
	gnome_canvas_item_set (note->priv->rect,
			       "x2", (gdouble) allocation->width - 1,
			       "y2", (gdouble) allocation->height - 1,
			       NULL);
	gnome_canvas_item_set (note->priv->move_button,
			       "width", (gdouble) allocation->width - 29,
			       NULL);
	gnome_canvas_item_set (note->priv->resize_button,
			       "x", (gdouble) allocation->width - 23,
			       "y", (gdouble) allocation->height - 23,
			       NULL);
	gnome_canvas_item_set (note->priv->close_button,
			       "x", (gdouble) allocation->width - 23,
			       NULL);
}

static void
e_note_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	gdk_window_set_decorations (widget->window, 0);
}

static void
e_note_class_init (ENoteClass *klass)
{
	GtkWidgetClass *widget_class;
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)klass;
	widget_class = (GtkWidgetClass *)klass;
	parent_class = gtk_type_class (PARENT_TYPE);
	
	widget_class->realize = e_note_realize;

	e_note_signals [E_NOTE_TEXT_CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (ENoteClass, text_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, e_note_signals, E_NOTE_LAST_SIGNAL);
}

static void
e_note_init (ENote *note)
{
	ENotePrivate *priv;
	GtkWidget *button;

	priv = g_new (ENotePrivate, 1);

	note->priv = priv;

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());
	
	priv->canvas = e_canvas_new ();

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();
	
	gtk_signal_connect (GTK_OBJECT (priv->canvas), "size_allocate",
			    GTK_SIGNAL_FUNC (e_note_canvas_size_allocate), note);
	gtk_widget_show (priv->canvas);
	gtk_container_add (GTK_CONTAINER (note), priv->canvas);

	priv->rect = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
					    gnome_canvas_rect_get_type (),
					    "x1", 0.0,
					    "y1", 0.0,
					    "x2", 100.0,
					    "y2", 100.0,
					    "fill_color_rgba", 0xf5ffa0ff,
					    NULL);
	priv->frame = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
					     gnome_canvas_rect_get_type (),
					     "x1", 0.0,
					     "y1", 0.0,
					     "x2", 100.0,
					     "y2", 100.0,
					     "outline_color", "black",
					     NULL);

	priv->text_item = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
						 e_text_get_type (),
						 "x", 5.0,
						 "y", 25.0,
						 "text", "",
						 "font_gdk", priv->canvas->style->font,
						 "fill_color", "black",
						 "anchor", GTK_ANCHOR_NW,
						 "clip", TRUE,
						 "editable", TRUE,
						 "line_wrap", TRUE,
						 "width", 150.0,
						 NULL);
	gtk_signal_connect (GTK_OBJECT (E_TEXT (priv->text_item)->model), "changed",
			    GTK_SIGNAL_FUNC (e_note_text_changed), note);
					
	button = e_bevel_button_new ();
	gtk_signal_connect (GTK_OBJECT (button), "button_press_event",
			    GTK_SIGNAL_FUNC (e_note_move_button_changed), note);
	gtk_signal_connect (GTK_OBJECT (button), "button_release_event",
			    GTK_SIGNAL_FUNC (e_note_move_button_changed), note);
	gtk_signal_connect (GTK_OBJECT (button), "motion_notify_event",
			    GTK_SIGNAL_FUNC (e_note_move_motion_event), note);
	gtk_widget_show (button);
	priv->move_button = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
						   gnome_canvas_widget_get_type (),
						   "widget", button,
						   "x", 3.0,
						   "y", 3.0,
						   "width", 20.0,
						   "height", 20.0,
						   NULL);
	button = e_bevel_button_new ();
	gtk_widget_show (button);
	priv->close_button = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
						    gnome_canvas_widget_get_type (),
						    "widget", button,
						    "x", 3.0,
						    "y", 3.0,
						    "width", 20.0,
						    "height", 20.0,
						    NULL);

	button = e_bevel_button_new ();
	gtk_signal_connect (GTK_OBJECT (button), "button_press_event",
			    GTK_SIGNAL_FUNC (e_note_resize_button_changed), note);
	gtk_signal_connect (GTK_OBJECT (button), "button_release_event",
			    GTK_SIGNAL_FUNC (e_note_resize_button_changed), note);
	gtk_signal_connect (GTK_OBJECT (button), "motion_notify_event",
			    GTK_SIGNAL_FUNC (e_note_resize_motion_event), note);	
	gtk_widget_show (button);
	priv->resize_button = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (priv->canvas)),
						     gnome_canvas_widget_get_type (),
						     "widget", button,
						     "x", 3.0,
						     "y", 3.0,
						     "width", 20.0,
						     "height", 20.0,
						     NULL);
}

void
e_note_set_text (ENote *note, gchar *text)
{
	g_return_if_fail (note != NULL);
	g_return_if_fail (E_IS_NOTE (note));
	g_return_if_fail (text != NULL);

	gnome_canvas_item_set (note->priv->text_item,
			       "text", text,
			       NULL);
}

gchar *
e_note_get_text (ENote *note)
{
	gchar *text;
	
	g_return_val_if_fail (note != NULL, NULL);
	g_return_val_if_fail (E_IS_NOTE (note), NULL);
	g_return_val_if_fail (text != NULL, NULL);

	gtk_object_get (GTK_OBJECT (note->priv->text_item),
			"text", &text,
			NULL);

	return text;
}


GtkWidget *
e_note_new (void)
{
	ENote *note;

	note = gtk_type_new (E_TYPE_NOTE);

	return GTK_WIDGET (note);
}

E_MAKE_TYPE (e_note, "ENote", ENote, e_note_class_init, e_note_init, PARENT_TYPE);
