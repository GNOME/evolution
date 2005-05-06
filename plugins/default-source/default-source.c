/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 D
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <addressbook/gui/widgets/eab-config.h>
#include <libebook/e-book.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-source.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>
GtkWidget* org_gnome_default_book (EPlugin *epl, EConfigHookItemFactoryData *data);
void commit_default_calendar (EPlugin *epl, EConfigTarget *target);
void commit_default_book (EPlugin *epl, EConfigTarget *target);
void 
commit_default_calendar (EPlugin *epl, EConfigTarget *target)
{
	ECalConfigTargetSource *cal_target;
	ESource *source;

	cal_target = (ECalConfigTargetSource *) target;
	source = cal_target->source;
	if (e_source_get_property (source, "default")) 
		if (!e_cal_set_default_source (source, E_CAL_SOURCE_TYPE_EVENT, NULL))
			e_cal_set_default_source (source, E_CAL_SOURCE_TYPE_TODO, NULL);
}

void
commit_default_book (EPlugin *epl, EConfigTarget *target)
{
	EABConfigTargetSource *book_target;
	ESource *source;

	book_target = (EABConfigTargetSource *) target;
	source = book_target->source;
	if (e_source_get_property (source, "default"))
		e_book_set_default_source (source, NULL);
  
    
}

static void
default_source_changed (GtkWidget *check_box,  ESource *source)
{
    
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_box))) 
		e_source_set_property (source, "default", "true");
	else 
		e_source_set_property (source, "default", NULL);
}


GtkWidget *
org_gnome_default_book (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GtkWidget *widget;
	ESource *source;
	EABConfigTargetSource *book_target;
 
	if (data->old)
		return data->old;
	widget = gtk_check_button_new_with_label (_("Mark as default folder"));
	book_target = (EABConfigTargetSource *) data->target;
	source = book_target->source;
    
	if (e_source_get_property (source, "default"))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	else 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (data->parent), widget);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled", G_CALLBACK (default_source_changed), source);
	gtk_widget_show (widget);
	return widget;
}


GtkWidget *
org_gnome_default_cal (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	GtkWidget *widget;
	ESource *source;
	ECalConfigTargetSource *cal_target;
	int i;

	if (data->old)
		return data->old;
	widget = gtk_check_button_new_with_label (_("Mark as default folder"));
	cal_target = (ECalConfigTargetSource *) data->target;
	source = cal_target->source;
    
	if (e_source_get_property (source, "default"))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	else 
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	
	i = ((GtkTable *)data->parent)->nrows;
	gtk_table_attach((GtkTable *)data->parent, widget, 1, 2, i, i+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	g_signal_connect (GTK_TOGGLE_BUTTON (widget), "toggled", G_CALLBACK (default_source_changed), source);
	gtk_widget_show (widget);
	return widget;
}
