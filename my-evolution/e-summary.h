/*
 * e-summary.h: Header file for the ESummary object.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef _E_SUMMARY_H__
#define _E_SUMMARY_H__

#include <gtk/gtkvbox.h>
#include "e-summary-type.h"
#include "e-summary-mail.h"
#include "e-summary-calendar.h"
#include "e-summary-rdf.h"
#include "e-summary-weather.h"

#include <Evolution.h>

#define E_SUMMARY_TYPE (e_summary_get_type ())
#define E_SUMMARY(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TYPE, ESummary))
#define E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE, ESummaryClass))
#define IS_E_SUMMARY(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TYPE))
#define IS_E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TYPE))

typedef struct _ESummaryPrivate ESummaryPrivate;
typedef struct _ESummaryClass ESummaryClass;

typedef void (* ESummaryProtocolListener) (ESummary *summary,
					   const char *uri,
					   void *closure);

struct _ESummary {
	GtkVBox parent;

	ESummaryMail *mail;
	ESummaryCalendar *calendar;
	ESummaryRDF *rdf;
	ESummaryWeather *weather;

	ESummaryPrivate *priv;

	GNOME_Evolution_ShellView shell_view_interface;
};

struct _ESummaryClass {
	GtkVBoxClass parent_class;
};

GtkType e_summary_get_type (void);
GtkWidget *e_summary_new (const GNOME_Evolution_Shell shell);
void e_summary_print (GtkWidget *widget,
		      ESummary *summary);
void e_summary_draw (ESummary *summary);
void e_summary_change_current_view (ESummary *summary,
				    const char *uri);
void e_summary_set_message (ESummary *summary,
			    const char *message,
			    gboolean busy);
void e_summary_unset_message (ESummary *summary);
void e_summary_add_protocol_listener (ESummary *summary,
				      const char *protocol,
				      ESummaryProtocolListener listener,
				      void *closure);

#endif
