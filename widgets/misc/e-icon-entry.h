/*
 *  e-icon-entry.h
 *
 *  Authors: Johnny Jacob <jjohnny@novell.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  Adapted and modified from Epiphany.
 *
 *  Copyright (C) 2003, 2004, 2005  Christian Persch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 *  Adapted and modified from gtk+ code:
 *
 *  Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *  Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 *  file in the gtk+ distribution for a list of people on the GTK+ Team.
 *  See the ChangeLog in the gtk+ distribution files for a list of changes.
 *  These files are distributed with GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 *
 */

#ifndef E_ICON_ENTRY_H
#define E_ICON_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Standard GObject macros */
#define E_TYPE_ICON_ENTRY \
	(e_icon_entry_get_type())
#define E_ICON_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ICON_ENTRY, EIconEntry))
#define E_ICON_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ICON_ENTRY, EIconEntryClass))
#define E_IS_ICON_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ICON_ENTRY))
#define E_IS_ICON_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ICON_ENTRY))
#define E_ICON_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ICON_ENTRY, EIconEntryClass))

typedef struct _EIconEntry EIconEntry;
typedef struct _EIconEntryClass	EIconEntryClass;
typedef struct _EIconEntryPrivate EIconEntryPrivate;
 
struct _EIconEntry
{
	GtkBin parent;
	EIconEntryPrivate *priv;
};

struct _EIconEntryClass
{
	GtkBinClass parent_class;
};

GType		e_icon_entry_get_type		(void);
GtkWidget *	e_icon_entry_new		(void);
GtkWidget *	e_icon_entry_get_entry		(EIconEntry *entry);
void		e_icon_entry_add_action_start	(EIconEntry *entry,
						 GtkAction *action);
void		e_icon_entry_add_action_end	(EIconEntry *entry,
						 GtkAction *action);

G_END_DECLS

#endif
