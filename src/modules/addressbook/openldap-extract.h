/* This is extracted from the OpenLDAP sources.
 *
 * Stuff that isn't used in e-book-backend-ldap.c was dropped, like
 * the LDAPSchemaExtensionItem stuff.
 *
 * This file basically has three parts:
 *
 * - some general macros from OpenLDAP that work as such on all
 *   implementations.
 *
 * - ldap_str2objectclass()
 *
 * - ldap_url_parse()
 */

/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2005 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file COPYING.OPENLDAP in
 * the top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include <string.h>
#include <assert.h>

/* from various header files */

#define LDAP_CONST const

#define LDAP_PORT		389		/* ldap:///		default LDAP port */
#define LDAPS_PORT		636		/* ldaps:///	default LDAP over TLS port */

#define LDAP_ROOT_DSE				""

#define LDAP_SPACE(c)		((c) == ' ' || (c) == '\t' || (c) == '\n')
#define LDAP_DIGIT(c)		((c) >= '0' && (c) <= '9')

#define LDAP_EXOP_START_TLS		"1.3.6.1.4.1.1466.20037"	/* RFC 2830 */

#define LDAP_MALLOC(n) malloc((n))
#define LDAP_CALLOC(n,s) calloc((n),(s))
#define LDAP_REALLOC(p,s) realloc((p),(s))
#define LDAP_FREE(p) free((p))
#define LDAP_VFREE(p) vfree((gpointer *)(p))
#define LDAP_STRDUP(s) strdup((s))

#define LDAP_RANGE(n,x,y)	(((x) <= (n)) && ((n) <= (y)))
#define LDAP_NAME_ERROR(n)	LDAP_RANGE((n),0x20,0x24) /* 32-34,36 */

#define ldap_msgtype(lm) (lm)->lm_msgtype
#define ldap_msgid(lm) (lm)->lm_msgid
#ifndef LDAP_TYPE_OR_VALUE_EXISTS
#define LDAP_TYPE_OR_VALUE_EXISTS 0x14
#endif
#ifndef LDAP_SCOPE_DEFAULT
#define LDAP_SCOPE_DEFAULT -1
#endif
#ifndef LDAP_OPT_SUCCESS
#define LDAP_OPT_SUCCESS 0x00
#endif
#ifndef LDAP_INSUFFICIENT_ACCESS
#define LDAP_INSUFFICIENT_ACCESS 0x32
#endif

#define LDAP_SCHERR_OUTOFMEM		1
#define LDAP_SCHERR_UNEXPTOKEN		2
#define LDAP_SCHERR_NOLEFTPAREN		3
#define LDAP_SCHERR_NORIGHTPAREN	4
#define LDAP_SCHERR_NODIGIT		5
#define LDAP_SCHERR_BADNAME		6
#define LDAP_SCHERR_BADDESC		7
#define LDAP_SCHERR_BADSUP		8
#define LDAP_SCHERR_DUPOPT		9
#define LDAP_SCHERR_EMPTY		10
#define LDAP_SCHERR_MISSING		11
#define LDAP_SCHERR_OUT_OF_ORDER	12

#define LDAP_SCHEMA_YES				1

#define LDAP_SCHEMA_ABSTRACT			0
#define LDAP_SCHEMA_STRUCTURAL			1
#define LDAP_SCHEMA_AUXILIARY			2

#define LDAP_SCHEMA_ALLOW_NONE		0x00U /* Strict parsing               */
#define LDAP_SCHEMA_ALLOW_NO_OID	0x01U /* Allow missing oid            */
#define LDAP_SCHEMA_ALLOW_QUOTED	0x02U /* Allow bogus extra quotes     */
#define LDAP_SCHEMA_ALLOW_DESCR		0x04U /* Allow descr instead of OID   */
#define LDAP_SCHEMA_ALLOW_DESCR_PREFIX	0x08U /* Allow descr as OID prefix    */
#define LDAP_SCHEMA_ALLOW_OID_MACRO	0x10U /* Allow OID macros in slapd    */
#define LDAP_SCHEMA_ALLOW_OUT_OF_ORDER_FIELDS 0x20U /* Allow fields in most any order */
#define LDAP_SCHEMA_ALLOW_ALL		0x3fU /* Be very liberal in parsing   */
#define	LDAP_SCHEMA_SKIP			0x80U /* Don't malloc any result      */

typedef struct ldap_objectclass {
	gchar *oc_oid;		/* REQUIRED */
	gchar **oc_names;	/* OPTIONAL */
	gchar *oc_desc;		/* OPTIONAL */
	gint  oc_obsolete;	/* 0=no, 1=yes */
	gchar **oc_sup_oids;	/* OPTIONAL */
	gint  oc_kind;		/* 0=ABSTRACT, 1=STRUCTURAL, 2=AUXILIARY */
	gchar **oc_at_oids_must;	/* OPTIONAL */
	gchar **oc_at_oids_may;	/* OPTIONAL */
} LDAPObjectClass;

static void
vfree (gpointer *vec)
{
  gint i;

  for (i = 0; vec[i] != NULL; i++)
    free (vec[i]);
}

/* from schema.c */

