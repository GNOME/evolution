/* Evolution calendar - Base class for calendar component editor pages
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef EDITOR_PAGE_H
#define EDITOR_PAGE_H

#include <time.h>
#include <libgnome/gnome-defs.h>
#include <gtk/gtkwidget.h>
#include <cal-util/cal-component.h>

BEGIN_GNOME_DECLS



#define TYPE_EDITOR_PAGE            (editor_page_get_type ())
#define EDITOR_PAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_EDITOR_PAGE, EditorPage))
#define EDITOR_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_EDITOR_PAGE,		\
				     EditorPageClass))
#define IS_EDITOR_PAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_EDITOR_PAGE))
#define IS_EDITOR_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), TYPE_EDITOR_PAGE))

typedef struct {
	GtkObject object;
} EditorPage;

typedef struct {
	GtkObjectClass parent_class;

	/* Notification signals */

	void (* changed) (EditorPage *page);
	void (* summary_changed) (EditorPage *page);
	void (* dtstart_changed) (EditorPage *page);

	/* Virtual methods */

	GtkWidget *(* get_widget) (EditorPage *page);

	void (* fill_widgets) (EditorPage *page, CalComponent *comp);
	void (* fill_component) (EditorPage *page, CalComponent *comp);

	void (* set_summary) (EditorPage *page, const char *summary);
	char *(* get_summary) (EditorPage *page);

	void (* set_dtstart) (EditorPage *page, time_t start);
} EditorPageClass;

GtkType editor_page_get_type (void);

GtkWidget *editor_page_get_widget (EditorPage *page);

void editor_page_fill_widgets (EditorPage *page, CalComponent *comp);
void editor_page_fill_component (EditorPage *page, CalComponent *comp);

void editor_page_set_summary (EditorPage *page, const char *summary);
char *editor_page_get_summary (EditorPage *page);

void editor_page_set_dtstart (EditorPage *page, time_t start);

void editor_page_notify_changed (EditorPage *page);
void editor_page_notify_summary_changed (EditorPage *page);



END_GNOME_DECLS

#endif
