
/* Copyright (C) 2004 Michael Zucchi */

/* This file is licensed under the GNU GPL v2 or later */

/* Add 'copy to clipboard' things to various menu's.

   Uh, so far only to copy mail addresses from mail content */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include "mail/em-popup.h"

#include <gtk/gtkmain.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtkselection.h>

#include "camel/camel-internet-address.h"
#include "camel/camel-url.h"

static GtkWidget *invisible;
static char *address_uri;

void org_gnome_copy_tool_copy_address(void *ep, EMPopupTargetURI *t);

void
org_gnome_copy_tool_copy_address(void *ep, EMPopupTargetURI *t)
{
	g_free(address_uri);
	address_uri = g_strdup(t->uri);

	printf("copying address '%s'\n", address_uri);

	gtk_selection_owner_set(invisible, GDK_SELECTION_PRIMARY, gtk_get_current_event_time());
	gtk_selection_owner_set(invisible, GDK_SELECTION_CLIPBOARD, gtk_get_current_event_time());
}

static void
ct_selection_get(GtkWidget *widget, GtkSelectionData *data, guint info, guint time_stamp, void *dummy)
{
	printf("get selection, address is '%s'\n", address_uri);

	if (address_uri == NULL)
		return;

	if (strncmp (address_uri, "mailto:", 7) == 0) {
		CamelInternetAddress *cia = camel_internet_address_new();
		CamelURL *curl;
		char *addr;
		const char *tmp;

		curl = camel_url_new(address_uri, NULL);
		camel_address_decode((CamelAddress *)cia, curl->path);
		/* should it perhaps use address format? */
		addr = camel_address_encode((CamelAddress *)cia);
		tmp = addr && addr[0] ? addr : address_uri + 7;
		printf("get selection, setting to' %s'\n", tmp);

		gtk_selection_data_set(data, data->target, 8, tmp, strlen(tmp));
		g_free(addr);
		camel_url_free(curl);
		camel_object_unref(cia);
	}
}

static void
ct_selection_clear_event(GtkWidget *widget, GdkEventSelection *event, void *dummy)
{
	printf("selection clear event\n");

	g_free(address_uri);
	address_uri = NULL;
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	if (enable) {
		invisible = gtk_invisible_new();
		g_signal_connect(invisible, "selection_get", G_CALLBACK(ct_selection_get), NULL);
		g_signal_connect(invisible, "selection_clear_event", G_CALLBACK(ct_selection_clear_event), NULL);
		gtk_selection_add_target(invisible, GDK_SELECTION_PRIMARY, GDK_SELECTION_TYPE_STRING, 0);
		gtk_selection_add_target(invisible, GDK_SELECTION_CLIPBOARD, GDK_SELECTION_TYPE_STRING, 1);
	} else {
		g_free(address_uri);
		address_uri = NULL;
		gtk_widget_destroy(invisible);
		invisible = NULL;
	}

	return 0;
}
