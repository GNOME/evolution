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
#include "camel/camel-mime-message.h"
#include "camel/camel-mime-part.h"
#include "camel/camel-stream.h"
#include "camel/camel-stream-fs.h"
#include "camel/camel.h"
#include "camel-mbox-folder.h"

#define HAVE_IBEX
#ifdef HAVE_IBEX
#include "ibex.h"
#endif

#define p(x)			/* parse debug */
#define r(x)			/* run debug */
#define d(x)			/* general debug */


/*

  This is not yet complete.

  The following s-exp's are supported:

  list = (and list*)
	perform an intersection of a number of lists, and return that.

  bool = (and bool*)
	perform a boolean AND of boolean values.

  list = (or list*)
	perform a union of a number of lists, returning the new list.

  bool = (or bool*)
	perform a boolean OR of boolean values.

  Comparison operators:

  bool = (lt int int)
  bool = (gt int int)
  bool = (eq int int)

  bool = (lt string string)
  bool = (gt string string)
  bool = (eq string string)
	Perform a comparision of 2 integers, or 2 string values.

  Matching operators:

  list = (contains string)
  	Returns a list of all messages containing the string in their body.

  list = (match-all bool-expr)
  	Returns a list of all messages for which the bool expression is true.
	The bool-expr is evaluated for each message in turn.
	It is more efficient not to perform body-content comparisons inside a
	match-all operator.

  int = (date-sent)
  	Returns a time_t of the date-sent of the message.

  bool = (header-contains string1 string2)
  	Returns true if the current message (inside a match-all operator)
	has a header 'string1', which contains 'string2'
*/

static GScannerConfig scanner_config =
{
  (
   " \t\r\n"
   )                    /* cset_skip_characters */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )                    /* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "_0123456789-"
   G_CSET_A_2_Z
   G_CSET_LATINS
   G_CSET_LATINC
   )                    /* cset_identifier_nth */,
  ( "#\n" )             /* cpair_comment_single */,
  
  FALSE                 /* case_sensitive */,
  
  TRUE                  /* skip_comment_multi */,
  TRUE                  /* skip_comment_single */,
  TRUE                  /* scan_comment_multi */,
  TRUE                  /* scan_identifier */,
  FALSE                 /* scan_identifier_1char */,
  FALSE                 /* scan_identifier_NULL */,
  TRUE                  /* scan_symbols */,
  FALSE                 /* scan_binary */,
  TRUE                  /* scan_octal */,
  TRUE                  /* scan_float */,
  TRUE                  /* scan_hex */,
  FALSE                 /* scan_hex_dollar */,
  TRUE                  /* scan_string_sq */,
  TRUE                  /* scan_string_dq */,
  TRUE                  /* numbers_2_int */,
  FALSE                 /* int_2_float */,
  FALSE                 /* identifier_2_string */,
  TRUE                  /* char_2_token */,
  FALSE                 /* symbol_2_token */,
  FALSE                 /* scope_0_fallback */,
};


enum _searchtermtype_t {
	SEARCH_AND,
	SEARCH_OR,
	SEARCH_LT,
	SEARCH_GT,
	SEARCH_EQ,
	SEARCH_CONTAINS,
	SEARCH_DATESENT,
	SEARCH_STRING,
	SEARCH_INT,
	SEARCH_FUNC,
};

struct _searchterm {
	enum _searchtermtype_t type;
	union {
		char *string;
		int number;
		struct {
			struct _searchterm_symbol *sym;
			struct _searchterm **terms;
			int termcount;
		} func;
	} value;
};

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

enum _searchresulttype_t {
	RESULT_ARRAY_PTR=0,
	RESULT_INT,
	RESULT_STRING,
	RESULT_BOOL,
	RESULT_UNDEFINED
};

struct _searchresult {
	enum _searchresulttype_t type;
	union {
		GPtrArray *ptrarray;
		int number;
		char *string;
		int bool;
	} value;
};

