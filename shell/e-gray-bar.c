/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-gray-bar.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-gray-bar.h"

#include <gtk/gtkrc.h>
#include <gtk/gtktypeutils.h>
#include <gtk/gtkwidget.h>

#include <gal/util/e-util.h>


#define PARENT_TYPE gtk_event_box_get_type ()
static GtkEventBoxClass *parent_class = NULL;


static void
endarken_style (GtkWidget *widget)
{
	GtkStyle *style;
	GtkRcStyle *new_rc_style;
	int i;

	style = widget->style;

	new_rc_style = gtk_rc_style_new ();

	for (i = 0; i < 5; i++) {
		new_rc_style->bg[i].red      = 0xffff;
		new_rc_style->bg[i].green    = 0x0000;
		new_rc_style->bg[i].blue     = 0xffff;
		new_rc_style->base[i].red    = style->base[i].red * .8;
		new_rc_style->base[i].green  = style->base[i].green * .8;
		new_rc_style->base[i].blue   = style->base[i].blue * .8;
		new_rc_style->fg[i].red      = 0xffff;
		new_rc_style->fg[i].green    = 0xffff;
		new_rc_style->fg[i].blue     = 0xffff;
		new_rc_style->text[i].red    = 0xffff;
		new_rc_style->text[i].green  = 0xffff;
		new_rc_style->text[i].blue   = 0xffff;

		new_rc_style->color_flags[i] = GTK_RC_BG | GTK_RC_FG | GTK_RC_BASE | GTK_RC_TEXT;
	}

	gtk_widget_modify_style (widget, new_rc_style);

	gtk_rc_style_unref (new_rc_style);
}


static void
impl_style_set (GtkWidget *widget,
		GtkStyle *previous_style)
{
	static int in_style_set = 0;

	if (in_style_set > 0)
		return;

	in_style_set ++;

	endarken_style (widget);

	in_style_set --;
}


static void
class_init (GtkObjectClass *object_class)
{
	GtkWidgetClass *widget_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	widget_class = GTK_WIDGET_CLASS (object_class);
	widget_class->style_set = impl_style_set;
}

static void
init (EGrayBar *gray_bar)
{
}


GtkWidget *
e_gray_bar_new (void)
{
	GtkWidget *new;

	new = gtk_type_new (e_gray_bar_get_type ());

	return new;
}


E_MAKE_TYPE (e_gray_bar, "EGrayBar", EGrayBar, class_init, init, PARENT_TYPE)
