/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include "e-book-query.h"
#include <e-util/e-sexp.h>

#include <stdarg.h>
#include <string.h>

typedef enum {
	E_BOOK_QUERY_TYPE_AND,
	E_BOOK_QUERY_TYPE_OR,
	E_BOOK_QUERY_TYPE_NOT,
	E_BOOK_QUERY_TYPE_FIELD_EXISTS,
	E_BOOK_QUERY_TYPE_FIELD_TEST,
	E_BOOK_QUERY_TYPE_ANY_FIELD_CONTAINS
} EBookQueryType;

struct EBookQuery {
	EBookQueryType type;
	int ref_count;

	union {
		struct {
			guint          nqs;
			EBookQuery   **qs;
		} andor;

		struct {
			EBookQuery    *q;
		} not;

		struct {
			EBookQueryTest test;
			EContactField  field;
			char          *value;
		} field_test;

		struct {
			EContactField  field;
		} exist;

		struct {
			char          *value;
		} any_field_contains;
	} query;
};

static EBookQuery *
conjoin (EBookQueryType type, int nqs, EBookQuery **qs, gboolean unref)
{
	EBookQuery *ret = g_new0 (EBookQuery, 1);
	int i;

	ret->type = type;
	ret->query.andor.nqs = nqs;
	ret->query.andor.qs = g_new (EBookQuery *, nqs);
	for (i = 0; i < nqs; i++) {
		ret->query.andor.qs[i] = qs[i];
		if (!unref)
			e_book_query_ref (qs[i]);
	}

	return ret;
}

EBookQuery *
e_book_query_and (int nqs, EBookQuery **qs, gboolean unref)
{
	return conjoin (E_BOOK_QUERY_TYPE_AND, nqs, qs, unref);
}

EBookQuery *
e_book_query_or (int nqs, EBookQuery **qs, gboolean unref)
{
	return conjoin (E_BOOK_QUERY_TYPE_OR, nqs, qs, unref);
}

static EBookQuery *
conjoinv (EBookQueryType type, EBookQuery *q, va_list ap)
{
	EBookQuery *ret = g_new0 (EBookQuery, 1);
	GPtrArray *qs;

	qs = g_ptr_array_new ();
	while (q) {
		g_ptr_array_add (qs, q);
		q = va_arg (ap, EBookQuery *);
	}
	va_end (ap);

	ret->type = type;
	ret->query.andor.nqs = qs->len;
	ret->query.andor.qs = (EBookQuery **)qs->pdata;
	g_ptr_array_free (qs, FALSE);

	return ret;
}

EBookQuery *
e_book_query_andv (EBookQuery *q, ...)
{
	va_list ap;

	va_start (ap, q);
	return conjoinv (E_BOOK_QUERY_TYPE_AND, q, ap);
}

EBookQuery *
e_book_query_orv (EBookQuery *q, ...)
{
	va_list ap;

	va_start (ap, q);
	return conjoinv (E_BOOK_QUERY_TYPE_OR, q, ap);
}

EBookQuery *
e_book_query_not (EBookQuery *q, gboolean unref)
{
	EBookQuery *ret = g_new0 (EBookQuery, 1);

	ret->type = E_BOOK_QUERY_TYPE_NOT;
	ret->query.not.q = q;
	if (!unref)
		e_book_query_ref (q);

	return ret;
}

EBookQuery *
e_book_query_field_test (EContactField field,
			 EBookQueryTest test,
			 const char *value)
{
	EBookQuery *ret = g_new0 (EBookQuery, 1);

	ret->type = E_BOOK_QUERY_TYPE_FIELD_TEST;
	ret->query.field_test.field = field;
	ret->query.field_test.test = test;
	ret->query.field_test.value = g_strdup (value);

	return ret;
}

EBookQuery *
e_book_query_field_exists (EContactField field)
{
	EBookQuery *ret = g_new0 (EBookQuery, 1);

	ret->type = E_BOOK_QUERY_TYPE_FIELD_EXISTS;
	ret->query.exist.field = field;

	return ret;
}

EBookQuery *
e_book_query_any_field_contains (const char *value)
{
	EBookQuery *ret = g_new0 (EBookQuery, 1);

	ret->type = E_BOOK_QUERY_TYPE_ANY_FIELD_CONTAINS;
	ret->query.any_field_contains.value = g_strdup (value);

	return ret;
}

void
e_book_query_unref (EBookQuery *q)
{
	int i;

	if (q->ref_count--)
		return;

	switch (q->type) {
	case E_BOOK_QUERY_TYPE_AND:
	case E_BOOK_QUERY_TYPE_OR:
		for (i = 0; i < q->query.andor.nqs; i++)
			e_book_query_unref (q->query.andor.qs[i]);
		g_free (q->query.andor.qs);
		break;

	case E_BOOK_QUERY_TYPE_NOT:
		e_book_query_unref (q->query.not.q);
		break;

	case E_BOOK_QUERY_TYPE_FIELD_TEST:
		g_free (q->query.field_test.value);
		break;

	case E_BOOK_QUERY_TYPE_ANY_FIELD_CONTAINS:
		g_free (q->query.any_field_contains.value);
		break;

	default:
		break;
	}

	g_free (q);
}

void
e_book_query_ref (EBookQuery *q)
{
	q->ref_count++;
}

