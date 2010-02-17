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
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Add 'copy to clipboard' things to various menu's.

   Uh, so far only to copy mail addresses from mail content */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>

#include "mail/em-popup.h"

#include <gtk/gtk.h>

#include "camel/camel-internet-address.h"
#include "camel/camel-url.h"

void org_gnome_copy_tool_copy_address(gpointer ep, EMPopupTargetURI *t);

void
org_gnome_copy_tool_copy_address(gpointer ep, EMPopupTargetURI *t)
{
	if  (t->uri) {
		CamelInternetAddress *cia = camel_internet_address_new();
		CamelURL *curl;
		GtkClipboard *clipboard;
		gchar *addr;
		const gchar *tmp;

		curl = camel_url_new(t->uri, NULL);
		camel_address_decode ((CamelAddress *) cia, curl->path);
		addr = camel_address_format ((CamelAddress *) cia);
		tmp = addr && addr[0] ? addr : t->uri + 7;

		clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

		gtk_clipboard_set_text (clipboard, tmp, strlen (tmp));

		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_text (clipboard, tmp, strlen (tmp));

		g_free(addr);
		camel_url_free(curl);
		camel_object_unref(cia);
	}
}
