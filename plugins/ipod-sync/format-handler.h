/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Philip Van Hoof <pvanhoof@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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

	void (*save) (FormatHandler *handler, EPlugin *ep, ECalPopupTargetSource *target, ECalSourceType type, gchar *dest_uri);
};

FormatHandler *ical_format_handler_new (void);

GOutputStream *open_for_writing (GtkWindow *parent, const gchar *uri, GError **error);
