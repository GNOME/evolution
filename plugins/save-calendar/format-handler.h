/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Philip Van Hoof <pvanhoof@gnome.org>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <calendar/gui/e-cal-popup.h>

typedef struct _FormatHandler FormatHandler;

struct _FormatHandler
{
	gboolean isdefault;
	const gchar *combo_label;
	const gchar *filename_ext;
	GtkWidget *options_widget;

	gpointer data;

	void (*save) (FormatHandler *handler, EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, char *dest_uri);
};

FormatHandler *csv_format_handler_new (void);
FormatHandler *ical_format_handler_new (void);
FormatHandler *rdf_format_handler_new (void);

GOutputStream *open_for_writing (GtkWindow *parent, const char *uri, GError **error);
