/*
 * Copyright (C) 2019 Red Hat (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of version 2.1. of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
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
