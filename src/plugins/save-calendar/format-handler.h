/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Philip Van Hoof <pvanhoof@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <calendar/gui/itip-utils.h>
#include <calendar/gui/comp-util.h>

typedef struct _FormatHandler FormatHandler;

struct _FormatHandler
{
	gboolean isdefault;
	const gchar *combo_label;
	const gchar *filename_ext;
	GtkWidget *options_widget;

	gpointer data;

	void	(*save)		(FormatHandler *handler,
				 ESourceSelector *selector,
				 EClientCache *client_cache,
				 gchar *dest_uri);
};

FormatHandler *csv_format_handler_new (void);
FormatHandler *ical_format_handler_new (void);
FormatHandler *rdf_format_handler_new (void);

GOutputStream *open_for_writing (GtkWindow *parent, const gchar *uri, GError **error);
