/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef SEND_COMP_H
#define SEND_COMP_H

#include <gtk/gtk.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>

gboolean send_component_dialog (GtkWindow *parent, ECal *client, ECalComponent *comp, gboolean new, gboolean *strip_alarms, gboolean *only_new_attendees);
gboolean send_component_prompt_subject (GtkWindow *parent, ECal *client, ECalComponent *comp);

#endif
