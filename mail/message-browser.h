/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifndef _MESSAGE_BROWSER_H_
#define _MESSAGE_BROWSER_H_

#include <gnome.h>
#include <camel/camel-folder.h>
#include "folder-browser.h"
#include "mail-display.h"
#include "mail-types.h"

#define MESSAGE_BROWSER_TYPE        (message_browser_get_type ())
#define MESSAGE_BROWSER(o)          (GTK_CHECK_CAST ((o), MESSAGE_BROWSER_TYPE, MessageBrowser))
#define MESSAGE_BROWSER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MESSAGE_BROWSER_TYPE, MessageBrowserClass))
#define IS_MESSAGE_BROWSER(o)       (GTK_CHECK_TYPE ((o), MESSAGE_BROWSER_TYPE))
#define IS_MESSAGE_BROWSER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MESSAGE_BROWSER_TYPE))


struct _MessageBrowser {
	GnomeApp parent;
	
	/*
	 * The current URI being displayed by the MessageBrowser
	 */
	FolderBrowser *fb;
};


typedef struct {
	GnomeAppClass parent_class;
	
} MessageBrowserClass;

GtkType    message_browser_get_type (void);

GtkWidget *message_browser_new      (const GNOME_Evolution_Shell shell,
				     const char *uri, const char *uid);

#endif /* _MESSAGE_BROWSER_H_ */

