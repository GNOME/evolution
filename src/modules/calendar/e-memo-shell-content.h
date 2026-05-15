/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2014 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MEMO_SHELL_CONTENT_H
#define E_MEMO_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-memo-table.h>

#include "e-cal-base-shell-content.h"

/* Standard GObject macros */
#define E_TYPE_MEMO_SHELL_CONTENT \
	(e_memo_shell_content_get_type ())
#define E_MEMO_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEMO_SHELL_CONTENT, EMemoShellContent))
#define E_MEMO_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEMO_SHELL_CONTENT, EMemoShellContentClass))
#define E_IS_MEMO_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEMO_SHELL_CONTENT))
#define E_IS_MEMO_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEMO_SHELL_CONTENT))
#define E_MEMO_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEMO_SHELL_CONTENT, EMemoShellContentClass))

G_BEGIN_DECLS

typedef struct _EMemoShellContent EMemoShellContent;
typedef struct _EMemoShellContentClass EMemoShellContentClass;
typedef struct _EMemoShellContentPrivate EMemoShellContentPrivate;

struct _EMemoShellContent {
	ECalBaseShellContent parent;
	EMemoShellContentPrivate *priv;
};

struct _EMemoShellContentClass {
	ECalBaseShellContentClass parent_class;
};

GType		e_memo_shell_content_get_type		(void);
void		e_memo_shell_content_type_register	(GTypeModule *type_module);
GtkWidget *	e_memo_shell_content_new		(EShellView *shell_view);

EMemoTable *	e_memo_shell_content_get_memo_table	(EMemoShellContent *memo_shell_content);
EPreviewPane *	e_memo_shell_content_get_preview_pane	(EMemoShellContent *memo_shell_content);
gboolean	e_memo_shell_content_get_preview_visible(EMemoShellContent *memo_shell_content);
void		e_memo_shell_content_set_preview_visible(EMemoShellContent *memo_shell_content,
							 gboolean preview_visible);
EShellSearchbar *
		e_memo_shell_content_get_searchbar	(EMemoShellContent *memo_shell_content);

G_END_DECLS

#endif /* E_MEMO_SHELL_CONTENT_H */
