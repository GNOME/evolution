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

#ifndef _CAMEL_FOLDER_SEARCH_H
#define _CAMEL_FOLDER_SEARCH_H

#include <camel/camel-object.h>
#include <e-util/e-sexp.h>
#include <libibex/ibex.h>
#include <camel/camel-folder.h>

#define CAMEL_FOLDER_SEARCH(obj)         GTK_CHECK_CAST (obj, camel_folder_search_get_type (), CamelFolderSearch)
#define CAMEL_FOLDER_SEARCH_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, camel_folder_search_get_type (), CamelFolderSearchClass)
#define IS_CAMEL_FOLDER_SEARCH(obj)      GTK_CHECK_TYPE (obj, camel_folder_search_get_type ())

typedef struct _CamelFolderSearchClass CamelFolderSearchClass;

struct _CamelFolderSearch {
	CamelObject parent;

	struct _CamelFolderSearchPrivate *priv;

	ESExp *sexp;		/* s-exp evaluator */
	char *last_search;	/* last searched expression */

	/* these are only valid during the search, and are reset afterwards */
	CamelFolder *folder;	/* folder for current search */
	GPtrArray *summary;	/* summary array for current search */
	CamelMessageInfo *current; /* current message info, when searching one by one */
	ibex *body_index;
};

struct _CamelFolderSearchClass {
	CamelObjectClass parent_class;

	/* general bool/comparison options, usually these wont need to be set, unless it is compiling into another language */
	ESExpResult * (*and)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*or)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*not)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*lt)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*gt)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);
	ESExpResult * (*eq)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);

	/* search options */
	/* (match-all [boolean expression]) Apply match to all messages */
	ESExpResult * (*match_all)(struct _ESExp *f, int argc, struct _ESExpTerm **argv, CamelFolderSearch *s);

	/* (body-contains "string1" "string2" ...) Returns a list of matches, or true if in single-message mode */
	ESExpResult * (*body_contains)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (header-contains "headername" "string1" ...) List of matches, or true if in single-message mode */
	ESExpResult * (*header_contains)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);

	/* (user-flag "flagname" "flagname" ...) If one of user-flag set */
	ESExpResult * (*user_flag)(struct _ESExp *f, int argc, struct _ESExpResult **argv, CamelFolderSearch *s);
};

guint		camel_folder_search_get_type	(void);
CamelFolderSearch      *camel_folder_search_new	(void);
void camel_folder_search_construct (CamelFolderSearch *search);

void camel_folder_search_set_folder(CamelFolderSearch *search, CamelFolder *folder);
void camel_folder_search_set_summary(CamelFolderSearch *search, GPtrArray *summary);
void camel_folder_search_set_body_index(CamelFolderSearch *search, ibex *index);
GList *camel_folder_search_execute_expression(CamelFolderSearch *search, const char *expr, CamelException *ex);

#endif /* ! _CAMEL_FOLDER_SEARCH_H */
