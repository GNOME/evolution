/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Srinivasa Ragavan <sragavan@novell.com>
 *  Copyright (C) 2007 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef EXCHANGE_ACCOUNT_LISTENER_H
#define EXCHANGE_ACCOUNT_LISTENER_H

#include <libedataserver/e-account-list.h>
#include<libedataserver/e-source.h>
#include<libedataserver/e-source-list.h>
#include <camel/camel-url.h>
                         
G_BEGIN_DECLS

#define EXCHANGE_TYPE_ACCOUNT_LISTENER            (exchange_account_listener_get_type ())
#define EXCHANGE_ACCOUNT_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXCHANGE_TYPE_ACCOUNT_LISTENER, ExchangeAccountListener))
#define EXCHANGE_ACCOUNT_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXCHANGE_TYPE_ACCOUNT_LISTENER,  ExchangeAccountListenerClass))
#define EXCHANGE_IS_ACCOUNT_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXCHANGE_TYPE_ACCOUNT_LISTENER))
#define EXCHANGE_IS_ACCOUNT_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EXCHANGE_TYPE_ACCOUNT_LISTENER))

typedef struct _ExchangeAccountListener ExchangeAccountListener;
typedef struct _ExchangeAccountListenerClass ExchangeAccountListenerClass;
typedef struct _ExchangeAccountListenerPrivate ExchangeAccountListenerPrivate;
struct _ExchangeAccountListener {
       GObject parent;
                                                                                                                        
       ExchangeAccountListenerPrivate *priv;
};

struct _ExchangeAccountListenerClass {
       GObjectClass parent_class;     
};

GType                   exchange_account_listener_get_type (void);
ExchangeAccountListener *exchange_account_listener_new (void);
void exchange_account_fetch_folders ();


G_END_DECLS

#endif
