/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-component.h
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

#ifndef __EXECUTIVE_SUMMARY_COMPONENT_VIEW_H__
#define __EXECUTIVE_SUMMARY_COMPONENT_VIEW_H__

#include <evolution-services/executive-summary-component.h>

#define EXECUTIVE_SUMMARY_COMPONENT_VIEW_TYPE (executive_summary_component_view_get_type ())
#define EXECUTIVE_SUMMARY_COMPONENT_VIEW(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_COMPONENT_VIEW_TYPE, ExecutiveSummaryComponentView))
#define EXECUTIVE_SUMMARY_COMPONENT_VIEW_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_COMPONENT_VIEW_TYPE, ExecutiveSummaryComponentClass))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_VIEW_TYPE))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_VIEW_CLASS_TYPE))

typedef struct _ExecutiveSummaryComponentViewPrivate ExecutiveSummaryComponentViewPrivate;
typedef struct _ExecutiveSummaryComponentView ExecutiveSummaryComponentView;
typedef struct _ExecutiveSummaryComponentViewClass ExecutiveSummaryComponentViewClass;

struct _ExecutiveSummaryComponentView {
  GtkObject object;

  ExecutiveSummaryComponentViewPrivate *private;
};

struct _ExecutiveSummaryComponentViewClass {
  GtkObjectClass parent_class;
};

GtkType executive_summary_component_view_get_type (void);
void 
executive_summary_component_view_construct (ExecutiveSummaryComponentView *view,
					    ExecutiveSummaryComponent *component,
					    BonoboControl *control,
					    const char *html,
					    const char *title,
					    const char *icon);
ExecutiveSummaryComponentView *
executive_summary_component_view_new (ExecutiveSummaryComponent *component,
				      BonoboControl *control,
				      const char *html,
				      const char *title,
				      const char *icon);

void executive_summary_component_view_set_title (ExecutiveSummaryComponentView *view,
						 const char *title);
const char *executive_summary_component_view_get_title (ExecutiveSummaryComponentView *view);

void executive_summary_component_view_set_icon (ExecutiveSummaryComponentView *view,
						const char *icon);
const char *executive_summary_component_view_get_icon (ExecutiveSummaryComponentView *view);

void executive_summary_component_view_flash (ExecutiveSummaryComponentView *view);

void executive_summary_component_view_set_html (ExecutiveSummaryComponentView *view,
						const char *html);
const char *executive_summary_component_view_get_html (ExecutiveSummaryComponentView *view);
BonoboObject *executive_summary_component_view_get_control (ExecutiveSummaryComponentView *view);

int executive_summary_component_view_get_id (ExecutiveSummaryComponentView *view);

#endif


