/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <config.h>
#include <gtk/gtkbutton.h>

#include "e-util/e-util.h"

#include "e-bevel-button.h"
#include "e-bevel-button-util.h"

#define PARENT_TYPE GTK_TYPE_BUTTON

static GtkButtonClass *parent_class = NULL;

struct _EBevelButtonPrivate {
	GdkColor base_color;
	GdkColor dark_color;
	GdkColor light_color;
	GdkGC *gc;
};

static void
e_bevel_button_paint (GtkWidget *widget, GdkRectangle *area)
{
	EBevelButton *button;

	button = E_BEVEL_BUTTON (widget);
	
	if (GTK_WIDGET_DRAWABLE (widget)) {
		gdk_window_set_back_pixmap (widget->window, NULL, TRUE);
		gdk_window_clear_area (widget->window, area->x, area->y, area->width, area->height);

		gdk_gc_set_foreground (button->priv->gc, &button->priv->base_color);
		gdk_draw_rectangle (widget->window,
				    button->priv->gc,
				    TRUE,
				    0, 0,
				    widget->allocation.width, widget->allocation.height);

		if (GTK_BUTTON (button)->in_button) {
			gdk_gc_set_foreground (button->priv->gc,
					       GTK_BUTTON (button)->button_down ?
					       &button->priv->dark_color :
					       &button->priv->light_color);
			gdk_draw_line (widget->window, button->priv->gc,
				       0, 0, 0, widget->allocation.height - 2);
			gdk_draw_line (widget->window, button->priv->gc,
				       0, 0, widget->allocation.width - 2, 0);

			gdk_gc_set_foreground (button->priv->gc,
					       GTK_BUTTON (button)->button_down ?
					       &button->priv->light_color :
					       &button->priv->dark_color);
			gdk_draw_line (widget->window, button->priv->gc,
				       widget->allocation.width - 1 , 1,
				       widget->allocation.width - 1, widget->allocation.height - 1);
			gdk_draw_line (widget->window, button->priv->gc,
				       1, widget->allocation.height - 1,
				       widget->allocation.width - 1, widget->allocation.height - 1);
		}
	}
}

static gint
e_bevel_button_expose (GtkWidget *widget, GdkEventExpose *event)
{
	  GtkBin *bin;
	  GdkEventExpose child_event;
	  
	  if (GTK_WIDGET_DRAWABLE (widget)) {
		  bin = GTK_BIN (widget);
		  
		  e_bevel_button_paint (widget, &event->area);
		  
		  child_event = *event;
		  if (bin->child && GTK_WIDGET_NO_WINDOW (bin->child) &&
		      gtk_widget_intersect (bin->child, &event->area, &child_event.area))
			  gtk_widget_event (bin->child, (GdkEvent*) &child_event);
	  }

	  return FALSE;
}

static void
e_bevel_button_draw (GtkWidget *widget, GdkRectangle *area)
{
	GdkRectangle child_area;
	GdkRectangle tmp_area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_BEVEL_BUTTON (widget));
	g_return_if_fail (area != NULL);

	if (GTK_WIDGET_DRAWABLE (widget)) {
		tmp_area = *area;
		tmp_area.x -= GTK_CONTAINER (widget)->border_width;
		tmp_area.y -= GTK_CONTAINER (widget)->border_width;

		e_bevel_button_paint (widget, &tmp_area);

		if (GTK_BIN (widget)->child && gtk_widget_intersect (GTK_BIN (widget)->child, &tmp_area, &child_area))
			gtk_widget_draw (GTK_BIN (widget)->child, &child_area);
	}
}

static void
e_bevel_button_realize (GtkWidget *widget)
{
	EBevelButton *button = E_BEVEL_BUTTON (widget);

	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	button->priv->gc = gdk_gc_new (widget->window);

	gdk_color_parse ("#d0d888", &button->priv->base_color);
	e_bevel_button_util_shade (&button->priv->base_color,
				   &button->priv->light_color,
				   LIGHTNESS_MULT);
	e_bevel_button_util_shade (&button->priv->base_color,
				   &button->priv->dark_color,
				   DARKNESS_MULT);
	gdk_colormap_alloc_color (gdk_rgb_get_cmap (), &button->priv->base_color, FALSE, TRUE);
	gdk_colormap_alloc_color (gdk_rgb_get_cmap (), &button->priv->light_color, FALSE, TRUE);
	gdk_colormap_alloc_color (gdk_rgb_get_cmap (), &button->priv->dark_color, FALSE, TRUE);
}

static void
e_bevel_button_class_init (EBevelButtonClass *klass)
{
	GtkWidgetClass *widget_class;

	widget_class = (GtkWidgetClass *)klass;

	parent_class = gtk_type_class (PARENT_TYPE);
	
	widget_class->draw = e_bevel_button_draw;
	widget_class->expose_event = e_bevel_button_expose;
	widget_class->realize = e_bevel_button_realize;
}

static void
e_bevel_button_init (EBevelButton *button)
{
	EBevelButtonPrivate *priv;

	GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);

	priv = g_new (EBevelButtonPrivate, 1);

	button->priv = priv;
}

GtkWidget *
e_bevel_button_new (void)
{
	EBevelButton *button;

	button = gtk_type_new (E_TYPE_BEVEL_BUTTON);

	return GTK_WIDGET (button);
}

E_MAKE_TYPE (e_bevel_button, "EBevelButton", EBevelButton, e_bevel_button_class_init, e_bevel_button_init, PARENT_TYPE);












