/*
 * e-memo-shell-content.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MEMO_SHELL_CONTENT_H
#define E_MEMO_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-memo-table.h>

#include <menus/gal-view-instance.h>
#include <misc/e-preview-pane.h>

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

enum {
	E_MEMO_SHELL_CONTENT_SELECTION_SINGLE		= 1 << 0,
	E_MEMO_SHELL_CONTENT_SELECTION_MULTIPLE		= 1 << 1,
	E_MEMO_SHELL_CONTENT_SELECTION_CAN_EDIT		= 1 << 2,
	E_MEMO_SHELL_CONTENT_SELECTION_HAS_URL		= 1 << 3
};

struct _EMemoShellContent {
	EShellContent parent;
	EMemoShellContentPrivate *priv;
};

struct _EMemoShellContentClass {
	EShellContentClass parent_class;
};

GType		e_memo_shell_content_get_type	(void);
void		e_memo_shell_content_register_type
					(GTypeModule *type_module);
GtkWidget *	e_memo_shell_content_new
					(EShellView *shell_view);
ECalModel *	e_memo_shell_content_get_memo_model
					(EMemoShellContent *memo_shell_conent);
EMemoTable *	e_memo_shell_content_get_memo_table
					(EMemoShellContent *memo_shell_content);
EPreviewPane *	e_memo_shell_content_get_preview_pane
					(EMemoShellContent *memo_shell_content);
gboolean	e_memo_shell_content_get_preview_visible
					(EMemoShellContent *memo_shell_content);
void		e_memo_shell_content_set_preview_visible
					(EMemoShellContent *memo_shell_content,
					 gboolean preview_visible);
EShellSearchbar *
		e_memo_shell_content_get_searchbar
					(EMemoShellContent *memo_shell_content);
GalViewInstance *
		e_memo_shell_content_get_view_instance
					(EMemoShellContent *memo_shell_content);

G_END_DECLS

#endif /* E_MEMO_SHELL_CONTENT_H */
