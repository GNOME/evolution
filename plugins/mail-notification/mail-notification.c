/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Miguel Angel Lopez Hernandez <miguel@gulev.org.mx>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <e-util/e-config.h>
#include <e-util/e-icon-factory.h>

#include <mail/em-utils.h>
#include <mail/em-event.h>
#include <mail/em-folder-tree-model.h>
#include <camel/camel-folder.h>

#ifdef HAVE_LIBNOTIFY	
#include <libnotify/notify.h>
#endif

int e_plugin_lib_enable (EPluginLib *ep, int enable);
void org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_mail_read_notify (EPlugin *ep, EMEventTargetMessage *t);

static gboolean enabled = FALSE;
GtkStatusIcon *status_icon = NULL;

static GStaticMutex mlock = G_STATIC_MUTEX_INIT;

void
org_gnome_mail_read_notify (EPlugin *ep, EMEventTargetMessage *t)
{
	g_static_mutex_lock (&mlock);

	if (!status_icon) {
		g_static_mutex_unlock (&mlock);
		return;
	}

	gtk_status_icon_set_visible (status_icon, FALSE);

	g_static_mutex_unlock (&mlock);

}

static void
icon_activated (GtkStatusIcon *icon)
{
	g_static_mutex_lock (&mlock);
	gtk_status_icon_set_visible (status_icon, FALSE);
	g_static_mutex_unlock (&mlock);

}

void
org_gnome_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t)
{
	char *msg = NULL;
	char *folder;
#ifdef HAVE_LIBNOTIFY	
	NotifyUrgency urgency = NOTIFY_URGENCY_NORMAL;
	long expire_timeout = NOTIFY_EXPIRES_DEFAULT;
	NotifyNotification *notify;
#endif
	/* FIXME: Should this is_inbox be configurable? */
	if (!t->new || !t->is_inbox)
		return;

	g_static_mutex_lock (&mlock);

	if (!status_icon) {
		status_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_pixbuf (status_icon, e_icon_factory_get_icon ("stock_mail", E_ICON_SIZE_LARGE_TOOLBAR));
		gtk_status_icon_set_blinking (status_icon, TRUE);
	}
	
	folder = em_utils_folder_name_from_uri (t->uri);
	msg = g_strdup_printf (_("You have received %d new messages in %s."), t->new, folder);

	gtk_status_icon_set_tooltip (status_icon, msg);
	gtk_status_icon_set_visible (status_icon, TRUE);

#ifdef HAVE_LIBNOTIFY	
	if (!notify_init("notify-send"))
       		fprintf(stderr,"notify init error");

	notify  = notify_notification_new("New email in Evolution", 
					msg, 
					"stock_mail", 
					NULL);
	if(notify==NULL) 
		fprintf(stderr,"notify = NULL !!\n");
	else {
		notify_notification_set_urgency(notify, urgency);
		notify_notification_set_timeout(notify, expire_timeout);
		notify_notification_show(notify, NULL);
	}
#endif

	g_free (folder);
	g_free (msg);
	g_signal_connect (G_OBJECT (status_icon), "activate",
			  G_CALLBACK (icon_activated), NULL);
	g_static_mutex_unlock (&mlock);
}




int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (enable)
		enabled = TRUE;
	else
		enabled = FALSE;
	
	return 0;
}

