/*
 * e-hinted-entry.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_HINTED_ENTRY_H
#define E_HINTED_ENTRY_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_HINTED_ENTRY \
	(e_hinted_entry_get_type ())
#define E_HINTED_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HINTED_ENTRY, EHintedEntry))
#define E_HINTED_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HINTED_ENTRY, EHintedEntryClass))
#define E_IS_HINTED_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HINTED_ENTRY))
#define E_IS_HINTED_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HINTED_ENTRY))
#define E_HINTED_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HINTED_ENTRY, EHintedEntryClass))

G_BEGIN_DECLS

typedef struct _EHintedEntry EHintedEntry;
typedef struct _EHintedEntryClass EHintedEntryClass;
typedef struct _EHintedEntryPrivate EHintedEntryPrivate;

struct _EHintedEntry {
	GtkEntry parent;
	EHintedEntryPrivate *priv;
};

struct _EHintedEntryClass {
	GtkEntryClass parent_class;
};

GType		e_hinted_entry_get_type		(void);
GtkWidget *	e_hinted_entry_new		(void);
const gchar *	e_hinted_entry_get_hint		(EHintedEntry *entry);
void		e_hinted_entry_set_hint		(EHintedEntry *entry,
						 const gchar *hint);
gboolean	e_hinted_entry_get_hint_shown	(EHintedEntry *entry);
const gchar *	e_hinted_entry_get_text		(EHintedEntry *entry);
void		e_hinted_entry_set_text		(EHintedEntry *entry,
						 const gchar *text);

G_END_DECLS

#endif /* E_HINTED_ENTRY_H */
