/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#ifndef _CAMEL_FOLDER_SUMMARY_H
#define _CAMEL_FOLDER_SUMMARY_H

#include <camel/camel-object.h>
#include <stdio.h>
#include <time.h>
#include <camel/camel-mime-parser.h>
#include <libibex/ibex.h>

#define CAMEL_FOLDER_SUMMARY(obj)         GTK_CHECK_CAST (obj, camel_folder_summary_get_type (), CamelFolderSummary)
#define CAMEL_FOLDER_SUMMARY_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_folder_summary_get_type (), CamelFolderSummaryClass)
#define IS_CAMEL_FOLDER_SUMMARY(obj)      GTK_CHECK_TYPE (obj, camel_folder_summary_get_type ())

/*typedef struct _CamelFolderSummary      CamelFolderSummary;*/
typedef struct _CamelFolderSummaryClass CamelFolderSummaryClass;

/* these structs from camel-folder-summary.h ... (remove comment after cleanup soon) */
/* TODO: perhaps they should be full-block objects? */
/* FIXME: rename this to something more suitable */
typedef struct {
	gchar *name;
	gint nb_message;	/* ick, these should be renamed to something better */
	gint nb_unread_message;
	gint nb_deleted_message;
} CamelFolderInfo;

/* A tree of message content info structures
   describe the content structure of the message (if it has any) */
typedef struct _CamelMessageContentInfo {
	struct _CamelMessageContentInfo *next;

	struct _CamelMessageContentInfo *childs;
	struct _CamelMessageContentInfo *parent;

	struct _header_content_type *type;
	char *id;
	char *description;
	char *encoding;

	/* information about where this object lives in the stream.
	   if pos is -1 these are all invalid */
	off_t pos;
	off_t bodypos;
	off_t endpos;
} CamelMessageContentInfo;

/* information about a given object */
typedef struct {
	/* public fields */
	gchar *subject;
	gchar *to;
	gchar *from;

	gchar *uid;
	guint32 flags;
	guint32 size;

	time_t date_sent;
	time_t date_received;

	/* Message-ID / References structures */
	char *message_id;	/* for this message */
	struct _header_references *references; /* from parent to root */

	struct _CamelFlag *user_flags;

	/* tree of content description - NULL if it is not available */
	CamelMessageContentInfo *content;
} CamelMessageInfo;

enum _CamelFolderSummaryFlags {
	CAMEL_SUMMARY_DIRTY = 1<<0,
};

struct _CamelFolderSummary {
	CamelObject parent;

	struct _CamelFolderSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of file required, should be set by implementors */
	guint32 flags;		/* flags */
	guint32 nextuid;	/* next uid? */
	guint32 saved_count;	/* how many were saved/loaded */
	time_t time;		/* timestamp for this summary (for implementors to use) */

	/* sizes of memory objects */
	guint32 message_info_size;
	guint32 content_info_size;

	char *summary_path;
	gboolean build_content;	/* do we try and parse/index the content, or not? */

	GPtrArray *messages;	/* CamelMessageInfo's */
	GHashTable *messages_uid; /* CamelMessageInfo's by uid */
};

struct _CamelFolderSummaryClass {
	CamelObjectClass parent_class;

	/* load/save the global info */
	int (*summary_header_load)(CamelFolderSummary *, FILE *);
	int (*summary_header_save)(CamelFolderSummary *, FILE *);

	/* create/save/load an individual message info */
	CamelMessageInfo * (*message_info_new)(CamelFolderSummary *, struct _header_raw *);
	CamelMessageInfo * (*message_info_new_from_parser)(CamelFolderSummary *, CamelMimeParser *);
	CamelMessageInfo * (*message_info_load)(CamelFolderSummary *, FILE *);
	int		   (*message_info_save)(CamelFolderSummary *, FILE *, CamelMessageInfo *);
	void		   (*message_info_free)(CamelFolderSummary *, CamelMessageInfo *);

	/* save/load individual content info's */
	CamelMessageContentInfo * (*content_info_new)(CamelFolderSummary *, struct _header_raw *);
	CamelMessageContentInfo * (*content_info_new_from_parser)(CamelFolderSummary *, CamelMimeParser *);
	CamelMessageContentInfo * (*content_info_load)(CamelFolderSummary *, FILE *);
	int		          (*content_info_save)(CamelFolderSummary *, FILE *, CamelMessageContentInfo *);
	void		          (*content_info_free)(CamelFolderSummary *, CamelMessageContentInfo *);
};

guint			 camel_folder_summary_get_type	(void);
CamelFolderSummary      *camel_folder_summary_new	(void);

void camel_folder_summary_set_filename(CamelFolderSummary *, const char *);
void camel_folder_summary_set_index(CamelFolderSummary *, ibex *);
void camel_folder_summary_set_uid(CamelFolderSummary *, guint32);
void camel_folder_summary_set_build_content(CamelFolderSummary *, gboolean state);

guint32 camel_folder_summary_next_uid(CamelFolderSummary *s);

/* load/save the summary in its entirety */
int camel_folder_summary_load(CamelFolderSummary *);
int camel_folder_summary_save(CamelFolderSummary *);

/* set the dirty bit on the summary */
void camel_folder_summary_touch(CamelFolderSummary *s);

/* add a new raw summary item */
void camel_folder_summary_add(CamelFolderSummary *, CamelMessageInfo *info);

/* build/add raw summary items */
CamelMessageInfo *camel_folder_summary_add_from_header(CamelFolderSummary *, struct _header_raw *);
CamelMessageInfo *camel_folder_summary_add_from_parser(CamelFolderSummary *, CamelMimeParser *);

/* removes a summary item, doesn't fix content offsets */
void camel_folder_summary_remove(CamelFolderSummary *s, CamelMessageInfo *info);
void camel_folder_summary_remove_uid(CamelFolderSummary *s, const char *uid);
/* remove all items */
void camel_folder_summary_clear(CamelFolderSummary *s);

/* lookup functions */
int camel_folder_summary_count(CamelFolderSummary *);
CamelMessageInfo *camel_folder_summary_index(CamelFolderSummary *, int);
CamelMessageInfo *camel_folder_summary_uid(CamelFolderSummary *, const char *uid);

/* utility functions */
void camel_folder_summary_set_flags_by_uid(CamelFolderSummary *s, const char *uid, guint32 flags);
/* shift content ... */
void camel_folder_summary_offset_content(CamelMessageContentInfo *content, off_t offset);

/* summary file loading/saving helper functions */
int camel_folder_summary_encode_fixed_int32(FILE *, gint32);
int camel_folder_summary_decode_fixed_int32(FILE *, gint32 *);

int camel_folder_summary_encode_uint32(FILE *, guint32);
int camel_folder_summary_decode_uint32(FILE *, guint32 *);

int camel_folder_summary_encode_string(FILE *, char *);
int camel_folder_summary_decode_string(FILE *, char **);

/* basically like strings, but certain keywords can be compressed and de-cased */
int camel_folder_summary_encode_token(FILE *, char *);
int camel_folder_summary_decode_token(FILE *, char **);

#endif /* ! _CAMEL_FOLDER_SUMMARY_H */
