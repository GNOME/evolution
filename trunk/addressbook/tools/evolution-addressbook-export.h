/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-addressbook-export.h
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Gilbert Fang <gilbert.fang@sun.com>
 *
 */

#ifndef _EVOLUTION_ADDRESSBOOK_EXPORT_H_
#define _EVOLUTION_ADDRESSBOOK_EXPORT_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define SUCCESS 0
#define FAILED  -1

#define ACTION_NOTHING       0
#define ACTION_LIST_FOLDERS  1
#define ACTION_LIST_CARDS    2

#define DEFAULT_SIZE_NUMBER 100

union _ActionContext
{

	guint action_type;

	struct
	{
		gint action_type;
		gchar *output_file;
	}
	action_list_folders;

	struct
	{
		gint action_type;
		gchar *output_file;
		gint IsCSV;
		gint IsVCard;
		gchar *addressbook_folder_uri;
		gint async_mode;
		gint file_size;
	}
	action_list_cards;
};

typedef union _ActionContext ActionContext;


/* action_list_folders */
guint action_list_folders_init (ActionContext * p_actctx);

/*action list cards*/
guint action_list_cards_init (ActionContext * p_actctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _EVOLUTION_ADDRESSBOOK_EXPORT_H_ */
