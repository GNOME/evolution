/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary.h
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

#ifndef _EXECUTIVE_SUMMARY_H__
#define _EXECUTIVE_SUMMARY_H__

#include <gtk/gtksignal.h>
#include <bonobo.h>
#include "Executive-Summary.h"

#define EXECUTIVE_SUMMARY_TYPE (executive_summary_get_type ())
#define EXECUTIVE_SUMMARY(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_TYPE, ExecutiveSummary))
#define EXECUTIVE_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_TYPE, ExecutiveSummaryClass))
#define IS_EXECUTIVE_SUMMARY(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_TYPE))
#define IS_EXECUTIVE_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_TYPE))

typedef struct _ExecutiveSummaryPrivate ExecutiveSummaryPrivate;
typedef struct _ExecutiveSummary ExecutiveSummary;
typedef struct _ExecutiveSummaryClass ExecutiveSummaryClass;

struct _ExecutiveSummary {
	BonoboObject parent;
	
	ExecutiveSummaryPrivate *private;
};

struct _ExecutiveSummaryClass {
	BonoboObjectClass parent_class;

	void (* update) (ExecutiveSummary *summary,
			 const GNOME_Evolution_Summary_Component component,
			 const char *html);
	void (* set_title) (ExecutiveSummary *summary,
			    const GNOME_Evolution_Summary_Component component,
			    const char *title);
	void (* set_icon) (ExecutiveSummary *summary,
			   const GNOME_Evolution_Summary_Component component,
			   const char *icon);
	void (* flash) (ExecutiveSummary *summary,
			const GNOME_Evolution_Summary_Component component);
};

GtkType executive_summary_get_type (void);
void executive_summary_construct (ExecutiveSummary *es,
				  GNOME_Evolution_Summary_ViewFrame corba_object);
BonoboObject *executive_summary_new (void);
#endif

