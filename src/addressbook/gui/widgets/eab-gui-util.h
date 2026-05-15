/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
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
gboolean	eab_fullname_matches_nickname	(EContact *contact);

G_END_DECLS

#endif /* __E_ADDRESSBOOK_UTIL_H__ */
