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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#include "message-browser.h"

#include "mail-display.h"

#include "mail-local.h"
#include "mail-config.h"

#define d(x) 

#define MINIMUM_WIDTH  600
#define MINIMUM_HEIGHT 400

#define PARENT_TYPE GTK_TYPE_WINDOW

/* Size of the window last time it was changed.  */
static GtkAllocation last_allocation = { 0, 0 };

static GtkWindowClass *parent_class = NULL;


static void
message_browser_class_init (GtkObjectClass *object_class)
{
	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
message_browser_init (GtkObject *object)
{
	
}

GtkType
message_browser_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MessageBrowser",
			sizeof (MessageBrowser),
			sizeof (MessageBrowserClass),
			(GtkClassInitFunc) message_browser_class_init,
			(GtkObjectInitFunc) message_browser_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_window_get_type (), &type_info);
	}
	
	return type;
}


static void
message_browser_size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation)
{
	last_allocation = *allocation;
}

static void
set_default_size (GtkWidget *widget)
{
	int width, height;
	
	width  = MAX (MINIMUM_WIDTH, last_allocation.width);
	height = MAX (MINIMUM_HEIGHT, last_allocation.height);
	
	gtk_window_set_default_size (GTK_WINDOW (widget), width, height);
}

GtkWidget *
message_browser_new (CamelMimeMessage *message)
{
	GtkWidget *new, *mail_display;
	
	new = gtk_widget_new (message_browser_get_type (), NULL);
	gtk_signal_connect (GTK_OBJECT (new), "size_allocate", 
			    GTK_SIGNAL_FUNC (message_browser_size_allocate_cb), NULL);
	
	mail_display = mail_display_new ();
	gtk_container_add (GTK_CONTAINER (new), mail_display);
	gtk_widget_show (mail_display);
	
	mail_display_set_message (MAIL_DISPLAY (mail_display), CAMEL_MEDIUM (message), NULL);
	
	set_default_size (new);
	
	return new;
}
