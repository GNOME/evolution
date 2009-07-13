/*
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
 *		Veerapuram Varadhan  <vvaradhan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_NETWORK_PREFS_H__
#define __EM_NETWORK_PREFS_H__

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#define EM_NETWORK_PREFS_TYPE        (em_network_prefs_get_type ())
#define EM_NETWORK_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_NETWORK_PREFS_TYPE, EMNetworkPrefs))
#define EM_NETWORK_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_NETWORK_PREFS_TYPE, EMNetworkPrefsClass))
#define EM_IS_NETWORK_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_NETWORK_PREFS_TYPE))
#define EM_IS_NETWORK_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_NETWORK_PREFS_TYPE))

G_BEGIN_DECLS

typedef struct _EMNetworkPrefs EMNetworkPrefs;
typedef struct _EMNetworkPrefsClass EMNetworkPrefsClass;

typedef enum {
	NETWORK_PROXY_SYS_SETTINGS,
	NETWORK_PROXY_DIRECT_CONNECTION,
	NETWORK_PROXY_MANUAL,
	NETWORK_PROXY_AUTOCONFIG
} NetworkConfigProxyType;

struct _EMNetworkPrefs {
	GtkVBox parent_object;

	GConfClient *gconf;

	GladeXML *gui;

	/* Default Behavior */
	GtkToggleButton *sys_proxy;
	GtkToggleButton *no_proxy;
	GtkToggleButton *manual_proxy;
#if 0
	GtkToggleButton *auto_proxy;
#endif
	GtkToggleButton *use_auth;

	GtkEntry *http_host;
	GtkEntry *https_host;
	GtkEntry *ignore_hosts;
#if 0
	GtkEntry *auto_proxy_url;
#endif
	GtkEntry *auth_user;
	GtkEntry *auth_pwd;

	GtkLabel *lbl_http_host;
	GtkLabel *lbl_http_port;
	GtkLabel *lbl_https_host;
	GtkLabel *lbl_https_port;
	GtkLabel *lbl_ignore_hosts;
	GtkLabel *lbl_auth_user;
	GtkLabel *lbl_auth_pwd;

	GtkSpinButton *http_port;
	GtkSpinButton *https_port;
#if 0
	GtkLabel *lbl_socks_host;
	GtkEntry *socks_host;
	GtkLabel *lbl_socks_port;
	GtkSpinButton *socks_port;
#endif
};

struct _EMNetworkPrefsClass {
	GtkVBoxClass parent_class;

	/* signals */

};

GType em_network_prefs_get_type (void);

GtkWidget *em_network_prefs_new (void);

/* needed by global config */
#define EM_NETWORK_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_NetworkPrefs_ConfigControl:" BASE_VERSION

G_END_DECLS

#endif /* __EM_NETWORK_PREFS_H__ */
