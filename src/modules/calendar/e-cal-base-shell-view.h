/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#ifndef E_CAL_BASE_SHELL_VIEW_H
#define E_CAL_BASE_SHELL_VIEW_H

#include <e-util/e-util.h>
#include <shell/e-shell-view.h>
#include <calendar/gui/e-cal-model.h>

/* Standard GObject macros */
#define E_TYPE_CAL_BASE_SHELL_VIEW \
	(e_cal_base_shell_view_get_type ())
#define E_CAL_BASE_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_BASE_SHELL_VIEW, ECalBaseShellView))
#define E_CAL_BASE_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_BASE_SHELL_VIEW, ECalBaseShellViewClass))
#define E_IS_CAL_BASE_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_BASE_SHELL_VIEW))
#define E_IS_CAL_BASE_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_BASE_SHELL_VIEW))
#define E_CAL_BASE_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_BASE_SHELL_VIEW, ECalBaseShellViewClass))

G_BEGIN_DECLS

typedef struct _ECalBaseShellView ECalBaseShellView;
typedef struct _ECalBaseShellViewClass ECalBaseShellViewClass;
typedef struct _ECalBaseShellViewPrivate ECalBaseShellViewPrivate;

struct _ECalBaseShellView {
	EShellView parent;
	ECalBaseShellViewPrivate *priv;
};

struct _ECalBaseShellViewClass {
	EShellViewClass parent_class;

	ECalClientSourceType source_type;
};

GType		e_cal_base_shell_view_get_type		(void);

ECalClientSourceType
		e_cal_base_shell_view_get_source_type	(EShellView *shell_view);

void		e_cal_base_shell_view_allow_auth_prompt_and_refresh
							(EShellView *shell_view,
							 EClient *client);
void		e_cal_base_shell_view_model_row_appended
							(EShellView *shell_view,
							 ECalModel *model);
void		e_cal_base_shell_view_copy_calendar	(EShellView *shell_view);
GtkWidget *	e_cal_base_shell_view_show_popup_menu	(EShellView *shell_view,
							 const gchar *widget_path,
							 GdkEvent *button_event,
							 ESource *clicked_source);
ESource *	e_cal_base_shell_view_get_clicked_source
							(EShellView *shell_view);
void		e_cal_base_shell_view_refresh_backend	(EShellView *shell_view,
							 ESource *collection_source);
void		e_cal_base_shell_view_preselect_source_config
							(EShellView *shell_view,
							 GtkWidget *source_config);

G_END_DECLS

#endif /* E_CAL_BASE_SHELL_VIEW_H */
