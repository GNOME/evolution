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

#ifndef CA_TRUST_DIALOG_H
#define CA_TRUST_DIALOG_H

#include <gtk/gtk.h>
#include "e-cert.h"

GtkWidget * ca_trust_dialog_show (ECert *cert, gboolean importing);

void       ca_trust_dialog_set_trust (GtkWidget *widget, gboolean ssl, gboolean email, gboolean objsign);
void       ca_trust_dialog_get_trust (GtkWidget *widget, gboolean *ssl, gboolean *email, gboolean *objsign);

#endif /* CA_TRUST_DIALOG_H */
