/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-colors.c - General color allocation utilities
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* We keep our own color context, as the color allocation might take
 * place before things are realized.
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
