/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-browser.h
 *
 * Copyright (C) 2003  Ximian, Inc.
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

#ifndef _E_STORAGE_BROWSER_H_
#define _E_STORAGE_BROWSER_H_

#include "e-storage-set.h"

#include <glib-object.h>
#include <gtk/gtkwidget.h>


#define E_TYPE_STORAGE_BROWSER		(e_storage_browser_get_type ())
#define E_STORAGE_BROWSER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_STORAGE_BROWSER, EStorageBrowser))
#define E_STORAGE_BROWSER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_STORAGE_BROWSER, EStorageBrowserClass))
#define E_IS_BROWSER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_STORAGE_BROWSER))
#define E_IS_BROWSER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_STORAGE_BROWSER))


typedef struct _EStorageBrowser        EStorageBrowser;
typedef struct _EStorageBrowserPrivate EStorageBrowserPrivate;
typedef struct _EStorageBrowserClass   EStorageBrowserClass;

/* FIXME: Use a GClosure instead of void *?  */
typedef GtkWidget * (* EStorageBrowserCreateViewCallback)  (EStorageBrowser   *browser,
							    const char *path,
							    void       *data);


struct _EStorageBrowser {
	GObject parent;

	EStorageBrowserPrivate *priv;
};

struct _EStorageBrowserClass {
	GObjectClass parent_class;

	void (* widgets_gone) (EStorageBrowser *browser);

	void (* page_switched) (EStorageBrowser *browser,
				GtkWidget *old_page,
				GtkWidget *new_page);
};


GType    e_storage_browser_get_type (void);

EStorageBrowser *e_storage_browser_new  (EStorageSet                       *storage_set,
					 const char                        *starting_path,
					 EStorageBrowserCreateViewCallback  create_view_callback,
					 void                              *create_view_callback_data);

GtkWidget   *e_storage_browser_peek_tree_widget           (EStorageBrowser *browser);
GtkWidget   *e_storage_browser_peek_tree_widget_scrolled  (EStorageBrowser *browser);
GtkWidget   *e_storage_browser_peek_view_widget           (EStorageBrowser *browser);
EStorageSet *e_storage_browser_peek_storage_set           (EStorageBrowser *browser);

gboolean  e_storage_browser_show_path             (EStorageBrowser   *browser,
						   const char *path);
void      e_storage_browser_remove_view_for_path  (EStorageBrowser   *browser,
						   const char *path);


#endif /* _E_STORAGE_BROWSER_H_ */
