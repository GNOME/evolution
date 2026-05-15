/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
 */

#ifndef _CERTIFICATE_MANAGER_H_
#define _CERTIFICATE_MANAGER_H_

#include <gtk/gtk.h>
#include <shell/e-shell.h>

/* Standard GObject macros */
#define E_TYPE_CERT_MANAGER_CONFIG \
	(e_cert_manager_config_get_type ())
#define E_CERT_MANAGER_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CERT_MANAGER_CONFIG, ECertManagerConfig))
#define E_CERT_MANAGER_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CERT_MANAGER_CONFIG, ECertManagerConfigClass))
#define E_IS_CERT_MANAGER_CONFIG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CERT_MANAGER_CONFIG))
#define E_IS_CERT_MANAGER_CONFIG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CERT_MANAGER_CONFIG))
#define E_CERT_MANAGER_CONFIG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CERT_MANAGER_CONFIG, ECertManagerConfigClass))

typedef struct _ECertManagerConfig ECertManagerConfig;
typedef struct _ECertManagerConfigClass ECertManagerConfigClass;
typedef struct _ECertManagerConfigPrivate ECertManagerConfigPrivate;

struct _ECertManagerConfig {
	GtkBox parent;
	ECertManagerConfigPrivate *priv;
};

struct _ECertManagerConfigClass {
	GtkBoxClass parent_class;
};

G_BEGIN_DECLS

GType	  e_cert_manager_config_get_type (void) G_GNUC_CONST;

GtkWidget *e_cert_manager_config_new (EPreferencesWindow *window);

struct _ECert; /* forward declaration */
GtkWidget *e_cert_manager_new_certificate_viewer (GtkWindow *parent,
						  struct _ECert *cert);

G_END_DECLS

#endif /* _CERTIFICATE_MANAGER_H_ */
