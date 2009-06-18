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
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_URL_ENTRY_H_
#define _E_URL_ENTRY_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_URL_ENTRY			(e_url_entry_get_type ())
#define E_URL_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_URL_ENTRY, EUrlEntry))
#define E_URL_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_URL_ENTRY, EUrlEntryClass))
#define E_IS_URL_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_URL_ENTRY))
#define E_IS_URL_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_URL_ENTRY))


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



GType      e_url_entry_get_type  (void);
GtkWidget *e_url_entry_new       (void);
GtkWidget *e_url_entry_get_entry (EUrlEntry *url_entry);

G_END_DECLS

#endif /* _E_URL_ENTRY_H_ */