static ESExpResult *
func_and(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	EBookQuery **qs;

	if (argc > 0) {
		int i;

		qs = g_new0(EBookQuery*, argc);
		
		for (i = 0; i < argc; i ++) {
			GList *list_head = *list;
			if (!list_head)
				break;
			qs[i] = list_head->data;
			*list = g_list_remove_link(*list, list_head);
			g_list_free_1(list_head);
		}

		*list = g_list_prepend(*list, 
				       e_book_query_and (argc, qs, TRUE));

		g_free (qs);
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_or(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;
	EBookQuery **qs;

	if (argc > 0) {
		int i;

		qs = g_new0(EBookQuery*, argc);
		
		for (i = 0; i < argc; i ++) {
			GList *list_head = *list;
			if (!list_head)
				break;
			qs[i] = list_head->data;
			*list = g_list_remove_link(*list, list_head);
			g_list_free_1(list_head);
		}

		*list = g_list_prepend(*list, 
				       e_book_query_or (argc, qs, TRUE));

		g_free (qs);
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_not(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	/* just replace the head of the list with the NOT of it. */
	if (argc > 0) {
		EBookQuery *term = (*list)->data;
		(*list)->data = e_book_query_not (term, TRUE);
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;

		if (!strcmp (propname, "x-evolution-any-field")) {
			*list = g_list_prepend (*list, e_book_query_any_field_contains (str));
		}
		else {
			EContactField field = e_contact_field_id (propname);

			if (field)
				*list = g_list_prepend (*list, e_book_query_field_test (field,
											E_BOOK_QUERY_CONTAINS,
											str));
		}
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_is(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		EContactField field = e_contact_field_id (propname);

		if (field)
			*list = g_list_prepend (*list, e_book_query_field_test (field,
										E_BOOK_QUERY_IS,
										str));
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_beginswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		EContactField field = e_contact_field_id (propname);

		if (field)
			*list = g_list_prepend (*list, e_book_query_field_test (field,
										E_BOOK_QUERY_BEGINS_WITH,
										str));
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

static ESExpResult *
func_endswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	GList **list = data;
	ESExpResult *r;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname = argv[0]->value.string;
		char *str = argv[1]->value.string;
		EContactField field = e_contact_field_id (propname);

		if (field)
			*list = g_list_prepend (*list, e_book_query_field_test (field,
										E_BOOK_QUERY_ENDS_WITH,
										str));
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = FALSE;

	return r;
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "and", func_and, 0 },
	{ "or", func_or, 0 },
	{ "not", func_not, 0 },
	{ "contains", func_contains, 0 },
	{ "is", func_is, 0 },
	{ "beginswith", func_beginswith, 0 },
	{ "endswith", func_endswith, 0 },
};

EBookQuery*
e_book_query_from_string  (const char *query_string)
{
	ESExp *sexp;
	ESExpResult *r;
	EBookQuery *retval;
	GList *list = NULL;
	int i;

	sexp = e_sexp_new();

	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(sexp, 0, symbols[i].name,
					     (ESExpIFunc *)symbols[i].func, &list);
		} else {
			e_sexp_add_function(sexp, 0, symbols[i].name,
					    symbols[i].func, &list);
		}
	}

	e_sexp_input_text(sexp, query_string, strlen(query_string));
	e_sexp_parse(sexp);

	r = e_sexp_eval(sexp);

	e_sexp_result_free(sexp, r);
	e_sexp_unref (sexp);

	if (list) {
		if (list->next) {
			g_warning ("conversion to EBookQuery");
			retval = NULL;
			g_list_foreach (list, (GFunc)e_book_query_unref, NULL);
		}
		else {
			retval = list->data;
		}
	}
	else {
		g_warning ("conversion to EBookQuery failed");
		retval = NULL;
	}

	g_list_free (list);
	return retval;
}

char*
e_book_query_to_string    (EBookQuery *q)
{
	GString *str = g_string_new ("(");
	int i;
	char *s = NULL;

	switch (q->type) {
	case E_BOOK_QUERY_TYPE_AND:
		g_string_append (str, "and ");
		for (i = 0; i < q->query.andor.nqs; i ++) {
			s = e_book_query_to_string (q->query.andor.qs[i]);
			g_string_append (str, s);
			g_free (s);
			g_string_append_c (str, ' ');
		}
		break;
	case E_BOOK_QUERY_TYPE_OR:
		g_string_append (str, "or ");
		for (i = 0; i < q->query.andor.nqs; i ++) {
			s = e_book_query_to_string (q->query.andor.qs[i]);
			g_string_append (str, s);
			g_free (s);
			g_string_append_c (str, ' ');
		}
		break;
	case E_BOOK_QUERY_TYPE_NOT:
		s = e_book_query_to_string (q->query.not.q);
		g_string_append_printf (str, "not %s", s);
		g_free (s);
		break;
	case E_BOOK_QUERY_TYPE_FIELD_EXISTS:
		g_string_append_printf (str, "exists \"%s\"", e_contact_field_name (q->query.exist.field));
		break;
	case E_BOOK_QUERY_TYPE_FIELD_TEST:
		switch (q->query.field_test.test) {
		case E_BOOK_QUERY_IS: s = "is"; break;
		case E_BOOK_QUERY_CONTAINS: s = "contains"; break;
		case E_BOOK_QUERY_BEGINS_WITH: s = "beginswith"; break;
		case E_BOOK_QUERY_ENDS_WITH: s = "endswith"; break;
		default:
			g_assert_not_reached();
			break;
		}

		/* XXX need to escape q->query.field_test.value */
		g_string_append_printf (str, "%s \"%s\" \"%s\"",
					s,
					e_contact_field_name (q->query.field_test.field),
					q->query.field_test.value);
		break;
	case E_BOOK_QUERY_TYPE_ANY_FIELD_CONTAINS:
		g_string_append_printf (str, "contains \"x-evolution-any-field\" \"%s\"", q->query.any_field_contains.value);
		break;
	}
	 

	g_string_append (str, ")");

	return g_string_free (str, FALSE);
}
