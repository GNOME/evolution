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
  Abstract class for formatting mime messages
*/

#ifndef EM_FORMAT_H
#define EM_FORMAT_H

#include <glib-object.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define EM_TYPE_FORMAT \
	(em_format_get_type ())
#define EM_FORMAT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FORMAT, EMFormat))
#define EM_FORMAT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FORMAT, EMFormatClass))
#define EM_IS_FORMAT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FORMAT))
#define EM_IS_FORMAT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FORMAT))
#define EM_FORMAT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FORMAT, EMFormatClass))

G_BEGIN_DECLS

typedef struct _EMFormat EMFormat;
typedef struct _EMFormatClass EMFormatClass;
typedef struct _EMFormatPrivate EMFormatPrivate;

typedef struct _EMFormatHandler EMFormatHandler;
typedef struct _EMFormatHeader EMFormatHeader;

typedef void	(*EMFormatFunc)			(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part,
						 const EMFormatHandler *info,
						 gboolean is_fallback);

typedef enum {
	EM_FORMAT_MODE_NORMAL,
	EM_FORMAT_MODE_ALLHEADERS,
	EM_FORMAT_MODE_SOURCE
} EMFormatMode;

/**
 * EMFormatHandlerFlags - Format handler flags.
 *
 * @EM_FORMAT_HANDLER_INLINE: This type should be shown expanded
 * inline by default.
 * @EM_FORMAT_HANDLER_INLINE_DISPOSITION: This type should always be
 * shown inline, despite what the Content-Disposition suggests.
 *
 **/
typedef enum {
	EM_FORMAT_HANDLER_INLINE = 1<<0,
	EM_FORMAT_HANDLER_INLINE_DISPOSITION = 1<<1
} EMFormatHandlerFlags;

/**
 * struct _EMFormatHandler - MIME type handler.
 *
 * @mime_type: Type this handler handles.
 * @handler: The handler callback.
 * @flags: Handler flags
 * @old: The last handler set on this type.  Allows overrides to
 * fallback to previous implementation.
 *
 **/
struct _EMFormatHandler {
	gchar *mime_type;
	EMFormatFunc handler;
	EMFormatHandlerFlags flags;

	EMFormatHandler *old;
};

typedef struct _EMFormatPURI EMFormatPURI;
typedef void	(*EMFormatPURIFunc)		(EMFormat *emf,
						 CamelStream *stream,
						 EMFormatPURI *puri);

/**
 * struct _EMFormatPURI - Pending URI object.
 *
 * @free: May be set by allocator and will be called when no longer needed.
 * @format:
 * @uri: Calculated URI of the part, if the part has one in its
 * Content-Location field.
 * @cid: The RFC2046 Content-Id of the part.  If none is present, a unique value
 * is calculated from @part_id.
 * @part_id: A unique identifier for each part.
 * @func: Callback for when the URI is requested.  The callback writes
 * its data to the supplied stream.
 * @part:
 * @use_count:
 *
 * This is used for multipart/related, and other formatters which may
 * need to include a reference to out-of-band data in the content
 * stream.
 *
 * This object may be subclassed as a struct.
 **/
struct _EMFormatPURI {
	void (*free)(EMFormatPURI *p); /* optional callback for freeing user-fields */
	EMFormat *format;

	gchar *uri;		/* will be the location of the part, may be empty */
	gchar *cid;		/* will always be set, a fake one created if needed */
	gchar *part_id;		/* will always be set, emf->part_id->str for this part */

	EMFormatPURIFunc func;
	CamelMimePart *part;

	guint use_count;	/* used by multipart/related to see if it was accessed */
};

struct _EMFormatHeader {
	guint32 flags;		/* E_FORMAT_HEADER_* */
	gchar name[1];
};

#define EM_FORMAT_HEADER_BOLD (1<<0)
#define EM_FORMAT_HEADER_LAST (1<<4) /* reserve 4 slots */

