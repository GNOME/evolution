/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_SHELL_HEADER_BAR_H
#define E_SHELL_HEADER_BAR_H

#include <gtk/gtk.h>

#include <e-util/e-util.h>
#include <shell/e-shell-window.h>

#define E_TYPE_SHELL_HEADER_BAR \
	(e_shell_header_bar_get_type ())
#define E_SHELL_HEADER_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_HEADER_BAR, EShellHeaderBar))
#define E_SHELL_HEADER_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_HEADER_BAR, EShellHeaderBarClass))
#define E_IS_SHELL_HEADER_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_HEADER_BAR))
#define E_IS_SHELL_HEADER_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_HEADER_BAR))
#define E_SHELL_HEADER_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_HEADER_BAR, EShellHeaderBarClass))

G_BEGIN_DECLS

typedef struct _EShellHeaderBar EShellHeaderBar;
typedef struct _EShellHeaderBarClass EShellHeaderBarClass;
typedef struct _EShellHeaderBarPrivate EShellHeaderBarPrivate;

struct _EShellHeaderBar {
	GtkHeaderBar parent;
	EShellHeaderBarPrivate *priv;
};

struct _EShellHeaderBarClass {
	GtkHeaderBarClass parent_class;
};

GType			e_shell_header_bar_get_type			(void);
GtkWidget *		e_shell_header_bar_new				(EShellWindow *shell_window,
									 GtkWidget *menu_button);
GtkWidget *		e_shell_header_bar_get_new_button		(EShellHeaderBar *headerbar);
void			e_shell_header_bar_pack_start			(EShellHeaderBar *headerbar,
									 GtkWidget *widget);
void			e_shell_header_bar_pack_end			(EShellHeaderBar *headerbar,
									 GtkWidget *widget);
void			e_shell_header_bar_clear			(EShellHeaderBar *headerbar,
									 const gchar *name);
G_END_DECLS

#endif /* E_SHELL_HEADER_BAR_H */
