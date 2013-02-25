/*
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
 * Authors:
 *		Gilbert Fang <gilbert.fang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EVOLUTION_ADDRESSBOOK_EXPORT_H_
#define _EVOLUTION_ADDRESSBOOK_EXPORT_H__

#include <libedataserver/libedataserver.h>

G_BEGIN_DECLS

#define SUCCESS 0
#define FAILED  -1

#define ACTION_NOTHING       0
#define ACTION_LIST_FOLDERS  1
#define ACTION_LIST_CARDS    2

#define DEFAULT_SIZE_NUMBER 100

struct _ActionContext {

	guint action_type;
	gchar *output_file;

	/* for cards only */
	gint IsCSV;
	gint IsVCard;
	gchar *addressbook_source_uid;
};

typedef struct _ActionContext ActionContext;

/* action_list_folders */
guint		action_list_folders_init	(ESourceRegistry *registry,
						 ActionContext *p_actctx);

/*action list cards*/
guint		action_list_cards_init		(ESourceRegistry *registry,
						 ActionContext *p_actctx);

G_END_DECLS

#endif /* _EVOLUTION_ADDRESSBOOK_EXPORT_H_ */
