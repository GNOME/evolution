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

#ifndef _CAMEL_FOLDER_SUMMARY_H
#define _CAMEL_FOLDER_SUMMARY_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <stdio.h>
#include <time.h>
#include <camel/camel-mime-parser.h>
#include <camel/camel-object.h>
#include <camel/camel-index.h>

#define CAMEL_FOLDER_SUMMARY_TYPE         camel_folder_summary_get_type ()
#define CAMEL_FOLDER_SUMMARY(obj)         CAMEL_CHECK_CAST (obj, camel_folder_summary_get_type (), CamelFolderSummary)
#define CAMEL_FOLDER_SUMMARY_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_folder_summary_get_type (), CamelFolderSummaryClass)
#define CAMEL_IS_FOLDER_SUMMARY(obj)      CAMEL_CHECK_TYPE (obj, camel_folder_summary_get_type ())

/*typedef struct _CamelFolderSummary      CamelFolderSummary;*/
typedef struct _CamelFolderSummaryClass CamelFolderSummaryClass;

/* A tree of message content info structures
   describe the content structure of the message (if it has any) */
struct _CamelMessageContentInfo {
	struct _CamelMessageContentInfo *next;
	
	struct _CamelMessageContentInfo *childs;
	struct _CamelMessageContentInfo *parent;
	
	CamelContentType *type;
	char *id;
	char *description;
	char *encoding;		/* this should be an enum?? */
	guint32 size;
};

/* system flag bits */
enum _CamelMessageFlags {
	CAMEL_MESSAGE_ANSWERED = 1<<0,
	CAMEL_MESSAGE_DELETED = 1<<1,
	CAMEL_MESSAGE_DRAFT = 1<<2,
	CAMEL_MESSAGE_FLAGGED = 1<<3,
	CAMEL_MESSAGE_SEEN = 1<<4,

	/* these aren't really system flag bits, but are convenience flags */
	CAMEL_MESSAGE_ATTACHMENTS = 1<<5,
	CAMEL_MESSAGE_ANSWERED_ALL = 1<<6,
	CAMEL_MESSAGE_JUNK = 1<<7,

	/* following flags are for the folder, and are not really permanent flags */
	CAMEL_MESSAGE_FOLDER_FLAGGED = 1<<16, /* for use by the folder implementation */

	/* flags after 1<<16 are used by camel providers,
           if adding non permanent flags, add them to the end  */

	CAMEL_MESSAGE_JUNK_LEARN = 1<<30, /* used when setting CAMEL_MESSAGE_JUNK flag
					     to say that we request junk plugin
					     to learn that message as junk/non junk */
	CAMEL_MESSAGE_USER = 1<<31 /* supports user flags */
};

typedef struct _CamelFlag {
	struct _CamelFlag *next;
	char name[1];		/* name allocated as part of the structure */
} CamelFlag;

typedef struct _CamelTag {
	struct _CamelTag *next;
	char *value;
	char name[1];		/* name allocated as part of the structure */
} CamelTag;

/* a summary messageid is a 64 bit identifier (partial md5 hash) */
typedef struct _CamelSummaryMessageID {
	union {
		guint64 id;
		unsigned char hash[8];
		struct {
			guint32 hi;
			guint32 lo;
		} part;
	} id;
} CamelSummaryMessageID;

/* summary references is a fixed size array of references */
typedef struct _CamelSummaryReferences {
	int size;
	CamelSummaryMessageID references[1];
} CamelSummaryReferences;

/* #define DOESTRV */
#define DOEPOOLV

#if defined (DOEPOOLV) || defined (DOESTRV)

/* string array indices */
enum {
	CAMEL_MESSAGE_INFO_UID,
	CAMEL_MESSAGE_INFO_SUBJECT,
	CAMEL_MESSAGE_INFO_FROM,
	CAMEL_MESSAGE_INFO_TO,
	CAMEL_MESSAGE_INFO_CC,
	CAMEL_MESSAGE_INFO_MLIST,
	CAMEL_MESSAGE_INFO_LAST,
};
#endif

