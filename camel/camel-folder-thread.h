/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
 */

#ifndef _CAMEL_FOLDER_THREAD_H
#define _CAMEL_FOLDER_THREAD_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-folder-summary.h>
#include <camel/camel-folder.h>

typedef struct _CamelFolderThreadNode {
	struct _CamelFolderThreadNode *next,
		*parent,
		*child;
	const CamelMessageInfo *message;
	char *root_subject;	/* cached root equivalent subject */
	guint32 order:31;
	guint32 re:1;			/* re version of subject? */
} CamelFolderThreadNode;

typedef struct _CamelFolderThread {
	guint32 refcount  : 31;
	guint32 subject   : 1;
	
	struct _CamelFolderThreadNode *tree;
	struct _EMemChunk *node_chunks;
	CamelFolder *folder;
	GPtrArray *summary;
} CamelFolderThread;

/* interface 1: using uid's */
CamelFolderThread *camel_folder_thread_messages_new(CamelFolder *folder, GPtrArray *uids, gboolean thread_subject);
void camel_folder_thread_messages_apply(CamelFolderThread *thread, GPtrArray *uids);

/* interface 2: using messageinfo's.  Currently disabled. */
#if 0
/* new improved interface */
CamelFolderThread *camel_folder_thread_messages_new_summary(GPtrArray *summary);
void camel_folder_thread_messages_add(CamelFolderThread *thread, GPtrArray *summary);
void camel_folder_thread_messages_remove(CamelFolderThread *thread, GPtrArray *uids);
#endif

void camel_folder_thread_messages_ref(CamelFolderThread *threads);
void camel_folder_thread_messages_unref(CamelFolderThread *threads);

/* debugging function only */
int camel_folder_threaded_messages_dump(CamelFolderThreadNode *c);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_CAMEL_FOLDER_THREAD_H */
