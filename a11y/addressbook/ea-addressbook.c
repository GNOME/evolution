/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-addressbook.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Leon Zhang <leon.zhang@sun.com> Sun Microsystem Inc., 2003
 *
 */

#include <gal/e-text/e-text.h>
#include "ea-factory.h"
#include "ea-addressbook.h"
#include "addressbook/ea-minicard.h"
#include "addressbook/ea-minicard-view.h"
#include "addressbook/ea-addressbook-view.h"

EA_FACTORY_GOBJECT (EA_TYPE_MINICARD, ea_minicard, ea_minicard_new);
EA_FACTORY_GOBJECT (EA_TYPE_MINICARD_VIEW, ea_minicard_view, ea_minicard_view_new);
EA_FACTORY_GOBJECT (EA_TYPE_AB_VIEW, ea_ab_view, ea_ab_view_new);

static gboolean ea_addressbook_focus_watcher (GSignalInvocationHint *ihint,
					      guint n_param_values,
					      const GValue *param_values,
					      gpointer data);

void e_minicard_a11y_init (void)
{
	EA_SET_FACTORY (e_minicard_get_type (), ea_minicard);
}

void e_minicard_view_a11y_init (void)
{
	EA_SET_FACTORY (e_minicard_view_get_type (), ea_minicard_view);

	if (atk_get_root ()) {
		g_signal_add_emission_hook (g_signal_lookup ("event",
					    e_minicard_get_type()),
					    0, ea_addressbook_focus_watcher,
					    NULL, (GDestroyNotify) NULL);
	}
}

void eab_view_a11y_init (void)
{
	EA_SET_FACTORY (eab_view_get_type (), ea_ab_view);
}

gboolean
ea_addressbook_focus_watcher (GSignalInvocationHint *ihint,
			   guint n_param_values,
			   const GValue *param_values,
			   gpointer data)
{
	GObject *object;
	GdkEvent *event;
	AtkObject *ea_event = NULL;

	object = g_value_get_object (param_values + 0);
	event = g_value_get_boxed (param_values + 1);

	if (E_IS_MINICARD (object)) {
                GnomeCanvasItem *item = GNOME_CANVAS_ITEM (object);
		ea_event = atk_gobject_accessible_for_object (object);
		if (event->type == GDK_FOCUS_CHANGE) {
			if ((event->focus_change.in) && (E_IS_MINICARD (item->canvas->focused_item)))
				atk_focus_tracker_notify (ea_event);
		}
	}

	return TRUE;
}
