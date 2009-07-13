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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_ACCOUNT_PREFS_H__
#define __EM_ACCOUNT_PREFS_H__

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <table/e-table.h>

#include "evolution-config-control.h"

#include <shell/Evolution.h>

G_BEGIN_DECLS

#define EM_ACCOUNT_PREFS_TYPE        (em_account_prefs_get_type ())
#define EM_ACCOUNT_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_ACCOUNT_PREFS_TYPE, EMAccountPrefs))
#define EM_ACCOUNT_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_ACCOUNT_PREFS_TYPE, EMAccountPrefsClass))
#define EM_IS_ACCOUNT_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_ACCOUNT_PREFS_TYPE))
#define EM_IS_ACCOUNT_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_ACCOUNT_PREFS_TYPE))

typedef struct _EMAccountPrefs EMAccountPrefs;
typedef struct _EMAccountPrefsClass EMAccountPrefsClass;

struct _EMAccountPrefs {
	GtkVBox parent_object;

	GNOME_Evolution_Shell shell;

	GladeXML *gui;

	GtkWidget *druid;
	GtkWidget *editor;

	GtkTreeView *table;

	GtkButton *mail_add;
	GtkButton *mail_edit;
	GtkButton *mail_delete;
	GtkButton *mail_default;

	guint destroyed : 1;
	guint changed : 1;
};

struct _EMAccountPrefsClass {
	GtkVBoxClass parent_class;

	/* signals */

};

GType em_account_prefs_get_type (void);

GtkWidget *em_account_prefs_new (GNOME_Evolution_Shell shell);

/* needed by global config */
#define EM_ACCOUNT_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_AccountPrefs_ConfigControl:" BASE_VERSION

G_END_DECLS

#endif /* __EM_ACCOUNT_PREFS_H__ */
