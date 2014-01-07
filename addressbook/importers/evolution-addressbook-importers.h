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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <gtk/gtk.h>

struct _EImportImporter *evolution_ldif_importer_peek (void);
struct _EImportImporter *evolution_vcard_importer_peek (void);
struct _EImportImporter *evolution_csv_outlook_importer_peek (void);
struct _EImportImporter *evolution_csv_mozilla_importer_peek (void);
struct _EImportImporter *evolution_csv_evolution_importer_peek (void);

/* private utility function for importers only */
GtkWidget *evolution_contact_importer_get_preview_widget (const GSList *contacts);