/*
 * Now come the parsers.  There is one parser for each entity type:
 * objectclasses, attributetypes, etc.
 *
 * Each of them is written as a recursive-descent parser, except that
 * none of them is really recursive.  But the idea is kept: there
 * is one routine per non-terminal that eithers gobbles lexical tokens
 * or calls lower-level routines, etc.
 *
 * The scanner is implemented in the routine get_token.  Actually,
 * get_token is more than a scanner and will return tokens that are
 * in fact non-terminals in the grammar.  So you can see the whole
 * approach as the combination of a low-level bottom-up recognizer
 * combined with a scanner and a number of top-down parsers.  Or just
 * consider that the real grammars recognized by the parsers are not
 * those of the standards.  As a matter of fact, our parsers are more
 * liberal than the spec when there is no ambiguity.
 *
 * The difference is pretty academic (modulo bugs or incorrect
 * interpretation of the specs).
 */

#define TK_NOENDQUOTE	-2
#define TK_OUTOFMEM	-1
#define TK_EOS		0
#define TK_UNEXPCHAR	1
#define TK_BAREWORD	2
#define TK_QDSTRING	3
#define TK_LEFTPAREN	4
#define TK_RIGHTPAREN	5
#define TK_DOLLAR	6
#define TK_QDESCR	TK_QDSTRING

struct token {
	gint type;
	gchar *sval;
};

static gint
get_token (const gchar **sp,
           gchar **token_val)
{
	gint kind;
	const gchar *p;
	const gchar *q;
	gchar *res;

	*token_val = NULL;
	switch (**sp) {
	case '\0':
		kind = TK_EOS;
		(*sp)++;
		break;
	case '(':
		kind = TK_LEFTPAREN;
		(*sp)++;
		break;
	case ')':
		kind = TK_RIGHTPAREN;
		(*sp)++;
		break;
	case '$':
		kind = TK_DOLLAR;
		(*sp)++;
		break;
	case '\'':
		kind = TK_QDSTRING;
		(*sp)++;
		p = *sp;
		while (**sp != '\'' && **sp != '\0')
			(*sp)++;
		if (**sp == '\'') {
			q = *sp;
			res = LDAP_MALLOC (q - p + 1);
			if (!res) {
				kind = TK_OUTOFMEM;
			} else {
				strncpy (res,p,q - p);
				res[q - p] = '\0';
				*token_val = res;
			}
			(*sp)++;
		} else {
			kind = TK_NOENDQUOTE;
		}
		break;
	default:
		kind = TK_BAREWORD;
		p = *sp;
		while (!LDAP_SPACE (**sp) &&
			**sp != '(' &&
			**sp != ')' &&
			**sp != '$' &&
			**sp != '\'' &&
			**sp != '\0')
			(*sp)++;
		q = *sp;
		res = LDAP_MALLOC (q - p + 1);
		if (!res) {
			kind = TK_OUTOFMEM;
		} else {
			strncpy (res,p,q - p);
			res[q - p] = '\0';
			*token_val = res;
		}
		break;
/*		kind = TK_UNEXPCHAR; */
/*		break; */
	}

	return kind;
}

/* Gobble optional whitespace */
static void
parse_whsp (const gchar **sp)
{
	while (LDAP_SPACE (**sp))
		(*sp)++;
}

/* Parse a sequence of dot-separated decimal strings */
static gchar *
ldap_int_parse_numericoid (const gchar **sp, gint *code, const gint flags)
{
	gchar *res = NULL;
	const gchar *start = *sp;
	gint len;
	gint quoted = 0;

	/* Netscape puts the SYNTAX value in quotes (incorrectly) */
	if (flags & LDAP_SCHEMA_ALLOW_QUOTED && **sp == '\'') {
		quoted = 1;
		(*sp)++;
		start++;
	}
	/* Each iteration of this loop gets one decimal string */
	while (**sp) {
		if (!LDAP_DIGIT (**sp)) {
			/*
			 * Initial gchar is not a digit or gchar after dot is
			 * not a digit
			 */
			*code = LDAP_SCHERR_NODIGIT;
			return NULL;
		}
		(*sp)++;
		while (LDAP_DIGIT (**sp))
			(*sp)++;
		if (**sp != '.')
			break;
		/* Otherwise, gobble the dot and loop again */
		(*sp)++;
	}
	/* Now *sp points at the gchar past the numericoid. Perfect. */
	len = *sp - start;
	if (flags & LDAP_SCHEMA_ALLOW_QUOTED && quoted) {
		if (**sp == '\'') {
			(*sp)++;
		} else {
			*code = LDAP_SCHERR_UNEXPTOKEN;
			return NULL;
		}
	}
	if (flags & LDAP_SCHEMA_SKIP) {
		res = (gchar *) start;
	} else {
		res = LDAP_MALLOC (len + 1);
		if (!res) {
			*code = LDAP_SCHERR_OUTOFMEM;
			return (NULL);
		}
		strncpy (res,start,len);
		res[len] = '\0';
	}
	return (res);
}