#define EM_FORMAT_VALIDITY_FOUND_PGP       (1<<0)
#define EM_FORMAT_VALIDITY_FOUND_SMIME     (1<<1)
#define EM_FORMAT_VALIDITY_FOUND_SIGNED    (1<<2)
#define EM_FORMAT_VALIDITY_FOUND_ENCRYPTED (1<<3)

/**
 * struct _EMFormat - Mail formatter object.
 *
 * @parent:
 * @priv:
 * @message:
 * @folder:
 * @uid:
 * @part_id:
 * @header_list:
 * @session:
 * @base url:
 * @snoop_mime_type:
 * @valid:
 * @valid_parent:
 * @inline_table:
 * @pending_uri_table:
 * @pending_uri_tree:
 * @pending_uri_level:
 * @mode:
 * @charset:
 * @default_charset:
 *
 * Most fields are private or read-only.
 *
 * This is the base MIME formatter class.  It provides no formatting
 * itself, but drives most of the basic types, including multipart / * types.
 **/
struct _EMFormat {
	GObject parent;
	EMFormatPrivate *priv;

	/* The current message */
	CamelMimeMessage *message;

	CamelFolder *folder;
	gchar *uid;

	/* Current part ID prefix for identifying parts directly. */
	GString *part_id;

	/* If empty, then all. */
	GQueue header_list;

	/* Used for authentication when required. */
	CamelSession *session;

	/* Content-Base header or absolute Content-Location, for any part. */
	CamelURL *base;

	/* If we snooped an application/octet-stream, what we snooped. */
	const gchar *snoop_mime_type;

	/* For validity enveloping. */
	CamelCipherValidity *valid;
	CamelCipherValidity *valid_parent;

	/* For checking whether we found any signed or encrypted parts. */
	guint32 validity_found;

	/* For forcing inlining. */
	GHashTable *inline_table;

	/* Global URI lookup table for message. */
	GHashTable *pending_uri_table;

	/* This structure is used internally to form a visibility tree of
	 * parts in the current formatting stream.  This is to implement the
	 * part resolution rules for RFC2387 to implement multipart/related. */
	GNode *pending_uri_tree;

	/* The current level to search from. */
	GNode *pending_uri_level;

	EMFormatMode mode;		/* source/headers/etc */
	gchar *charset;			/* charset override */
	gchar *default_charset;		/* charset fallback */
	gboolean composer;		/* formatting from composer? */
	gboolean print;			/* formatting for printing? */
};

struct _EMFormatClass {
	GObjectClass parent_class;

	GHashTable *type_handlers;

	/* lookup handler, default falls back to hashtable above */
	const EMFormatHandler *
			(*find_handler)		(EMFormat *emf,
						 const gchar *mime_type);

	/* start formatting a message */
	void		(*format_clone)		(EMFormat *emf,
						 CamelFolder *folder,
						 const gchar *uid,
						 CamelMimeMessage *message,
						 EMFormat *source);

	/* some internel error/inconsistency */
	void		(*format_error)		(EMFormat *emf,
						 CamelStream *stream,
						 const gchar *errmsg);

	/* use for external structured parts */
	void		(*format_attachment)	(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part,
						 const gchar *mime_type,
						 const EMFormatHandler *info);

	/* use for unparsable content */
	void		(*format_source)	(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part);
	/* for outputing secure(d) content */
	void		(*format_secure)	(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part,
						 CamelCipherValidity *validity);

	/* returns true if the formatter is still busy with pending stuff */
	gboolean	(*busy)			(EMFormat *);

	/* Shows optional way to open messages  */
	void		(*format_optional)	(EMFormat *emf,
						 CamelStream *filter_stream,
						 CamelMimePart *mime_part,
						 CamelStream *mem_stream);

	gboolean	(*is_inline)		(EMFormat *emf,
						 const gchar *part_id,
						 CamelMimePart *mime_part,
						 const EMFormatHandler *handle);

