/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _CA_TRUST_DIALOG_H_
#define _CA_TRUST_DIALOG_H

#include <gtk/gtkwidget.h>
#include "e-cert.h"

GtkWidget* ca_trust_dialog_show (ECert *cert, gboolean importing);

void       ca_trust_dialog_set_trust (GtkWidget *widget, gboolean ssl, gboolean email, gboolean objsign);
void       ca_trust_dialog_get_trust (GtkWidget *widget, gboolean *ssl, gboolean *email, gboolean *objsign);

#endif /* _CA_TRUST_DIALOG_H_ */
