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
  Abstract class for formatting mime messages
*/

#ifndef _EM_FORMAT_H
#define _EM_FORMAT_H

#include <glib-object.h>
#include "libedataserver/e-msgport.h"

struct _CamelStream;
struct _CamelMimePart;
struct _CamelMedium;
struct _CamelSession;
struct _CamelURL;
struct _CamelDataWrapper;
struct _CamelMimeMessage;
struct _CamelCipherValidity;

typedef struct _EMFormat EMFormat;
typedef struct _EMFormatClass EMFormatClass;

typedef struct _EMFormatHandler EMFormatHandler;
typedef struct _EMFormatHeader EMFormatHeader;

typedef void (*EMFormatFunc) (EMFormat *md, struct _CamelStream *stream, struct _CamelMimePart *part, const EMFormatHandler *info);

typedef enum _em_format_mode_t {
	EM_FORMAT_NORMAL,
	EM_FORMAT_ALLHEADERS,
	EM_FORMAT_SOURCE,
} em_format_mode_t;

/**
 * struct _EMFormatHandler - MIME type handler.
 * 
 * @mime_type: Type this handler handles.
 * @handler: The handler callback.
 * @flags: Handling flags, see enum _em_format_handler_t.
 * @old: The last handler set on this type.  Allows overrides to
 * fallback to previous implementation.
 * 
 **/
struct _EMFormatHandler {
	char *mime_type;
	EMFormatFunc handler;
	guint32 flags;

	struct _EMFormatHandler *old;
};

/**
 * enum _em_format_handler_t - Format handler flags.
 * 
 * @EM_FORMAT_HANDLER_INLINE: This type should be shown expanded
 * inline by default.
 * @EM_FORMAT_HANDLER_INLINE_DISPOSITION: This type should always be
 * shown inline, despite what the Content-Disposition suggests.
 * 
 **/
enum _em_format_handler_t {
	EM_FORMAT_HANDLER_INLINE = 1<<0,
	EM_FORMAT_HANDLER_INLINE_DISPOSITION = 1<<1,
};


typedef struct _EMFormatPURI EMFormatPURI;
typedef void (*EMFormatPURIFunc)(EMFormat *md, struct _CamelStream *stream, EMFormatPURI *puri);

/**
 * struct _EMFormatPURI - Pending URI object.
 * 
 * @next: Double-linked list header.
 * @prev: Double-linked list header.
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
	struct _EMFormatPURI *next;
	struct _EMFormatPURI *prev;

	void (*free)(struct _EMFormatPURI *p); /* optional callback for freeing user-fields */
	struct _EMFormat *format;

	char *uri;		/* will be the location of the part, may be empty */
	char *cid;		/* will always be set, a fake one created if needed */
	char *part_id;		/* will always be set, emf->part_id->str for this part */

	EMFormatPURIFunc func;
	struct _CamelMimePart *part;

	unsigned int use_count;	/* used by multipart/related to see if it was accessed */
};

/**
 * struct _EMFormatPURITree - Pending URI visibility tree.
 * 
 * @next: Double-linked list header.
 * @prev: Double-linked list header.
 * @parent: Parent in tree.
 * @uri_list: List of EMFormatPURI objects at this level.
 * @children: Child nodes of EMFormatPURITree.
 *
 * This structure is used internally to form a visibility tree of
 * parts in the current formatting stream.  This is to implement the
 * part resolution rules for RFC2387 to implement multipart/related.
 **/
struct _EMFormatPURITree {
	struct _EMFormatPURITree *next;
	struct _EMFormatPURITree *prev;
	struct _EMFormatPURITree *parent;

	EDList uri_list;
	EDList children;
};

struct _EMFormatHeader {
	struct _EMFormatHeader *next, *prev;

	guint32 flags;		/* E_FORMAT_HEADER_* */
	char name[1];
};

#define EM_FORMAT_HEADER_BOLD (1<<0)
#define EM_FORMAT_HEADER_LAST (1<<4) /* reserve 4 slots */

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
	
	struct _EMFormatPrivate *priv;
	
	struct _CamelMimeMessage *message; /* the current message */

	struct _CamelFolder *folder;
	char *uid;

	GString *part_id;	/* current part id prefix, for identifying parts directly */

	EDList header_list;	/* if empty, then all */

	struct _CamelSession *session; /* session, used for authentication when required */
	struct _CamelURL *base;	/* content-base header or absolute content-location, for any part */

	const char *snoop_mime_type; /* if we snooped an application/octet-stream type, what we snooped */

	/* for validity enveloping */
	struct _CamelCipherValidity *valid;
	struct _CamelCipherValidity *valid_parent;

	/* for forcing inlining */
	GHashTable *inline_table;

	/* global lookup table for message */
	GHashTable *pending_uri_table;

	/* visibility tree, also stores every puri permanently */
	struct _EMFormatPURITree *pending_uri_tree;
	/* current level to search from */
	struct _EMFormatPURITree *pending_uri_level;

	em_format_mode_t mode;	/* source/headers/etc */
	char *charset;		/* charset override */
	char *default_charset;	/* charset fallback */
};

struct _EMFormatClass {
	GObjectClass parent_class;

	GHashTable *type_handlers;

