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

#ifndef COMP_EDITOR_PAGE_H
#define COMP_EDITOR_PAGE_H

#include <time.h>
#include <libgnome/gnome-defs.h>
#include <gtk/gtkwidget.h>
#include <cal-util/cal-component.h>

BEGIN_GNOME_DECLS



#define TYPE_COMP_EDITOR_PAGE            (comp_editor_page_get_type ())
#define COMP_EDITOR_PAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_COMP_EDITOR_PAGE, CompEditorPage))
#define COMP_EDITOR_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_COMP_EDITOR_PAGE,	CompEditorPageClass))
#define IS_COMP_EDITOR_PAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_COMP_EDITOR_PAGE))
#define IS_COMP_EDITOR_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), TYPE_COMP_EDITOR_PAGE))

typedef struct {
	time_t start;
	time_t end;
	time_t due;
	time_t complete;
} CompEditorPageDates;

typedef struct {
	GtkObject object;
} CompEditorPage;

typedef struct {
	GtkObjectClass parent_class;

	/* Notification signals */

	void (* changed)    (CompEditorPage *page);
	void (* needs_send) (CompEditorPage *page);

	void (* summary_changed) (CompEditorPage *page, const char *summary);
	void (* dates_changed)   (CompEditorPage *page, const char *dates);

	/* Virtual methods */

	GtkWidget *(* get_widget) (CompEditorPage *page);

	void (* fill_widgets) (CompEditorPage *page, CalComponent *comp);
	void (* fill_component) (CompEditorPage *page, CalComponent *comp);

	void (* set_summary) (CompEditorPage *page, const char *summary);
	void (* set_dates) (CompEditorPage *page, CompEditorPageDates *dates);
} CompEditorPageClass;


GtkType    comp_editor_page_get_type               (void);
GtkWidget *comp_editor_page_get_widget             (CompEditorPage      *page);
void       comp_editor_page_fill_widgets           (CompEditorPage      *page,
						    CalComponent        *comp);
void       comp_editor_page_fill_component         (CompEditorPage      *page,
						    CalComponent        *comp);
void       comp_editor_page_set_summary            (CompEditorPage      *page,
						    const char          *summary);
void       comp_editor_page_set_dates              (CompEditorPage      *page,
						    CompEditorPageDates *dates);
void       comp_editor_page_notify_changed         (CompEditorPage      *page);
void       comp_editor_page_notify_needs_send      (CompEditorPage      *page);
void       comp_editor_page_notify_summary_changed (CompEditorPage      *page,
						    const char          *summary);
void       comp_editor_page_notify_dates_changed   (CompEditorPage      *page,
						    CompEditorPageDates *dates);




END_GNOME_DECLS

#endif
