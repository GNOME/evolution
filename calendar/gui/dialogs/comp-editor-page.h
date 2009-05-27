/*
 * Evolution calendar - Base class for calendar component editor pages
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef COMP_EDITOR_PAGE_H
#define COMP_EDITOR_PAGE_H

#include <time.h>
#include <gtk/gtk.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal.h>

/* Standard GObject macros */
#define TYPE_COMP_EDITOR_PAGE \
	(comp_editor_page_get_type ())
#define COMP_EDITOR_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_COMP_EDITOR_PAGE, CompEditorPage))
#define COMP_EDITOR_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_COMP_EDITOR_PAGE, CompEditorPageClass))
#define IS_COMP_EDITOR_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_COMP_EDITOR_PAGE))
#define IS_COMP_EDITOR_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), TYPE_COMP_EDITOR_PAGE))
#define COMP_EDITOR_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_COMP_EDITOR_PAGE, CompEditorPageClass))

G_BEGIN_DECLS

/* Use a forward declaration to avoid a circular reference. */
struct _CompEditor;

typedef struct _CompEditorPage CompEditorPage;
typedef struct _CompEditorPageClass CompEditorPageClass;
typedef struct _CompEditorPagePrivate CompEditorPagePrivate;

typedef struct {
	ECalComponentDateTime *start;
	ECalComponentDateTime *end;
	ECalComponentDateTime *due;
	struct icaltimetype *complete;
} CompEditorPageDates;

struct _CompEditorPage {
	GObject object;

	/* The GtkAccelGroup for the page. We install this when the page is
	   mapped, and uninstall when it is unmapped. libglade would do this
	   normally, but we create our pages individually so have to do it
	   ourselves. */
	GtkAccelGroup *accel_group;

	CompEditorPagePrivate *priv;
};

struct _CompEditorPageClass {
	GObjectClass parent_class;

	/* Notification signals */

	void (* dates_changed)   (CompEditorPage *page, const gchar *dates);

	/* Virtual methods */

	GtkWidget *(* get_widget) (CompEditorPage *page);
	void (* focus_main_widget) (CompEditorPage *page);

	gboolean (* fill_widgets) (CompEditorPage *page, ECalComponent *comp);
	gboolean (* fill_component) (CompEditorPage *page, ECalComponent *comp);
	gboolean (* fill_timezones) (CompEditorPage *page, GHashTable *timezones);

	void (* set_dates) (CompEditorPage *page, CompEditorPageDates *dates);
};

GType		comp_editor_page_get_type	(void);
struct _CompEditor *
		comp_editor_page_get_editor	(CompEditorPage *page);
GtkWidget *	comp_editor_page_get_widget	(CompEditorPage *page);
gboolean	comp_editor_page_get_updating	(CompEditorPage *page);
void		comp_editor_page_set_updating	(CompEditorPage *page,
						 gboolean updating);
void		comp_editor_page_changed	(CompEditorPage *page);
void		comp_editor_page_focus_main_widget
						(CompEditorPage *page);
gboolean	comp_editor_page_fill_widgets	(CompEditorPage *page,
						 ECalComponent *comp);
gboolean	comp_editor_page_fill_component	(CompEditorPage *page,
						 ECalComponent *comp);
gboolean	comp_editor_page_fill_timezones	(CompEditorPage *page,
						 GHashTable *timezones);
void		comp_editor_page_set_dates	(CompEditorPage *page,
						 CompEditorPageDates *dates);
void		comp_editor_page_notify_dates_changed
						(CompEditorPage *page,
						 CompEditorPageDates *dates);
void		comp_editor_page_display_validation_error
						(CompEditorPage *page,
						 const gchar *msg,
						 GtkWidget *field);

G_END_DECLS

#endif /* COMP_EDITOR_PAGE_H */