/* Parse a qdescr or a list of them enclosed in () */
static gchar **
parse_qdescrs (const gchar **sp, gint *code)
{
	gchar ** res;
	gchar ** res1;
	gint kind;
	gchar *sval;
	gint size;
	gint pos;

	parse_whsp (sp);
	kind = get_token (sp,&sval);
	if (kind == TK_LEFTPAREN) {
		/* Let's presume there will be at least 2 entries */
		size = 3;
		res = LDAP_CALLOC (3,sizeof (gchar *));
		if (!res) {
			*code = LDAP_SCHERR_OUTOFMEM;
			return NULL;
		}
		pos = 0;
		while (1) {
			parse_whsp (sp);
			kind = get_token (sp,&sval);
			if (kind == TK_RIGHTPAREN)
				break;
			if (kind == TK_QDESCR) {
				if (pos == size - 2) {
					size++;
					res1 = LDAP_REALLOC (res,size *sizeof (gchar *));
					if (!res1) {
						LDAP_VFREE (res);
						LDAP_FREE (sval);
						*code = LDAP_SCHERR_OUTOFMEM;
						return (NULL);
					}
					res = res1;
				}
				res[pos++] = sval;
				res[pos] = NULL;
				parse_whsp (sp);
			} else {
				LDAP_VFREE (res);
				LDAP_FREE (sval);
				*code = LDAP_SCHERR_UNEXPTOKEN;
				return (NULL);
			}
		}
		parse_whsp (sp);
		return (res);
	} else if (kind == TK_QDESCR) {
		res = LDAP_CALLOC (2,sizeof (gchar *));
		if (!res) {
			*code = LDAP_SCHERR_OUTOFMEM;
			return NULL;
		}
		res[0] = sval;
		res[1] = NULL;
		parse_whsp (sp);
		return res;
	} else {
		LDAP_FREE (sval);
		*code = LDAP_SCHERR_BADNAME;
		return NULL;
	}
}

/* Parse a woid or a $-separated list of them enclosed in () */
static gchar **
parse_oids (const gchar **sp, gint *code, const gint allow_quoted)
{
	gchar ** res;
	gchar ** res1;
	gint kind;
	gchar *sval;
	gint size;
	gint pos;

	/*
	 * Strictly speaking, doing this here accepts whsp before the
	 * ( at the beginning of an oidlist, but this is harmless.  Also,
	 * we are very liberal in what we accept as an OID.  Maybe
	 * refine later.
	 */
	parse_whsp (sp);
	kind = get_token (sp,&sval);
	if (kind == TK_LEFTPAREN) {
		/* Let's presume there will be at least 2 entries */
		size = 3;
		res = LDAP_CALLOC (3,sizeof (gchar *));
		if (!res) {
			*code = LDAP_SCHERR_OUTOFMEM;
			return NULL;
		}
		pos = 0;
		parse_whsp (sp);
		kind = get_token (sp,&sval);
		if (kind == TK_BAREWORD ||
		     (allow_quoted && kind == TK_QDSTRING)) {
			res[pos++] = sval;
			res[pos] = NULL;
		} else {
			*code = LDAP_SCHERR_UNEXPTOKEN;
			LDAP_FREE (sval);
			LDAP_VFREE (res);
			return NULL;
		}
		parse_whsp (sp);
		while (1) {
			kind = get_token (sp,&sval);
			if (kind == TK_RIGHTPAREN)
				break;
			if (kind == TK_DOLLAR) {
				parse_whsp (sp);
				kind = get_token (sp,&sval);
				if (kind == TK_BAREWORD ||
				     (allow_quoted &&
				       kind == TK_QDSTRING)) {
					if (pos == size - 2) {
						size++;
						res1 = LDAP_REALLOC (res,size *sizeof (gchar *));
						if (!res1) {
							LDAP_FREE (sval);
							LDAP_VFREE (res);
							*code = LDAP_SCHERR_OUTOFMEM;
							return (NULL);
						}
						res = res1;
					}
					res[pos++] = sval;
					res[pos] = NULL;
				} else {
					*code = LDAP_SCHERR_UNEXPTOKEN;
					LDAP_FREE (sval);
					LDAP_VFREE (res);
					return NULL;
				}
				parse_whsp (sp);
			} else {
				*code = LDAP_SCHERR_UNEXPTOKEN;
				LDAP_FREE (sval);
				LDAP_VFREE (res);
				return NULL;
			}
		}
		parse_whsp (sp);
		return (res);
	} else if (kind == TK_BAREWORD ||
		    (allow_quoted && kind == TK_QDSTRING)) {
		res = LDAP_CALLOC (2,sizeof (gchar *));
		if (!res) {
			LDAP_FREE (sval);
			*code = LDAP_SCHERR_OUTOFMEM;
			return NULL;
		}
		res[0] = sval;
		res[1] = NULL;
		parse_whsp (sp);
		return res;
	} else {
		LDAP_FREE (sval);
		*code = LDAP_SCHERR_BADNAME;
		return NULL;
	}
}

static void
ldap_objectclass_free (LDAPObjectClass *oc)
{
	LDAP_FREE (oc->oc_oid);
	if (oc->oc_names) LDAP_VFREE (oc->oc_names);
	if (oc->oc_desc) LDAP_FREE (oc->oc_desc);
	if (oc->oc_sup_oids) LDAP_VFREE (oc->oc_sup_oids);
	if (oc->oc_at_oids_must) LDAP_VFREE (oc->oc_at_oids_must);
	if (oc->oc_at_oids_may) LDAP_VFREE (oc->oc_at_oids_may);
	LDAP_FREE (oc);
}