/* function callbacks */
static struct _searchresult *search_contains(struct _searchcontext *ctx, struct _searchterm *t);
static struct _searchresult *search_matches(struct _searchcontext *ctx, struct _searchterm *t);
static struct _searchresult *search_date_sent(struct _searchcontext *ctx, struct _searchterm *t);
static struct _searchresult *header_contains(struct _searchcontext *ctx, struct _searchterm *t);

struct _searchterm_symbol {
	char *name;
	int type;
	int argtype;
	struct _searchresult * (*func)(struct _searchcontext *ctx, struct _searchterm *t);
} symbols[] = {
	{ "and", SEARCH_AND, 0, NULL },
	{ "or", SEARCH_OR, 0, NULL },
	{ "lt", SEARCH_LT, 1, NULL },
	{ "gt", SEARCH_GT, 1, NULL },
	{ "eq", SEARCH_EQ, 1, NULL },
	{ "contains", SEARCH_FUNC, 1, search_contains },
	{ "match-all", SEARCH_FUNC, 1, search_matches },
	{ "date-sent", SEARCH_FUNC, 1, search_date_sent },
	{ "header-contains", SEARCH_FUNC, 1, header_contains },
};

static struct _searchterm * parse_list(GScanner *gs, int gotbrace);
static struct _searchterm * parse_value(GScanner *gs);

static struct _searchresult *term_eval(struct _searchcontext *ctx, struct _searchterm *t);
static void parse_dump_term(struct _searchterm *t, int depth);

/* can you tell, i dont like glib? */
struct _glib_sux_donkeys {
	int count;
	GPtrArray *uids;
};


/* ok, store any values that are in all sets */
static void
g_lib_sux_htand(char *key, int value, struct _glib_sux_donkeys *fuckup)
{
	if (value == fuckup->count) {
		g_ptr_array_add(fuckup->uids, key);
	}
}

/* or, store all unique values */
static void
g_lib_sux_htor(char *key, int value, struct _glib_sux_donkeys *fuckup)
{
	g_ptr_array_add(fuckup->uids, key);
}

static struct _searchresult *
result_new(int type)
{
	struct _searchresult *r = g_malloc0(sizeof(*r));
	r->type = type;
	return r;
}

static void
result_free(struct _searchresult *t)
{
	switch(t->type) {
	case RESULT_ARRAY_PTR:
		g_ptr_array_free(t->value.ptrarray, TRUE);
		break;
	case RESULT_BOOL:
	case RESULT_INT:
		break;
	case RESULT_STRING:
		g_free(t->value.string);
		break;
	case RESULT_UNDEFINED:
		break;
	}
	g_free(t);
}

static struct _searchresult *search_contains(struct _searchcontext *ctx, struct _searchterm *t)
{
	struct _searchresult *r, *r1;

	r = result_new(RESULT_UNDEFINED);

	if (t->value.func.termcount>0) {
		if (t->value.func.termcount!=1) {
			printf("warning, only looking for first string in contains clause\n");
		}
		r1 = term_eval(ctx, t->value.func.terms[0]);
		if (r1->type == RESULT_STRING) {
			if (ctx->message_current) {
				int truth = FALSE;
#ifdef HAVE_IBEX
				int i;
				GPtrArray *array;

				if (ctx->index) {
					array = ibex_find(ctx->index, r1->value.string);

					for (i=0;i<array->len;i++) {
						if (!strcmp(g_ptr_array_index(array, i), ctx->message_current->uid)) {
							truth = TRUE;
							break;
						}
					}
					g_ptr_array_free(array, TRUE);
				}
#endif
				r->type = RESULT_BOOL;
				r->value.bool = truth;
			} else {
				r->type = RESULT_ARRAY_PTR;
#ifdef HAVE_IBEX
				if (ctx->index) {
				/* blah, this should probably copy the index strings? */
					r->value.ptrarray = ibex_find(ctx->index, r1->value.string);
				} else {
					r->value.ptrarray = g_ptr_array_new();
				}
#endif
			}
		} else {
			printf("you can't search for a contents of a non-string, fool\n");
		}
		result_free(r1);
	}
	return r;
}

