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
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef SEND_COMP_H
#define SEND_COMP_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

gboolean send_component_dialog (GtkWindow *parent, ECalClient *client, ECalComponent *comp, gboolean new, gboolean *strip_alarms, gboolean *only_new_attendees);
gboolean send_component_prompt_subject (GtkWindow *parent, ECalClient *client, ECalComponent *comp);
GtkResponseType send_dragged_or_resized_component_dialog (GtkWindow *parent, ECalClient *client, ECalComponent *comp, gboolean *strip_alarms, gboolean *only_new_attendees);

#endif
