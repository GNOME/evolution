/*
 * e-shell-searchbar.h
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

#ifndef E_SHELL_SEARCHBAR_H
#define E_SHELL_SEARCHBAR_H

#include <shell/e-shell-common.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_SHELL_SEARCHBAR \
	(e_shell_searchbar_get_type ())
#define E_SHELL_SEARCHBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_SEARCHBAR, EShellSearchbar))
#define E_SHELL_SEARCHBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_SEARCHBAR, EShellSearchbarClass))
#define E_IS_SHELL_SEARCHBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_SEARCHBAR))
#define E_IS_SHELL_SEARCHBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_SEARCHBAR))
#define E_SHELL_SEARCHBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_SEARCHBAR, EShellSearchbarClass))

G_BEGIN_DECLS

typedef struct _EShellSearchbar EShellSearchbar;
typedef struct _EShellSearchbarClass EShellSearchbarClass;
typedef struct _EShellSearchbarPrivate EShellSearchbarPrivate;

/**
 * EShellSearchbar:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EShellSearchbar {
	GtkBox parent;
	EShellSearchbarPrivate *priv;
};

struct _EShellSearchbarClass {
	GtkBoxClass parent_class;
};

GType		e_shell_searchbar_get_type	(void);
GtkWidget *	e_shell_searchbar_new		(EShellView *shell_view);
void		e_shell_searchbar_init_ui_data	(EShellSearchbar *searchbar);
EShellView *	e_shell_searchbar_get_shell_view
						(EShellSearchbar *searchbar);
EActionComboBox *
		e_shell_searchbar_get_filter_combo_box
						(EShellSearchbar *searchbar);
gboolean	e_shell_searchbar_get_filter_visible
						(EShellSearchbar *searchbar);
void		e_shell_searchbar_set_filter_visible
						(EShellSearchbar *searchbar,
						 gboolean filter_visible);
const gchar *	e_shell_searchbar_get_search_hint
						(EShellSearchbar *searchbar);
void		e_shell_searchbar_set_search_hint
						(EShellSearchbar *searchbar,
						 const gchar *search_hint);
EUIAction *	e_shell_searchbar_get_search_option
						(EShellSearchbar *searchbar);
void		e_shell_searchbar_set_search_option
						(EShellSearchbar *searchbar,
						 EUIAction *search_option);
const gchar *	e_shell_searchbar_get_search_text
						(EShellSearchbar *searchbar);
void		e_shell_searchbar_set_search_text
						(EShellSearchbar *searchbar,
						 const gchar *search_text);
GtkWidget *	e_shell_searchbar_get_search_box
						(EShellSearchbar *searchbar);
EActionComboBox *
		e_shell_searchbar_get_scope_combo_box
						(EShellSearchbar *searchbar);
gboolean	e_shell_searchbar_get_scope_visible
						(EShellSearchbar *searchbar);
void		e_shell_searchbar_set_scope_visible
						(EShellSearchbar *searchbar,
						 gboolean scope_visible);
void		e_shell_searchbar_set_state_dirty
						(EShellSearchbar *searchbar);
const gchar *	e_shell_searchbar_get_state_group
						(EShellSearchbar *searchbar);
void		e_shell_searchbar_set_state_group
						(EShellSearchbar *searchbar,
						 const gchar *state_group);
void		e_shell_searchbar_load_state	(EShellSearchbar *searchbar);
void		e_shell_searchbar_save_state	(EShellSearchbar *searchbar);
void		e_shell_searchbar_search_entry_grab_focus
						(EShellSearchbar *searchbar);
gboolean	e_shell_searchbar_search_entry_has_focus
						(EShellSearchbar *searchbar);

G_END_DECLS

#endif /* E_SHELL_SEARCHBAR_H */
