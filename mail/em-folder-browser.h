/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef _EM_FOLDER_BROWSER_H
#define _EM_FOLDER_BROWSER_H

#include "mail/em-folder-view.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _EM_FOLDER_BROWSER_H */
