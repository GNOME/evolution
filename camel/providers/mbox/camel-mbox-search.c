/* 
 * Copyright 2000 HelixCode (http://www.helixcode.com).
 *
 * Author : 
 *  Michael Zucchi <notzed@helixcode.com>

 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <glib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


#include <camel/gmime-utils.h>
#include <camel/camel-log.h>
#include "camel/camel-folder-summary.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-mime-part.h"
#include "camel/camel-stream.h"
#include "camel/camel-stream-fs.h"
#include "camel/camel.h"
#include "camel-mbox-folder.h"

#include "camel-mbox-search.h"
#define HAVE_FILTER
#ifdef HAVE_FILTER
#include "e-sexp.h"

#define HAVE_IBEX
#ifdef HAVE_IBEX
#include "ibex.h"
#endif

#define p(x)			/* parse debug */
#define r(x)			/* run debug */
#define d(x)			/* general debug */


/*

  Matching operators:

  list = (body-contains string+)
  bool = (body-contains string+)
  	Returns a list of all messages containing any of the strings in the message.
	If within a match-all, then returns true for the current message.

  list = (match-all bool-expr)
  	Returns a list of all messages for which the bool expression is true.
	The bool-expr is evaluated for each message in turn.
	It is more efficient not to perform body-content comparisons inside a
	match-all operator.

  int = (date-sent)
  	Returns a time_t of the date-sent of the message.

  bool = (header-contains string string+)
  	Returns true if the current message (inside a match-all operator)
	has a header 'string1', which contains any of the following strings.
*/


struct _searchcontext {
	int whatever;

	CamelFolder *folder;

#ifdef HAVE_IBEX
	ibex *index;		/* index of content for this folder */
#endif

	CamelFolderSummary *summary;
	const GArray *message_info;

	CamelMessageInfo *message_current;	/* when performing a (match  operation */
};

struct _glib_sux_donkeys {
	int count;
	GPtrArray *uids;
};
/* or, store all unique values */
static void
g_lib_sux_htor(char *key, int value, struct _glib_sux_donkeys *fuckup)
{
	g_ptr_array_add(fuckup->uids, key);
}

static ESExpResult *
func_body_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	ESExpResult *r;
	int i, j;
	struct _searchcontext *ctx = data;

	if (ctx->message_current) {
		int truth = FALSE;

		r = e_sexp_result_new(ESEXP_RES_BOOL);
		if (ctx->index) {
			for (i=0;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING) {
					truth = ibex_find_name(ctx->index, ctx->message_current->uid, argv[i]->value.string);
				} else {
					g_warning("Invalid type passed to body-contains match function");
				}
			}
		} else {
			g_warning("Cannot perform indexed query with no index");
		}
		r->value.bool = truth;
	} else {
		r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);

		if (ctx->index) {
			if (argc==1) {
				/* common case */
				r->value.ptrarray = ibex_find(ctx->index, argv[0]->value.string);
			} else {
				GHashTable *ht = g_hash_table_new(g_str_hash, g_str_equal);
				GPtrArray *pa;
				struct _glib_sux_donkeys lambdafoo;

				/* this sux, perform an or operation on the result(s) of each word */
				for (i=0;i<argc;i++) {
					if (argv[i]->type == ESEXP_RES_STRING) {
						pa = ibex_find(ctx->index, argv[i]->value.string);
						for (j=0;j<pa->len;j++) {
							g_hash_table_insert(ht, g_ptr_array_index(pa, j), (void *)1);
						}
						g_ptr_array_free(pa, FALSE);
					}
				}
				lambdafoo.uids = g_ptr_array_new();
				g_hash_table_foreach(ht, (GHFunc)g_lib_sux_htor, &lambdafoo);
				r->value.ptrarray = lambdafoo.uids;
			}
		} else {
			r->value.ptrarray = g_ptr_array_new();
		}
	}

	return r;
}

static ESExpResult *
func_date_sent(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	ESExpResult *r;
	struct _searchcontext *ctx = data;

	r = e_sexp_result_new(ESEXP_RES_INT);

	if (ctx->message_current) {
		g_warning("FIXME: implement date parsing ...");
		/* r->value.number = get_date(ctx->message_current); */
	} else {
		r->value.number = time(0);
	}
	return r;
}


