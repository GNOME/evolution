/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_COMPOSER_PREFS_H
#define EM_COMPOSER_PREFS_H

#include <gtk/gtk.h>

#include <shell/e-shell.h>

/* Standard GObject macros */
#define EM_TYPE_COMPOSER_PREFS \
	(em_composer_prefs_get_type ())
#define EM_COMPOSER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_COMPOSER_PREFS, EMComposerPrefs))
#define EM_COMPOSER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_COMPOSER_PREFS, EMComposerPrefsClass))
#define EM_IS_COMPOSER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_COMPOSER_PREFS))
#define EM_IS_COMPOSER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_COMPOSER_PREFS))
#define EM_COMPOSER_PREFS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_COMPOSER_PREFS, EMComposerPrefsClass))

G_BEGIN_DECLS

typedef struct _EMComposerPrefs EMComposerPrefs;
typedef struct _EMComposerPrefsClass EMComposerPrefsClass;

struct _ESignature;

struct _EMComposerPrefs {
	GtkBox parent;

	GtkBuilder *builder;

	/* General tab */

	GtkTreeModel *language_model;

	/* Forwards and Replies */
	GtkComboBox *forward_style;
	GtkComboBox *reply_style;

	/* Signatures */
	EWebViewPreview *sig_preview;

	ESpellChecker *spell_checker;
};

struct _EMComposerPrefsClass {
	GtkBoxClass parent_class;
};

GType		em_composer_prefs_get_type	(void);
GtkWidget *	em_composer_prefs_new		(EPreferencesWindow *window);

G_END_DECLS

#endif /* EM_COMPOSER_PREFS_H */