/* run a sub-tree of commands which match on header fields etc */
static struct _searchresult *search_matches(struct _searchcontext *ctx, struct _searchterm *t)
{
	int i;
	struct _searchresult *r, *r1;

	if (t->value.func.termcount == 1) {
		r = result_new(RESULT_ARRAY_PTR);
		r->value.ptrarray = g_ptr_array_new();

		for (i=0;i<ctx->message_info->len;i++) {
			ctx->message_current = &g_array_index(ctx->message_info, CamelMessageInfo, i);
			r1 = term_eval(ctx, t->value.func.terms[0]);
			if (r1->type == RESULT_BOOL) {
				if (r1->value.bool) {
					r(printf("adding message %s\n", ctx->message_current->uid));
					g_ptr_array_add(r->value.ptrarray, ctx->message_current->uid);
				}
			} else {
				printf("invalid syntax, matches require a single bool result\n");
			}
			result_free(r1);
		}
		ctx->message_current = NULL;
	} else {
		r = result_new(RESULT_UNDEFINED);
		printf("invalid syntax, matches only allows a single bool arg\n");
	}
	return r;
}

/* these variable-getting things could be put into 1 function */
static struct _searchresult *search_date_sent(struct _searchcontext *ctx, struct _searchterm *t)
{
	struct _searchresult *r;

	if (ctx->message_current) {
		r = result_new(RESULT_INT);
		r->value.number = time(0);
		/*		r->value.number = ctx->current_message->date_sent;*/
	} else {
		r = result_new(RESULT_UNDEFINED);
	}
	return r;
}

/* header contains - can only be used inside a match-all construct */
/* all headers should be inside a lookup table, so this can search
   all header types */
static struct _searchresult *header_contains(struct _searchcontext *ctx, struct _searchterm *t)
{
	struct _searchresult *r;

	r(printf("executing header-contains\n"));

	/* are we inside a match-all? */
	if (ctx->message_current
	    && t->value.func.termcount == 2) {
		char *header, *substring;
		int truth = FALSE;
		struct _searchresult *r1, *r2;

		r1 = term_eval(ctx, t->value.func.terms[0]);
		r2 = term_eval(ctx, t->value.func.terms[1]);

		if (r1->type == RESULT_STRING
		    && r2->type == RESULT_STRING) {

			header = r1->value.string;
			substring = r2->value.string;
			if (!strcasecmp(header, "subject")) {
				r(printf("comparing subject: %s\n", ctx->message_current->subject));
				if (ctx->message_current->subject)
					truth = (strstr(ctx->message_current->subject, substring)) != NULL;
				else
					printf("Warning: no subject line in message?\n");
			}
			r(printf("header-contains %s %s = %s\n", header, substring, truth?"TRUE":"FALSE"));
		}

		result_free(r1);
		result_free(r2);

		r = result_new(RESULT_BOOL);
		r->value.number = truth;
	} else {
		r = result_new(RESULT_UNDEFINED);
	}
	return r;
}