/* information about a given object */
struct _CamelMessageInfo {
	/* public fields */
#ifdef DOEPOOLV
	struct _EPoolv *strings;
#elif defined (DOESTRV)
	struct _EStrv *strings;		/* all strings packed into a single compact array */
#else
	char *subject;
	char *from;
	char *to;
	char *cc;
	char *mlist;

	char *uid;
#endif /* DOEPOOLV */
	guint32 flags;
	guint32 size;
	guint32 refcount;

	time_t date_sent;
	time_t date_received;

	CamelSummaryMessageID message_id;/* for this message */
	CamelSummaryReferences *references;/* from parent to root */

	struct _CamelFlag *user_flags;
	struct _CamelTag *user_tags;

	/* tree of content description - NULL if it is not available */
	CamelMessageContentInfo *content;
};

enum _CamelFolderSummaryFlags {
	CAMEL_SUMMARY_DIRTY = 1<<0,
};

struct _CamelFolderSummary {
	CamelObject parent;

	struct _CamelFolderSummaryPrivate *priv;

	/* header info */
	guint32 version;	/* version of file loaded/loading */
	guint32 flags;		/* flags */
	guint32 nextuid;	/* next uid? */
	time_t time;		/* timestamp for this summary (for implementors to use) */
	guint32 saved_count;	/* how many were saved/loaded */
	guint32 unread_count;	/* handy totals */
	guint32 deleted_count;
	guint32 junk_count;

	/* sizes of memory objects */
	guint32 message_info_size;
	guint32 content_info_size;

#if defined (DOEPOOLV) || defined (DOESTRV)
	guint32 message_info_strings;
#endif	
	/* memory allocators (setup automatically) */
	struct _EMemChunk *message_info_chunks;
	struct _EMemChunk *content_info_chunks;

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
	CamelMessageInfo * (*message_info_new)(CamelFolderSummary *, struct _camel_header_raw *);
	CamelMessageInfo * (*message_info_new_from_parser)(CamelFolderSummary *, CamelMimeParser *);
	CamelMessageInfo * (*message_info_new_from_message)(CamelFolderSummary *, CamelMimeMessage *);
	CamelMessageInfo * (*message_info_load)(CamelFolderSummary *, FILE *);
	int		   (*message_info_save)(CamelFolderSummary *, FILE *, CamelMessageInfo *);
	void		   (*message_info_free)(CamelFolderSummary *, CamelMessageInfo *);

	/* save/load individual content info's */
	CamelMessageContentInfo * (*content_info_new)(CamelFolderSummary *, struct _camel_header_raw *);
	CamelMessageContentInfo * (*content_info_new_from_parser)(CamelFolderSummary *, CamelMimeParser *);
	CamelMessageContentInfo * (*content_info_new_from_message)(CamelFolderSummary *, CamelMimePart *);
	CamelMessageContentInfo * (*content_info_load)(CamelFolderSummary *, FILE *);
	int		          (*content_info_save)(CamelFolderSummary *, FILE *, CamelMessageContentInfo *);
	void		          (*content_info_free)(CamelFolderSummary *, CamelMessageContentInfo *);

	/* get the next uid */
	char *(*next_uid_string)(CamelFolderSummary *);
};

CamelType			 camel_folder_summary_get_type	(void);
CamelFolderSummary      *camel_folder_summary_new	(void);

void camel_folder_summary_set_filename(CamelFolderSummary *, const char *);
void camel_folder_summary_set_index(CamelFolderSummary *, CamelIndex *);
void camel_folder_summary_set_build_content(CamelFolderSummary *, gboolean state);

guint32  camel_folder_summary_next_uid        (CamelFolderSummary *s);
char    *camel_folder_summary_next_uid_string (CamelFolderSummary *s);
void 	 camel_folder_summary_set_uid	      (CamelFolderSummary *s, guint32 uid);

/* load/save the summary in its entirety */
int camel_folder_summary_load(CamelFolderSummary *);
int camel_folder_summary_save(CamelFolderSummary *);

/* only load the header */
int camel_folder_summary_header_load(CamelFolderSummary *);

