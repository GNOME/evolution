/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-subwindow.h
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

#ifndef _E_SUMMARY_SUBWINDOW_H__
#define _E_SUMMARY_SUBWINDIW_H__

#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas.h>

#define E_SUMMARY_SUBWINDOW_TYPE (e_summary_subwindow_get_type ())
#define E_SUMMARY_SUBWINDOW(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_SUBWINDOW_TYPE, ESummarySubwindow))
#define E_SUMMARY_SUBWINDOW_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_SUBWINDOW_TYPE, ESummarySubwindowClass))
#define IS_E_SUMMARY_SUBWINDOW(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_SUBWINDOW_TYPE))
#define IS_E_SUMMARY_SUBWINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_SUBWINDOW_TYPE))

typedef struct _ESummarySubwindowPrivate ESummarySubwindowPrivate;
typedef struct _ESummarySubwindow ESummarySubwindow;
typedef struct _ESummarySubwindowClass ESummarySubwindowClass;

struct _ESummarySubwindow {
  GnomeCanvasGroup parent;

  ESummarySubwindowPrivate *private;
};

struct _ESummarySubwindowClass {
  GnomeCanvasGroupClass parent_class;

  void (*close_clicked) (ESummarySubwindow *window);
  void (*shade_clicked) (ESummarySubwindow *window);
  void (*edit_clicked) (ESummarySubwindow *window);
};

GtkType e_summary_subwindow_get_type (void);

void e_summary_subwindow_construct (GnomeCanvasItem *subwindow);
GnomeCanvasItem *e_summary_subwindow_new (GnomeCanvasGroup *parent,
					  double x,
					  double y);

void e_summary_subwindow_add (ESummarySubwindow *subwindow,
			      GtkWidget *widget);
void e_summary_subwindow_remove (ESummarySubwindow *subwindow,
				 GtkWidget *widget);
void e_summary_subwindow_set_title (ESummarySubwindow *subwindow,
				    const char *title);

#endif
