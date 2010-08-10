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

#ifndef EM_NETWORK_PREFS_H
#define EM_NETWORK_PREFS_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <widgets/misc/e-preferences-window.h>

/* Standard GObject macros */
#define EM_TYPE_NETWORK_PREFS \
	(em_network_prefs_get_type ())
#define EM_NETWORK_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_NETWORK_PREFS, EMNetworkPrefs))
#define EM_NETWORK_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_NETWORK_PREFS, EMNetworkPrefsClass))
#define EM_IS_NETWORK_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_NETWORK_PREFS))
#define EM_IS_NETWORK_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_NETWORK_PREFS))
#define EM_NETWORK_PREFS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_NETWORK_PREFS, EMNetworkPrefsClass))

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

	GtkBuilder *builder;

	/* Default Behavior */
	GtkToggleButton *sys_proxy;
	GtkToggleButton *no_proxy;
	GtkToggleButton *manual_proxy;
	GtkToggleButton *use_auth;

	GtkEntry *http_host;
	GtkEntry *https_host;
	GtkEntry *socks_host;
	GtkEntry *ignore_hosts;
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
};

struct _EMNetworkPrefsClass {
	GtkVBoxClass parent_class;
};

GType	   em_network_prefs_get_type (void);
GtkWidget *em_network_prefs_new      (EPreferencesWindow *window);

G_END_DECLS

#endif /* EM_NETWORK_PREFS_H */