/* set the dirty bit on the summary */
void camel_folder_summary_touch(CamelFolderSummary *s);

/* add a new raw summary item */
void camel_folder_summary_add(CamelFolderSummary *, CamelMessageInfo *info);

/* build/add raw summary items */
CamelMessageInfo *camel_folder_summary_add_from_header(CamelFolderSummary *, struct _camel_header_raw *);
CamelMessageInfo *camel_folder_summary_add_from_parser(CamelFolderSummary *, CamelMimeParser *);
CamelMessageInfo *camel_folder_summary_add_from_message(CamelFolderSummary *, CamelMimeMessage *);

/* Just build raw summary items */
CamelMessageInfo *camel_folder_summary_info_new(CamelFolderSummary *s);
CamelMessageInfo *camel_folder_summary_info_new_from_header(CamelFolderSummary *, struct _camel_header_raw *);
CamelMessageInfo *camel_folder_summary_info_new_from_parser(CamelFolderSummary *, CamelMimeParser *);
CamelMessageInfo *camel_folder_summary_info_new_from_message(CamelFolderSummary *, CamelMimeMessage *);

void camel_folder_summary_info_ref(CamelFolderSummary *, CamelMessageInfo *);
void camel_folder_summary_info_free(CamelFolderSummary *, CamelMessageInfo *);

CamelMessageContentInfo *camel_folder_summary_content_info_new(CamelFolderSummary *s);
void camel_folder_summary_content_info_free(CamelFolderSummary *s, CamelMessageContentInfo *ci);

/* removes a summary item, doesn't fix content offsets */
void camel_folder_summary_remove(CamelFolderSummary *s, CamelMessageInfo *info);
void camel_folder_summary_remove_uid(CamelFolderSummary *s, const char *uid);
void camel_folder_summary_remove_index(CamelFolderSummary *s, int);
void camel_folder_summary_remove_range(CamelFolderSummary *s, int start, int end);

/* remove all items */
void camel_folder_summary_clear(CamelFolderSummary *s);

/* lookup functions */
int camel_folder_summary_count(CamelFolderSummary *);
CamelMessageInfo *camel_folder_summary_index(CamelFolderSummary *, int);
CamelMessageInfo *camel_folder_summary_uid(CamelFolderSummary *, const char *uid);
GPtrArray *camel_folder_summary_array(CamelFolderSummary *s);
void camel_folder_summary_array_free(CamelFolderSummary *s, GPtrArray *array);

/* summary formatting utils */
char *camel_folder_summary_format_address(struct _camel_header_raw *h, const char *name);
char *camel_folder_summary_format_string(struct _camel_header_raw *h, const char *name);

/* basically like strings, but certain keywords can be compressed and de-cased */
int camel_folder_summary_encode_token(FILE *, const char *);
int camel_folder_summary_decode_token(FILE *, char **);

/* message flag operations */
gboolean	camel_flag_get(CamelFlag **list, const char *name);
gboolean	camel_flag_set(CamelFlag **list, const char *name, gboolean state);
gboolean	camel_flag_list_copy(CamelFlag **to, CamelFlag **from);
int		camel_flag_list_size(CamelFlag **list);
void		camel_flag_list_free(CamelFlag **list);

guint32         camel_system_flag (const char *name);
gboolean        camel_system_flag_get (guint32 flags, const char *name);

/* message tag operations */
const char	*camel_tag_get(CamelTag **list, const char *name);
gboolean	camel_tag_set(CamelTag **list, const char *name, const char *value);
gboolean	camel_tag_list_copy(CamelTag **to, CamelTag **from);
int		camel_tag_list_size(CamelTag **list);
void		camel_tag_list_free(CamelTag **list);

/* message info utils for working with pseudo-messageinfo structures
   NOTE: These cannot be added to a real summary object, but suffice for all
   other external interfaces that use message info's */
CamelMessageInfo *camel_message_info_new(void);
void camel_message_info_ref(CamelMessageInfo *info);
CamelMessageInfo *camel_message_info_new_from_header(struct _camel_header_raw *header);
void camel_message_info_dup_to(const CamelMessageInfo *from, CamelMessageInfo *to);
void camel_message_info_free(CamelMessageInfo *mi);

