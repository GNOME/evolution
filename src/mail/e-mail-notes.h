/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_NOTES_H
#define E_MAIL_NOTES_H

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <camel/camel.h>

#define E_MAIL_NOTES_USER_FLAG "$has_note"
#define E_MAIL_NOTES_HEADER "X-Evolution-Note"

G_BEGIN_DECLS

void		e_mail_notes_edit		(GtkWindow *parent,
						 CamelFolder *folder,
						 const gchar *uid);
gboolean	e_mail_notes_remove_sync	(CamelFolder *folder,
						 const gchar *uid,
						 GCancellable *cancellable,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_NOTES_H */
