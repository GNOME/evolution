/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtkimage.h>

#include "e-info-label.h"
#include <gtk/gtklabel.h>
#include "e-clipped-label.h"

#include <e-util/e-icon-factory.h>

static GtkHBoxClass *el_parent;

static void
el_init(GObject *o)
{
	/*EInfoLabel *el = (EInfoLabel *)o;*/
}

static void
el_finalise(GObject *o)
{
	((GObjectClass *)el_parent)->finalize(o);
}

static void
el_destroy (GtkObject *o)
{
	((EInfoLabel *)o)->location = NULL;
	((EInfoLabel *)o)->info = NULL;

	((GtkObjectClass *)el_parent)->destroy(o);
}

static int
el_expose_event(GtkWidget *w, GdkEventExpose *event)
{
	int x = ((GtkContainer *)w)->border_width;

	/* This seems a hack to me, but playing with styles wouldn't affect the background */
	gtk_paint_flat_box(w->style, w->window,
			   GTK_STATE_ACTIVE, GTK_SHADOW_NONE,
			   &event->area, w, "EInfoLabel",
			   w->allocation.x+x, w->allocation.y+x,
			   w->allocation.width-x*2, w->allocation.height-x*2);

	return ((GtkWidgetClass *)el_parent)->expose_event(w, event);
}

static void
el_class_init(GObjectClass *klass)
{
	klass->finalize = el_finalise;
	
	((GtkObjectClass *)klass)->destroy = el_destroy;
	((GtkWidgetClass *)klass)->expose_event = el_expose_event;
}

GType
e_info_label_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EInfoLabelClass),
			NULL, NULL,
			(GClassInitFunc)el_class_init,
			NULL, NULL,
			sizeof(EInfoLabel), 0,
			(GInstanceInitFunc)el_init
		};
		el_parent = g_type_class_ref(gtk_hbox_get_type());
		type = g_type_register_static(gtk_hbox_get_type(), "EInfoLabel", &info, 0);
	}

	return type;
}

/**
 * e_info_label_new:
 * @icon: 
 * 
 * Create a new info label widget.  @icon is the name of the icon
 * (from the icon theme) to use for the icon image.
 * 
 * Return value: 
 **/
GtkWidget *
e_info_label_new(const char *icon)
{
	EInfoLabel *el = g_object_new(e_info_label_get_type(), 0);
	GtkWidget *image;
	char *name = e_icon_factory_get_icon_filename (icon, E_ICON_SIZE_MENU);
	
	image = gtk_image_new_from_file(name);
	g_free(name);
	gtk_misc_set_padding((GtkMisc *)image, 6, 6);
	gtk_box_pack_start((GtkBox *)el, image, FALSE, TRUE, 0);
	gtk_widget_show(image);

	gtk_container_set_border_width((GtkContainer *)el, 3);

	return (GtkWidget *)el;
}

/**
 * e_info_label_set_info:
 * @el: 
 * @location: 
 * @info: 
 * 
 * Set the information to show on the label.  @location is some
 * context about the current view.  e.g. the folder name.  If the
 * label is too wide, this will be truncated.
 *
 * @info is some info about this location.
 **/
void
e_info_label_set_info(EInfoLabel *el, const char *location, const char *info)
{
	char *tmp;

	if (el->location == NULL) {
		el->location = e_clipped_label_new(location, PANGO_WEIGHT_BOLD, 1.0);
		el->info = gtk_label_new(NULL);

		gtk_misc_set_alignment((GtkMisc *)el->location, 0.0, 0.0);
		gtk_misc_set_padding((GtkMisc *)el->location, 0, 6);
		gtk_misc_set_alignment((GtkMisc *)el->info, 0.0, 1.0);
		gtk_misc_set_padding((GtkMisc *)el->info, 0, 6);

		gtk_widget_show(el->location);
		gtk_widget_show(el->info);

		gtk_box_pack_start((GtkBox *)el, (GtkWidget *)el->location, TRUE, TRUE, 0);
		gtk_box_pack_end((GtkBox *)el, (GtkWidget *)el->info, FALSE, TRUE, 6);
		gtk_widget_set_state((GtkWidget *)el, GTK_STATE_ACTIVE);
	} else {
		e_clipped_label_set_text((EClippedLabel *)el->location, location);
	}

	tmp = g_strdup_printf("<span size=\"smaller\">%s</span>", info);
	gtk_label_set_markup((GtkLabel *)el->info, tmp);
	g_free(tmp);
}

