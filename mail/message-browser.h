/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 */

#ifndef _MESSAGE_BROWSER_H_
#define _MESSAGE_BROWSER_H_

#include <gnome.h>
#include <bonobo/bonobo-window.h>

#include <camel/camel-folder.h>
#include "folder-browser.h"
#include "mail-display.h"
#include "mail-types.h"

#define MESSAGE_BROWSER_TYPE        (message_browser_get_type ())
#define MESSAGE_BROWSER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MESSAGE_BROWSER_TYPE, MessageBrowser))
#define MESSAGE_BROWSER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), MESSAGE_BROWSER_TYPE, MessageBrowserClass))
#define IS_MESSAGE_BROWSER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MESSAGE_BROWSER_TYPE))
#define IS_MESSAGE_BROWSER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MESSAGE_BROWSER_TYPE))

struct _MessageBrowser {
	BonoboWindow parent;
	
	/*
	 * The current URI being displayed by the MessageBrowser
	 */
	FolderBrowser *fb;
	gulong ml_built_id;
	gulong loaded_id;
};


typedef struct {
	BonoboWindowClass parent_class;
	
} MessageBrowserClass;

GtkType    message_browser_get_type (void);

GtkWidget *message_browser_new      (const GNOME_Evolution_Shell shell,
				     const char *uri, const char *uid);

#endif /* _MESSAGE_BROWSER_H_ */

