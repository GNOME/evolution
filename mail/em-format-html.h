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

/**
 * struct _EMFormatHTMLJob - A formatting job.
 * 
 * @next: Double linked list header.
 * @prev: Double linked list header.
 * @format: Set by allocation function.
 * @stream: Free for use by caller.
 * @puri_level: Set by allocation function.
 * @base: Set by allocation function, used to save state.
 * @callback: This callback will always be invoked, only once, even if the user
 * cancelled the display.  So the callback should free any extra data
 * it allocated every time it is called.
 * @u: Union data, free for caller to use.
 * 
 * This object is used to queue a long-running-task which cannot be
 * processed in the primary thread.  When its turn comes, the job will
 * be de-queued and the @callback invoked to perform its processing,
 * restoring various state to match the original state.  This is used
 * for image loading and other internal tasks.
 *
 * This object is struct-subclassable.  Only em_format_html_job_new()
 * may be used to allocate these.
 **/
struct _EMFormatHTMLJob {
	struct _EMFormatHTMLJob *next;
	struct _EMFormatHTMLJob *prev;

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

/**
 * struct _EMFormatHTMLPObject - Pending object.
 * 
 * @next: Double linked list header.
 * @prev: Double linked list header.
 * @free: Invoked when the object is no longer needed.
 * @format: The parent formatter.
 * @classid: The assigned class id as passed to add_pobject().
 * @func: Callback function.
 * @part: The part as passed to add_pobject().
 * 
 * This structure is used to track OBJECT tags which have been
 * inserted into the HTML stream.  When GtkHTML requests them the
 * @func will be invoked to create the embedded widget.
 *
 * This object is struct-subclassable.  Only
 * em_format_html_add_pobject() may be used to allocate these.
 **/
struct _EMFormatHTMLPObject {
	struct _EMFormatHTMLPObject *next;
	struct _EMFormatHTMLPObject *prev;

	void (*free)(struct _EMFormatHTMLPObject *);
	struct _EMFormatHTML *format;

	char *classid;

	EMFormatHTMLPObjectFunc func;
	struct _CamelMimePart *part;
};

#define EM_FORMAT_HTML_HEADER_NOCOLUMNS (EM_FORMAT_HEADER_LAST)
#define EM_FORMAT_HTML_HEADER_HTML (EM_FORMAT_HEADER_LAST<<1) /* header already in html format */
#define EM_FORMAT_HTML_HEADER_LAST (EM_FORMAT_HEADER_LAST<<8)

#define EM_FORMAT_HTML_VPAD "<table cellspacing=0 cellpadding=3><tr><td><a name=\"padding\"></a></td></tr></table>\n"

/**
 * struct _EMFormatHTML - HTML formatter object.
 * 
 * @format: 
 * @priv: 
 * @html: 
 * @pending_object_list: 
 * @headers: 
 * @text_html_flags: 
 * @body_colour: 
 * @text_colour: 
 * @frame_colour: 
 * @content_colour: 
 * @citation_colour: 
 * @load_http:2: 
 * @load_http_now:1: 
 * @mark_citations:1: 
 * @simple_headers:1: 
 * @hide_headers:1: 
 * @show_rupert:1: 
 * 
 * Most of these fields are private or read-only.
 *
 * The base HTML formatter object.  This object drives HTML generation
 * into a GtkHTML parser.  It also handles text to HTML conversion,
 * multipart/related objects and inline images.
 **/
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
	unsigned int load_http:2;
	unsigned int load_http_now:1;
	unsigned int mark_citations:1;
	unsigned int simple_headers:1; /* simple header format, no box/table */
	unsigned int hide_headers:1; /* no headers at all */
	unsigned int show_rupert:1; /* whether we print rupert or not */
};

struct _EMFormatHTMLClass {
	EMFormatClass format_class;
	
};

GType em_format_html_get_type(void);
EMFormatHTML *em_format_html_new(void);

void em_format_html_load_http(EMFormatHTML *emf);

void em_format_html_set_load_http(EMFormatHTML *emf, int style);
void em_format_html_set_mark_citations(EMFormatHTML *emf, int state, guint32 citation_colour);

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