static LDAPObjectClass *
ldap_str2objectclass (LDAP_CONST gchar *s,
                      gint *code,
                      LDAP_CONST gchar **errp,
                      LDAP_CONST unsigned flags)
{
	gint kind;
	const gchar *ss = s;
	gchar *sval;
	gint seen_name = 0;
	gint seen_desc = 0;
	gint seen_obsolete = 0;
	gint seen_sup = 0;
	gint seen_kind = 0;
	gint seen_must = 0;
	gint seen_may = 0;
	LDAPObjectClass *oc;
	gchar ** ext_vals;
	const gchar *savepos;

	if (!s) {
		*code = LDAP_SCHERR_EMPTY;
		*errp = "";
		return NULL;
	}

	*errp = s;
	oc = LDAP_CALLOC (1,sizeof (LDAPObjectClass));

	if (!oc) {
		*code = LDAP_SCHERR_OUTOFMEM;
		return NULL;
	}
	oc->oc_kind = LDAP_SCHEMA_STRUCTURAL;

	kind = get_token (&ss,&sval);
	if (kind != TK_LEFTPAREN) {
		*code = LDAP_SCHERR_NOLEFTPAREN;
		LDAP_FREE (sval);
		ldap_objectclass_free (oc);
		return NULL;
	}

	/*
	 * Definitions MUST begin with an OID in the numericoid format.
	 * However, this routine is used by clients to parse the response
	 * from servers and very well known servers will provide an OID
	 * in the wrong format or even no OID at all.  We do our best to
	 * extract info from those servers.
	 */
	parse_whsp (&ss);
	savepos = ss;
	oc->oc_oid = ldap_int_parse_numericoid (&ss,code,0);
	if (!oc->oc_oid) {
		if ((flags & LDAP_SCHEMA_ALLOW_ALL) && (ss == savepos)) {
			/* Backtracking */
			ss = savepos;
			kind = get_token (&ss,&sval);
			if (kind == TK_BAREWORD) {
				if (!strcasecmp (sval, "NAME") ||
				    !strcasecmp (sval, "DESC") ||
				    !strcasecmp (sval, "OBSOLETE") ||
				    !strcasecmp (sval, "SUP") ||
				    !strcasecmp (sval, "ABSTRACT") ||
				    !strcasecmp (sval, "STRUCTURAL") ||
				    !strcasecmp (sval, "AUXILIARY") ||
				    !strcasecmp (sval, "MUST") ||
				    !strcasecmp (sval, "MAY") ||
				    !strncasecmp (sval, "X-", 2)) {
					/* Missing OID, backtrack */
					ss = savepos;
				} else if (flags &
					LDAP_SCHEMA_ALLOW_OID_MACRO) {
					/* Non-numerical OID, ignore */
					gint len = ss - savepos;
					oc->oc_oid = LDAP_MALLOC (len + 1);
					strncpy (oc->oc_oid, savepos, len);
					oc->oc_oid[len] = 0;
				}
			}
			LDAP_FREE (sval);
		} else {
			*errp = ss;
			ldap_objectclass_free (oc);
			return NULL;
		}
	}
	parse_whsp (&ss);

	/*
	 * Beyond this point we will be liberal an accept the items
	 * in any order.
	 */
	while (1) {
		kind = get_token (&ss,&sval);
		switch (kind) {
		case TK_EOS:
			*code = LDAP_SCHERR_NORIGHTPAREN;
			*errp = ss;
			ldap_objectclass_free (oc);
			return NULL;
		case TK_RIGHTPAREN:
			return oc;
		case TK_BAREWORD:
			if (!strcasecmp (sval,"NAME")) {
				LDAP_FREE (sval);
				if (seen_name) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_name = 1;
				oc->oc_names = parse_qdescrs (&ss,code);
				if (!oc->oc_names) {
					if (*code != LDAP_SCHERR_OUTOFMEM)
						*code = LDAP_SCHERR_BADNAME;
					*errp = ss;
					ldap_objectclass_free (oc);
					return NULL;
				}
			} else if (!strcasecmp (sval,"DESC")) {
				LDAP_FREE (sval);
				if (seen_desc) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_desc = 1;
				parse_whsp (&ss);
				kind = get_token (&ss,&sval);
				if (kind != TK_QDSTRING) {
					*code = LDAP_SCHERR_UNEXPTOKEN;
					*errp = ss;
					LDAP_FREE (sval);
					ldap_objectclass_free (oc);
					return NULL;
				}
				oc->oc_desc = sval;
				parse_whsp (&ss);
			} else if (!strcasecmp (sval,"OBSOLETE")) {
				LDAP_FREE (sval);
				if (seen_obsolete) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_obsolete = 1;
				oc->oc_obsolete = LDAP_SCHEMA_YES;
				parse_whsp (&ss);
			} else if (!strcasecmp (sval,"SUP")) {
				LDAP_FREE (sval);
				if (seen_sup) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_sup = 1;
				oc->oc_sup_oids = parse_oids (&ss,
							     code,
							     flags);
				if (!oc->oc_sup_oids) {
					*errp = ss;
					ldap_objectclass_free (oc);
					return NULL;
				}
			} else if (!strcasecmp (sval,"ABSTRACT")) {
				LDAP_FREE (sval);
				if (seen_kind) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_kind = 1;
				oc->oc_kind = LDAP_SCHEMA_ABSTRACT;
				parse_whsp (&ss);
			} else if (!strcasecmp (sval,"STRUCTURAL")) {
				LDAP_FREE (sval);
				if (seen_kind) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_kind = 1;
				oc->oc_kind = LDAP_SCHEMA_STRUCTURAL;
				parse_whsp (&ss);
			} else if (!strcasecmp (sval,"AUXILIARY")) {
				LDAP_FREE (sval);
				if (seen_kind) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_kind = 1;
				oc->oc_kind = LDAP_SCHEMA_AUXILIARY;
				parse_whsp (&ss);
			} else if (!strcasecmp (sval,"MUST")) {
				LDAP_FREE (sval);
				if (seen_must) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_must = 1;
				oc->oc_at_oids_must = parse_oids (&ss,code,0);
				if (!oc->oc_at_oids_must) {
					*errp = ss;
					ldap_objectclass_free (oc);
					return NULL;
				}
				parse_whsp (&ss);
			} else if (!strcasecmp (sval,"MAY")) {
				LDAP_FREE (sval);
				if (seen_may) {
					*code = LDAP_SCHERR_DUPOPT;
					*errp = ss;
					ldap_objectclass_free (oc);
					return (NULL);
				}
				seen_may = 1;
				oc->oc_at_oids_may = parse_oids (&ss,code,0);
				if (!oc->oc_at_oids_may) {
					*errp = ss;
					ldap_objectclass_free (oc);
					return NULL;
				}
				parse_whsp (&ss);
			} else if (sval[0] == 'X' && sval[1] == '-') {
				/* Should be parse_qdstrings */
				ext_vals = parse_qdescrs (&ss, code);
				if (!ext_vals) {
					*errp = ss;
					ldap_objectclass_free (oc);
					return NULL;
				}
#if 0
				if (add_extension (&oc->oc_extensions,
						    sval, ext_vals)) {
					*code = LDAP_SCHERR_OUTOFMEM;
					*errp = ss;
					LDAP_FREE (sval);
					ldap_objectclass_free (oc);
					return NULL;
				}
#endif
			} else {
				*code = LDAP_SCHERR_UNEXPTOKEN;
				*errp = ss;
				LDAP_FREE (sval);
				ldap_objectclass_free (oc);
				return NULL;
			}
			break;
		default:
			*code = LDAP_SCHERR_UNEXPTOKEN;
			*errp = ss;
			LDAP_FREE (sval);
			ldap_objectclass_free (oc);
			return NULL;
		}
	}
}

