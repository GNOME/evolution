/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __FOLDER_BROWSER_WINDOW_H__
#define __FOLDER_BROWSER_WINDOW_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <bonobo/bonobo-win.h>

#include "folder-browser.h"

#define FOLDER_BROWSER_WINDOW_TYPE        (folder_browser_window_get_type ())
#define FOLDER_BROWSER_WINDOW(o)          (GTK_CHECK_CAST ((o), FOLDER_BROWSER_WINDOW_TYPE, FolderBrowserWindow))
#define FOLDER_BROWSER_WINDOW_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), FOLDER_BROWSER_WINDOW_TYPE, FolderBrowserWindowClass))
#define IS_FOLDER_BROWSER_WINDOW(o)       (GTK_CHECK_TYPE ((o), FOLDER_BROWSER_WINDOW_TYPE))
#define IS_FOLDER_BROWSER_WINDOW_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), FOLDER_BROWSER_WINDOW_TYPE))

typedef struct _FolderBrowserWindowClass FolderBrowserWindowClass;
typedef struct _FolderBrowserWindow FolderBrowserWindow;

struct _FolderBrowserWindow {
	BonoboWindow parent;
	
	GtkWidget *fb_parent;
	FolderBrowser *folder_browser;
};

struct _FolderBrowserWindowClass {
	BonoboWindowClass parent_class;
	
};

GtkType folder_browser_window_get_type (void);

GtkWidget *folder_browser_window_new (FolderBrowser *fb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FOLDER_BROWSER_WINDOW_H__ */
