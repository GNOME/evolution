/*
 * e-summary-offline-handler.h:
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_OFFLINE_HANDLER_H__
#define __E_SUMMARY_OFFLINE_HANDLER_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-xobject.h>
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
	BonoboXObject parent;

	ESummaryOfflineHandlerPriv *priv;
};

struct _ESummaryOfflineHandlerClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Offline__epv epv;
};


GtkType e_summary_offline_handler_get_type (void);
ESummaryOfflineHandler *e_summary_offline_handler_new (void);
void e_summary_offline_handler_set_summary (ESummaryOfflineHandler *handler,
					    ESummary *summary);
#ifdef __cplusplus
}
#endif

#endif
