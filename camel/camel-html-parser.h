/*
 *  Copyright (C) 2001 Ximian Inc.
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

/** WARNING
 **
 ** DO NOT USE THIS CODE OUTSIDE OF CAMEL
 **
 ** IT IS SUBJECT TO CHANGE OR MAY VANISH AT ANY TIME
 **/

#ifndef _CAMEL_HTML_PARSER_H
#define _CAMEL_HTML_PARSER_H

#include <camel/camel-object.h>

#define CAMEL_HTML_PARSER(obj)         CAMEL_CHECK_CAST (obj, camel_html_parser_get_type (), CamelHTMLParser)
#define CAMEL_HTML_PARSER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_html_parser_get_type (), CamelHTMLParserClass)
#define CAMEL_IS_HTML_PARSER(obj)      CAMEL_CHECK_TYPE (obj, camel_html_parser_get_type ())

typedef struct _CamelHTMLParserClass CamelHTMLParserClass;
typedef struct _CamelHTMLParser CamelHTMLParser;

/* Parser/tokeniser states */
typedef enum _camel_html_parser_t {
	CAMEL_HTML_PARSER_DATA,			/* raw data */
	CAMEL_HTML_PARSER_ENT,			/* entity in data */
	CAMEL_HTML_PARSER_ELEMENT,		/* element (tag + attributes scanned) */
	CAMEL_HTML_PARSER_TAG,			/* tag */
	CAMEL_HTML_PARSER_DTDENT,		/* dtd entity? <! blah blah > */
	CAMEL_HTML_PARSER_COMMENT0,		/* start of comment */
	CAMEL_HTML_PARSER_COMMENT,		/* body of comment */
	CAMEL_HTML_PARSER_ATTR0,		/* start of attribute */
	CAMEL_HTML_PARSER_ATTR,			/* attribute */
	CAMEL_HTML_PARSER_VAL0,			/* start of value */
	CAMEL_HTML_PARSER_VAL,			/* value */
	CAMEL_HTML_PARSER_VAL_ENT,		/* entity in value */
	CAMEL_HTML_PARSER_EOD,			/* end of current data */
	CAMEL_HTML_PARSER_EOF,			/* end of file */
} camel_html_parser_t;

struct _CamelHTMLParser {
	CamelObject parent;

	struct _CamelHTMLParserPrivate *priv;
};

struct _CamelHTMLParserClass {
	CamelObjectClass parent_class;
};

CamelType		camel_html_parser_get_type	(void);
CamelHTMLParser      *camel_html_parser_new	(void);

void camel_html_parser_set_data(CamelHTMLParser *hp, const char *start, int len, int last);
camel_html_parser_t camel_html_parser_step(CamelHTMLParser *hp, const char **datap, int *lenp);
const char *camel_html_parser_left(CamelHTMLParser *hp, int *lenp);
const char *camel_html_parser_tag(CamelHTMLParser *hp);
const char *camel_html_parser_attr(CamelHTMLParser *hp, const char *name);
const GPtrArray *camel_html_parser_attr_list(CamelHTMLParser *hp, const GPtrArray **values);

#endif /* ! _CAMEL_HTML_PARSER_H */
