/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-client.h
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

#ifndef _EXECUTIVE_SUMMARY_CLIENT_H__
#define _EXECUTIVE_SUMMARY_CLIENT_H__

#include <bonobo/bonobo-object.h>
#include <evolution-services/executive-summary-component.h>

#define EXECUTIVE_SUMMARY_CLIENT_TYPE (executive_summary_client_get_type ())
#define EXECUTIVE_SUMMARY_CLIENT(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_CLIENT_TYPE, ExecutiveSummaryClient))
#define EXECUTIVE_SUMMARY_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_CLIENT_TYPE, ExecutiveSummaryClientClass))
#define IS_EXECUTIVE_SUMMARY_CLIENT(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_CLIENT_TYPE))
#define IS_EXECUTIVE_SUMMARY_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_CLIENT_TYPE))

typedef struct _ExecutiveSummaryClientPrivate ExecutiveSummaryClientPrivate;
typedef struct _ExecutiveSummaryClient ExecutiveSummaryClient;
typedef struct _ExecutiveSummaryClientClass ExecutiveSummaryClientClass;

struct _ExecutiveSummaryClient {
	BonoboObjectClient parent;
	
	ExecutiveSummaryClientPrivate *private;
};

struct _ExecutiveSummaryClientClass {
	BonoboObjectClientClass parent_class;
};

GtkType executive_summary_client_get_type (void);
void executive_summary_client_construct (ExecutiveSummaryClient *client,
					 CORBA_Object object);
void executive_summary_client_set_title (ExecutiveSummaryClient *client,
					 int id,
					 const char *title);
void executive_summary_client_set_icon (ExecutiveSummaryClient *client,
					int id,
					const char *icon);
void executive_summary_client_flash (ExecutiveSummaryClient *client,
				     int id);
void executive_summary_client_update (ExecutiveSummaryClient *client,
				      int id,
				      const char *html);

#endif
