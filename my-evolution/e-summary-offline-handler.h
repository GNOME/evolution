/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-offline-handler.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: 
 * Ettore Perazzoli <ettore@ximian.com>
 * Dan Winship <danw@ximian.com>
 * Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_OFFLINE_HANDLER_H__
#define __E_SUMMARY_OFFLINE_HANDLER_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include "e-summary.h"
#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#define E_SUMMARY_TYPE_OFFLINE_HANDLER (e_summary_offline_handler_get_type ())
#define E_SUMMARY_OFFLINE_HANDLER(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TYPE_OFFLINE_HANDLER, ESummaryOfflineHandler))
#define E_SUMMARY_OFFLINE_HANDLER_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE_OFFLINE_HANDLER, ESummaryOfflineHandlerClass))


typedef struct _ESummaryOfflineHandler ESummaryOfflineHandler;
typedef struct _ESummaryOfflineHandlerPriv ESummaryOfflineHandlerPriv;
typedef struct _ESummaryOfflineHandlerClass ESummaryOfflineHandlerClass;

struct _ESummaryOfflineHandler {
	BonoboObject parent;

	ESummaryOfflineHandlerPriv *priv;
};

struct _ESummaryOfflineHandlerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Offline__epv epv;
};


GtkType                 e_summary_offline_handler_get_type  (void);
ESummaryOfflineHandler *e_summary_offline_handler_new       (void);

void  e_summary_offline_handler_add_summary  (ESummaryOfflineHandler *handler,
					      ESummary               *summary);

#ifdef __cplusplus
}
#endif

#endif
