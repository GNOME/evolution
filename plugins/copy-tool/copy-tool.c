
/* Copyright (C) 2004 Michael Zucchi */

/* This file is licensed under the GNU GPL v2 or later */

/* Add 'copy to clipboard' things to various menu's.

   Uh, so far only to copy mail addresses from mail content */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>

#include "mail/em-popup.h"

#include <gtk/gtkclipboard.h>

#include "camel/camel-internet-address.h"
#include "camel/camel-url.h"

void org_gnome_copy_tool_copy_address(void *ep, EMPopupTargetURI *t);

void
org_gnome_copy_tool_copy_address(void *ep, EMPopupTargetURI *t)
{
	if  (t->uri) {
		CamelInternetAddress *cia = camel_internet_address_new();
		CamelURL *curl;
		GtkClipboard *clipboard;
		char *addr;
		const char *tmp;

		curl = camel_url_new(t->uri, NULL);
		camel_address_decode((CamelAddress *)cia, curl->path);
		/* should it perhaps use address format? */
		addr = camel_address_encode((CamelAddress *)cia);
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
