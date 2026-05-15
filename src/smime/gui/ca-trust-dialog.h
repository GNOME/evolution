/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
 */

#ifndef CA_TRUST_DIALOG_H
#define CA_TRUST_DIALOG_H

#include <gtk/gtk.h>
#include "e-cert.h"

GtkWidget * ca_trust_dialog_show (ECert *cert, gboolean importing);

void       ca_trust_dialog_set_trust (GtkWidget *widget, gboolean ssl, gboolean email, gboolean objsign);
void       ca_trust_dialog_get_trust (GtkWidget *widget, gboolean *ssl, gboolean *email, gboolean *objsign);

#endif /* CA_TRUST_DIALOG_H */
