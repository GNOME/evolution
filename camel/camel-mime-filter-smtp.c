/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
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

#include "camel-mime-filter-smtp.h"
#include <string.h>

#include <stdio.h>

#define d(x)

struct _CamelMimeFilterSmtpPrivate {
};

#define _PRIVATE(o) (((CamelMimeFilterSmtp *)(o))->priv)

static void camel_mime_filter_smtp_class_init (CamelMimeFilterSmtpClass *klass);
static void camel_mime_filter_smtp_init       (CamelMimeFilterSmtp *obj);
static void camel_mime_filter_smtp_finalise   (GtkObject *obj);

static CamelMimeFilterClass *camel_mime_filter_smtp_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_mime_filter_smtp_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelMimeFilterSmtp",
			sizeof (CamelMimeFilterSmtp),
			sizeof (CamelMimeFilterSmtpClass),
			(GtkClassInitFunc) camel_mime_filter_smtp_class_init,
			(GtkObjectInitFunc) camel_mime_filter_smtp_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_mime_filter_get_type (), &type_info);
	}
	
	return type;
}

typedef enum { EOLN_NODE, DOT_NODE } node_t;

struct smtpnode {
	struct smtpnode *next;
	node_t type;
	char *pointer;
};

static void
complete(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	*out = in;
	*outlen = len;
	*outprespace = prespace;
}


/* Yes, it is complicated ... */
static void
filter(CamelMimeFilter *mf, char *in, size_t len, size_t prespace, char **out, size_t *outlen, size_t *outprespace)
{
	CamelMimeFilterSmtp *f = (CamelMimeFilterSmtp *)mf;
	register gchar *inptr, *inend;
	guint linecount = 0;
	guint dotcount = 0;
	gint left;
	gint midline = f->midline;
	struct smtpnode *head = NULL, *tail = (struct smtpnode *)&head, *node;
	gchar *outptr = NULL;

	inptr = in;
	inend = inptr + len;

	while (inptr < inend) {
		register gint c = -1;

		if (midline)
			while (inptr < inend && (c = *inptr++) != '\n')
				;

		if (c == '\n' || !midline) {
			/* if there isn't already a carriage-return before the line-feed, count it */
			if (c == '\n') {
				linecount++;
				node = alloca (sizeof (*node));
				node->type = EOLN_NODE;
				node->pointer = inptr - 1;
				node->next = NULL;
				tail->next = node;
				tail = node;
			}

			left = inend - inptr;
		        if (left < 2) {
				if (*inptr == '.') {
					camel_mime_filter_backup (mf, inptr, left);
					midline = FALSE;
					inend = inptr;
					break;
				}
			} else {
				/* we only need to escape dots if they start the line */
				if (left > 0 && *inptr == '.' && *(inptr+1) != '.') {
					midline = TRUE;
					dotcount++;
					node = alloca (sizeof (*node));
					node->type = DOT_NODE;
					node->pointer = inptr;
					node->next = NULL;
					tail->next = node;
					tail = node;
					inptr++;
				} else {
					midline = TRUE;
				}
			}
		} else {
			/* \n is at end of line, check next buffer */
			midline = FALSE;
		}
	}

	f->midline = midline;

	if (dotcount > 0 || linecount > 0) {
		camel_mime_filter_set_size (mf, len + dotcount + linecount, FALSE);
		node = head;
		inptr = in;
		outptr = mf->outbuf;
		while (node) {
			if (node->type == EOLN_NODE) {
				memcpy (outptr, inptr, node->pointer - inptr);
				outptr += node->pointer - inptr;
				*outptr++ = '\r';
			} else {
				if (node->type == DOT_NODE) {
					memcpy (outptr, inptr, node->pointer - inptr);
					outptr += node->pointer - inptr;
					*outptr++ = '.';
				}
			}
			inptr = node->pointer;
			node = node->next;
		}
		memcpy (outptr, inptr, inend - inptr);
		outptr += inend - inptr;
		*out = mf->outbuf;
		*outlen = outptr - mf->outbuf;
		*outprespace = mf->outbuf - mf->outreal;

		d(printf ("Filtered '%.*s'\n", *outlen, *out));
	} else {
		*out = in;
		*outlen = inend - in;
		*outprespace = prespace;
		
		d(printf ("Filtered '%.*s'\n", *outlen, *out));
	}
}

static void
camel_mime_filter_smtp_class_init (CamelMimeFilterSmtpClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_smtp_parent = gtk_type_class (camel_mime_filter_get_type ());

	object_class->finalize = camel_mime_filter_smtp_finalise;

	filter_class->filter = filter;
	filter_class->complete = complete;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_mime_filter_smtp_init (CamelMimeFilterSmtp *obj)
{
	struct _CamelMimeFilterSmtpPrivate *p;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));
	obj->midline = FALSE;
}

static void
camel_mime_filter_smtp_finalise (GtkObject *obj)
{
	((GtkObjectClass *)(camel_mime_filter_smtp_parent))->finalize((GtkObject *)obj);
}

/**
 * camel_mime_filter_smtp_new:
 *
 * Create a new CamelMimeFilterSmtp object.
 * 
 * Return value: A new CamelMimeFilterSmtp widget.
 **/
CamelMimeFilterSmtp *
camel_mime_filter_smtp_new (void)
{
	CamelMimeFilterSmtp *new = CAMEL_MIME_FILTER_SMTP (gtk_type_new (camel_mime_filter_smtp_get_type ()));
	return new;
}

#ifdef TEST_PROGRAM

#include <stdio.h>

int main(int argc, char **argv)
{
	CamelMimeFilterSmtp *f;
	char *buffer;
	int len, prespace;

	gtk_init(&argc, &argv);

	f = camel_mime_filter_smtp_new();

	buffer = "This is a test\nFrom Someone\nTo someone. From Someone else, From\n From blah\nFromblah\nBye! \nFrom \n.\n.\r\nprevious 2 lines had .'s\nfrom should also be escaped\n";
	len = strlen(buffer);
	prespace = 0;

	printf("input = '%.*s'\n", len, buffer);
	camel_mime_filter_filter(f, buffer, len, prespace, &buffer, &len, &prespace);
	printf("output = '%.*s'\n", len, buffer);
	buffer = "";
	len = 0;
	prespace = 0;
	camel_mime_filter_complete(f, buffer, len, prespace, &buffer, &len, &prespace);
	printf("complete = '%.*s'\n", len, buffer);
	

	return 0;
}

#endif





















