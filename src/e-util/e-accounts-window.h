/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ACCOUNTS_WINDOW_H
#define E_ACCOUNTS_WINDOW_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_ACCOUNTS_WINDOW \
	(e_accounts_window_get_type ())
#define E_ACCOUNTS_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ACCOUNTS_WINDOW, EAccountsWindow))
#define E_ACCOUNTS_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ACCOUNTS_WINDOW, EAccountsWindowClass))
#define E_IS_ACCOUNTS_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ACCOUNTS_WINDOW))
#define E_IS_ACCOUNTS_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ACCOUNTS_WINDOW))
#define E_ACCOUNTS_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ACCOUNTS_WINDOW, EAccountsWindowClass))

G_BEGIN_DECLS

typedef enum {
	E_SOURCE_EDITING_FLAG_NONE		= 0,
	E_SOURCE_EDITING_FLAG_CAN_ENABLE	= (1 << 0),
	E_SOURCE_EDITING_FLAG_CAN_EDIT		= (1 << 1),
	E_SOURCE_EDITING_FLAG_CAN_DELETE	= (1 << 2)
} ESourceEditingFlags;

typedef struct _EAccountsWindow EAccountsWindow;
typedef struct _EAccountsWindowClass EAccountsWindowClass;
typedef struct _EAccountsWindowPrivate EAccountsWindowPrivate;

/**
 * EAccountsWindow:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 *
 * Since: 3.26
 **/
struct _EAccountsWindow {
	GtkWindow parent;
	EAccountsWindowPrivate *priv;
};

struct _EAccountsWindowClass {
	GtkWindowClass parent_class;

	/* Signals */
	gboolean	(* get_editing_flags)	(EAccountsWindow *accounts_window,
						 ESource *source,
						 guint *out_flags); /* bit-or of ESourceEditingFlags */
	gboolean	(* add_source)		(EAccountsWindow *accounts_window,
						 const gchar *kind);
	gboolean	(* edit_source)		(EAccountsWindow *accounts_window,
						 ESource *source);
	gboolean	(* delete_source)	(EAccountsWindow *accounts_window,
						 ESource *source);
	void		(* enabled_toggled)	(EAccountsWindow *accounts_window,
						 ESource *source);
	void		(* populate_add_popup)	(EAccountsWindow *accounts_window,
						 GtkMenuShell *popup_menu);
	void		(* selection_changed)	(EAccountsWindow *accounts_window,
						 ESource *source);
};

GType		e_accounts_window_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_accounts_window_new			(ESourceRegistry *registry);
ESourceRegistry *
		e_accounts_window_get_registry		(EAccountsWindow *accounts_window);
void		e_accounts_window_show_with_parent	(EAccountsWindow *accounts_window,
							 GtkWindow *parent);
ESource *	e_accounts_window_ref_selected_source	(EAccountsWindow *accounts_window);
void		e_accounts_window_select_source		(EAccountsWindow *accounts_window,
							 const gchar *uid);
void		e_accounts_window_insert_to_add_popup	(EAccountsWindow *accounts_window,
							 GtkMenuShell *popup_menu,
							 const gchar *kind,
							 const gchar *label,
							 const gchar *icon_name);
GtkButtonBox *	e_accounts_window_get_button_box	(EAccountsWindow *accounts_window);
gint		e_accounts_window_add_page		(EAccountsWindow *accounts_window,
							 GtkWidget *content);
void		e_accounts_window_activate_page		(EAccountsWindow *accounts_window,
							 gint page_index);

G_END_DECLS

#endif /* E_ACCOUNTS_WINDOW_H */
