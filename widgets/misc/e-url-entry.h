/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-url-entry.h
 *
 * Copyright (C) 2002  JP Rosevear
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: JP Rosevear
 */

#ifndef _E_URL_ENTRY_H_
#define _E_URL_ENTRY_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_URL_ENTRY			(e_url_entry_get_type ())
#define E_URL_ENTRY(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_URL_ENTRY, EUrlEntry))
#define E_URL_ENTRY_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_URL_ENTRY, EUrlEntryClass))
#define E_IS_URL_ENTRY(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_URL_ENTRY))
#define E_IS_URL_ENTRY_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_URL_ENTRY))


typedef struct _EUrlEntry        EUrlEntry;
typedef struct _EUrlEntryPrivate EUrlEntryPrivate;
typedef struct _EUrlEntryClass   EUrlEntryClass;

struct _EUrlEntry {
	GtkHBox parent;

	EUrlEntryPrivate *priv;
};

struct _EUrlEntryClass {
	GtkHBoxClass parent_class;
};



GtkType    e_url_entry_get_type  (void);
GtkWidget *e_url_entry_new       (void);
GtkWidget *e_url_entry_get_entry (EUrlEntry *url_entry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_URL_ENTRY_H_ */
