/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* executive-summary-html-view.h
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

#ifndef _EXECUTIVE_SUMMARY_HTML_VIEW_H__
#define _EXECUTIVE_SUMMARY_HTML_VIEW_H__

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-event-source.h>

#define EXECUTIVE_SUMMARY_HTML_VIEW_HTML_CHANGED "GNOME:Evolution:Summary:HTMLView:html_changed"

#define EXECUTIVE_SUMMARY_HTML_VIEW_TYPE (executive_summary_html_view_get_type ())
#define EXECUTIVE_SUMMARY_HTML_VIEW(obj) (GTK_CHECK_CAST ((obj), EXECUTIVE_SUMMARY_HTML_VIEW_TYPE, ExecutiveSummaryHtmlView))
#define EXECUTIVE_SUMMARY_HTML_VIEW_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EXECUTIVE_SUMMARY_HTML_VIEW_TYPE, ExecutiveSummaryHtmlViewClass))
#define IS_EXECUTIVE_SUMMARY_HTML_VIEW(obj) (GTK_CHECK_TYPE ((obj), EXECUTIVE_SUMMARY_HTML_VIEW_TYPE))
#define IS_EXECUTIVE_SUMMARY_HTML_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EXECUTIVE_SUMMARY_HTML_VIEW_TYPE))

typedef struct _ExecutiveSummaryHtmlViewPrivate ExecutiveSummaryHtmlViewPrivate;
typedef struct _ExecutiveSummaryHtmlView ExecutiveSummaryHtmlView;
typedef struct _ExecutiveSummaryHtmlViewClass ExecutiveSummaryHtmlViewClass;

struct _ExecutiveSummaryHtmlView {
	BonoboObject parent;
	
	ExecutiveSummaryHtmlViewPrivate *private;
};

struct _ExecutiveSummaryHtmlViewClass {
	BonoboObjectClass parent_class;
};

GtkType executive_summary_html_view_get_type (void);
BonoboObject *executive_summary_html_view_new_full (BonoboEventSource *event_source);
BonoboObject *executive_summary_html_view_new (void);

void executive_summary_html_view_set_html (ExecutiveSummaryHtmlView *view,
					   const char *html);
const char *executive_summary_html_view_get_html (ExecutiveSummaryHtmlView *view);
BonoboEventSource *executive_summary_html_view_get_event_source (ExecutiveSummaryHtmlView *view);

#endif

