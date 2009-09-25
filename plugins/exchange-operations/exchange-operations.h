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
 *		Praveen Kumar <kpraveen@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EXCHANGE_OPERATIONS_H__
#define __EXCHANGE_OPERATIONS_H__

#include <gtk/gtk.h>

#include "e-util/e-plugin.h"
#include "exchange-config-listener.h"
#include <exchange-account.h>

G_BEGIN_DECLS

#define ERROR_DOMAIN "org-gnome-exchange-operations"

extern ExchangeConfigListener *exchange_global_config_listener;

gint e_plugin_lib_enable (EPlugin *eplib, gint enable);

ExchangeAccount *exchange_operations_get_exchange_account (void);
ExchangeConfigListenerStatus exchange_is_offline (gint *mode);

gboolean exchange_operations_tokenize_string (gchar **string, gchar *token, gchar delimit, guint maxsize);

gboolean exchange_operations_cta_add_node_to_tree (GtkTreeStore *store, GtkTreeIter *parent, const gchar *nuri);
void exchange_operations_cta_select_node_from_tree (GtkTreeStore *store, GtkTreeIter *parent, const gchar *nuri, const gchar *ruri, GtkTreeSelection *selection);

void exchange_operations_report_error (ExchangeAccount *account, ExchangeAccountResult result);

void exchange_operations_update_child_esources (ESource *source, const gchar *old_path, const gchar *new_path);

gboolean is_exchange_personal_folder (ExchangeAccount *account, gchar *uri);

G_END_DECLS

#endif
