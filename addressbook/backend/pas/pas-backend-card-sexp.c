/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include "pas-backend-card-sexp.h"

#include <string.h>
#include <e-util/e-sexp.h>
#include <ebook/e-card-simple.h>
#include <gal/widgets/e-unicode.h>

static GtkObjectClass *parent_class;

typedef struct _SearchContext SearchContext;

struct _PASBackendCardSExpPrivate {
	ESExp *search_sexp;
	SearchContext *search_context;
};

struct _SearchContext {
	ECardSimple *card;
};

static gboolean
compare_email (ECardSimple *card, const char *str,
	       char *(*compare)(const char*, const char*))
{
	int i;

	for (i = E_CARD_SIMPLE_EMAIL_ID_EMAIL; i < E_CARD_SIMPLE_EMAIL_ID_LAST; i ++) {
		const char *email = e_card_simple_get_email (card, i);

		if (email && compare(email, str))
			return TRUE;
	}

	return FALSE;
}

static gboolean
compare_phone (ECardSimple *card, const char *str,
	       char *(*compare)(const char*, const char*))
{
	int i;

	for (i = E_CARD_SIMPLE_PHONE_ID_ASSISTANT; i < E_CARD_SIMPLE_PHONE_ID_LAST; i ++) {
		const ECardPhone *phone = e_card_simple_get_phone (card, i);

		if (phone && compare(phone->number, str))
			return TRUE;
	}

	return FALSE;
}

static gboolean
compare_address (ECardSimple *card, const char *str,
		 char *(*compare)(const char*, const char*))
{
	g_warning("address searching not implemented\n");
	return FALSE;
}

static gboolean
compare_category (ECardSimple *card, const char *str,
		  char *(*compare)(const char*, const char*))
{
	EList *categories;
	EIterator *iterator;
	ECard *ecard;
	gboolean ret_val = FALSE;

	gtk_object_get (GTK_OBJECT (card),
			"card", &ecard,
			NULL);
	gtk_object_get (GTK_OBJECT (ecard),
			"category_list", &categories,
			NULL);

	for (iterator = e_list_get_iterator(categories); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
		const char *category = e_iterator_get (iterator);

		if (compare(category, str)) {
			ret_val = TRUE;
			break;
		}
	}

	gtk_object_unref (GTK_OBJECT (iterator));
	e_card_free_empty_lists (ecard);
	return ret_val;
}

static gboolean
compare_arbitrary (ECardSimple *card, const char *str,
		   char *(*compare)(const char*, const char*))
{
	EList *list;
	EIterator *iterator;
	ECard *ecard;
	gboolean ret_val = FALSE;

	gtk_object_get (GTK_OBJECT (card),
			"card", &ecard,
			NULL);
	gtk_object_get (GTK_OBJECT (ecard),
			"arbitrary", &list,
			NULL);

	for (iterator = e_list_get_iterator(list); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
		const ECardArbitrary *arbitrary = e_iterator_get (iterator);

		if (compare(arbitrary->key, str)) {
			ret_val = TRUE;
			break;
		}
	}

	gtk_object_unref (GTK_OBJECT (iterator));
	e_card_free_empty_lists (ecard);
	return ret_val;
}

