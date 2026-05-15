/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#ifndef EM_MAILER_PREFS_H
#define EM_MAILER_PREFS_H

#include <gtk/gtk.h>

#include <shell/e-shell.h>

/* Standard GObject macros */
#define EM_TYPE_MAILER_PREFS \
	(em_mailer_prefs_get_type ())
#define EM_MAILER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_MAILER_PREFS, EMMailerPrefs))
#define EM_MAILER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_MAILER_PREFS, EMMailerPrefsClass))
#define EM_IS_MAILER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_MAILER_PREFS))
#define EM_IS_MAILER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_MAILER_PREFS))
#define EM_MAILER_PREFS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_MAILER_PREFS))

G_BEGIN_DECLS

typedef struct _EMMailerPrefs EMMailerPrefs;
typedef struct _EMMailerPrefsClass EMMailerPrefsClass;
typedef struct _EMMailerPrefsPrivate EMMailerPrefsPrivate;

struct _EMMailerPrefs {
	GtkBox parent_object;

	EMMailerPrefsPrivate *priv;
};

struct _EMMailerPrefsClass {
	GtkBoxClass parent_class;
};

GType		em_mailer_prefs_get_type	(void);
GtkWidget *	em_mailer_prefs_new		(EPreferencesWindow *window);

G_END_DECLS

#endif /* EM_MAILER_PREFS_H */
