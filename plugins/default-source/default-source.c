/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>

#include <e-util/e-config.h>
#include <e-util/e-plugin-util.h>
#include <calendar/gui/e-cal-config.h>
#include <libedataserver/e-source.h>
#include <addressbook/gui/widgets/eab-config.h>
#include <libebook/e-book.h>
#include <libecal/e-cal.h>
#include <glib/gi18n.h>
#include <string.h>

GtkWidget *org_gnome_default_book (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *org_gnome_autocomplete_book (EPlugin *epl, EConfigHookItemFactoryData *data);

void commit_default_calendar (EPlugin *epl, EConfigTarget *target);
void commit_default_book (EPlugin *epl, EConfigTarget *target);

gint e_plugin_lib_enable (EPlugin *ep, gint enable);

gint
e_plugin_lib_enable (EPlugin *ep, gint enable)
{
	return 0;
}

void
commit_default_calendar (EPlugin *epl, EConfigTarget *target)
{
	ECalConfigTargetSource *cal_target;
	ESource *source;

	cal_target = (ECalConfigTargetSource *) target;
	source = cal_target->source;
	if (e_source_get_property (source, "default"))
		e_cal_set_default_source (source, cal_target->source_type, NULL);
}

void
commit_default_book (EPlugin *epl, EConfigTarget *target)
{
	EABConfigTargetSource *book_target;
	ESource *source;

	book_target = (EABConfigTargetSource *) target;
	source = book_target->source;
	if (e_source_get_property (source, "default"))
		e_book_set_default_source (source, NULL);

}

GtkWidget *
org_gnome_default_book (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *book_target;

	if (data->old)
		return data->old;

	book_target = (EABConfigTargetSource *) data->target;

	return e_plugin_util_add_check (data->parent, _("Mark as _default address book"), book_target->source, "default", "true", NULL);
}

GtkWidget *
org_gnome_autocomplete_book (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *book_target;

	if (data->old)
		return data->old;

	book_target = (EABConfigTargetSource *) data->target;

	return e_plugin_util_add_check (data->parent, _("A_utocomplete with this address book"), book_target->source, "completion", "true", NULL);
}

static const gchar *
get_calendar_option_caption (ECalSourceType source_type)
{
	const gchar *res = "???";

	switch (source_type) {
		case E_CAL_SOURCE_TYPE_EVENT:   res = _("Mark as _default calendar"); break;
		case E_CAL_SOURCE_TYPE_TODO:    res = _("Mark as _default task list"); break;
		case E_CAL_SOURCE_TYPE_JOURNAL: res = _("Mark as _default memo list"); break;
		default: break;
	}

	return res;
}

GtkWidget *org_gnome_default_cal (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
org_gnome_default_cal (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	ECalConfigTargetSource *cal_target;

	if (data->old)
		return data->old;

	cal_target = (ECalConfigTargetSource *) data->target;

	return e_plugin_util_add_check (data->parent, get_calendar_option_caption (cal_target->source_type), cal_target->source, "default", "true", NULL);
}
