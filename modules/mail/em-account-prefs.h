/*
 * em-account-prefs.h
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

#ifndef EM_ACCOUNT_PREFS_H
#define EM_ACCOUNT_PREFS_H

#include <gtk/gtk.h>
#include <mail/e-mail-backend.h>
#include <mail/e-mail-account-manager.h>

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
	EMailAccountManager parent;
	EMAccountPrefsPrivate *priv;
};

struct _EMAccountPrefsClass {
	EMailAccountManagerClass parent_class;
};

GType		em_account_prefs_get_type	(void);
void		em_account_prefs_type_register	(GTypeModule *type_module);
GtkWidget *	em_account_prefs_new		(EPreferencesWindow *window);
EMailBackend *	em_account_prefs_get_backend	(EMAccountPrefs *prefs);

G_END_DECLS

#endif /* EM_ACCOUNT_PREFS_H */
