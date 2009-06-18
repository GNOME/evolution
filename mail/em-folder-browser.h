/*
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EM_FOLDER_BROWSER_H
#define _EM_FOLDER_BROWSER_H

#include "mail/em-folder-view.h"

G_BEGIN_DECLS

typedef struct _EMFolderBrowser EMFolderBrowser;
typedef struct _EMFolderBrowserClass EMFolderBrowserClass;

struct _EMFolderBrowser {
	EMFolderView view;

	struct _EMFolderBrowserPrivate *priv;

	GtkWidget *vpane;
	struct _EFilterBar *search;
};

struct _EMFolderBrowserClass {
	EMFolderViewClass parent_class;

	/* Signals*/
	void (*account_search_activated) (EMFolderBrowser *emfb);
	void (*account_search_cleared) (EMFolderBrowser *emfb);
};

GType em_folder_browser_get_type(void);

GtkWidget *em_folder_browser_new(void);

void em_folder_browser_show_preview(EMFolderBrowser *emfv, gboolean state);
void em_folder_browser_show_wide(EMFolderBrowser *emfv, gboolean state);
gboolean em_folder_browser_get_wide(EMFolderBrowser *emfv);
void em_folder_browser_suppress_message_selection(EMFolderBrowser *emfb);

G_END_DECLS

#endif /* ! _EM_FOLDER_BROWSER_H */