	/* signals */
	/* complete, alternative to polling busy, for asynchronous work */
	void		(*complete)		(EMFormat *emf);
};

void		em_format_set_mode		(EMFormat *emf,
						 EMFormatMode mode);
void		em_format_set_charset		(EMFormat *emf,
						 const gchar *charset);
void		em_format_set_default_charset	(EMFormat *emf,
						 const gchar *charset);

/* also indicates to show all headers */
void		em_format_clear_headers		(EMFormat *emf);

void		em_format_default_headers	(EMFormat *emf);
void		em_format_add_header		(EMFormat *emf,
						 const gchar *name,
						 guint32 flags);

/* FIXME: Need a 'clone' api to copy details about the current view (inlines etc)
   Or maybe it should live with sub-classes? */

gint		em_format_is_attachment		(EMFormat *emf,
						 CamelMimePart *part);

gboolean	em_format_is_inline		(EMFormat *emf,
						 const gchar *part_id,
						 CamelMimePart *mime_part,
						 const EMFormatHandler *handle);
void		em_format_set_inline		(EMFormat *emf,
						 const gchar *partid,
						 gint state);

gchar *		em_format_describe_part		(CamelMimePart *part,
						 const gchar *mime_type);

/* for implementers */
GType		em_format_get_type		(void);

void		em_format_class_add_handler	(EMFormatClass *emfc,
						 EMFormatHandler *info);
void		em_format_class_remove_handler	(EMFormatClass *emfc,
						 EMFormatHandler *info);
const EMFormatHandler *
		em_format_find_handler		(EMFormat *emf,
						 const gchar *mime_type);
const EMFormatHandler *
		em_format_fallback_handler	(EMFormat *emf,
						 const gchar *mime_type);

/* puri is short for pending uri ... really */
EMFormatPURI *	em_format_add_puri		(EMFormat *emf,
						 gsize size,
						 const gchar *uri,
						 CamelMimePart *part,
						 EMFormatPURIFunc func);
EMFormatPURI *	em_format_find_visible_puri	(EMFormat *emf,
						 const gchar *uri);
EMFormatPURI *	em_format_find_puri		(EMFormat *emf,
						 const gchar *uri);
void		em_format_clear_puri_tree	(EMFormat *emf);
void		em_format_push_level		(EMFormat *emf);
void		em_format_pull_level		(EMFormat *emf);

/* clones inline state/view and format, or use to redraw */
void		em_format_format_clone		(EMFormat *emf,
						 CamelFolder *folder,
						 const gchar *uid,
						 CamelMimeMessage *message,
						 EMFormat *source);

/* formats a new message */
void		em_format_format		(EMFormat *emf,
						 CamelFolder *folder,
						 const gchar *uid,
						 CamelMimeMessage *message);
void		em_format_queue_redraw		(EMFormat *emf);
void		em_format_format_attachment	(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part,
						 const gchar *mime_type,
						 const EMFormatHandler *info);
void		em_format_format_error		(EMFormat *emf,
						 CamelStream *stream,
						 const gchar *format,
						 ...) G_GNUC_PRINTF (3, 4);
void		em_format_format_secure		(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part,
						 CamelCipherValidity *valid);
void		em_format_format_source		(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *mime_part);

gboolean	em_format_busy			(EMFormat *emf);

/* raw content only */
void		em_format_format_content	(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *part);

/* raw content text parts - should this just be checked/done by above? */
void		em_format_format_text		(EMFormat *emf,
						 CamelStream *stream,
						 CamelDataWrapper *part);

void		em_format_part_as		(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *part,
						 const gchar *mime_type);
void		em_format_part			(EMFormat *emf,
						 CamelStream *stream,
						 CamelMimePart *part);
void		em_format_merge_handler		(EMFormat *new,
						 EMFormat *old);

const gchar *	em_format_snoop_type		(CamelMimePart *part);

G_END_DECLS

#endif /* EM_FORMAT_H */
