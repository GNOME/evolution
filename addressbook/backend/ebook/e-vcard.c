/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-vcard.c
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "e-vcard.h"

#define CRLF "\r\n"

typedef enum {
	EVC_ENCODING_RAW,    /* no encoding */
	EVC_ENCODING_BASE64, /* base64 */
	EVC_ENCODING_QP      /* quoted-printable */
} EVCardEncoding;

struct _EVCardPrivate {
	GList *attributes;
};

struct _EVCardAttribute {
	char  *group;
	char  *name;
	GList *params; /* EVCardParam */
	GList *values;
	GList *decoded_values;
	EVCardEncoding encoding;
	gboolean encoding_set;
};

struct _EVCardAttributeParam {
	char     *name;
	GList    *values;  /* GList of char*'s*/
};

static GObjectClass *parent_class;

static void   _evc_base64_init(void);
static size_t _evc_base64_encode_step(unsigned char *in, size_t len, gboolean break_lines, unsigned char *out, int *state, int *save);
static size_t _evc_base64_decode_step(unsigned char *in, size_t len, unsigned char *out, int *state, unsigned int *save);
size_t _evc_base64_decode_simple (char *data, size_t len);
char  *_evc_base64_encode_simple (const char *data, size_t len);

static void
e_vcard_dispose (GObject *object)
{
	EVCard *evc = E_VCARD (object);

	if (!evc->priv)
		return;

	g_list_foreach (evc->priv->attributes, (GFunc)e_vcard_attribute_free, NULL);
	g_list_free (evc->priv->attributes);

	g_free (evc->priv);
	evc->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_vcard_class_init (EVCardClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->dispose = e_vcard_dispose;

	_evc_base64_init();
}

static void
e_vcard_init (EVCard *evc)
{
	evc->priv = g_new0 (EVCardPrivate, 1);
}

GType
e_vcard_get_type (void)
{
	static GType vcard_type = 0;

	if (!vcard_type) {
		static const GTypeInfo vcard_info =  {
			sizeof (EVCardClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_vcard_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EVCard),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_vcard_init,
		};

		vcard_type = g_type_register_static (G_TYPE_OBJECT, "EVCard", &vcard_info, 0);
	}

	return vcard_type;
}



static char*
fold_lines (char *buf)
{
	GString *str = g_string_new ("");
	char *p = buf;
	char *next, *next2;

	/* we're pretty liberal with line folding here.  We handle
	   lines folded with \r\n<WS>... and \n\r<WS>... and
	   \n<WS>... We also turn single \r's and \n's not followed by
	   WS into \r\n's. */
	while (*p) {
		if (*p == '\r' || *p == '\n') {
			next = g_utf8_next_char (p);
			if (*next == '\n' || *next == '\r') {
				next2 = g_utf8_next_char (next);
				if (*next2 == ' ' || *next2 == '\t') {
					p = g_utf8_next_char (next2);
				}
				else {
					str = g_string_append (str, CRLF);
					p = g_utf8_next_char (next);
				}
			}
			else if (*next == ' ' || *next == '\t') {
				p = g_utf8_next_char (next);
			}
			else {
				str = g_string_append (str, CRLF);
				p = g_utf8_next_char (p);
			}
		}
		else {
			str = g_string_append_unichar (str, g_utf8_get_char (p));
			p = g_utf8_next_char (p);
		}
	}

	g_free (buf);

	return g_string_free (str, FALSE);
}

/* skip forward until we hit the CRLF, or \0 */
static void
skip_to_next_line (char **p)
{
	char *lp;
	lp = *p;

	while (*lp != '\r' && *lp != '\0')
		lp = g_utf8_next_char (lp);

	if (*lp == '\r') {
		lp = g_utf8_next_char (lp); /* \n */
		lp = g_utf8_next_char (lp); /* start of the next line */
	}

	*p = lp;
}

/* skip forward until we hit a character in @s, CRLF, or \0 */
static void
skip_until (char **p, char *s)
{
	/* XXX write me plz k thx */
	g_assert_not_reached();
}

static void
read_attribute_value (EVCardAttribute *attr, char **p, gboolean quoted_printable)
{
	char *lp = *p;
	GString *str;

	/* read in the value */
	str = g_string_new ("");
	while (*lp != '\r' && *lp != '\0') {
		if (*lp == '=' && quoted_printable) {
			char a, b;
			if ((a = *(++lp)) == '\0') break;
			if ((b = *(++lp)) == '\0') break;
			if (a == '\r' && b == '\n') {
				/* it was a = at the end of the line,
				 * just ignore this and continue
				 * parsing on the next line.  yay for
				 * 2 kinds of line folding
				 */
			}
			else if (isalnum(a) && isalnum (b)) {
				char c;

				a = tolower (a);
				b = tolower (b);

				c = (((a>='a'?a-'a'+10:a-'0')&0x0f) << 4)
					| ((b>='a'?b-'a'+10:b-'0')&0x0f);

				str = g_string_append_c (str, c);
			}
			/* silently consume malformed input, and
			   continue parsing */
			lp++;
		}
		else if (*lp == '\\') {
			/* convert back to the non-escaped version of
			   the characters */
			lp = g_utf8_next_char(lp);
			if (*lp == '\0') {
				str = g_string_append_c (str, '\\');
				break;
			}
			switch (*lp) {
			case 'n': str = g_string_append_c (str, '\n'); break;
			case 'r': str = g_string_append_c (str, '\r'); break;
			case ';': str = g_string_append_c (str, ';'); break;
			case ',': str = g_string_append_c (str, ','); break;
			case '\\': str = g_string_append_c (str, '\\'); break;
			default:
				g_warning ("invalid escape, passing it through");
				str = g_string_append_c (str, '\\');
				str = g_string_append_unichar (str, g_utf8_get_char(lp));
				break;
			}
			lp = g_utf8_next_char(lp);
		}
		else if (*lp == ';') {
			e_vcard_attribute_add_value (attr, str->str);
			g_string_assign (str, "");
			lp = g_utf8_next_char(lp);
		}
		else {
			str = g_string_append_unichar (str, g_utf8_get_char (lp));
			lp = g_utf8_next_char(lp);
		}
	}
	if (str) {
		e_vcard_attribute_add_value (attr, str->str);
		g_string_free (str, TRUE);
	}

	if (*lp == '\r') {
		lp = g_utf8_next_char (lp); /* \n */
		lp = g_utf8_next_char (lp); /* start of the next line */
	}

	*p = lp;
}

static void
read_attribute_params (EVCardAttribute *attr, char **p, gboolean *quoted_printable)
{
	char *lp = *p;
	GString *str;
	EVCardAttributeParam *param = NULL;

	str = g_string_new ("");
	while (*lp != '\0') {
		/* accumulate until we hit the '=' or ';'.  If we hit
		 * a '=' the string contains the parameter name.  if
		 * we hit a ';' the string contains the parameter
		 * value and the name is either ENCODING (if value ==
		 * QUOTED-PRINTABLE) or TYPE (in any other case.)
		 */
		if (*lp == '=') {
			if (str->len > 0) {
				param = e_vcard_attribute_param_new (str->str);
				g_string_assign (str, "");
				lp = g_utf8_next_char (lp);
			}
			else {
				skip_until (&lp, ":;");
				if (*lp == '\r') {
					lp = g_utf8_next_char (lp); /* \n */
					lp = g_utf8_next_char (lp); /* start of the next line */
					break;
				}
				else if (*lp == ';')
					lp = g_utf8_next_char (lp);
			}
		}
		else if (*lp == ';' || *lp == ':' || *lp == ',') {
			gboolean colon = (*lp == ':');
			gboolean comma = (*lp == ',');

			if (param) {
				if (str->len > 0) {
					e_vcard_attribute_param_add_value (param, str->str);
					g_string_assign (str, "");
					if (!colon)
						lp = g_utf8_next_char (lp);
				}
				else {
					/* we've got a parameter of the form:
					 * PARAM=(.*,)?[:;]
					 * so what we do depends on if there are already values
					 * for the parameter.  If there are, we just finish
					 * this parameter and skip past the offending character
					 * (unless it's the ':'). If there aren't values, we free
					 * the parameter then skip past the character.
					 */
					if (!param->values) {
						e_vcard_attribute_param_free (param);
						param = NULL;
					}
				}

				if (param
				    && !g_ascii_strcasecmp (param->name, "encoding")
				    && !g_ascii_strcasecmp (param->values->data, "quoted-printable")) {
					*quoted_printable = TRUE;
					e_vcard_attribute_param_free (param);
					param = NULL;
				}
			}
			else {
				if (str->len > 0) {
					char *param_name;
					if (!g_ascii_strcasecmp (str->str,
								 "quoted-printable")) {
						param_name = NULL;
						*quoted_printable = TRUE;
					}
					else {
						param_name = "TYPE";
					}

					if (param_name) {
						param = e_vcard_attribute_param_new (param_name);
						e_vcard_attribute_param_add_value (param, str->str);
					}
					g_string_assign (str, "");
					if (!colon)
						lp = g_utf8_next_char (lp);
				}
				else {
					/* XXX more here */
					g_assert_not_reached ();
				}
			}
			if (param && !comma) {
				e_vcard_attribute_add_param (attr, param);
				param = NULL;
			}
			if (colon)
				break;
		}
		else if (g_unichar_isalnum (g_utf8_get_char (lp)) || *lp == '-' || *lp == '_') {
			str = g_string_append_unichar (str, g_utf8_get_char (lp));
			lp = g_utf8_next_char (lp);
		}
		else {
			g_warning ("invalid character found in parameter spec");
			g_string_assign (str, "");
			skip_until (&lp, ":;");
		}
	}

	if (str)
		g_string_free (str, TRUE);

	*p = lp;
}

/* reads an entire attribute from the input buffer, leaving p pointing
   at the start of the next line (past the \r\n) */
static EVCardAttribute*
read_attribute (char **p)
{
	char *attr_group = NULL;
	char *attr_name = NULL;
	EVCardAttribute *attr = NULL;
	GString *str;
	char *lp = *p;
	gboolean is_qp = FALSE;

	/* first read in the group/name */
	str = g_string_new ("");
	while (*lp != '\r' && *lp != '\0') {
		if (*lp == ':' || *lp == ';') {
			if (str->len != 0) {
				/* we've got a name, break out to the value/attribute parsing */
				attr_name = g_string_free (str, FALSE);
				break;
			}
			else {
				/* a line of the form:
				 * (group.)?[:;]
				 *
				 * since we don't have an attribute
				 * name, skip to the end of the line
				 * and try again.
				 */
				g_string_free (str, TRUE);
				*p = lp;
				skip_to_next_line(p);
				goto lose;
			}
		}
		else if (*lp == '.') {
			if (attr_group) {
				g_warning ("extra `.' in attribute specification.  ignoring extra group `%s'",
					   str->str);
				g_string_free (str, TRUE);
				str = g_string_new ("");
			}
			if (str->len != 0) {
				attr_group = g_string_free (str, FALSE);
				str = g_string_new ("");
			}
		}
		else if (g_unichar_isalnum (g_utf8_get_char (lp)) || *lp == '-' || *lp == '_') {
			str = g_string_append_unichar (str, g_utf8_get_char (lp));
		}
		else {
			g_warning ("invalid character found in attribute group/name");
			g_string_free (str, TRUE);
			*p = lp;
			skip_to_next_line(p);
			goto lose;
		}

		lp = g_utf8_next_char(lp);
	}

	if (!attr_name) {
		skip_to_next_line (p);
		goto lose;
	}

	attr = e_vcard_attribute_new (attr_group, attr_name);
	g_free (attr_group);
	g_free (attr_name);

	if (*lp == ';') {
		/* skip past the ';' */
		lp = g_utf8_next_char(lp);
		read_attribute_params (attr, &lp, &is_qp);
	}
	if (*lp == ':') {
		/* skip past the ':' */
		lp = g_utf8_next_char(lp);
		read_attribute_value (attr, &lp, is_qp);
	}

	*p = lp;

	if (!attr->values)
		goto lose;

	return attr;
 lose:
	if (attr)
		e_vcard_attribute_free (attr);
	return NULL;
}

/* we try to be as forgiving as we possibly can here - this isn't a
 * validator.  Almost nothing is considered a fatal error.  We always
 * try to return *something*.
 */
static void
parse (EVCard *evc, const char *str)
{
	char *buf = g_strdup (str);
	char *p, *end;
	EVCardAttribute *attr;

	/* first validate the string is valid utf8 */
	if (!g_utf8_validate (buf, -1, (const char **)&end)) {
		/* if the string isn't valid, we parse as much as we can from it */
		g_warning ("invalid utf8 passed to EVCard.  Limping along.");
		*end = '\0';
	}
	
#if DEBUG_FOLDING
	printf ("BEFORE FOLDING:\n");
	printf (str);
#endif
	buf = fold_lines (buf);

#if DEBUG_FOLDING
	printf ("\n\nAFTER FOLDING:\n");
	printf (buf);
#endif
	p = buf;

	attr = read_attribute (&p);
	if (!attr || attr->group || g_ascii_strcasecmp (attr->name, "begin")) {
		g_warning ("vcard began without a BEGIN:VCARD\n");
	}
	if (attr)
		e_vcard_attribute_free (attr);

	while (*p) {
		EVCardAttribute *next_attr = read_attribute (&p);

		if (next_attr) {
			if (g_ascii_strcasecmp (next_attr->name, "end"))
				e_vcard_add_attribute (evc, next_attr);
			attr = next_attr;
		}
	}

	if (!attr || attr->group || g_ascii_strcasecmp (attr->name, "end")) {
		g_warning ("vcard ended without END:VCARD\n");
	}

	if (attr && !g_ascii_strcasecmp (attr->name, "end"))
		e_vcard_attribute_free (attr);

	g_free (buf);
}

static char*
escape_string (const char *s)
{
	GString *str = g_string_new ("");
	const char *p;

	/* Escape a string as described in RFC2426, section 5 */
	for (p = s; p && *p; p++) {
		switch (*p) {
		case '\n':
			str = g_string_append (str, "\\n");
			break;
		case '\r':
			if (*(p+1) == '\n')
				p++;
			str = g_string_append (str, "\\n");
			break;
		case ';':
			str = g_string_append (str, "\\;");
			break;
		case ',':
			str = g_string_append (str, "\\,");
			break;
		case '\\':
			str = g_string_append (str, "\\\\");
			break;
		default:
			str = g_string_append_c (str, *p);
			break;
		}
	}

	return g_string_free (str, FALSE);
}

#if notyet
static char*
unescape_string (const char *s)
{
	GString *str = g_string_new ("");
	const char *p;

	/* Unescape a string as described in RFC2426, section 5 */
	for (p = s; *p; p++) {
		if (*p == '\\') {
			p++;
			if (*p == '\0') {
				str = g_string_append_c (str, '\\');
				break;
			}
			switch (*p) {
			case 'n':  str = g_string_append_c (str, '\n'); break;
			case 'r':  str = g_string_append_c (str, '\r'); break;
			case ';':  str = g_string_append_c (str, ';'); break;
			case ',':  str = g_string_append_c (str, ','); break;
			case '\\': str = g_string_append_c (str, '\\'); break;
			default:
				g_warning ("invalid escape, passing it through");
				str = g_string_append_c (str, '\\');
				str = g_string_append_unichar (str, g_utf8_get_char(p));
				break;
			}
		}
	}

	return g_string_free (str, FALSE);
}
#endif

void
e_vcard_construct (EVCard *evc, const char *str)
{
	if (*str)
		parse (evc, str);
}

EVCard *
e_vcard_new ()
{
	return e_vcard_new_from_string ("");
}

EVCard *
e_vcard_new_from_string (const char *str)
{
	EVCard *evc;

	g_return_val_if_fail (str, NULL);

	evc = g_object_new (E_TYPE_VCARD, NULL);

	e_vcard_construct (evc, str);

	return evc;
}

static char*
e_vcard_to_string_vcard_21  (EVCard *evc)
{
	g_warning ("need to implement e_vcard_to_string_vcard_21");
	return g_strdup ("");
}

static char*
e_vcard_to_string_vcard_30 (EVCard *evc)
{
	GList *l;
	GList *v;

	GString *str = g_string_new ("");

	str = g_string_append (str, "BEGIN:vCard" CRLF);

	for (l = evc->priv->attributes; l; l = l->next) {
		GList *p;
		EVCardAttribute *attr = l->data;
		GString *attr_str = g_string_new ("");
		int l;

		/* From rfc2425, 5.8.2
		 *
		 * contentline  = [group "."] name *(";" param) ":" value CRLF
		 */

		if (attr->group) {
			attr_str = g_string_append (attr_str, attr->group);
			attr_str = g_string_append_c (attr_str, '.');
		}
		attr_str = g_string_append (attr_str, attr->name);

		/* handle the parameters */
		for (p = attr->params; p; p = p->next) {
			EVCardAttributeParam *param = p->data;
			/* 5.8.2:
			 * param        = param-name "=" param-value *("," param-value)
			 */
			attr_str = g_string_append_c (attr_str, ';');
			attr_str = g_string_append (attr_str, param->name);
			if (param->values) {
				attr_str = g_string_append_c (attr_str, '=');
				for (v = param->values; v; v = v->next) {
					char *value = v->data;
					attr_str = g_string_append (attr_str, value);
					if (v->next)
						attr_str = g_string_append_c (attr_str, ',');
				}
			}
		}

		attr_str = g_string_append_c (attr_str, ':');

		for (v = attr->values; v; v = v->next) {
			char *value = v->data;
			char *escaped_value = NULL;

			escaped_value = escape_string (value);

			attr_str = g_string_append (attr_str, escaped_value);
			if (v->next)
				attr_str = g_string_append_c (attr_str, ';');

			g_free (escaped_value);
		}

		/* 5.8.2:
		 * When generating a content line, lines longer than 75
		 * characters SHOULD be folded
		 */
		l = 0;
		do {
			if (attr_str->len - l > 75) {
				l += 75;
				attr_str = g_string_insert_len (attr_str, l, CRLF " ", sizeof (CRLF " ") - 1);
			}
			else
				break;
		} while (l < attr_str->len);

		attr_str = g_string_append (attr_str, CRLF);

		str = g_string_append (str, attr_str->str);
		g_string_free (attr_str, TRUE);
	}

	str = g_string_append (str, "END:vCard");

	return g_string_free (str, FALSE);
}

char*
e_vcard_to_string (EVCard *evc, EVCardFormat format)
{
	switch (format) {
	case EVC_FORMAT_VCARD_21:
		return e_vcard_to_string_vcard_21 (evc);
	case EVC_FORMAT_VCARD_30:
		return e_vcard_to_string_vcard_30 (evc);
	default:
		g_warning ("invalid format specifier passed to e_vcard_to_string");
		return g_strdup ("");
	}
}

void
e_vcard_dump_structure (EVCard *evc)
{
	GList *a;
	GList *v;
	int i;

	printf ("vCard\n");
	for (a = evc->priv->attributes; a; a = a->next) {
		GList *p;
		EVCardAttribute *attr = a->data;
		printf ("+-- %s\n", attr->name);
		if (attr->params) {
			printf ("    +- params=\n");

			for (p = attr->params, i = 0; p; p = p->next, i++) {
				EVCardAttributeParam *param = p->data;
				printf ("    |   [%d] = %s", i,param->name);
				printf ("(");
				for (v = param->values; v; v = v->next) {
					char *value = escape_string ((char*)v->data);
					printf ("%s", value);
					if (v->next)
						printf (",");
					g_free (value);
				}

				printf (")\n");
			}
		}
		printf ("    +- values=\n");
		for (v = attr->values, i = 0; v; v = v->next, i++) {
			printf ("        [%d] = `%s'\n", i, (char*)v->data);
		}
	}
}


EVCardAttribute*
e_vcard_attribute_new (const char *attr_group, const char *attr_name)
{
	EVCardAttribute *attr = g_new0 (EVCardAttribute, 1);

	attr->group = g_strdup (attr_group);
	attr->name = g_strdup (attr_name);

	return attr;
}

void
e_vcard_attribute_free (EVCardAttribute *attr)
{
	g_free (attr->group);
	g_free (attr->name);

	e_vcard_attribute_remove_values (attr);

	e_vcard_attribute_remove_params (attr);

	g_free (attr);
}

EVCardAttribute*
e_vcard_attribute_copy (EVCardAttribute *attr)
{
	EVCardAttribute *a = e_vcard_attribute_new (e_vcard_attribute_get_group (attr),
						    e_vcard_attribute_get_name (attr));
	GList *p;

	for (p = attr->values; p; p = p->next)
		e_vcard_attribute_add_value (a, p->data);

	for (p = attr->params; p; p = p->next)
		e_vcard_attribute_add_param (a, e_vcard_attribute_param_copy (p->data));

	return a;
}

void
e_vcard_remove_attributes (EVCard *evc, const char *attr_group, const char *attr_name)
{
	GList *attr;

	attr = evc->priv->attributes;
	while (attr) {
		GList *next_attr;
		EVCardAttribute *a = attr->data;

		next_attr = attr->next;

		if (((!attr_group && !a->group) || !g_ascii_strcasecmp (attr_group, a->group)) &&
		    ((!attr_name && !a->name) || !g_ascii_strcasecmp (attr_name, a->name))) {

			/* matches, remove/delete the attribute */
			evc->priv->attributes = g_list_remove_link (evc->priv->attributes, attr);

			e_vcard_attribute_free (a);
		}

		attr = next_attr;
	}
}

void
e_vcard_remove_attribute (EVCard *evc, EVCardAttribute *attr)
{
	evc->priv->attributes = g_list_remove (evc->priv->attributes, attr);
	e_vcard_attribute_free (attr);
}

void
e_vcard_add_attribute (EVCard *evc, EVCardAttribute *attr)
{
	evc->priv->attributes = g_list_append (evc->priv->attributes, attr);
}

void
e_vcard_add_attribute_with_value (EVCard *evcard,
				  EVCardAttribute *attr, const char *value)
{
	e_vcard_attribute_add_value (attr, value);

	e_vcard_add_attribute (evcard, attr);
}

void
e_vcard_add_attribute_with_values (EVCard *evcard, EVCardAttribute *attr, ...)
{
	va_list ap;
	char *v;

	va_start (ap, attr);

	while ((v = va_arg (ap, char*))) {
		e_vcard_attribute_add_value (attr, v);
	}

	va_end (ap);

	e_vcard_add_attribute (evcard, attr);
}

void
e_vcard_attribute_add_value (EVCardAttribute *attr, const char *value)
{
	attr->values = g_list_append (attr->values, g_strdup (value));
}

void
e_vcard_attribute_add_value_decoded (EVCardAttribute *attr, const char *value, int len)
{
	switch (attr->encoding) {
	case EVC_ENCODING_RAW:
		g_warning ("can't add_value_decoded with an attribute using RAW encoding.  you must set the ENCODING parameter first");
		break;
	case EVC_ENCODING_BASE64: {
		char *b64_data = _evc_base64_encode_simple (value, len);
		GString *decoded = g_string_new_len (value, len);

		/* make sure the decoded list is up to date */
		e_vcard_attribute_get_values_decoded (attr);

		printf ("base64 encoded value: %s\n", b64_data);
		printf ("original length: %d\n", len);

		attr->values = g_list_append (attr->values, b64_data);
		attr->decoded_values = g_list_append (attr->decoded_values, decoded);
		break;
	}
	case EVC_ENCODING_QP:
		g_warning ("need to implement quoted printable decoding");
		break;
	}
}

void
e_vcard_attribute_add_values (EVCardAttribute *attr,
			      ...)
{
	va_list ap;
	char *v;

	va_start (ap, attr);

	while ((v = va_arg (ap, char*))) {
		e_vcard_attribute_add_value (attr, v);
	}

	va_end (ap);
}

static void
free_gstring (GString *str)
{
	g_string_free (str, TRUE);
}

void
e_vcard_attribute_remove_values (EVCardAttribute *attr)
{
	g_list_foreach (attr->values, (GFunc)g_free, NULL);
	g_list_free (attr->values);
	g_list_foreach (attr->decoded_values, (GFunc)free_gstring, NULL);
	g_list_free (attr->decoded_values);
	attr->values = NULL;
}

void
e_vcard_attribute_remove_params (EVCardAttribute *attr)
{
	g_list_foreach (attr->params, (GFunc)e_vcard_attribute_param_free, NULL);
	g_list_free (attr->params);
	attr->params = NULL;
}

EVCardAttributeParam*
e_vcard_attribute_param_new (const char *name)
{
	EVCardAttributeParam *param = g_new0 (EVCardAttributeParam, 1);
	param->name = g_strdup (name);

	return param;
}

void
e_vcard_attribute_param_free (EVCardAttributeParam *param)
{
	g_free (param->name);

	e_vcard_attribute_param_remove_values (param);

	g_free (param);
}

EVCardAttributeParam*
e_vcard_attribute_param_copy (EVCardAttributeParam *param)
{
	EVCardAttributeParam *p = e_vcard_attribute_param_new (e_vcard_attribute_param_get_name (param));
	GList *l;

	for (l = param->values; l; l = l->next) {
		e_vcard_attribute_param_add_value (p, l->data);
	}

	return p;
}

void
e_vcard_attribute_add_param (EVCardAttribute *attr,
			     EVCardAttributeParam *param)
{
	attr->params = g_list_append (attr->params, param);

	/* we handle our special encoding stuff here */

	if (!g_ascii_strcasecmp (param->name, EVC_ENCODING)) {
		if (attr->encoding_set) {
			g_warning ("ENCODING specified twice");
			return;
		}

		if (param->values && param->values->data) {
			if (!g_ascii_strcasecmp ((char*)param->values->data, "b"))
				attr->encoding = EVC_ENCODING_BASE64;
			else if (!g_ascii_strcasecmp ((char*)param->values->data, EVC_QUOTEDPRINTABLE))
				attr->encoding = EVC_ENCODING_QP;
			else {
				g_warning ("Unknown value `%s' for ENCODING parameter.  values will be treated as raw",
					   (char*)param->values->data);
			}

			attr->encoding_set = TRUE;
		}
		else {
			g_warning ("ENCODING parameter added with no value");
		}
	}
}

void
e_vcard_attribute_param_add_value (EVCardAttributeParam *param,
				   const char *value)
{
	param->values = g_list_append (param->values, g_strdup (value));
}

void
e_vcard_attribute_param_add_values (EVCardAttributeParam *param,
				    ...)
{
	va_list ap;
	char *v;

	va_start (ap, param);

	while ((v = va_arg (ap, char*))) {
		e_vcard_attribute_param_add_value (param, v);
	}

	va_end (ap);
}

void
e_vcard_attribute_add_param_with_value (EVCardAttribute *attr,
					EVCardAttributeParam *param, const char *value)
{
	e_vcard_attribute_param_add_value (param, value);

	e_vcard_attribute_add_param (attr, param);
}

void
e_vcard_attribute_add_param_with_values (EVCardAttribute *attr,
					 EVCardAttributeParam *param, ...)
{
	va_list ap;
	char *v;

	va_start (ap, param);

	while ((v = va_arg (ap, char*))) {
		e_vcard_attribute_param_add_value (param, v);
	}

	va_end (ap);

	e_vcard_attribute_add_param (attr, param);
}

void
e_vcard_attribute_param_remove_values (EVCardAttributeParam *param)
{
	g_list_foreach (param->values, (GFunc)g_free, NULL);
	g_list_free (param->values);
	param->values = NULL;
}

GList*
e_vcard_get_attributes (EVCard *evcard)
{
	return evcard->priv->attributes;
}

const char*
e_vcard_attribute_get_group (EVCardAttribute *attr)
{
	return attr->group;
}

const char*
e_vcard_attribute_get_name (EVCardAttribute *attr)
{
	return attr->name;
}

GList*
e_vcard_attribute_get_values (EVCardAttribute *attr)
{
	return attr->values;
}

GList*
e_vcard_attribute_get_values_decoded (EVCardAttribute *attr)
{
	if (!attr->decoded_values) {
		GList *l;
		switch (attr->encoding) {
		case EVC_ENCODING_RAW:
			for (l = attr->values; l; l = l->next)
				attr->decoded_values = g_list_append (attr->decoded_values, g_string_new ((char*)l->data));
			break;
		case EVC_ENCODING_BASE64:
			for (l = attr->values; l; l = l->next) {
				char *decoded = g_strdup ((char*)l->data);
				int len = _evc_base64_decode_simple (decoded, strlen (decoded));
				attr->decoded_values = g_list_append (attr->decoded_values, g_string_new_len (decoded, len));
				g_free (decoded);
			}
			break;
		case EVC_ENCODING_QP:
			g_warning ("need to implement quoted printable decoding");
			break;
		}
	}

	return attr->decoded_values;
}

GList*
e_vcard_attribute_get_params (EVCardAttribute *attr)
{
	return attr->params;
}

const char*
e_vcard_attribute_param_get_name (EVCardAttributeParam *param)
{
	return param->name;
}

GList*
e_vcard_attribute_param_get_values (EVCardAttributeParam *param)
{
	return param->values;
}



/* encoding/decoding stuff ripped from camel-mime-utils.c */

static char *_evc_base64_alphabet =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char _evc_base64_rank[256];

static void
_evc_base64_init(void)
{
	int i;

	memset(_evc_base64_rank, 0xff, sizeof(_evc_base64_rank));
	for (i=0;i<64;i++) {
		_evc_base64_rank[(unsigned int)_evc_base64_alphabet[i]] = i;
	}
	_evc_base64_rank['='] = 0;
}

/* call this when finished encoding everything, to
   flush off the last little bit */
static size_t
_evc_base64_encode_close(unsigned char *in, size_t inlen, gboolean break_lines, unsigned char *out, int *state, int *save)
{
	int c1, c2;
	unsigned char *outptr = out;

	if (inlen>0)
		outptr += _evc_base64_encode_step(in, inlen, break_lines, outptr, state, save);

	c1 = ((unsigned char *)save)[1];
	c2 = ((unsigned char *)save)[2];
	
#if 0
	d(printf("mode = %d\nc1 = %c\nc2 = %c\n",
		 (int)((char *)save)[0],
		 (int)((char *)save)[1],
		 (int)((char *)save)[2]));
#endif

	switch (((char *)save)[0]) {
	case 2:
		outptr[2] = _evc_base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
		g_assert(outptr[2] != 0);
		goto skip;
	case 1:
		outptr[2] = '=';
	skip:
		outptr[0] = _evc_base64_alphabet[ c1 >> 2 ];
		outptr[1] = _evc_base64_alphabet[ c2 >> 4 | ( (c1&0x3) << 4 )];
		outptr[3] = '=';
		outptr += 4;
		break;
	}
	if (break_lines)
		*outptr++ = '\n';

	*save = 0;
	*state = 0;

	return outptr-out;
}

/*
  performs an 'encode step', only encodes blocks of 3 characters to the
  output at a time, saves left-over state in state and save (initialise to
  0 on first invocation).
*/
static size_t
_evc_base64_encode_step(unsigned char *in, size_t len, gboolean break_lines, unsigned char *out, int *state, int *save)
{
	register unsigned char *inptr, *outptr;

	if (len<=0)
		return 0;

	inptr = in;
	outptr = out;

#if 0
	d(printf("we have %d chars, and %d saved chars\n", len, ((char *)save)[0]));
#endif

	if (len + ((char *)save)[0] > 2) {
		unsigned char *inend = in+len-2;
		register int c1, c2, c3;
		register int already;

		already = *state;

		switch (((char *)save)[0]) {
		case 1:	c1 = ((unsigned char *)save)[1]; goto skip1;
		case 2:	c1 = ((unsigned char *)save)[1];
			c2 = ((unsigned char *)save)[2]; goto skip2;
		}
		
		/* yes, we jump into the loop, no i'm not going to change it, it's beautiful! */
		while (inptr < inend) {
			c1 = *inptr++;
		skip1:
			c2 = *inptr++;
		skip2:
			c3 = *inptr++;
			*outptr++ = _evc_base64_alphabet[ c1 >> 2 ];
			*outptr++ = _evc_base64_alphabet[ c2 >> 4 | ( (c1&0x3) << 4 ) ];
			*outptr++ = _evc_base64_alphabet[ ( (c2 &0x0f) << 2 ) | (c3 >> 6) ];
			*outptr++ = _evc_base64_alphabet[ c3 & 0x3f ];
			/* this is a bit ugly ... */
			if (break_lines && (++already)>=19) {
				*outptr++='\n';
				already = 0;
			}
		}

		((char *)save)[0] = 0;
		len = 2-(inptr-inend);
		*state = already;
	}

#if 0
	d(printf("state = %d, len = %d\n",
		 (int)((char *)save)[0],
		 len));
#endif

	if (len>0) {
		register char *saveout;

		/* points to the slot for the next char to save */
		saveout = & (((char *)save)[1]) + ((char *)save)[0];

		/* len can only be 0 1 or 2 */
		switch(len) {
		case 2:	*saveout++ = *inptr++;
		case 1:	*saveout++ = *inptr++;
		}
		((char *)save)[0]+=len;
	}

#if 0
	d(printf("mode = %d\nc1 = %c\nc2 = %c\n",
		 (int)((char *)save)[0],
		 (int)((char *)save)[1],
		 (int)((char *)save)[2]));
#endif

	return outptr-out;
}


/**
 * base64_decode_step: decode a chunk of base64 encoded data
 * @in: input stream
 * @len: max length of data to decode
 * @out: output stream
 * @state: holds the number of bits that are stored in @save
 * @save: leftover bits that have not yet been decoded
 *
 * Decodes a chunk of base64 encoded data
 **/
static size_t
_evc_base64_decode_step(unsigned char *in, size_t len, unsigned char *out, int *state, unsigned int *save)
{
	register unsigned char *inptr, *outptr;
	unsigned char *inend, c;
	register unsigned int v;
	int i;

	inend = in+len;
	outptr = out;

	/* convert 4 base64 bytes to 3 normal bytes */
	v=*save;
	i=*state;
	inptr = in;
	while (inptr<inend) {
		c = _evc_base64_rank[*inptr++];
		if (c != 0xff) {
			v = (v<<6) | c;
			i++;
			if (i==4) {
				*outptr++ = v>>16;
				*outptr++ = v>>8;
				*outptr++ = v;
				i=0;
			}
		}
	}

	*save = v;
	*state = i;

	/* quick scan back for '=' on the end somewhere */
	/* fortunately we can drop 1 output char for each trailing = (upto 2) */
	i=2;
	while (inptr>in && i) {
		inptr--;
		if (_evc_base64_rank[*inptr] != 0xff) {
			if (*inptr == '=' && outptr>out)
				outptr--;
			i--;
		}
	}

	/* if i!= 0 then there is a truncation error! */
	return outptr-out;
}

char *
_evc_base64_encode_simple (const char *data, size_t len)
{
	unsigned char *out;
	int state = 0, outlen;
	unsigned int save = 0;
	
	out = g_malloc (len * 4 / 3 + 5);
	outlen = _evc_base64_encode_close ((unsigned char *)data, len, FALSE,
				      out, &state, &save);
	out[outlen] = '\0';
	return (char *)out;
}

size_t
_evc_base64_decode_simple (char *data, size_t len)
{
	int state = 0;
	unsigned int save = 0;

	return _evc_base64_decode_step ((unsigned char *)data, len,
					(unsigned char *)data, &state, &save);
}
