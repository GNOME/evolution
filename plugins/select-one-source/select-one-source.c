/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: JP Rosevear <jpr@novell.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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
