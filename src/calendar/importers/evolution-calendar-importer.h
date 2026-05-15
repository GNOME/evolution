/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Rodrigo Moya <rodrigo@ximian.com>
 */

#ifndef EVOLUTION_CALENDAR_IMPORTER_H
#define EVOLUTION_CALENDAR_IMPORTER_H

G_BEGIN_DECLS

struct _EImportImporter *ical_importer_peek (void);
struct _EImportImporter *vcal_importer_peek (void);

struct _EImportImporter *gnome_calendar_importer_peek (void);

G_END_DECLS

#endif
