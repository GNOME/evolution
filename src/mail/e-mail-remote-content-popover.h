/*
 * SPDX-FileCopyrightText: (C) 2019 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_REMOTE_CONTENT_POPOVER_H
#define E_MAIL_REMOTE_CONTENT_POPOVER_H

#include <gtk/gtk.h>
#include <mail/e-mail-reader.h>

G_BEGIN_DECLS

void		e_mail_remote_content_popover_run	(EMailReader *reader,
							 GtkWidget *parent,
							 const GtkAllocation *position);

G_END_DECLS

#endif /* E_MAIL_REMOTE_CONTENT_POPOVER_H */
