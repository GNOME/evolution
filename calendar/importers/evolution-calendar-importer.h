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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EVOLUTION_CALENDAR_IMPORTER_H
#define EVOLUTION_CALENDAR_IMPORTER_H

G_BEGIN_DECLS

struct _EImportImporter *ical_importer_peek (void);
struct _EImportImporter *vcal_importer_peek (void);

struct _EImportImporter *gnome_calendar_importer_peek (void);

G_END_DECLS

#endif