static struct _searchresult *
term_eval(struct _searchcontext *ctx, struct _searchterm *t)
{
	struct _searchresult *r, *r1, *r2;
	int i;

	r(printf("eval term :\n"));
	r(parse_dump_term(t, 0));

	r = g_malloc0(sizeof(*r));
	r->type = RESULT_UNDEFINED;

	switch (t->type) {
	case SEARCH_AND: {
		GHashTable *ht = g_hash_table_new(g_str_hash, g_str_equal);
		struct _glib_sux_donkeys lambdafoo;
		int type=-1;
		int bool = TRUE;

		r(printf("( and\n"));

		for (i=0;bool && i<t->value.func.termcount;i++) {
			r1 = term_eval(ctx, t->value.func.terms[i]);
			if (type == -1)
				type = r1->type;
			if (type != r1->type) {
				printf("invalid types in and operation, all types must be the same\n");
			} else if ( r1->type == RESULT_ARRAY_PTR ) {
				char **a1;
				int l1, j;

				a1 = (char **)r1->value.ptrarray->pdata;
				l1 = r1->value.ptrarray->len;
				for (j=0;i<l1;j++) {
					int n;
					n = (int)g_hash_table_lookup(ht, a1[i]);
					g_hash_table_insert(ht, a1[i], (void *)n+1);
				}
			} else if ( r1->type == RESULT_BOOL ) {
				bool &= r1->value.bool;
			}
			result_free(r1);
		}

		if (type == RESULT_ARRAY_PTR) {
			lambdafoo.count = t->value.func.termcount;
			lambdafoo.uids = g_ptr_array_new();
			g_hash_table_foreach(ht, (GHFunc)g_lib_sux_htand, &lambdafoo);
			r->type = RESULT_ARRAY_PTR;
			r->value.ptrarray = lambdafoo.uids;
		} else if (type == RESULT_BOOL) {
			r->type = RESULT_BOOL;
			r->value.bool = bool;
		}

		g_hash_table_destroy(ht);

		break; }
	case SEARCH_OR: {
		GHashTable *ht = g_hash_table_new(g_str_hash, g_str_equal);
		struct _glib_sux_donkeys lambdafoo;
		int type = -1;
		int bool = FALSE;
		
		r(printf("(or \n"));

		for (i=0;!bool && i<t->value.func.termcount;i++) {
			r1 = term_eval(ctx, t->value.func.terms[i]);
			if (type == -1)
				type = r1->type;
			if (r1->type != type) {
				printf("wrong types in or operation\n");
			} else if (r1->type == RESULT_ARRAY_PTR) {
				char **a1;
				int l1, j;

				a1 = (char **)r1->value.ptrarray->pdata;
				l1 = r1->value.ptrarray->len;
				for (j=0;i<l1;j++) {
					g_hash_table_insert(ht, a1[j], (void *)1);
				}
			} else if (r1->type == RESULT_BOOL) {
				bool |= r1->value.bool;				
			}
			result_free(r1);
		}

		if (type == RESULT_ARRAY_PTR) {
			lambdafoo.count = t->value.func.termcount;
			lambdafoo.uids = g_ptr_array_new();
			g_hash_table_foreach(ht, (GHFunc)g_lib_sux_htor, &lambdafoo);
			r->type = RESULT_ARRAY_PTR;
			r->value.ptrarray = lambdafoo.uids;
		} else if (type == RESULT_BOOL) {
			r->type = RESULT_BOOL;
			r->value.bool = bool;
		}
		g_hash_table_destroy(ht);

		break; }
	case SEARCH_LT:
		r(printf("(lt \n"));
		if (t->value.func.termcount == 2) {
			r1 = term_eval(ctx, t->value.func.terms[0]);
			r2 = term_eval(ctx, t->value.func.terms[1]);
			if (r1->type != r2->type) {
				printf("error, invalid types in compare\n");
			} else if (r1->type == RESULT_INT) {
				r->type = RESULT_BOOL;
				r->value.bool = r1->value.number < r2->value.number;
			} else if (r1->type == RESULT_STRING) {
				r->type = RESULT_BOOL;
				r->value.bool = strcmp(r1->value.string, r2->value.string) < 0;
			}
		}
		break;
	case SEARCH_GT:
		r(printf("(gt \n"));
		if (t->value.func.termcount == 2) {
			r1 = term_eval(ctx, t->value.func.terms[0]);
			r2 = term_eval(ctx, t->value.func.terms[1]);
			if (r1->type != r2->type) {
				printf("error, invalid types in compare\n");
			} else if (r1->type == RESULT_INT) {
				r->type = RESULT_BOOL;
				r->value.bool = r1->value.number > r2->value.number;
			} else if (r1->type == RESULT_STRING) {
				r->type = RESULT_BOOL;
				r->value.bool = strcmp(r1->value.string, r2->value.string) > 0;
			}
		}
		break;
	case SEARCH_STRING:
		r(printf(" (string \"%s\")\n", t->value.string));
		r->type = RESULT_STRING;
		/* erk, this shoul;dn't need to strdup this ... */
		r->value.string = g_strdup(t->value.string);
		break;
	case SEARCH_INT:
		r(printf(" (int %d)\n", t->value.number));
		r->type = RESULT_INT;
		r->value.number = t->value.number;
		break;
	case SEARCH_FUNC:
		g_free(r);	/* <---- FIXME: ICK !! */
		r(printf("function '%s'\n", t->value.func.sym->name));
		return t->value.func.sym->func(ctx, t);
	default:
		printf("Warning: Unknown type encountered in parse tree: %d\n", t->type);
		r->type = RESULT_UNDEFINED;
	}

	return r;
}


