/*
 *  Copyright (C) 2001 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "camel-mime-filter-html.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "xmlmemory.h"
#include "HTMLparser.h"
#include "HTMLtree.h"

#define d(x)

static void camel_mime_filter_html_class_init (CamelMimeFilterHTMLClass *klass);
static void camel_mime_filter_html_init       (CamelObject *o);
static void camel_mime_filter_html_finalize   (CamelObject *o);

static CamelMimeFilterClass *camel_mime_filter_html_parent;

struct _CamelMimeFilterHTMLPrivate {
	htmlParserCtxtPtr ctxt;
};

/* ********************************************************************** */

/* HTML parser */

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

static struct {
	char *element;
	char *remap;
} map_start[] = {
	{ "p", "\n\n" },
	{ "br", "\n" },
	{ "h1", "\n" }, { "h2", "\n" }, { "h3", "\n" }, { "h4", "\n" }, { "h5", "\n" }, { "h6", "\n" },
};


static struct {
	char *element;
	char *remap;
} map_end[] = {
	{ "h1", "\n" }, { "h2", "\n" }, { "h3", "\n" }, { "h4", "\n" }, { "h5", "\n" }, { "h6", "\n" },
};

static void
characters(void *ctx, const xmlChar *ch, int len)
{
	CamelMimeFilter *mf = ctx;

	memcpy(mf->outptr, ch, len);
	mf->outptr+= len;
}

#if 0
/* we probably dont want to index comments */
static void
comment(void *ctx, const xmlChar *value)
{
	CamelMimeFilter *mf = ctx;

	mf->outptr += sprintf(mf->outptr, " %s \n", value);
}
#endif

/* we map element starts to stuff sometimes, so we can properly break up
   words and lines.
   This is very dumb, and needs to be smarter: e.g.
   <b>F</b>\nooBar should -> "FooBar"
*/
static void
startElement(void *ctx, const xmlChar *name, const xmlChar **atts)
{
	int i;
	CamelMimeFilter *mf = ctx;

	/* we grab all "content" from "meta" tags, and dump it in the output,
	   it might be useful for searching with.  This should probably be pickier */
	if (!strcasecmp(name, "meta")) {
		if (atts) {
			for (i=0;atts[i];i+=2) {
				if (!strcmp(atts[i], "content"))
					mf->outptr += sprintf(mf->outptr, " %s \n", atts[i+1]);
			}
		}
		return;
	}

	/* FIXME: use a hashtable */
	for (i=0;i<ARRAY_LEN(map_start);i++) {
		if (!strcasecmp(map_start[i].element, name)) {
			characters(ctx, map_start[i].remap, strlen(map_start[i].remap));
			break;
		}
	}
}

static void
endElement(void *ctx, const xmlChar *name)
{
	int i;

	/* FIXME: use a hashtable */
	for (i=0;i<ARRAY_LEN(map_end);i++) {
		if (!strcasecmp(map_end[i].element, name)) {
			characters(ctx, map_end[i].remap, strlen(map_end[i].remap));
			break;
		}
	}
}

/* dum de dum, well we can print out some crap for now */
static void
warning(void *ctx, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stdout, "SAX.warning: ");
	vfprintf(stdout, msg, args);
	va_end(args);
}

static void
error(void *ctx, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stdout, "SAX.error: ");
	vfprintf(stdout, msg, args);
	va_end(args);
}

static void
fatalError(void *ctx, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fprintf(stdout, "SAX.fatalError: ");
	vfprintf(stdout, msg, args);
	va_end(args);
}

