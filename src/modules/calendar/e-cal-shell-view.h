/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_SHELL_VIEW_H
#define E_CAL_SHELL_VIEW_H

#include <e-util/e-util.h>
#include "e-cal-base-shell-view.h"
#include "e-cal-shell-content.h"

/* Standard GObject macros */
#define E_TYPE_CAL_SHELL_VIEW \
	(e_cal_shell_view_get_type ())
#define E_CAL_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_SHELL_VIEW, ECalShellView))
#define E_CAL_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_SHELL_VIEW, ECalShellViewClass))
#define E_IS_CAL_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_SHELL_VIEW))
#define E_IS_CAL_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_SHELL_VIEW))
#define E_CAL_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_SHELL_VIEW, ECalShellViewClass))

G_BEGIN_DECLS

typedef struct _ECalShellView ECalShellView;
typedef struct _ECalShellViewClass ECalShellViewClass;
typedef struct _ECalShellViewPrivate ECalShellViewPrivate;

struct _ECalShellView {
	ECalBaseShellView parent;
	ECalShellViewPrivate *priv;
};

struct _ECalShellViewClass {
	ECalBaseShellViewClass parent_class;
};

GType		e_cal_shell_view_get_type		(void);
void		e_cal_shell_view_type_register		(GTypeModule *type_module);
void		e_cal_shell_view_set_view_id_from_view_kind
							(ECalShellView *self,
							 ECalViewKind view_kind);

G_END_DECLS

#endif /* E_CAL_SHELL_VIEW_H */