static void
parse_dump_term(struct _searchterm *t, int depth)
{
	int dumpvals = FALSE;
	int i;

	if (t==NULL) {
		printf("null term??\n");
		return;
	}

	for (i=0;i<depth;i++)
		printf("   ");
	
	switch (t->type) {
	case SEARCH_AND:
		printf("(and \n");
		dumpvals = 1;
		break;
	case SEARCH_OR:
		printf("(or \n");
		dumpvals = 1;
		break;
	case SEARCH_LT:
		printf("(lt \n");
		dumpvals = 1;
		break;
	case SEARCH_GT:
		printf("(gt \n");
		dumpvals = 1;
		break;
	case SEARCH_STRING:
		printf(" \"%s\"", t->value.string);
		break;
	case SEARCH_INT:
		printf(" %d", t->value.number);
		break;
	case SEARCH_FUNC:
		printf(" (function %s", t->value.func.sym->name);
		dumpvals = 1;
		break;
	default:
		printf("unknown type: %d\n", t->type);
	}

	if (dumpvals) {
		/*printf(" [%d] ", t->value.func.termcount);*/
		for (i=0;i<t->value.func.termcount;i++) {
			parse_dump_term(t->value.func.terms[i], depth+1);
		}
		for (i=0;i<depth;i++)
			printf("   ");
		printf(")\n");
	}
	printf("\n");
}

/*
  PARSER
*/

static struct _searchterm *
parse_new_term(int type)
{
	struct _searchterm *s = g_malloc0(sizeof(*s));
	s->type = type;
	return s;
}

static void
parse_term_free(struct _searchterm *t)
{
	int i;

	if (t==NULL) {
		return;
	}
	
	switch (t->type) {
	case SEARCH_AND:
	case SEARCH_OR:
	case SEARCH_LT:
	case SEARCH_GT:
	case SEARCH_FUNC:
		for (i=0;i<t->value.func.termcount;i++) {
			parse_term_free(t->value.func.terms[i]);
		}
		g_free(t->value.func.terms);
		break;
	case SEARCH_STRING:
		g_free(t->value.string);
		break;
	case SEARCH_INT:
		break;
	default:
		printf("parse_term_free: unknown type: %d\n", t->type);
	}
	g_free(t);
}

static struct _searchterm **
parse_lists(GScanner *gs, int *len)
{
	int token;
	struct _searchterm **terms;
	int i=0;

	p(printf("parsing lists\n"));

	terms = g_malloc0(20*sizeof(*terms));

	while ( (token = g_scanner_peek_next_token(gs)) != G_TOKEN_EOF
		&& token != ')') {
		terms[i]=parse_list(gs, FALSE);
		i++;
	}

	if (len)
		*len = i;

	p(printf("found %d subterms\n", i));

	p(printf("done parsing lists, token= %d %c\n", token, token));
	return terms;
}

static struct _searchterm **
parse_values(GScanner *gs, int *len)
{
	int token;
	struct _searchterm **terms;
	int i=0;

	p(printf("parsing values\n"));

	terms = g_malloc0(20*sizeof(*terms));

	while ( (token = g_scanner_peek_next_token(gs)) != G_TOKEN_EOF
		&& token != ')') {
		terms[i]=parse_value(gs);
		i++;
	}

	p(printf("found %d subterms\n", i));
	*len = i;
	
	p(printf("dont parsing values\n"));
	return terms;
}

