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
#include <evolution-services/executive-summary.h>
#include <evolution-services/executive-summary-component-client.h>
#include <evolution-services/executive-summary-component-view.h>

#include <Evolution.h>

#define E_SUMMARY_TYPE (e_summary_get_type ())
#define E_SUMMARY(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TYPE, ESummary))
#define E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE, ESummaryClass))
#define IS_E_SUMMARY(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TYPE))
#define IS_E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TYPE))

typedef struct _ESummaryPrivate ESummaryPrivate;
typedef struct _ESummary ESummary;
typedef struct _ESummaryClass ESummaryClass;
typedef struct _ESummaryWindow ESummaryWindow;

struct _ESummaryWindow {
	ExecutiveSummary *summary;
	ExecutiveSummaryComponentView *view;
	char *iid;
};

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
void e_summary_window_free (ESummaryWindow *window,
			    ESummary *esummary);
void  e_summary_window_remove_from_ht (ESummaryWindow *window,
				       ESummary *esummary);
void e_summary_add_service (ESummary *esummary,
			    ExecutiveSummary *summary,
			    ExecutiveSummaryComponentView *view,
			    const char *iid);
ExecutiveSummaryComponentView * e_summary_view_from_id (ESummary *esummary,
							int id);
void e_summary_set_shell_view_interface (ESummary *summary,
					 Evolution_ShellView svi);
void e_summary_set_message (ESummary *esummary,
			    const char *message,
			    gboolean busy);
void e_summary_unset_message (ESummary *esummary);
void e_summary_change_current_view (ESummary *esummary,
				    const char *uri);
void e_summary_set_title (ESummary *esummary,
			  const char *title);
ESummaryWindow *e_summary_window_from_view (ESummary *esummary,
					    ExecutiveSummaryComponentView *view);
void e_summary_window_move_left (ESummary *esummary,
				 ESummaryWindow *window);
void e_summary_window_move_right (ESummary *esummary,
				  ESummaryWindow *window);
void e_summary_window_move_up (ESummary *esummary,
			       ESummaryWindow *window);
void e_summary_window_move_down (ESummary *esummary,
				 ESummaryWindow *window);

#endif
