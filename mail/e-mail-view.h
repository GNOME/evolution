/*
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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifndef _E_MAIL_VIEW_H_
#define _E_MAIL_VIEW_H_

#include <gtk/gtk.h>
#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include "widgets/menus/gal-view-instance.h"

#define E_MAIL_VIEW_TYPE        (e_mail_view_get_type ())
#define E_MAIL_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_MAIL_VIEW_TYPE, EMailView))
#define E_MAIL_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_MAIL_VIEW_TYPE, EMailViewClass))
#define IS_E_MAIL_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_MAIL_VIEW_TYPE))
#define IS_E_MAIL_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_MAIL_VIEW_TYPE))
#define E_MAIL_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_MAIL_VIEW_TYPE, EMailViewClass))


typedef struct _EMailViewPrivate EMailViewPrivate;

typedef struct _EMailView {
	GtkVBox parent;

	EMailViewPrivate *priv;
        EShellContent *content;
	struct _EMailView *prev;
} EMailView;

typedef struct _EMailViewClass {
	GtkVBoxClass parent_class;

	void (*pane_close) (EMailView *);
	void (*view_changed) (EMailView *);
	void (*open_mail) (EMailView *, const char *);

	EShellSearchbar * (*get_searchbar) (EMailView *view);
	void (*set_search_strings) (EMailView *view, GSList *search_strings);
	GalViewInstance * (*get_view_instance) (EMailView *view);
	void (*update_view_instance) (EMailView *view);
	void (*set_orientation) (EMailView *view, GtkOrientation orientation);
	GtkOrientation (*get_orientation) (EMailView *);
	void (*set_preview_visible) (EMailView *view, gboolean visible);
	gboolean (*get_preview_visible) (EMailView *view);
	void (*set_show_deleted) (EMailView *view, gboolean show_deleted);
	gboolean (*get_show_deleted) (EMailView *view);

} EMailViewClass;

GType e_mail_view_get_type (void);

void e_mail_view_update_view_instance (EMailView *view);
GalViewInstance * e_mail_view_get_view_instance (EMailView *view);

void e_mail_view_set_search_strings (EMailView *view, GSList *search_strings);

void e_mail_view_set_orientation (EMailView *view, GtkOrientation orientation);
GtkOrientation  e_mail_view_get_orientation (EMailView *);
void e_mail_view_set_preview_visible (EMailView *view, gboolean visible);
gboolean e_mail_view_get_preview_visible (EMailView *view);
void e_mail_view_set_show_deleted (EMailView *view, gboolean show_deleted);
gboolean e_mail_view_get_show_deleted (EMailView *view);

EShellSearchbar * e_mail_view_get_searchbar (EMailView *view);

#endif
