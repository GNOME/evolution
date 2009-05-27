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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

#include <glib.h>
#include <glib/gi18n.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <calendar/gui/e-cal-popup.h>

void org_gnome_select_one_source (EPlugin *ep, ECalPopupTargetSource *target);

void
org_gnome_select_one_source (EPlugin *ep, ECalPopupTargetSource *target)
{
	GSList *selection, *l;
	ESource *primary_source;

	selection = e_source_selector_get_selection (target->selector);
	primary_source = e_source_selector_peek_primary_selection (target->selector);

	for (l = selection; l; l = l->next) {
		ESource *source = l->data;

		if (source != primary_source)
			e_source_selector_unselect_source (target->selector, source);
	}

	e_source_selector_select_source (target->selector, primary_source);

	e_source_selector_free_selection (selection);
}
