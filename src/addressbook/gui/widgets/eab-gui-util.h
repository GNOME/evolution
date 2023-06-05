/*
 *
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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_ADDRESSBOOK_UTIL_H__
#define __E_ADDRESSBOOK_UTIL_H__

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include <e-util/e-util.h>

G_BEGIN_DECLS

void		eab_error_dialog		(EAlertSink *alert_sink,
						 GtkWindow *parent,
						 const gchar *msg,
						 const GError *error);
void		eab_load_error_dialog		(GtkWidget *parent,
						 EAlertSink *alert_sink,
						 ESource *source,
						 const GError *error);
void		eab_search_result_dialog	(EAlertSink *alert_sink,
						 const GError *error);
gint		eab_prompt_save_dialog		(GtkWindow *parent);
void		eab_transfer_contacts		(ESourceRegistry *registry,
						 EBookClient *source_client,
						 GSList *contacts, /* adopted */
						 gboolean delete_from_source,
						 EAlertSink *alert_sink);
gchar *		eab_suggest_filename		(EContact *contact);
ESource *	eab_select_source		(ESourceRegistry *registry,
						 ESource *except_source,
						 const gchar *title,
						 const gchar *message,
						 const gchar *select_uid,
						 GtkWindow *parent);

gchar *		eab_format_address		(EContact *contact,
						 EContactField address_type);
gboolean	eab_fullname_matches_nickname	(EContact *contact);

G_END_DECLS

#endif /* __E_ADDRESSBOOK_UTIL_H__ */
