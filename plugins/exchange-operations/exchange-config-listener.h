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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EXCHANGE_CONFIG_LISTENER_H__
#define __EXCHANGE_CONFIG_LISTENER_H__

#include <exchange-constants.h>
#include <exchange-account.h>

#include "exchange-types.h"
#include "libedataserver/e-account-list.h"
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

G_BEGIN_DECLS

typedef enum {
	CONFIG_LISTENER_STATUS_OK,
	CONFIG_LISTENER_STATUS_NOT_FOUND
} ExchangeConfigListenerStatus;

#define EXCHANGE_TYPE_CONFIG_LISTENER            (exchange_config_listener_get_type ())
#define EXCHANGE_CONFIG_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXCHANGE_TYPE_CONFIG_LISTENER, ExchangeConfigListener))
#define EXCHANGE_CONFIG_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXCHANGE_TYPE_CONFIG_LISTENER, ExchangeConfigListenerClass))
#define EXCHANGE_IS_CONFIG_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXCHANGE_TYPE_CONFIG_LISTENER))
#define EXCHANGE_IS_CONFIG_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EXCHANGE_TYPE_CONFIG_LISTENER))

struct _ExchangeConfigListener {
	EAccountList parent;

	ExchangeConfigListenerPrivate *priv;
};

struct _ExchangeConfigListenerClass {
	EAccountListClass parent_class;

	/* signals */
	void (*exchange_account_created) (ExchangeConfigListener *,
					  ExchangeAccount *);
	void (*exchange_account_removed) (ExchangeConfigListener *,
					  ExchangeAccount *);
};

#define CONF_KEY_CAL "/apps/evolution/calendar/sources"
#define CONF_KEY_TASKS "/apps/evolution/tasks/sources"
#define CONF_KEY_CONTACTS "/apps/evolution/addressbook/sources"
#define EXCHANGE_URI_PREFIX "exchange://"

GType                   exchange_config_listener_get_type (void);
ExchangeConfigListener *exchange_config_listener_new      (void);

GSList                 *exchange_config_listener_get_accounts (ExchangeConfigListener *config_listener);

void			add_folder_esource (ExchangeAccount *account, FolderType folder_type, const gchar *folder_name, const gchar *physical_uri);
void			remove_folder_esource (ExchangeAccount *account, FolderType folder_type, const gchar *physical_uri);
ExchangeConfigListenerStatus exchange_config_listener_get_offline_status (ExchangeConfigListener *excl, gint *mode);

void exchange_config_listener_modify_esource_group_name (ExchangeConfigListener *excl,
							 const gchar *old_name,
							 const gchar *new_name);

ExchangeAccountResult exchange_config_listener_authenticate (ExchangeConfigListener *excl,
							ExchangeAccount *account);

G_END_DECLS

#endif /* __EXCHANGE_CONFIG_LISTENER_H__ */