static xmlSAXHandler indexSAXHandler = {
	NULL, /* internalSubset */
	NULL, /*isStandalone,*/
	NULL, /*hasInternalSubset,*/
	NULL, /*hasExternalSubset,*/
	NULL, /*resolveEntity,*/
	NULL, /*getEntity,*/
	NULL, /*entityDecl,*/
	NULL, /*notationDecl,*/
	NULL, /*attributeDecl,*/
	NULL, /*elementDecl,*/
	NULL, /*unparsedEntityDecl,*/
	NULL, /*setDocumentLocator,*/
	NULL, /*startDocument,*/
	NULL, /*endDocument,*/
	startElement,
	endElement,
	NULL, /*reference,*/
	characters,
	NULL, /*ignorableWhitespace,*/
	NULL, /*processingInstruction,*/
	NULL, /*comment,*/
	warning,
	error,
	fatalError,
	NULL, /*getParameterEntity,*/
};


/* ********************************************************************** */


CamelType
camel_mime_filter_html_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (), "CamelMimeFilterHTML",
					    sizeof (CamelMimeFilterHTML),
					    sizeof (CamelMimeFilterHTMLClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_html_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_html_init,
					    (CamelObjectFinalizeFunc) camel_mime_filter_html_finalize);
	}
	
	return type;
}

static void
camel_mime_filter_html_finalize(CamelObject *o)
{
	CamelMimeFilterHTML *f = (CamelMimeFilterHTML *)o;

	if (f->priv->ctxt)
		htmlFreeParserCtxt(f->priv->ctxt);
}

static void
camel_mime_filter_html_init       (CamelObject *o)
{
	CamelMimeFilterHTML *f = (CamelMimeFilterHTML *)o;

	f->priv = g_malloc0(sizeof(*f->priv));
}

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterHTML *f = (CamelMimeFilterHTML *)mf;

	camel_mime_filter_set_size(mf, len*2+256, FALSE);
	mf->outptr = mf->outbuf;

	d(printf("converting html end:\n%.*s\n", (int)len, in));

	if (f->priv->ctxt == NULL) {
		f->priv->ctxt = htmlCreatePushParserCtxt(&indexSAXHandler, f, in, len, "", 0);
		len = 0;
	}

	htmlParseChunk(f->priv->ctxt, in, len, 1);

	*out = mf->outbuf;
	*outlenptr = mf->outptr - mf->outbuf;
	*outprespace = mf->outbuf - mf->outreal;

	d(printf("converted html end:\n%.*s\n", (int)*outlenptr, *out));
}

static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlenptr, size_t *outprespace)
{
	CamelMimeFilterHTML *f = (CamelMimeFilterHTML *)mf;

	camel_mime_filter_set_size(mf, len*2+16, FALSE);
	mf->outptr = mf->outbuf;

	d(printf("converting html:\n%.*s\n", (int)len, in));

	if (f->priv->ctxt == NULL)
		f->priv->ctxt = htmlCreatePushParserCtxt(&indexSAXHandler, f, in, len, "", 0);
	else
		htmlParseChunk(f->priv->ctxt, in, len, 0);

	*out = mf->outbuf;
	*outlenptr = mf->outptr - mf->outbuf;
	*outprespace = mf->outbuf - mf->outreal;

	d(printf("converted html:\n%.*s\n", (int)*outlenptr, *out));
}

static void
reset(CamelMimeFilter *mf)
{
	CamelMimeFilterHTML *f = (CamelMimeFilterHTML *)mf;

	if (f->priv->ctxt != NULL) {
		htmlFreeParserCtxt(f->priv->ctxt);
		f->priv->ctxt = NULL;
	}
}

static void
camel_mime_filter_html_class_init (CamelMimeFilterHTMLClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_html_parent = CAMEL_MIME_FILTER_CLASS (camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));

	filter_class->reset = reset;
	filter_class->filter = filter;
	filter_class->complete = complete;
}

/**
 * camel_mime_filter_html_new:
 *
 * Create a new CamelMimeFilterHTML object.
 * 
 * Return value: A new CamelMimeFilterHTML widget.
 **/
CamelMimeFilterHTML *
camel_mime_filter_html_new (void)
{
	CamelMimeFilterHTML *new = CAMEL_MIME_FILTER_HTML ( camel_object_new (camel_mime_filter_html_get_type ()));
	return new;
}

