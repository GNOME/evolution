/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary.h
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 */

#ifndef _E_SUMMARY_H__
#define _E_SUMMARY_H__

#include <gtk/gtksignal.h>
#include <gtkhtml/gtkhtml.h>
#include <gtk/gtkvbox.h>
#include <executive-summary.h>
#include <executive-summary-component-client.h>

#include "shell/Evolution.h"

#define E_SUMMARY_TYPE (e_summary_get_type ())
#define E_SUMMARY(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TYPE, ESummary))
#define E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE, ESummaryClass))
#define IS_E_SUMMARY(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TYPE))
#define IS_E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TYPE))

typedef struct _ESummaryPrivate ESummaryPrivate;
typedef struct _ESummary ESummary;
typedef struct _ESummaryClass ESummaryClass;

struct _ESummary {
  GtkVBox parent;

  ESummaryPrivate *private;
};

struct _ESummaryClass {
  GtkVBoxClass parent_class;
};

GtkType e_summary_get_type (void);
GtkWidget *e_summary_new (const Evolution_Shell shell);
int e_summary_rebuild_page (ESummary *esummary);
void e_summary_add_html_service (ESummary *esummary,
				 ExecutiveSummary *summary,
				 ExecutiveSummaryComponentClient *client,
				 const char *html,
				 const char *title,
				 const char *icon);
void e_summary_add_bonobo_service (ESummary *esummary,
				   ExecutiveSummary *summary,
				   ExecutiveSummaryComponentClient *client,
				   GtkWidget *control,
				   const char *title,
				   const char *icon);
void e_summary_update_window (ESummary *esummary,
			      ExecutiveSummary *summary,
			      const char *html);

#endif
