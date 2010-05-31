/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
  Abstract class for formatting mails to html
*/

#ifndef EM_FORMAT_HTML_H
#define EM_FORMAT_HTML_H

#include <em-format/em-format.h>
#include <mail/mail-config.h>
#include <misc/e-web-view.h>
#include <gtkhtml/gtkhtml-embedded.h>

/* Standard GObject macros */
#define EM_TYPE_FORMAT_HTML \
	(em_format_html_get_type ())
#define EM_FORMAT_HTML(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FORMAT_HTML, EMFormatHTML))
#define EM_FORMAT_HTML_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FORMAT_HTML, EMFormatHTMLClass))
#define EM_IS_FORMAT_HTML(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FORMAT_HTML))
#define EM_IS_FORMAT_HTML_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FORMAT_HTML))
#define EM_FORMAT_HTML_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FORMAT_HTML, EMFormatHTMLClass))

G_BEGIN_DECLS

typedef struct _EMFormatHTML EMFormatHTML;
typedef struct _EMFormatHTMLClass EMFormatHTMLClass;
typedef struct _EMFormatHTMLPrivate EMFormatHTMLPrivate;

enum _em_format_html_header_flags {
	EM_FORMAT_HTML_HEADER_TO = 1<<0,
	EM_FORMAT_HTML_HEADER_CC = 1<<1,
	EM_FORMAT_HTML_HEADER_BCC = 1<<2
};

typedef enum {
	EM_FORMAT_HTML_STATE_NONE = 0,
	EM_FORMAT_HTML_STATE_RENDERING
} EMFormatHTMLState;

typedef enum {
	EM_FORMAT_HTML_COLOR_BODY,	/* header area background */
	EM_FORMAT_HTML_COLOR_CITATION,	/* citation font color */
	EM_FORMAT_HTML_COLOR_CONTENT,	/* message area background */
	EM_FORMAT_HTML_COLOR_FRAME,	/* frame around message area */
	EM_FORMAT_HTML_COLOR_HEADER,	/* header font color */
	EM_FORMAT_HTML_COLOR_TEXT,	/* message font color */
	EM_FORMAT_HTML_NUM_COLOR_TYPES
} EMFormatHTMLColorType;

/* A HTMLJob will be executed in another thread, in sequence.
   It's job is to write to its stream, close it if successful,
   then exit. */

typedef struct _EMFormatHTMLJob EMFormatHTMLJob;

/**
 * struct _EMFormatHTMLJob - A formatting job.
 *
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
	EMFormatHTML *format;
	CamelStream *stream;

	/* We need to track the state of the visibility tree at
	   the point this uri was generated */
	GNode *puri_level;
	CamelURL *base;

	void (*callback)(EMFormatHTMLJob *job, gint cancelled);
	union {
		gchar *uri;
		CamelMedium *msg;
		EMFormatPURI *puri;
		GNode *puri_level;
		gpointer data;
	} u;
};

/* Pending object (classid: url) */
typedef struct _EMFormatHTMLPObject EMFormatHTMLPObject;

typedef gboolean (*EMFormatHTMLPObjectFunc)(EMFormatHTML *md, GtkHTMLEmbedded *eb, EMFormatHTMLPObject *pobject);

/**
 * struct _EMFormatHTMLPObject - Pending object.
 *
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
	void (*free)(EMFormatHTMLPObject *);
	EMFormatHTML *format;

	gchar *classid;

	EMFormatHTMLPObjectFunc func;
	CamelMimePart *part;
};

#define EM_FORMAT_HTML_HEADER_NOCOLUMNS (EM_FORMAT_HEADER_LAST)
#define EM_FORMAT_HTML_HEADER_HTML (EM_FORMAT_HEADER_LAST<<1) /* header already in html format */
#define EM_FORMAT_HTML_HEADER_NODEC (EM_FORMAT_HEADER_LAST<<2) /* header already in html format */
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
 * @header_colour:
 * @text_colour:
 * @frame_colour:
 * @content_colour:
 * @citation_colour:
 * @load_http:2:
 * @load_http_now:1:
 * @mark_citations:1:
 * @simple_headers:1:
 * @hide_headers:1:
 * @show_icon:1:
 *
 * Most of these fields are private or read-only.
 *
 * The base HTML formatter object.  This object drives HTML generation
 * into a GtkHTML parser.  It also handles text to HTML conversion,
 * multipart/related objects and inline images.
 **/
