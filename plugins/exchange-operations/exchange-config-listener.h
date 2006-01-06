/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __EXCHANGE_CONFIG_LISTENER_H__
#define __EXCHANGE_CONFIG_LISTENER_H__

#include <exchange-constants.h>
#include <exchange-account.h>

#include "exchange-types.h"
#include "libedataserver/e-account-list.h"
#include <libedataserver/e-source-list.h>
#include <libedataserver/e-source-group.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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

void 			add_folder_esource (ExchangeAccount *account, FolderType folder_type, const char *folder_name, const char *physical_uri);
void 			remove_folder_esource (ExchangeAccount *account, FolderType folder_type, const char *physical_uri);
ExchangeConfigListenerStatus exchange_config_listener_get_offline_status (ExchangeConfigListener *excl, gint *mode);

void exchange_config_listener_modify_esource_group_name (ExchangeConfigListener *excl,
							 const char *old_name, 
							 const char *new_name);

ExchangeAccountResult exchange_config_listener_authenticate (ExchangeConfigListener *excl, 
							ExchangeAccount *account);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXCHANGE_CONFIG_LISTENER_H__ */
