/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MEMO_PAGE_H
#define MEMO_PAGE_H

#include "comp-editor.h"
#include "comp-editor-page.h"

/* Standard GObject macros */
#define TYPE_MEMO_PAGE \
	(memo_page_get_type ())
#define MEMO_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), TYPE_MEMO_PAGE, MemoPage))
#define MEMO_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), TYPE_MEMO_PAGE, MemoPageClass))
#define IS_MEMO_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), TYPE_MEMO_PAGE))
#define IS_MEMO_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), TYPE_MEMO_PAGE))
#define MEMO_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), TYPE_MEMO_PAGE, MemoPageClass))

G_BEGIN_DECLS

typedef struct _MemoPage MemoPage;
typedef struct _MemoPageClass MemoPageClass;
typedef struct _MemoPagePrivate MemoPagePrivate;

struct _MemoPage {
	CompEditorPage page;
	MemoPagePrivate *priv;
};

struct _MemoPageClass {
	CompEditorPageClass parent_class;
};

GType		memo_page_get_type		(void);
MemoPage *	memo_page_construct		(MemoPage *epage);
MemoPage *	memo_page_new			(CompEditor *editor);
void		memo_page_set_show_categories	(MemoPage *page,
						 gboolean state);
void		memo_page_set_info_string	(MemoPage *mpage,
						 const gchar *icon,
						 const gchar *msg);

G_END_DECLS

#endif
