/* Evolution calendar - Send calendar component dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef SEND_COMP_H
#define SEND_COMP_H

#include <glib.h>
#include <gtk/gtkwindow.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>

gboolean send_component_dialog (GtkWindow *parent, ECal *client, ECalComponent *comp, gboolean new);
gboolean send_component_prompt_subject (GtkWindow *parent, ECal *client, ECalComponent *comp);

#endif
