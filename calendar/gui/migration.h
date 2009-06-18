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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MIGRATION_H
#define MIGRATION_H

#include <libedataserver/e-source-group.h>
#include "calendar-component.h"
#include "tasks-component.h"
#include "memos-component.h"

gboolean migrate_calendars (CalendarComponent *component, gint major, gint minor, gint revision, GError **err);
gboolean migrate_tasks (TasksComponent *component, gint major, gint minor, gint revision, GError **err);
gboolean migrate_memos (MemosComponent *component, gint major, gint minor, gint revision, GError **err);
#endif