/* from utf-8.c */

#define LDAP_UTF8_NEXT(p) g_utf8_next_char((p))
#define LDAP_UTF8_INCR(p) ((p)=LDAP_UTF8_NEXT((p)))
#define ldap_x_utf8_to_ucs4(str) g_utf8_get_char(str)

static gchar *
ldap_utf8_strchr (const gchar *str,
                  const gchar *chr)
{
	for (; *str != '\0'; LDAP_UTF8_INCR (str)) {
		if (ldap_x_utf8_to_ucs4 (str) == ldap_x_utf8_to_ucs4 (chr)) {
			return (gchar *) str;
		}
	}

	return NULL;
}

static gsize
ldap_utf8_strcspn (const gchar *str,
                   const gchar *set)
{
	const gchar *cstr;
	const gchar *cset;

	for (cstr = str; *cstr != '\0'; LDAP_UTF8_INCR (cstr)) {
		for (cset = set; *cset != '\0'; LDAP_UTF8_INCR (cset)) {
			if (ldap_x_utf8_to_ucs4 (cstr) == ldap_x_utf8_to_ucs4 (cset)) {
				return cstr - str;
			}
		}
	}

	return cstr - str;
}

static gsize
ldap_utf8_strspn (const gchar *str,
                  const gchar *set)
{
	const gchar *cstr;
	const gchar *cset;

	for (cstr = str; *cstr != '\0'; LDAP_UTF8_INCR (cstr)) {
		for (cset = set; ; LDAP_UTF8_INCR (cset)) {
			if (*cset == '\0') {
				return cstr - str;
			}

			if (ldap_x_utf8_to_ucs4 (cstr) == ldap_x_utf8_to_ucs4 (cset)) {
				break;
			}
		}
	}

	return cstr - str;
}

static gchar *ldap_utf8_strtok (gchar *str, const gchar *sep, gchar **last)
{
	gchar *begin;
	gchar *end;

	if (last == NULL) return NULL;

	begin = str ? str : *last;

	begin += ldap_utf8_strspn (begin, sep);

	if (*begin == '\0') {
		*last = NULL;
		return NULL;
	}

	end = &begin[ ldap_utf8_strcspn (begin, sep) ];

	if (*end != '\0') {
		gchar *next = LDAP_UTF8_NEXT (end);
		*end = '\0';
		end = next;
	}

	*last = end;
	return begin;
}

/* from ldap.h */

#define LDAP_URL_SUCCESS		0x00	/* Success */
#define LDAP_URL_ERR_MEM		0x01	/* can't allocate memory space */
#define LDAP_URL_ERR_PARAM		0x02	/* parameter is bad */

#define LDAP_URL_ERR_BADSCHEME	0x03	/* URL doesn't begin with "ldap[si]://" */
#define LDAP_URL_ERR_BADENCLOSURE 0x04	/* URL is missing trailing ">" */
#define LDAP_URL_ERR_BADURL		0x05	/* URL is bad */
#define LDAP_URL_ERR_BADHOST	0x06	/* host port is bad */
#define LDAP_URL_ERR_BADATTRS	0x07	/* bad (or missing) attributes */
#define LDAP_URL_ERR_BADSCOPE	0x08	/* scope string is invalid (or missing) */
#define LDAP_URL_ERR_BADFILTER	0x09	/* bad or missing filter */
#define LDAP_URL_ERR_BADEXTS	0x0a	/* bad or missing extensions */