/* accessors */
#if defined (DOEPOOLV) || defined (DOESTRV)
const char *camel_message_info_string(const CamelMessageInfo *mi, int type);
#define camel_message_info_subject(x) camel_message_info_string((const CamelMessageInfo *)(x), CAMEL_MESSAGE_INFO_SUBJECT)
#define camel_message_info_from(x) camel_message_info_string((const CamelMessageInfo *)(x), CAMEL_MESSAGE_INFO_FROM)
#define camel_message_info_to(x) camel_message_info_string((const CamelMessageInfo *)(x), CAMEL_MESSAGE_INFO_TO)
#define camel_message_info_cc(x) camel_message_info_string((const CamelMessageInfo *)(x), CAMEL_MESSAGE_INFO_CC)
#define camel_message_info_uid(x) camel_message_info_string((const CamelMessageInfo *)(x), CAMEL_MESSAGE_INFO_UID)
#define camel_message_info_mlist(x) camel_message_info_string((const CamelMessageInfo *)(x), CAMEL_MESSAGE_INFO_MLIST)

void camel_message_info_set_string(CamelMessageInfo *mi, int type, char *str);
#define camel_message_info_set_subject(x, s) camel_message_info_set_string(x, CAMEL_MESSAGE_INFO_SUBJECT, s)
#define camel_message_info_set_from(x, s) camel_message_info_set_string(x, CAMEL_MESSAGE_INFO_FROM, s)
#define camel_message_info_set_to(x, s) camel_message_info_set_string(x, CAMEL_MESSAGE_INFO_TO, s)
#define camel_message_info_set_cc(x, s) camel_message_info_set_string(x, CAMEL_MESSAGE_INFO_CC, s)
#define camel_message_info_set_uid(x, s) camel_message_info_set_string(x, CAMEL_MESSAGE_INFO_UID, s)
#define camel_message_info_set_mlist(x, s) camel_message_info_set_string(x, CAMEL_MESSAGE_INFO_MLIST, s)

#else

#define camel_message_info_subject(x) (((CamelMessageInfo *)(x))->subject?((CamelMessageInfo *)(x))->subject:"")
#define camel_message_info_from(x) (((CamelMessageInfo *)(x))->from?((CamelMessageInfo *)(x))->from:"")
#define camel_message_info_to(x) (((CamelMessageInfo *)(x))->to?((CamelMessageInfo *)(x))->to:"")
#define camel_message_info_cc(x) (((CamelMessageInfo *)(x))->cc?((CamelMessageInfo *)(x))->cc:"")
#define camel_message_info_uid(x) (((CamelMessageInfo *)(x))->uid?((CamelMessageInfo *)(x))->uid:"")
#define camel_message_info_mlist(x) (((CamelMessageInfo *)(x))->mlist?((CamelMessageInfo *)(x))->mlist:"")

#define camel_message_info_set_subject(x, s) (g_free(((CamelMessageInfo *)(x))->subject),((CamelMessageInfo *)(x))->subject = (s))
#define camel_message_info_set_from(x, s) (g_free(((CamelMessageInfo *)(x))->from),((CamelMessageInfo *)(x))->from = (s))
#define camel_message_info_set_to(x, s) (g_free(((CamelMessageInfo *)(x))->to),((CamelMessageInfo *)(x))->to = (s))
#define camel_message_info_set_cc(x, s) (g_free(((CamelMessageInfo *)(x))->cc),((CamelMessageInfo *)(x))->cc = (s))
#define camel_message_info_set_uid(x, s) (g_free(((CamelMessageInfo *)(x))->uid),((CamelMessageInfo *)(x))->uid = (s))
#define camel_message_info_set_mlist(x, s) (g_free(((CamelMessageInfo *)(x))->mlist),((CamelMessageInfo *)(x))->mlist = (s))
#endif


/* debugging functions */

void camel_content_info_dump (CamelMessageContentInfo *ci, int depth);

void camel_message_info_dump (CamelMessageInfo *mi);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_FOLDER_SUMMARY_H */
