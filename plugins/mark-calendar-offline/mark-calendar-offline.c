/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Harish Krishnaswamy (kharish@novell.com)
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

