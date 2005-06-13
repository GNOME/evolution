/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __EXCHANGE_OPERATIONS_H__
#define __EXCHANGE_OPERATIONS_H__

#include <gtk/gtk.h>

#include "e-util/e-plugin.h"
#include "exchange-config-listener.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


extern ExchangeConfigListener *exchange_global_config_listener;

int 		e_plugin_lib_enable (EPluginLib *eplib, int enable);
gboolean 	exchange_operations_tokenize_string (char **string,
							char *token, 
							char delimit);
gboolean 	exchange_operations_cta_add_node_to_tree (GtkTreeStore *store,
							  GtkTreeIter *parent,
							  const char *nuri,
							  const char *ruri);
ExchangeAccount *exchange_operations_get_exchange_account (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
