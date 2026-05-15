/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MEMO_SHELL_VIEW_H
#define E_MEMO_SHELL_VIEW_H

#include <e-util/e-util.h>
#include "e-cal-base-shell-view.h"

/* Standard GObject macros */
#define E_TYPE_MEMO_SHELL_VIEW \
	(e_memo_shell_view_get_type ())
#define E_MEMO_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEMO_SHELL_VIEW, EMemoShellView))
#define E_MEMO_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEMO_SHELL_VIEW, EMemoShellViewClass))
#define E_IS_MEMO_SHELL_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEMO_SHELL_VIEW))
#define E_IS_MEMO_SHELL_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEMO_SHELL_VIEW))
#define E_MEMO_SHELL_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEMO_SHELL_VIEW, EMemoShellViewClass))

G_BEGIN_DECLS

typedef struct _EMemoShellView EMemoShellView;
typedef struct _EMemoShellViewClass EMemoShellViewClass;
typedef struct _EMemoShellViewPrivate EMemoShellViewPrivate;

struct _EMemoShellView {
	ECalBaseShellView parent;
	EMemoShellViewPrivate *priv;
};

struct _EMemoShellViewClass {
	ECalBaseShellViewClass parent_class;
};

GType		e_memo_shell_view_get_type		(void);
void		e_memo_shell_view_type_register		(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MEMO_SHELL_VIEW_H */
