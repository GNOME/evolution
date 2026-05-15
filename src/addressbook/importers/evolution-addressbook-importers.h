/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gtk/gtk.h>

struct _EImportImporter *evolution_ldif_importer_peek (void);
struct _EImportImporter *evolution_vcard_importer_peek (void);
struct _EImportImporter *evolution_csv_outlook_importer_peek (void);
struct _EImportImporter *evolution_csv_mozilla_importer_peek (void);
struct _EImportImporter *evolution_csv_evolution_importer_peek (void);

/* private utility function for importers only */
GtkWidget *evolution_contact_importer_get_preview_widget (const GSList *contacts);
