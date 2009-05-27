/*
 *
 * Evolution calendar - Delete calendar component dialog
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef DELETE_COMP_H
#define DELETE_COMP_H

#include <gtk/gtk.h>
#include <libecal/e-cal-component.h>

gboolean delete_component_dialog (ECalComponent *comp,
				  gboolean consider_as_untitled,
				  gint n_comps, ECalComponentVType vtype,
				  GtkWidget *widget);
gboolean prompt_retract_dialog (ECalComponent *comp, gchar **retract_text, GtkWidget *parent, gboolean *retract);

#endif
