/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * EEntry: An EText-based entry widget
 *
 * Authors:
 *   Miguel de Icaza <miguel@helixcode.com>
 *   Chris Lahey     <clahey@helixcode.com>
 *   Jon Trowbridge  <trow@ximian.com>
 *
 * Copyright (C) 1999, 2000, 2001 Ximian Inc.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef _E_ENTRY_H_
#define _E_ENTRY_H_

#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtktable.h>
#include <gnome-xml/tree.h>
#include <gal/e-text/e-text.h>
#include "e-completion.h"


BEGIN_GNOME_DECLS

#define E_ENTRY_TYPE        (e_entry_get_type ())
#define E_ENTRY(o)          (GTK_CHECK_CAST ((o), E_ENTRY_TYPE, EEntry))
#define E_ENTRY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_ENTRY_TYPE, EEntryClass))
#define E_IS_ENTRY(o)       (GTK_CHECK_TYPE ((o), E_ENTRY_TYPE))
#define E_IS_ENTRY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_ENTRY_TYPE))

typedef struct _EEntry EEntry;
typedef struct _EEntryClass EEntryClass;
struct _EEntryPrivate;

typedef void (*EEntryCompletionHandler) (EEntry *entry, const gchar *text, gpointer extra_data);

struct _EEntry {
	GtkTable parent;

	GnomeCanvas *canvas;
	EText *item;

	struct _EEntryPrivate *priv;
};

struct _EEntryClass {
	GtkTableClass parent_class;

	void (* changed)  (EEntry *entry);
	void (* activate) (EEntry *entry);
	void (* popup)    (EEntry *entry, GdkEventButton *ev, gint pos);
};

GtkType      e_entry_get_type          (void);

void         e_entry_construct         (EEntry *entry);
GtkWidget   *e_entry_new               (void);

const gchar *e_entry_get_text          (EEntry *entry);
void         e_entry_set_text          (EEntry *entry, const gchar *text);

gint         e_entry_get_position      (EEntry *entry);
void         e_entry_set_position      (EEntry *entry, gint);
void         e_entry_select_region     (EEntry *entry, gint start, gint end);

void         e_entry_set_editable      (EEntry *entry, gboolean editable);

void         e_entry_enable_completion      (EEntry *entry, ECompletion *completion);
void         e_entry_enable_completion_full (EEntry *entry, ECompletion *completion, gint autocomplete_delay,
					     EEntryCompletionHandler handler);

END_GNOME_DECLS

#endif /* _E_ENTRY_H_ */