#define LDAP_URL_PREFIX         "ldap://"
#define LDAP_URL_PREFIX_LEN     (sizeof(LDAP_URL_PREFIX)-1)
#define LDAPS_URL_PREFIX		"ldaps://"
#define LDAPS_URL_PREFIX_LEN	(sizeof(LDAPS_URL_PREFIX)-1)
#define LDAPI_URL_PREFIX	"ldapi://"
#define LDAPI_URL_PREFIX_LEN	(sizeof(LDAPI_URL_PREFIX)-1)

#define LDAP_URL_URLCOLON		"URL:"
#define LDAP_URL_URLCOLON_LEN	(sizeof(LDAP_URL_URLCOLON)-1)

typedef struct ldap_url_desc {
	struct ldap_url_desc *lud_next;
	gchar	*lud_scheme;
	gchar	*lud_host;
	gint		lud_port;
	gchar	*lud_dn;
	gchar	**lud_attrs;
	gint		lud_scope;
	gchar	*lud_filter;
	gchar	**lud_exts;
	gint		lud_crit_exts;
} LDAPURLDesc;

/* from url.c */

static const gchar *
skip_url_prefix (
	const gchar *url,
	gint *enclosedp,
	const gchar **scheme)
{
	/*
	 * return non-zero if this looks like a LDAP URL; zero if not
	 * if non-zero returned, *urlp will be moved past "ldap://" part of URL
	 */
	const gchar *p;

	if (url == NULL) {
		return (NULL);
	}

	p = url;

	/* skip leading '<' (if any) */
	if (*p == '<') {
		*enclosedp = 1;
		++p;
	} else {
		*enclosedp = 0;
	}

	/* skip leading "URL:" (if any) */
	if (strncasecmp (p, LDAP_URL_URLCOLON, LDAP_URL_URLCOLON_LEN) == 0) {
		p += LDAP_URL_URLCOLON_LEN;
	}

	/* check for "ldap://" prefix */
	if (strncasecmp (p, LDAP_URL_PREFIX, LDAP_URL_PREFIX_LEN) == 0) {
		/* skip over "ldap://" prefix and return success */
		p += LDAP_URL_PREFIX_LEN;
		*scheme = "ldap";
		return (p);
	}

	/* check for "ldaps://" prefix */
	if (strncasecmp (p, LDAPS_URL_PREFIX, LDAPS_URL_PREFIX_LEN) == 0) {
		/* skip over "ldaps://" prefix and return success */
		p += LDAPS_URL_PREFIX_LEN;
		*scheme = "ldaps";
		return (p);
	}

	/* check for "ldapi://" prefix */
	if (strncasecmp (p, LDAPI_URL_PREFIX, LDAPI_URL_PREFIX_LEN) == 0) {
		/* skip over "ldapi://" prefix and return success */
		p += LDAPI_URL_PREFIX_LEN;
		*scheme = "ldapi";
		return (p);
	}

#ifdef LDAP_CONNECTIONLESS
	/* check for "cldap://" prefix */
	if (strncasecmp (p, LDAPC_URL_PREFIX, LDAPC_URL_PREFIX_LEN) == 0) {
		/* skip over "cldap://" prefix and return success */
		p += LDAPC_URL_PREFIX_LEN;
		*scheme = "cldap";
		return (p);
	}
#endif

	return (NULL);
}

static gint
str2scope (const gchar *p)
{
	if (strcasecmp (p, "one") == 0) {
		return LDAP_SCOPE_ONELEVEL;

	} else if (strcasecmp (p, "onelevel") == 0) {
		return LDAP_SCOPE_ONELEVEL;

	} else if (strcasecmp (p, "base") == 0) {
		return LDAP_SCOPE_BASE;

	} else if (strcasecmp (p, "sub") == 0) {
		return LDAP_SCOPE_SUBTREE;

	} else if (strcasecmp (p, "subtree") == 0) {
		return LDAP_SCOPE_SUBTREE;
	}

	return (-1);
}

static void
ldap_free_urldesc (LDAPURLDesc *ludp)
{
	if (ludp == NULL) {
		return;
	}

	if (ludp->lud_scheme != NULL) {
		LDAP_FREE (ludp->lud_scheme);
	}

	if (ludp->lud_host != NULL) {
		LDAP_FREE (ludp->lud_host);
	}

	if (ludp->lud_dn != NULL) {
		LDAP_FREE (ludp->lud_dn);
	}

	if (ludp->lud_filter != NULL) {
		LDAP_FREE (ludp->lud_filter);
	}

	if (ludp->lud_attrs != NULL) {
		LDAP_VFREE (ludp->lud_attrs);
	}

	if (ludp->lud_exts != NULL) {
		LDAP_VFREE (ludp->lud_exts);
	}

	LDAP_FREE (ludp);
}

static gint
ldap_int_unhex (gint c)
{
	return (c >= '0' && c <= '9' ? c - '0'
	    : c >= 'A' && c <= 'F' ? c - 'A' + 10
	    : c - 'a' + 10);
}

static void
ldap_pvt_hex_unescape (gchar *s)
{
	/*
	 * Remove URL hex escapes from s... done in place.  The basic concept for
	 * this routine is borrowed from the WWW library HTUnEscape() routine.
	 */
	gchar	*p;

	for (p = s; *s != '\0'; ++s) {
		if (*s == '%') {
			if (*++s == '\0') {
				break;
			}
			*p = ldap_int_unhex(*s) << 4;
			if (*++s == '\0') {
				break;
			}
			*p++ += ldap_int_unhex(*s);
		} else {
			*p++ = *s;
		}
	}

	*p = '\0';
}

