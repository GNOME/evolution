/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-titlebar.h
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

#ifndef _E_SUMMARY_TITLEBAR_H__
#define _E_SUMMARY_TITLEBAR_H__

#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas.h>

#define E_SUMMARY_TITLEBAR_TYPE (e_summary_titlebar_get_type ())
#define E_SUMMARY_TITLEBAR(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TITLEBAR_TYPE, ESummaryTitlebar))
#define E_SUMMARY_TITLEBAR_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TITLEBAR_TYPE, ESummaryTitlebarClass))
#define IS_E_SUMMARY_TITLEBAR(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TITLEBAR_TYPE))
#define IS_E_SUMMARY_TITLEBAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TITLEBAR_TYPE))

typedef struct _ESummaryTitlebarPrivate ESummaryTitlebarPrivate;
typedef struct _ESummaryTitlebar ESummaryTitlebar;
typedef struct _ESummaryTitlebarClass ESummaryTitlebarClass;

struct _ESummaryTitlebar {
  GnomeCanvasGroup parent;

  ESummaryTitlebarPrivate *private;
};

struct _ESummaryTitlebarClass {
  GnomeCanvasGroupClass parent_class;

  void (*close) (ESummaryTitlebar *window);
  void (*shade) (ESummaryTitlebar *window);
  void (*edit) (ESummaryTitlebar *window);
};

GtkType e_summary_titlebar_get_type (void);

#endif