static struct prop_info {
	ECardSimpleField field_id;
	const char *query_prop;
	const char *ecard_prop;
#define PROP_TYPE_NORMAL   0x01
#define PROP_TYPE_LIST     0x02
#define PROP_TYPE_LISTITEM 0x03
#define PROP_TYPE_ID 0x04
	int prop_type;
	gboolean (*list_compare)(ECardSimple *ecard, const char *str,
				 char *(*compare)(const char*, const char*));

} prop_info_table[] = {
#define NORMAL_PROP(f,q,e) {f, q, e, PROP_TYPE_NORMAL, NULL}
#define ID_PROP {0, "id", NULL, PROP_TYPE_ID, NULL}
#define LIST_PROP(q,e,c) {0, q, e, PROP_TYPE_LIST, c}

	/* query prop,  ecard prop,   type,              list compare function */
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_FILE_AS, "file_as", "file_as" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_FULL_NAME, "full_name",  "full_name" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_URL, "url", "url" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_MAILER, "mailer", "mailer"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ORG, "org", "org"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ORG_UNIT, "org_unit", "org_unit"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_OFFICE, "office", "office"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_TITLE, "title", "title"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ROLE, "role", "role"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_MANAGER, "manager", "manager"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_ASSISTANT, "assistant", "assistant"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_NICKNAME, "nickname", "nickname"),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_SPOUSE, "spouse", "spouse" ),
	NORMAL_PROP ( E_CARD_SIMPLE_FIELD_NOTE, "note", "note"),
	ID_PROP,
	LIST_PROP ( "email", "email", compare_email ),
	LIST_PROP ( "phone", "phone", compare_phone ),
	LIST_PROP ( "address", "address", compare_address ),
	LIST_PROP ( "category", "category", compare_category ),
	LIST_PROP ( "arbitrary", "arbitrary", compare_arbitrary )
};
static int num_prop_infos = sizeof(prop_info_table) / sizeof(prop_info_table[0]);

static ESExpResult *
entry_compare(SearchContext *ctx, struct _ESExp *f,
	      int argc, struct _ESExpResult **argv,
	      char *(*compare)(const char*, const char*))
{
	ESExpResult *r;
	int truth = FALSE;

	if (argc == 2
	    && argv[0]->type == ESEXP_RES_STRING
	    && argv[1]->type == ESEXP_RES_STRING) {
		char *propname;
		struct prop_info *info = NULL;
		int i;
		gboolean any_field;

		propname = argv[0]->value.string;

		any_field = !strcmp(propname, "x-evolution-any-field");
		for (i = 0; i < num_prop_infos; i ++) {
			if (any_field
			    || !strcmp (prop_info_table[i].query_prop, propname)) {
				info = &prop_info_table[i];
				
				if (info->prop_type == PROP_TYPE_NORMAL) {
					char *prop = NULL;
					/* searches where the query's property
					   maps directly to an ecard property */
					
					prop = e_card_simple_get (ctx->card, info->field_id);

					if (prop && compare(prop, argv[1]->value.string)) {
						truth = TRUE;
					}
					if ((!prop) && compare("", argv[1]->value.string)) {
						truth = TRUE;
					}
					g_free (prop);
				} else if (info->prop_type == PROP_TYPE_LIST) {
				/* the special searches that match any of the list elements */
					truth = info->list_compare (ctx->card, argv[1]->value.string, compare);
				} else if (info->prop_type == PROP_TYPE_ID) {
					const char *prop = NULL;
					/* searches where the query's property
					   maps directly to an ecard property */
					
					prop = e_card_get_id (ctx->card->card);

					if (prop && compare(prop, argv[1]->value.string)) {
						truth = TRUE;
					}
					if ((!prop) && compare("", argv[1]->value.string)) {
						truth = TRUE;
					}
				}

				/* if we're looking at all fields and find a match,
				   or if we're just looking at this one field,
				   break. */
				if ((any_field && truth)
				    || !any_field)
					break;
			}
		}
		
	}
	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = truth;

	return r;
}

static ESExpResult *
func_contains(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	SearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, (char *(*)(const char*, const char*)) e_utf8_strstrcase);
}

static char *
is_helper (const char *s1, const char *s2)
{
	if (!strcmp(s1, s2))
		return (char*)s1;
	else
		return NULL;
}

static ESExpResult *
func_is(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	SearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, is_helper);
}

static char *
endswith_helper (const char *s1, const char *s2)
{
	char *p;
	if ((p = strstr(s1, s2))
	    && (strlen(p) == strlen(s2)))
		return p;
	else
		return NULL;
}

static ESExpResult *
func_endswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	SearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, endswith_helper);
}

