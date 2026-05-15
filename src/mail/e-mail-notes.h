/*
 * SPDX-FileCopyrightText: (C) 2015 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