	/* lookup handler, default falls back to hashtable above */
	const EMFormatHandler *(*find_handler)(EMFormat *, const char *mime_type);

	/* start formatting a message */
	void (*format_clone)(EMFormat *, struct _CamelFolder *, const char *uid, struct _CamelMimeMessage *, EMFormat *);

	/* called to insert prefix material, after format_clone but before format_message */
	void (*format_prefix)(EMFormat *, struct _CamelStream *);

	/* some internel error/inconsistency */
	void (*format_error)(EMFormat *, struct _CamelStream *, const char *msg);

	/* use for external structured parts */
	void (*format_attachment)(EMFormat *, struct _CamelStream *, struct _CamelMimePart *, const char *mime_type, const struct _EMFormatHandler *info);
	/* for any message parts */
	void (*format_message)(EMFormat *, struct _CamelStream *, struct _CamelMedium *);
	/* use for unparsable content */
	void (*format_source)(EMFormat *, struct _CamelStream *, struct _CamelMimePart *);
	/* for outputing secure(d) content */
	void (*format_secure)(EMFormat *, struct _CamelStream *, struct _CamelMimePart *, struct _CamelCipherValidity *);

	/* returns true if the formatter is still busy with pending stuff */
	gboolean (*busy)(EMFormat *);

	/* signals */
	/* complete, alternative to polling busy, for asynchronous work */
	void (*complete)(EMFormat *);
};

/* helper entry point */
void em_format_set_session(EMFormat *emf, struct _CamelSession *s);

void em_format_set_mode(EMFormat *emf, em_format_mode_t type);
void em_format_set_charset(EMFormat *emf, const char *charset);
void em_format_set_default_charset(EMFormat *emf, const char *charset);

void em_format_clear_headers(EMFormat *emf); /* also indicates to show all headers */
void em_format_default_headers(EMFormat *emf);
void em_format_add_header(EMFormat *emf, const char *name, guint32 flags);

/* FIXME: Need a 'clone' api to copy details about the current view (inlines etc)
   Or maybe it should live with sub-classes? */

int em_format_is_attachment(EMFormat *emf, struct _CamelMimePart *part);

int em_format_is_inline(EMFormat *emf, const char *partid, struct _CamelMimePart *part, const EMFormatHandler *handle);
void em_format_set_inline(EMFormat *emf, const char *partid, int state);

char *em_format_describe_part(struct _CamelMimePart *part, const char *mimetype);

/* for implementers */
GType em_format_get_type(void);

void em_format_class_add_handler(EMFormatClass *emfc, EMFormatHandler *info);
void em_format_class_remove_handler(EMFormatClass *emfc, EMFormatHandler *info);
#define em_format_find_handler(emf, type) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->find_handler((emf), (type))
const EMFormatHandler *em_format_fallback_handler(EMFormat *emf, const char *mime_type);

/* puri is short for pending uri ... really */
EMFormatPURI *em_format_add_puri(EMFormat *emf, size_t size, const char *uri, struct _CamelMimePart *part, EMFormatPURIFunc func);
EMFormatPURI *em_format_find_visible_puri(EMFormat *emf, const char *uri);
EMFormatPURI *em_format_find_puri(EMFormat *emf, const char *uri);
void em_format_clear_puri_tree(EMFormat *emf);
void em_format_push_level(EMFormat *emf);
void em_format_pull_level(EMFormat *emf);

/* clones inline state/view and format, or use to redraw */
#define em_format_format_clone(emf, folder, uid, msg, src) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_clone((emf), (folder), (uid), (msg), (src))
/* formats a new message */
#define em_format_format(emf, folder, uid, msg) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_clone((emf), (folder), (uid), (msg), NULL)
#define em_format_format_prefix(emf, stream) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_prefix((emf), (stream))
#define em_format_redraw(emf) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_clone((emf),				\
										       ((EMFormat *)(emf))->folder,	\
										       ((EMFormat *)(emf))->uid,	\
										       ((EMFormat *)(emf))->message,	\
										       (emf))
void em_format_format_error(EMFormat *emf, struct _CamelStream *stream, const char *fmt, ...);
#define em_format_format_attachment(emf, stream, msg, type, info) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_attachment((emf), (stream), (msg), (type), (info))
#define em_format_format_message(emf, stream, msg) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_message((emf), (stream), (msg))
#define em_format_format_source(emf, stream, msg) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_source((emf), (stream), (msg))
void em_format_format_secure(EMFormat *emf, struct _CamelStream *stream, struct _CamelMimePart *part, struct _CamelCipherValidity *valid);

#define em_format_busy(emf) ((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->busy((emf))

/* raw content only */
void em_format_format_content(EMFormat *emf, struct _CamelStream *stream, struct _CamelMimePart *part);
/* raw content text parts - should this just be checked/done by above? */
void em_format_format_text(EMFormat *emf, struct _CamelStream *stream, struct _CamelDataWrapper *part);

void em_format_part_as(EMFormat *emf, struct _CamelStream *stream, struct _CamelMimePart *part, const char *mime_type);
void em_format_part(EMFormat *emf, struct _CamelStream *stream, struct _CamelMimePart *part);

#endif /* ! _EM_FORMAT_H */