static char *
beginswith_helper (const char *s1, const char *s2)
{
	char *p;
	if ((p = strstr(s1, s2))
	    && (p == s1))
		return p;
	else
		return NULL;
}

static ESExpResult *
func_beginswith(struct _ESExp *f, int argc, struct _ESExpResult **argv, void *data)
{
	SearchContext *ctx = data;

	return entry_compare (ctx, f, argc, argv, beginswith_helper);
}

/* 'builtin' functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "contains", func_contains, 0 },
	{ "is", func_is, 0 },
	{ "beginswith", func_beginswith, 0 },
	{ "endswith", func_endswith, 0 },
};

gboolean
pas_backend_card_sexp_match_vcard (PASBackendCardSExp *sexp, const char *vcard)
{
	ECard *card;
	ESExpResult *r;
	gboolean retval;

	card = e_card_new ((char*)vcard);
	sexp->priv->search_context->card = e_card_simple_new (card);
	gtk_object_unref(GTK_OBJECT(card));

	/* if it's not a valid vcard why is it in our db? :) */
	if (!sexp->priv->search_context->card)
		return FALSE;

	r = e_sexp_eval(sexp->priv->search_sexp);

	retval = (r && r->type == ESEXP_RES_BOOL && r->value.bool);

	gtk_object_unref(GTK_OBJECT(sexp->priv->search_context->card));

	e_sexp_result_free(sexp->priv->search_sexp, r);

	return retval;
}



/**
 * pas_backend_card_sexp_new:
 */
PASBackendCardSExp *
pas_backend_card_sexp_new (const char *text)
{
	PASBackendCardSExp *sexp = gtk_type_new (pas_backend_card_sexp_get_type ());
	int esexp_error;
	int i;

	sexp->priv->search_sexp = e_sexp_new();

	for(i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++) {
		if (symbols[i].type == 1) {
			e_sexp_add_ifunction(sexp->priv->search_sexp, 0, symbols[i].name,
					     (ESExpIFunc *)symbols[i].func, sexp->priv->search_context);
		} else {
			e_sexp_add_function(sexp->priv->search_sexp, 0, symbols[i].name,
					    symbols[i].func, sexp->priv->search_context);
		}
	}

	e_sexp_input_text(sexp->priv->search_sexp, text, strlen(text));
	esexp_error = e_sexp_parse(sexp->priv->search_sexp);

	if (esexp_error == -1) {
		gtk_object_unref (GTK_OBJECT (sexp));
		sexp = NULL;
	}

	return sexp;
}

static void
pas_backend_card_sexp_destroy (GtkObject *object)
{
	PASBackendCardSExp *sexp = PAS_BACKEND_CARD_SEXP (object);
	e_sexp_unref(sexp->priv->search_sexp);

	g_free (sexp->priv->search_context);
	g_free (sexp->priv);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);	
}

static void
pas_backend_card_sexp_class_init (PASBackendCardSExpClass *klass)
{
	GtkObjectClass  *object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	/* Set the virtual methods. */

	object_class->destroy = pas_backend_card_sexp_destroy;
}

static void
pas_backend_card_sexp_init (PASBackendCardSExp *sexp)
{
	PASBackendCardSExpPrivate *priv;

	priv             = g_new0 (PASBackendCardSExpPrivate, 1);

	sexp->priv = priv;
	priv->search_context = g_new (SearchContext, 1);
}

/**
 * pas_backend_card_sexp_get_type:
 */
GtkType
pas_backend_card_sexp_get_type (void)
{
	static GtkType type = 0;

	if (! type) {
		GtkTypeInfo info = {
			"PASBackendCardSExp",
			sizeof (PASBackendCardSExp),
			sizeof (PASBackendCardSExpClass),
			(GtkClassInitFunc)  pas_backend_card_sexp_class_init,
			(GtkObjectInitFunc) pas_backend_card_sexp_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_object_get_type (), &info);
	}

	return type;
}
