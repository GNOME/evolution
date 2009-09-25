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

#ifndef __EXCHANGE_FOLDER_SUBSCRIPTION_H__
#define __EXCHANGE_FOLDER_SUBSCRIPTION_H__

#include <glib.h>
#include <libedataserver/e-source.h>
#include <exchange-account.h>

gboolean
create_folder_subscription_dialog (ExchangeAccount *account, const gchar *fname);

void call_folder_subscribe (const gchar *folder_name);
void call_folder_unsubscribe (const gchar *folder_type, const gchar *uri, ESource *source);

#endif