static ESExpResult *
func_match_all(struct _ESExp *f, int argc, struct _ESExpTerm **argv, void *data)
{
	int i;
	ESExpResult *r, *r1;
	struct _searchcontext *ctx = data;

	if (argc>1) {
		g_warning("match-all only takes a single argument, other arguments ignored");
	}
	r = e_sexp_result_new(ESEXP_RES_ARRAY_PTR);
	r->value.ptrarray = g_ptr_array_new();

	for (i=0;i<ctx->message_info->len;i++) {
		if (argc>0) {
			ctx->message_current = &g_array_index(ctx->message_info, CamelMessageInfo, i);
			r1 = e_sexp_term_eval(f, argv[0]);
			if (r1->type == ESEXP_RES_BOOL) {
				if (r1->value.bool)
					g_ptr_array_add(r->value.ptrarray, ctx->message_current->uid);
			} else {
				g_warning("invalid syntax, matches require a single bool result");
			}
			e_sexp_result_free(r1);
		} else {
			g_ptr_array_add(r->value.ptrarray, ctx->message_current->uid);
		}
	}
	ctx->message_current = NULL;

	return r;
}

static ESExpResult *
func_header_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	ESExpResult *r;
	struct _searchcontext *ctx = data;
	int truth = FALSE;

	r(printf("executing header-contains\n"));

	/* are we inside a match-all? */
	if (ctx->message_current && argc>1
	    && argv[0]->type == ESEXP_RES_STRING) {
		char *headername, *header;
		int i;

		/* only a subset of headers are supported .. */
		headername = argv[0]->value.string;
		if (!strcasecmp(headername, "subject")) {
			header = ctx->message_current->subject;
		} else if (!strcasecmp(headername, "date")) {
			header = ctx->message_current->date;
		} else if (!strcasecmp(headername, "from")) {
			header = ctx->message_current->sender;
		} else {
			g_warning("Performing query on unknown header: %s", headername);
			header = NULL;
		}

		if (header) {
			for (i=1;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING
				    && strstr(header, argv[i]->value.string)) {
					printf("%s got a match with %s of %s\n", ctx->message_current->uid, header, argv[i]->value.string);
					truth = TRUE;
					break;
				}
			}
		}
	}
	r = e_sexp_result_new(ESEXP_RES_BOOL);
	r->value.bool = truth;

	return r;
}


/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "body-contains", func_body_contains, 0 },
	{ "date-sent", func_date_sent, 0 },
	{ "match-all", (ESExpFunc *)func_match_all, 1 },
	{ "header-contains", func_header_contains, 0 },
};

GList *
camel_mbox_folder_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	int i;
	struct _searchcontext ctx;
	GList *matches = NULL;
	ESExp *f;
	ESExpResult *r;

	/* setup our expression evaluator */
	f = e_sexp_new();

	/* setup out context */
	ctx.folder = folder;
	ctx.summary = camel_folder_get_summary(folder, ex);
	ctx.message_info = camel_folder_summary_get_message_info_list(ctx.summary);
	ctx.message_current = NULL;
	ctx.index = ibex_open(CAMEL_MBOX_FOLDER(folder)->index_file_path, FALSE);
	if (!ctx.index) {
		perror("Cannot open index file");
	}

	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(f, 0, symbols[i].name, (ESExpIFunc *)symbols[i].func, &ctx);
		} else {
			e_sexp_add_function(f, 0, symbols[i].name, symbols[i].func, &ctx);
		}
	}

	e_sexp_input_text(f, expression, strlen(expression));
	e_sexp_parse(f);
	r = e_sexp_eval(f);

	/* now create a folder summary to return?? */
	if (r
	    && r->type == ESEXP_RES_ARRAY_PTR) {
		d(printf("got result ...\n"));
		for (i=0;i<r->value.ptrarray->len;i++) {
			d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
			matches = g_list_prepend(matches, g_strdup(g_ptr_array_index(r->value.ptrarray, i)));
		}
		e_sexp_result_free(r);
	} else {
		printf("no result!\n");
	}

	if (ctx.index)
		ibex_close(ctx.index);

	gtk_object_unref((GtkObject *)ctx.summary);
	gtk_object_unref((GtkObject *)f);

	return matches;
}

#else /* HAVE_FILTER */

GList *
camel_mbox_folder_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	return NULL;
}

#endif /*! HAVE_FILTER */
