/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-gray-bar.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
	GtkRcStyle *rc_style = gtk_rc_style_new();

	rc_style->color_flags[GTK_STATE_NORMAL] |= GTK_RC_BG;
	rc_style->bg[GTK_STATE_NORMAL].red = 0x8000;
	rc_style->bg[GTK_STATE_NORMAL].green = 0x8000;
	rc_style->bg[GTK_STATE_NORMAL].blue = 0x8000;

	rc_style->color_flags[GTK_STATE_INSENSITIVE] |= GTK_RC_BG;
	rc_style->bg[GTK_STATE_INSENSITIVE].red = 0x8000;
	rc_style->bg[GTK_STATE_INSENSITIVE].green = 0x8000;
	rc_style->bg[GTK_STATE_INSENSITIVE].blue = 0x8000;

	gtk_widget_modify_style (widget, rc_style);
        gtk_rc_style_unref (rc_style);
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

	(* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous_style);
}


static void
class_init (GtkObjectClass *object_class)
{
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_ref(PARENT_TYPE);

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

	new = g_object_new (e_gray_bar_get_type (), NULL);

	return new;
}


E_MAKE_TYPE (e_gray_bar, "EGrayBar", EGrayBar, class_init, init, PARENT_TYPE)
