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

#ifndef _CAMEL_FOLDER_SEARCH_H
#define _CAMEL_FOLDER_SEARCH_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <libedataserver/e-sexp.h>
#include <camel/camel-folder.h>
#include <camel/camel-object.h>
#include <camel/camel-index.h>

#define CAMEL_FOLDER_SEARCH_TYPE         (camel_folder_search_get_type ())
#define CAMEL_FOLDER_SEARCH(obj)         CAMEL_CHECK_CAST (obj, camel_folder_search_get_type (), CamelFolderSearch)
#define CAMEL_FOLDER_SEARCH_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_folder_search_get_type (), CamelFolderSearchClass)
#define CAMEL_IS_FOLDER_SEARCH(obj)      CAMEL_CHECK_TYPE (obj, camel_folder_search_get_type ())

typedef struct _CamelFolderSearchClass CamelFolderSearchClass;

struct _CamelFolderSearch {
	CamelObject parent;

	struct _CamelFolderSearchPrivate *priv;

	ESExp *sexp;		/* s-exp evaluator */
	char *last_search;	/* last searched expression */

	/* these are only valid during the search, and are reset afterwards */
	CamelFolder *folder;	/* folder for current search */
	GPtrArray *summary;	/* summary array for current search */
	GPtrArray *summary_set;	/* subset of summary to actually include in search */
	GHashTable *summary_hash; /* hashtable of summary items */
	CamelMessageInfo *current; /* current message info, when searching one by one */
	CamelMimeMessage *current_message; /* cache of current message, if required */
	CamelIndex *body_index;
};

struct _CamelFolderSearchClass {
	CamelObjectClass parent_class;

	/* general bool/comparison options, usually these wont need to be set, unless it is compiling into another language */
	ESExpResult * (*and)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*or)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*not)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	ESExpResult * (*lt)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*gt)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*eq)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);

	/* search options */
	/* (match-all [boolean expression]) Apply match to all messages */
	ESExpResult * (*match_all)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);

	/* (match-threads "type" [array expression]) add all related threads */
	ESExpResult * (*match_threads)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);

	/* (body-contains "string1" "string2" ...) Returns a list of matches, or true if in single-message mode */
	ESExpResult * (*body_contains)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (header-contains "headername" "string1" ...) List of matches, or true if in single-message mode */
	ESExpResult * (*header_contains)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (header-matches "headername" "string") */
	ESExpResult * (*header_matches)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (header-starts-with "headername" "string") */
	ESExpResult * (*header_starts_with)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (header-ends-with "headername" "string") */
	ESExpResult * (*header_ends_with)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (header-exists "headername") */
	ESExpResult * (*header_exists)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (user-flag "flagname" "flagname" ...) If one of user-flag set */
	ESExpResult * (*user_flag)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (user-tag "flagname") Returns the value of a user tag.  Can only be used in match-all */
	ESExpResult * (*user_tag)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (system-flag "flagname") Returns the value of a system flag.  Can only be used in match-all */
	ESExpResult * (*system_flag)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
	
	/* (get-sent-date) Retrieve the date that the message was sent on as a time_t */
	ESExpResult * (*get_sent_date)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (get-received-date) Retrieve the date that the message was received on as a time_t */
	ESExpResult * (*get_received_date)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (get-current-date) Retrieve 'now' as a time_t */
	ESExpResult * (*get_current_date)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (get-size) Retrieve message size as an int (in kilobytes) */
	ESExpResult * (*get_size)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (uid "uid" ...) True if the uid is in the list */
	ESExpResult * (*uid)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
};

CamelType		camel_folder_search_get_type	(void);
CamelFolderSearch      *camel_folder_search_new	(void);
void camel_folder_search_construct (CamelFolderSearch *search);

/* This stuff currently gets cleared when you run a search ... what on earth was i thinking ... */
void camel_folder_search_set_folder(CamelFolderSearch *search, CamelFolder *folder);
void camel_folder_search_set_summary(CamelFolderSearch *search, GPtrArray *summary);
void camel_folder_search_set_body_index(CamelFolderSearch *search, CamelIndex *index);
/* this interface is deprecated */
GPtrArray *camel_folder_search_execute_expression(CamelFolderSearch *search, const char *expr, CamelException *ex);

GPtrArray *camel_folder_search_search(CamelFolderSearch *search, const char *expr, GPtrArray *uids, CamelException *ex);
void camel_folder_search_free_result(CamelFolderSearch *search, GPtrArray *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_FOLDER_SEARCH_H */
