/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-content.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_MEMO_SHELL_CONTENT_H
#define E_MEMO_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-memo-preview.h>
#include <calendar/gui/e-memo-table.h>

#include <widgets/menus/gal-view-instance.h>

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
	EShellContent parent;
	EMemoShellContentPrivate *priv;
};

struct _EMemoShellContentClass {
	EShellContentClass parent_class;
};

GType		e_memo_shell_content_get_type	(void);
GtkWidget *	e_memo_shell_content_new	(EShellView *shell_view);
EMemoPreview *	e_memo_shell_content_get_memo_preview
						(EMemoShellContent *memo_shell_content);
EMemoTable *	e_memo_shell_content_get_memo_table
						(EMemoShellContent *memo_shell_content);
GalViewInstance *
		e_memo_shell_content_get_view_instance
						(EMemoShellContent *memo_shell_content);
gboolean	e_memo_shell_content_get_preview_visible
						(EMemoShellContent *memo_shell_content);
void		e_memo_shell_content_set_preview_visible
						(EMemoShellContent *memo_shell_content,
						 gboolean preview_visible);

G_END_DECLS

#endif /* E_MEMO_SHELL_CONTENT_H */
