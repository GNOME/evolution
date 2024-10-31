/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MENUBAR_H
#define E_MENUBAR_H

#include <gtk/gtk.h>
#include <e-util/e-util.h>

#define E_TYPE_MENU_BAR \
	(e_menu_bar_get_type ())
#define E_MENU_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MENU_BAR, EMenuBar))
#define E_MENU_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MENU_BAR, EMenuBarClass))
#define E_IS_MENU_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MENU_BAR))
#define E_IS_MENU_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MENU_BAR))
#define E_MENU_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MENU_BAR, EMenuBarClass))

G_BEGIN_DECLS

typedef struct _EMenuBar EMenuBar;
typedef struct _EMenuBarClass EMenuBarClass;
typedef struct _EMenuBarPrivate EMenuBarPrivate;

struct _EMenuBar {
	GObject parent;
	EMenuBarPrivate *priv;
};

struct _EMenuBarClass {
	GObjectClass parent_class;
};

GType		e_menu_bar_get_type	(void);
EMenuBar *	e_menu_bar_new		(GtkMenuBar *widget,
					 GtkWindow *window,
					 GtkWidget **out_menu_button);
gboolean	e_menu_bar_get_visible  (EMenuBar *menu_bar);
void		e_menu_bar_set_visible  (EMenuBar *menu_bar,
					 gboolean visible);

G_END_DECLS

#endif /* E_MENUBAR_H */
