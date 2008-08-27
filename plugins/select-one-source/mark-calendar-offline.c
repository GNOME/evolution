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
 *		Harish Krishnaswamy <kharish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls.
 * This code has been derived from the source of the sample eplugin
 * select_one_source.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>
#include <calendar/gui/e-cal-popup.h>

void org_gnome_mark_calendar_offline (EPlugin *ep, ECalPopupTargetSource *target);
void org_gnome_mark_calendar_no_offline (EPlugin *ep, ECalPopupTargetSource *target);

void
org_gnome_mark_calendar_no_offline (EPlugin *ep, ECalPopupTargetSource *target)
{
	ESource *source;

	source = e_source_selector_peek_primary_selection (target->selector);
	e_source_set_property (source, "offline", "0");
}

void
org_gnome_mark_calendar_offline (EPlugin *ep, ECalPopupTargetSource *target)
{
	ESource *source;

	source = e_source_selector_peek_primary_selection (target->selector);
	e_source_set_property (source, "offline", "1");
}