static struct _searchterm *
parse_value(GScanner *gs)
{
	int token;
	struct _searchterm *t = NULL;

	p(printf("parsing value\n"));

	token = g_scanner_get_next_token(gs);
	switch(token) {
	case G_TOKEN_LEFT_PAREN:
		p(printf("got brace, its a list!\n"));
		return parse_list(gs, TRUE);
	case G_TOKEN_STRING:
		p(printf("got string\n"));
		t = parse_new_term(SEARCH_STRING);
		t->value.string = g_strdup(g_scanner_cur_value(gs).v_string);
		break;
	case G_TOKEN_INT:
		t = parse_new_term(SEARCH_INT);
		t->value.number = g_scanner_cur_value(gs).v_int;
		p(printf("got int\n"));
		break;
	default:
		printf("Innvalid token trying to parse a list of values\n");
	}
	p(printf("done parsing value\n"));
	return t;
}

/* FIXME: this needs some robustification */
static struct _searchterm *
parse_list(GScanner *gs, int gotbrace)
{
	int token;
	struct _searchterm *t = NULL;

	p(printf("parsing list\n"));
	if (gotbrace)
		token = '(';
	else
		token = g_scanner_get_next_token(gs);
	if (token =='(') {
		token = g_scanner_get_next_token(gs);
		if (token == G_TOKEN_SYMBOL) {
			struct _searchterm_symbol *s;

			s = g_scanner_cur_value(gs).v_symbol;
			p(printf("got funciton: %s\n", s->name));
			t = parse_new_term(s->type);
			t->value.func.sym = s;
			p(printf("created new list %p\n", t));
			switch(s->argtype) {
			case 0:	/* it MUST be a list of lists */
				t->value.func.terms = parse_lists(gs, &t->value.func.termcount);
				break;
			case 1:
				t->value.func.terms = parse_values(gs, &t->value.func.termcount);
				break;
			default:
				printf("Error, internal error parsing symbols\n");
			}
		} else {
			printf("unknown sequence encountered, type = %d\n", token);
		}
		token = g_scanner_get_next_token(gs);
		if (token != ')') {
			printf("Error, expected ')' not found\n");
		}
	} else {
		printf("Error, list term without opening (\n");
	}

	p(printf("returning list %p\n", t));
	return t;
}

GList *
camel_mbox_folder_search_by_expression(CamelFolder *folder, char *expression, CamelException *ex)
{
	GScanner *gs;
	int i;
	struct _searchterm *t;
	struct _searchcontext *ctx;
	struct _searchresult *r;
	GList *matches = NULL;

	gs = g_scanner_new(&scanner_config);
	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++)
		g_scanner_scope_add_symbol(gs, 0, symbols[i].name, &symbols[i]);

	g_scanner_input_text(gs, expression, strlen(expression));
	t = parse_list(gs, 0);

	if (t) {
		ctx = g_malloc0(sizeof(*ctx));
		ctx->folder = folder;
		ctx->summary = camel_folder_get_summary(folder, ex);
		ctx->message_info = camel_folder_summary_get_message_info_list(ctx->summary);
#ifdef HAVE_IBEX
		ctx->index = ibex_open(CAMEL_MBOX_FOLDER(folder)->index_file_path, FALSE);
		if (!ctx->index) {
			perror("Cannot open index file, body searches will be ignored\n");
		}
#endif
		r = term_eval(ctx, t);

		/* now create a folder summary to return?? */
		if (r
		    && r->type == RESULT_ARRAY_PTR) {
			d(printf("got result ...\n"));
			for (i=0;i<r->value.ptrarray->len;i++) {
				d(printf("adding match: %s\n", (char *)g_ptr_array_index(r->value.ptrarray, i)));
				matches = g_list_prepend(matches, g_strdup(g_ptr_array_index(r->value.ptrarray, i)));
			}
			result_free(r);
		}

		if (ctx->index)
			ibex_close(ctx->index);

		gtk_object_unref((GtkObject *)ctx->summary);
		g_free(ctx);
		parse_term_free(t);
	} else {
		printf("Warning, Could not parse expression!\n %s\n", expression);
	}

	g_scanner_destroy(gs);

	return matches;
}
