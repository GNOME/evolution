/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

/*
  Concrete class for formatting mails to html
*/

#ifndef _EM_FORMAT_HTML_H
#define _EM_FORMAT_HTML_H

#include "em-format.h"

typedef struct _EMFormatHTML EMFormatHTML;
typedef struct _EMFormatHTMLClass EMFormatHTMLClass;

#if 0
struct _EMFormatHTMLHandler {
	EFrormatHandler base;
};
#endif

struct _GtkHTMLEmbedded;
struct _CamelMimePart;
struct _CamelMedium;
struct _CamelStream;

/* A HTMLJob will be executed in another thread, in sequence,
   It's job is to write to its stream, close it if successful,
   then exit */

typedef struct _EMFormatHTMLJob EMFormatHTMLJob;

struct _EMFormatHTMLJob {
	struct _EMFormatHTMLJob *next, *prev;

	EMFormatHTML *format;
	struct _CamelStream *stream;

	/* We need to track the state of the visibility tree at
	   the point this uri was generated */
	struct _EMFormatPURITree *puri_level;
	struct _CamelURL *base;

	void (*callback)(struct _EMFormatHTMLJob *job, int cancelled);
	union {
		char *uri;
		struct _CamelMedium *msg;
		EMFormatPURI *puri;
		struct _EMFormatPURITree *puri_level;
		void *data;
	} u;
};

/* Pending object (classid: url) */
typedef struct _EMFormatHTMLPObject EMFormatHTMLPObject;

typedef gboolean (*EMFormatHTMLPObjectFunc)(EMFormatHTML *md, struct _GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject);

struct _EMFormatHTMLPObject {
	struct _EMFormatHTMLPObject *next, *prev;

	void (*free)(struct _EMFormatHTMLPObject *);
	struct _EMFormatHTML *format;

	char *classid;

	EMFormatHTMLPObjectFunc func;
	struct _CamelMimePart *part;
};

#define EM_FORMAT_HTML_HEADER_NOCOLUMNS (EM_FORMAT_HEADER_LAST)
#define EM_FORMAT_HTML_HEADER_HTML (EM_FORMAT_HEADER_LAST<<1) /* header already in html format */
#define EM_FORMAT_HTML_HEADER_LAST (EM_FORMAT_HEADER_LAST<<8)

/* xmailer_mask bits */
#define EM_FORMAT_HTML_XMAILER_EVOLUTION (1<<0)
#define EM_FORMAT_HTML_XMAILER_OTHER     (1<<1)
#define EM_FORMAT_HTML_XMAILER_RUPERT    (1<<2)

#define EM_FORMAT_HTML_VPAD "<table cellspacing=0 cellpadding=3><tr><td><a name=\"padding\"></a></td></tr></table>\n"

struct _EMFormatHTML {
	EMFormat format;

	struct _EMFormatHTMLPrivate *priv;

	struct _GtkHTML *html;

	EDList pending_object_list;

	GSList *headers;

	guint32 text_html_flags; /* default flags for text to html conversion */
	guint32 body_colour;	/* header box colour */
	guint32 text_colour;
	guint32 frame_colour;
	guint32 content_colour;
	guint32 citation_colour;
	unsigned int xmailer_mask:4;
	unsigned int load_http:2;
	unsigned int load_http_now:1;
	unsigned int mark_citations:1;
	unsigned int simple_headers:1; /* simple header format, no box/table */
	unsigned int hide_headers:1; /* no headers at all */
};

struct _EMFormatHTMLClass {
	EMFormatClass format_class;
	
};

GType em_format_html_get_type(void);
EMFormatHTML *em_format_html_new(void);

void em_format_html_load_http(EMFormatHTML *emf);

void em_format_html_set_load_http(EMFormatHTML *emf, int style);
void em_format_html_set_mark_citations(EMFormatHTML *emf, int state, guint32 citation_colour);
void em_format_html_set_xmailer_mask(EMFormatHTML *emf, unsigned int xmailer_mask);

/* retrieves a pseudo-part icon wrapper for a file */
struct _CamelMimePart *em_format_html_file_part(EMFormatHTML *efh, const char *mime_type, const char *filename);

/* for implementers */
EMFormatHTMLPObject *em_format_html_add_pobject(EMFormatHTML *efh, size_t size, const char *classid, struct _CamelMimePart *part, EMFormatHTMLPObjectFunc func);
EMFormatHTMLPObject *em_format_html_find_pobject(EMFormatHTML *emf, const char *classid);
EMFormatHTMLPObject *em_format_html_find_pobject_func(EMFormatHTML *emf, struct _CamelMimePart *part, EMFormatHTMLPObjectFunc func);
void em_format_html_remove_pobject(EMFormatHTML *emf, EMFormatHTMLPObject *pobject);
void em_format_html_clear_pobject(EMFormatHTML *emf);

EMFormatHTMLJob *em_format_html_job_new(EMFormatHTML *emfh, void (*callback)(struct _EMFormatHTMLJob *job, int cancelled), void *data)
;
void em_format_html_job_queue(EMFormatHTML *emfh, struct _EMFormatHTMLJob *job);

#endif /* ! EM_FORMAT_HTML_H */