struct _EMFormatHTML {
	EMFormat parent;
	EMFormatHTMLPrivate *priv;

	GQueue pending_object_list;

	GSList *headers;

	guint32 text_html_flags; /* default flags for text to html conversion */
	guint simple_headers:1; /* simple header format, no box/table */
	guint hide_headers:1; /* no headers at all */
	guint show_icon:1; /* show an icon when the sender used Evo */
	guint32 header_wrap_flags;

	EMFormatHTMLState state; /* actual state of the object */
};

struct _EMFormatHTMLClass {
	EMFormatClass parent_class;

	GType html_widget_type;
};

GType		em_format_html_get_type		(void);
EWebView *	em_format_html_get_web_view	(EMFormatHTML *efh);
void		em_format_html_load_images	(EMFormatHTML *efh);
void		em_format_html_get_color	(EMFormatHTML *efh,
						 EMFormatHTMLColorType type,
						 GdkColor *color);
void		em_format_html_set_color	(EMFormatHTML *efh,
						 EMFormatHTMLColorType type,
						 const GdkColor *color);
MailConfigHTTPMode
		em_format_html_get_image_loading_policy
						(EMFormatHTML *efh);
void		em_format_html_set_image_loading_policy
						(EMFormatHTML *efh,
						 MailConfigHTTPMode policy);
gboolean	em_format_html_get_mark_citations
						(EMFormatHTML *efh);
void		em_format_html_set_mark_citations
						(EMFormatHTML *efh,
						 gboolean mark_citations);
gboolean	em_format_html_get_only_local_photos
						(EMFormatHTML *efh);
void		em_format_html_set_only_local_photos
						(EMFormatHTML *efh,
						 gboolean only_local_photos);
gboolean	em_format_html_get_show_sender_photo
						(EMFormatHTML *efh);
void		em_format_html_set_show_sender_photo
						(EMFormatHTML *efh,
						 gboolean show_sender_photo);

/* retrieves a pseudo-part icon wrapper for a file */
CamelMimePart *	em_format_html_file_part	(EMFormatHTML *efh,
						 const gchar *mime_type,
						 const gchar *filename);

/* for implementers */
EMFormatHTMLPObject *
		em_format_html_add_pobject	(EMFormatHTML *efh,
						 gsize size,
						 const gchar *classid,
						 CamelMimePart *part,
						 EMFormatHTMLPObjectFunc func);
EMFormatHTMLPObject *
		em_format_html_find_pobject	(EMFormatHTML *efh,
						 const gchar *classid);
EMFormatHTMLPObject *
		em_format_html_find_pobject_func(EMFormatHTML *efh,
						 CamelMimePart *part,
						 EMFormatHTMLPObjectFunc func);
void		em_format_html_remove_pobject	(EMFormatHTML *efh,
						 EMFormatHTMLPObject *pobject);
void		em_format_html_clear_pobject	(EMFormatHTML *efh);
EMFormatHTMLJob *
		em_format_html_job_new		(EMFormatHTML *efh,
						 void (*callback)(EMFormatHTMLJob *job, gint cancelled),
						 gpointer data);
void		em_format_html_job_queue	(EMFormatHTML *efh,
						 EMFormatHTMLJob *job);
gboolean	em_format_html_get_show_real_date(EMFormatHTML *efh);
void		em_format_html_set_show_real_date(EMFormatHTML *efh,
						 gboolean show_real_date);

gchar *		em_format_html_format_cert_infos (CamelCipherCertInfo *first_cinfo);

G_END_DECLS

#endif /* EM_FORMAT_HTML_H */
