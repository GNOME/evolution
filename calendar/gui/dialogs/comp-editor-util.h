/* Evolution calendar - Widget utilities
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _COMP_EDITOR_UTIL_H_
#define _COMP_EDITOR_UTIL_H_

#include <gtk/gtkwidget.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-event-source.h>
#include "Evolution-Addressbook-SelectNames.h"
#include "comp-editor-page.h"

void comp_editor_dates (CompEditorPageDates *date, CalComponent *comp);
void comp_editor_free_dates (CompEditorPageDates *dates);

void comp_editor_date_label (CompEditorPageDates *dates, GtkWidget *label);

GtkWidget *comp_editor_new_date_edit (gboolean show_date, gboolean show_time,
				      gboolean make_time_insensitive);

struct tm comp_editor_get_current_time (GtkObject *object, gpointer data);


GNOME_Evolution_Addressbook_SelectNames comp_editor_create_contacts_component (void);
GtkWidget * comp_editor_create_contacts_control (GNOME_Evolution_Addressbook_SelectNames corba_select_names);
Bonobo_EventSource_ListenerId comp_editor_connect_contacts_changed (GtkWidget *contacts_entry,
								    BonoboListenerCallbackFn changed_cb,
								    gpointer changed_cb_data);
void comp_editor_show_contacts_dialog (GNOME_Evolution_Addressbook_SelectNames corba_select_names);

void comp_editor_contacts_to_widget (GtkWidget *contacts_entry,
				     CalComponent *comp);
void comp_editor_contacts_to_component (GtkWidget *contacts_entry,
					CalComponent *comp);

char *comp_editor_strip_categories (const char *categories);

#endif
