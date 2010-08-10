/*
 * em-account-prefs.h
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

#ifndef EM_ACCOUNT_PREFS_H
#define EM_ACCOUNT_PREFS_H

#include <gtk/gtk.h>
#include <table/e-table.h>
#include <libedataserver/e-account-list.h>
#include <misc/e-account-manager.h>
#include <widgets/misc/e-preferences-window.h>

/* Standard GObject macros */
#define EM_TYPE_ACCOUNT_PREFS \
	(em_account_prefs_get_type ())
#define EM_ACCOUNT_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_ACCOUNT_PREFS, EMAccountPrefs))
#define EM_ACCOUNT_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_ACCOUNT_PREFS, EMAccountPrefsClass))
#define EM_IS_ACCOUNT_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_ACCOUNT_PREFS))
#define EM_IS_ACCOUNT_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_ACCOUNT_PREFS))
#define EM_ACCOUNT_PREFS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_ACCOUNT_PREFS, EMAccountPrefsClass))

G_BEGIN_DECLS

typedef struct _EMAccountPrefs EMAccountPrefs;
typedef struct _EMAccountPrefsClass EMAccountPrefsClass;
typedef struct _EMAccountPrefsPrivate EMAccountPrefsPrivate;

struct _EMAccountPrefs {
	EAccountManager parent;
	EMAccountPrefsPrivate *priv;
};

struct _EMAccountPrefsClass {
	EAccountManagerClass parent_class;
};

GType      em_account_prefs_get_type (void);
GtkWidget *em_account_prefs_new (EPreferencesWindow *window);

G_END_DECLS

#endif /* EM_ACCOUNT_PREFS_H */
