/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Philip Van Hoof <pvanhoof@gnome.org>
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
