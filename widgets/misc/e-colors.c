/*
 * e_color.c: General color allocation utilities
 *
 * Author:
 *  Miguel de Icaza (miguel@kernel.org)
 *
 * We keep our own color context, as the color allocation might take place
 * before things are realized.
 */
#include <config.h>
#include <gtk/gtkwidget.h>
#include "e-colors.h"

static gboolean e_color_inited;
static GdkColorContext *e_color_context;

GdkColor e_white, e_dark_gray, e_black;

int 
e_color_alloc (gushort red, gushort green, gushort blue)
{
	int failed;
	
	if (!e_color_inited)
		e_color_init ();
	
	return gdk_color_context_get_pixel (e_color_context,
					    red, green, blue, &failed);
}

void
e_color_alloc_gdk (GdkColor *c)
{
	int failed;
	
	g_return_if_fail (c != NULL);
	
	if (!e_color_inited)
		e_color_init ();
	
	c->pixel = gdk_color_context_get_pixel (e_color_context, c->red, c->green, c->blue, &failed);
}

void
e_color_alloc_name (const char *name, GdkColor *c)
{
	int failed;
	
	g_return_if_fail (name != NULL);
	g_return_if_fail (c != NULL);

	if (!e_color_inited)
		e_color_init ();
	
	gdk_color_parse (name, c);
	c->pixel = 0;
	c->pixel = gdk_color_context_get_pixel (e_color_context, c->red, c->green, c->blue, &failed);
}

void
e_color_init (void)
{
	GdkColormap *colormap;

	/* It's surprisingly easy to end up calling this twice.  Survive.  */
	if (e_color_inited)
		return;

	colormap = gtk_widget_get_default_colormap ();

	/* Initialize the color context */
	e_color_context = gdk_color_context_new (
		gtk_widget_get_default_visual (), colormap);

	e_color_inited = TRUE;

	/* Allocate the default colors */
	gdk_color_white (colormap, &e_white);
	gdk_color_black (colormap, &e_black);
	e_color_alloc_name ("gray20",  &e_dark_gray);
}
