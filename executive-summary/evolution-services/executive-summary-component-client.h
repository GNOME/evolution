/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-component-client.h
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 */

#ifndef _EXECUTIVE_SUMMARY_COMPONENT_CLIENT_H__
#define _EXECUTIVE_SUMMARY_COMPONENT_CLIENT_H__

#include <bonobo/bonobo-object.h>
#include <evolution-services/executive-summary.h>
#include <evolution-services/executive-summary-component-view.h>

#define EXECUTIVE_SUMMARY_COMPONENT_CLIENT_TYPE (executive_summary_component_client_get_type ())
#define EXECUTIVE_SUMMARY_COMPONENT_CLIENT(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_COMPONENT_CLIENT_TYPE, ExecutiveSummaryComponentClient))
#define EXECUTIVE_SUMMARY_COMPONENT_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_COMPONENT_CLIENT_TYPE, ExecutiveSummaryComponentClientClass))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_CLIENT_TYPE))
#define IS_EXECUTIVE_SUMMARY_COMPONENT_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_COMPONENT_CLIENT_TYPE))

typedef struct _ExecutiveSummaryComponentClientPrivate ExecutiveSummaryComponentClientPrivate;
typedef struct _ExecutiveSummaryComponentClient ExecutiveSummaryComponentClient;
typedef struct _ExecutiveSummaryComponentClientClass ExecutiveSummaryComponentClientClass;

struct _ExecutiveSummaryComponentClient {
  BonoboObjectClient parent;

  ExecutiveSummaryComponentClientPrivate *private;
};

struct _ExecutiveSummaryComponentClientClass {
  BonoboObjectClientClass parent_class;
};

GtkType executive_summary_component_client_get_type (void);
ExecutiveSummaryComponentClient *executive_summary_component_client_new (const char *id);

void executive_summary_component_client_set_owner (ExecutiveSummaryComponentClient *client,
						   ExecutiveSummary *summary);
void executive_summary_component_client_unset_owner (ExecutiveSummaryComponentClient *client);

void executive_summary_component_client_supports (ExecutiveSummaryComponentClient *client,
						  gboolean *bonobo,
						  gboolean *html);

void executive_summary_component_client_configure (ExecutiveSummaryComponentClient *client,
						   ExecutiveSummaryComponentView *view);
void executive_summary_component_client_destroy_view (ExecutiveSummaryComponentClient *client,
						      ExecutiveSummaryComponentView *view);

#endif

