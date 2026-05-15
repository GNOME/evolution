/*
 * SPDX-FileCopyrightText: (C) 2017 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef E_TO_DO_PANE_H
#define E_TO_DO_PANE_H

#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>

#include <shell/e-shell-view.h>

/* Standard GObject macros */

#define E_TYPE_TO_DO_PANE \
	(e_to_do_pane_get_type ())
#define E_TO_DO_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TO_DO_PANE, EToDoPane))
#define E_TO_DO_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TO_DO_PANE, EToDoPaneClass))
#define E_IS_TO_DO_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TO_DO_PANE))
#define E_IS_TO_DO_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TO_DO_PANE))
#define E_TO_DO_PANE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TO_DO_PANE, EToDoPaneClass))

#define E_TO_DO_PANE_MIN_SHOW_N_DAYS 7
#define E_TO_DO_PANE_MAX_SHOW_N_DAYS 367

G_BEGIN_DECLS

typedef struct _EToDoPane EToDoPane;
typedef struct _EToDoPaneClass EToDoPaneClass;
typedef struct _EToDoPanePrivate EToDoPanePrivate;

struct _EToDoPane {
	GtkGrid parent;

	EToDoPanePrivate *priv;
};

struct _EToDoPaneClass {
	GtkGridClass parent_class;
};

GType		e_to_do_pane_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_to_do_pane_new		(EShellView *shell_view);
EShellView *	e_to_do_pane_ref_shell_view	(EToDoPane *to_do_pane);
gboolean	e_to_do_pane_get_highlight_overdue
						(EToDoPane *to_do_pane);
void		e_to_do_pane_set_highlight_overdue
						(EToDoPane *to_do_pane,
						 gboolean highlight_overdue);
const GdkRGBA *	e_to_do_pane_get_overdue_color	(EToDoPane *to_do_pane);
void		e_to_do_pane_set_overdue_color	(EToDoPane *to_do_pane,
						 const GdkRGBA *overdue_color);
gboolean	e_to_do_pane_get_show_completed_tasks
						(EToDoPane *to_do_pane);
void		e_to_do_pane_set_show_completed_tasks
						(EToDoPane *to_do_pane,
						 gboolean show_completed_tasks);
gboolean	e_to_do_pane_get_show_no_duedate_tasks
						(EToDoPane *to_do_pane);
void		e_to_do_pane_set_show_no_duedate_tasks
						(EToDoPane *to_do_pane,
						 gboolean show_no_duedate_tasks);
gboolean	e_to_do_pane_get_use_24hour_format
						(EToDoPane *to_do_pane);
void		e_to_do_pane_set_use_24hour_format
						(EToDoPane *to_do_pane,
						 gboolean use_24hour_format);
guint		e_to_do_pane_get_show_n_days	(EToDoPane *to_do_pane);
void		e_to_do_pane_set_show_n_days	(EToDoPane *to_do_pane,
						 guint show_n_days);
gboolean	e_to_do_pane_get_time_in_smaller_font
						(EToDoPane *to_do_pane);
void		e_to_do_pane_set_time_in_smaller_font
						(EToDoPane *to_do_pane,
						 gboolean time_in_smaller_font);

G_END_DECLS

#endif /* E_TO_DO_PANE_H */