static gchar **
ldap_str2charray (const gchar *str_in,
                  const gchar *brkstr)
{
	gchar	**res;
	gchar	*str, *s;
	gchar	*lasts;
	gint	i;

	/* protect the input string from strtok */
	str = LDAP_STRDUP (str_in);
	if (str == NULL) {
		return NULL;
	}

	i = 1;
	for (s = str; *s; s++) {
		if (ldap_utf8_strchr (brkstr, s) != NULL) {
			i++;
		}
	}

	res = (gchar **) LDAP_MALLOC ((i + 1) * sizeof (gchar *));

	if (res == NULL) {
		LDAP_FREE (str);
		return NULL;
	}

	i = 0;

	for (s = ldap_utf8_strtok (str, brkstr, &lasts);
		s != NULL;
		s = ldap_utf8_strtok (NULL, brkstr, &lasts))
	{
		res[i] = LDAP_STRDUP (s);

		if (res[i] == NULL) {
			for (--i; i >= 0; i--) {
				LDAP_FREE (res[i]);
			}
			LDAP_FREE (res);
			LDAP_FREE (str);
			return NULL;
		}

		i++;
	}

	res[i] = NULL;

	LDAP_FREE (str);
	return (res);
}

static gint
ldap_url_parse_ext (LDAP_CONST gchar *url_in,
                    LDAPURLDesc **ludpp)
{
/*
 *  Pick apart the pieces of an LDAP URL.
 */

	LDAPURLDesc	*ludp;
	gchar	*p, *q, *r;
	gint		i, enclosed;
	const gchar *scheme = NULL;
	const gchar *url_tmp;
	gchar *url;

	if (url_in == NULL || ludpp == NULL) {
		return LDAP_URL_ERR_PARAM;
	}

	*ludpp = NULL;	/* pessimistic */

	url_tmp = skip_url_prefix (url_in, &enclosed, &scheme);

	if (url_tmp == NULL) {
		return LDAP_URL_ERR_BADSCHEME;
	}

	assert (scheme);

	/* make working copy of the remainder of the URL */
	url = LDAP_STRDUP (url_tmp);
	if (url == NULL) {
		return LDAP_URL_ERR_MEM;
	}

	if (enclosed) {
		p = &url[strlen (url) - 1];

		if (*p != '>') {
			LDAP_FREE (url);
			return LDAP_URL_ERR_BADENCLOSURE;
		}

		*p = '\0';
	}

	/* allocate return struct */
	ludp = (LDAPURLDesc *) LDAP_CALLOC (1, sizeof (LDAPURLDesc));

	if (ludp == NULL) {
		LDAP_FREE (url);
		return LDAP_URL_ERR_MEM;
	}

	ludp->lud_next = NULL;
	ludp->lud_host = NULL;
	ludp->lud_port = 0;
	ludp->lud_dn = NULL;
	ludp->lud_attrs = NULL;
	ludp->lud_filter = NULL;
	ludp->lud_scope = LDAP_SCOPE_DEFAULT;
	ludp->lud_filter = NULL;
	ludp->lud_exts = NULL;

	ludp->lud_scheme = LDAP_STRDUP (scheme);

	if (ludp->lud_scheme == NULL) {
		LDAP_FREE (url);
		ldap_free_urldesc (ludp);
		return LDAP_URL_ERR_MEM;
	}

	/* scan forward for '/' that marks end of hostport and begin. of dn */
	p = strchr (url, '/');

	if (p != NULL) {
		/* terminate hostport; point to start of dn */
		*p++ = '\0';
	}

	/* IPv6 syntax with [ip address]:port */
	if (*url == '[') {
		r = strchr (url, ']');
		if (r == NULL) {
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_BADURL;
		}
		*r++ = '\0';
		q = strchr (r, ':');
	} else {
		q = strchr (url, ':');
	}

	if (q != NULL) {
		gchar	*next;

		*q++ = '\0';
		ldap_pvt_hex_unescape (q);

		if (*q == '\0') {
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_BADURL;
		}

		ludp->lud_port = strtol (q, &next, 10);
		if (next == NULL || next[0] != '\0') {
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_BADURL;
		}
	}

	ldap_pvt_hex_unescape (url);

	/* If [ip address]:port syntax, url is [ip and we skip the [ */
	ludp->lud_host = LDAP_STRDUP (url + (*url == '['));

	if (ludp->lud_host == NULL) {
		LDAP_FREE (url);
		ldap_free_urldesc (ludp);
		return LDAP_URL_ERR_MEM;
	}

	/*
	 * Kludge.  ldap://111.222.333.444:389??cn=abc,o=company
	 *
	 * On early Novell releases, search references/referrals were returned
	 * in this format, i.e., the dn was kind of in the scope position,
	 * but the required slash is missing. The whole thing is illegal syntax,
	 * but we need to account for it. Fortunately it can't be confused with
	 * anything real.
	 */
	if ((p == NULL) && (q != NULL) && ((q = strchr (q, '?')) != NULL)) {
		q++;
		/* ? immediately followed by question */
		if (*q == '?') {
			q++;
			if (*q != '\0') {
				/* parse dn part */
				ldap_pvt_hex_unescape (q);
				ludp->lud_dn = LDAP_STRDUP (q);
			} else {
				ludp->lud_dn = LDAP_STRDUP ("");
			}

			if (ludp->lud_dn == NULL) {
				LDAP_FREE (url);
				ldap_free_urldesc (ludp);
				return LDAP_URL_ERR_MEM;
			}
		}
	}

	if (p == NULL) {
		LDAP_FREE (url);
		*ludpp = ludp;
		return LDAP_URL_SUCCESS;
	}

	/* scan forward for '?' that may marks end of dn */
	q = strchr (p, '?');

	if (q != NULL) {
		/* terminate dn part */
		*q++ = '\0';
	}

	if (*p != '\0') {
		/* parse dn part */
		ldap_pvt_hex_unescape (p);
		ludp->lud_dn = LDAP_STRDUP (p);
	} else {
		ludp->lud_dn = LDAP_STRDUP ("");
	}

	if (ludp->lud_dn == NULL) {
		LDAP_FREE (url);
		ldap_free_urldesc (ludp);
		return LDAP_URL_ERR_MEM;
	}

	if (q == NULL) {
		/* no more */
		LDAP_FREE (url);
		*ludpp = ludp;
		return LDAP_URL_SUCCESS;
	}

	/* scan forward for '?' that may marks end of attributes */
	p = q;
	q = strchr (p, '?');

	if (q != NULL) {
		/* terminate attributes part */
		*q++ = '\0';
	}

	if (*p != '\0') {
		/* parse attributes */
		ldap_pvt_hex_unescape (p);
		ludp->lud_attrs = ldap_str2charray (p, ",");

		if (ludp->lud_attrs == NULL) {
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_BADATTRS;
		}
	}

	if (q == NULL) {
		/* no more */
		LDAP_FREE (url);
		*ludpp = ludp;
		return LDAP_URL_SUCCESS;
	}

	/* scan forward for '?' that may marks end of scope */
	p = q;
	q = strchr (p, '?');

	if (q != NULL) {
		/* terminate the scope part */
		*q++ = '\0';
	}

	if (*p != '\0') {
		/* parse the scope */
		ldap_pvt_hex_unescape (p);
		ludp->lud_scope = str2scope (p);

		if (ludp->lud_scope == -1) {
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_BADSCOPE;
		}
	}

	if (q == NULL) {
		/* no more */
		LDAP_FREE (url);
		*ludpp = ludp;
		return LDAP_URL_SUCCESS;
	}

	/* scan forward for '?' that may marks end of filter */
	p = q;
	q = strchr (p, '?');

	if (q != NULL) {
		/* terminate the filter part */
		*q++ = '\0';
	}

	if (*p != '\0') {
		/* parse the filter */
		ldap_pvt_hex_unescape (p);

		if (!*p) {
			/* missing filter */
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_BADFILTER;
		}

		LDAP_FREE (ludp->lud_filter);
		ludp->lud_filter = LDAP_STRDUP (p);

		if (ludp->lud_filter == NULL) {
			LDAP_FREE (url);
			ldap_free_urldesc (ludp);
			return LDAP_URL_ERR_MEM;
		}
	}

	if (q == NULL) {
		/* no more */
		LDAP_FREE (url);
		*ludpp = ludp;
		return LDAP_URL_SUCCESS;
	}

	/* scan forward for '?' that may marks end of extensions */
	p = q;
	q = strchr (p, '?');

	if (q != NULL) {
		/* extra '?' */
		LDAP_FREE (url);
		ldap_free_urldesc (ludp);
		return LDAP_URL_ERR_BADURL;
	}

	/* parse the extensions */
	ludp->lud_exts = ldap_str2charray (p, ",");

	if (ludp->lud_exts == NULL) {
		LDAP_FREE (url);
		ldap_free_urldesc (ludp);
		return LDAP_URL_ERR_BADEXTS;
	}

	for (i = 0; ludp->lud_exts[i] != NULL; i++) {
		ldap_pvt_hex_unescape (ludp->lud_exts[i]);

		if (*ludp->lud_exts[i] == '!') {
			/* count the number of critical extensions */
			ludp->lud_crit_exts++;
		}
	}

	if (i == 0) {
		/* must have 1 or more */
		LDAP_FREE (url);
		ldap_free_urldesc (ludp);
		return LDAP_URL_ERR_BADEXTS;
	}

	/* no more */
	*ludpp = ludp;
	LDAP_FREE (url);
	return LDAP_URL_SUCCESS;
}

static gint
ldap_url_parse (LDAP_CONST gchar *url_in,
                LDAPURLDesc **ludpp)
{
	gint rc = ldap_url_parse_ext (url_in, ludpp);

	if (rc != LDAP_URL_SUCCESS) {
		return rc;
	}

	if ((*ludpp)->lud_scope == LDAP_SCOPE_DEFAULT) {
		(*ludpp)->lud_scope = LDAP_SCOPE_BASE;
	}

	if ((*ludpp)->lud_host != NULL && *(*ludpp)->lud_host == '\0') {
		LDAP_FREE ((*ludpp)->lud_host);
		(*ludpp)->lud_host = NULL;
	}

	if ((*ludpp)->lud_port == 0) {
		if (strcmp ((*ludpp)->lud_scheme, "ldap") == 0) {
			(*ludpp)->lud_port = LDAP_PORT;
#ifdef LDAP_CONNECTIONLESS
		} else if (strcmp ((*ludpp)->lud_scheme, "cldap") == 0) {
			(*ludpp)->lud_port = LDAP_PORT;
#endif
		} else if (strcmp ((*ludpp)->lud_scheme, "ldaps") == 0) {
			(*ludpp)->lud_port = LDAPS_PORT;
		}
	}

	return rc;
}

