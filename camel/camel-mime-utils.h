/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *           Jeffrey Stedfast <fejj@helixcode.com>
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

#ifndef _CAMEL_MIME_UTILS_H
#define _CAMEL_MIME_UTILS_H

#include <glib.h>
#include <time.h>

/* a list of references for this message */
struct _header_references {
	struct _header_references *next;
	char *id;
};

struct _header_param {
	struct _header_param *next;
	char *name;
	char *value;
};

/* describes a content-type */
struct _header_content_type {
	char *type;
	char *subtype;
	struct _header_param *params;
	unsigned int refcount;
};

/* a raw rfc822 header */
/* the value MUST be US-ASCII */
struct _header_raw {
	struct _header_raw *next;
	char *name;
	char *value;
	int offset;		/* in file, if known */
};

typedef struct _CamelMimeDisposition {
	char *disposition;
	struct _header_param *params;
	unsigned int refcount;
} CamelMimeDisposition;

enum _header_address_type {
	HEADER_ADDRESS_NONE,	/* uninitialised */
	HEADER_ADDRESS_NAME,
	HEADER_ADDRESS_GROUP
};

struct _header_address {
	struct _header_address *next;
	enum _header_address_type type;
	char *name;
	union {
		char *addr;
		struct _header_address *members;
	} v;
	unsigned int refcount;
};

/* Address lists */
struct _header_address *header_address_new(void);
struct _header_address *header_address_new_name(const char *name, const char *addr);
struct _header_address *header_address_new_group(const char *name);
void header_address_ref(struct _header_address *);
void header_address_unref(struct _header_address *);
void header_address_set_name(struct _header_address *, const char *name);
void header_address_set_addr(struct _header_address *, const char *addr);
void header_address_set_members(struct _header_address *, struct _header_address *group);
void header_address_add_member(struct _header_address *, struct _header_address *member);
void header_address_list_append_list(struct _header_address **l, struct _header_address **h);
void header_address_list_append(struct _header_address **, struct _header_address *);
void header_address_list_clear(struct _header_address **);

struct _header_address *header_address_decode(const char *in);
struct _header_address *header_mailbox_decode(const char *in);
char *header_address_list_format(struct _header_address *a);

/* structured header prameters */
char *header_param(struct _header_param *p, const char *name);
struct _header_param *header_set_param(struct _header_param **l, const char *name, const char *value);
void header_param_list_free(struct _header_param *p);

/* Content-Type header */
struct _header_content_type *header_content_type_new(const char *type, const char *subtype);
struct _header_content_type *header_content_type_decode(const char *in);
void header_content_type_unref(struct _header_content_type *ct);
void header_content_type_ref(struct _header_content_type *ct);
const char *header_content_type_param(struct _header_content_type *t, const char *name);
void header_content_type_set_param(struct _header_content_type *t, const char *name, const char *value);
int header_content_type_is(struct _header_content_type *ct, const char *type, const char *subtype);
char *header_content_type_format(struct _header_content_type *ct);

/* DEBUGGING function */
void header_content_type_dump(struct _header_content_type *ct);

/* Content-Disposition header */
CamelMimeDisposition *header_disposition_decode(const char *in);
void header_disposition_ref(CamelMimeDisposition *);
void header_disposition_unref(CamelMimeDisposition *);
char *header_disposition_format(CamelMimeDisposition *d);

/* decode the contents of a content-encoding header */
char *header_content_encoding_decode(const char *in);

/* raw headers */
void header_raw_append(struct _header_raw **list, const char *name, const char *value, int offset);
void header_raw_append_parse(struct _header_raw **list, const char *header, int offset);
const char *header_raw_find(struct _header_raw **list, const char *name, int *ofset);
const char *header_raw_find_next(struct _header_raw **list, const char *name, int *ofset, const char *last);
void header_raw_replace(struct _header_raw **list, const char *name, const char *value, int offset);
void header_raw_remove(struct _header_raw **list, const char *name);
void header_raw_clear(struct _header_raw **list);

/* decode a header which is a simple token */
char *header_token_decode(const char *in);

/* decode/encode a string type, like a subject line */
char *header_decode_string(const char *in);
char *header_encode_string(const unsigned char *in);

/* decode an email date field into a GMT time, + optional offset */
time_t header_decode_date(const char *in, int *saveoffset);
char *header_format_date(time_t time, int offset);

/* decode a message id */
char *header_msgid_decode(const char *in);

/* decode a References header */
struct _header_references *header_references_decode(const char *in);
void header_references_list_clear(struct _header_references **list);
void header_references_list_append_asis(struct _header_references **list, char *ref);
int header_references_list_size(struct _header_references **list);

/* decode the mime-type header */
void header_mime_decode(const char *in, int *maj, int *min);

/* do incremental base64/quoted-printable (de/en)coding */
int base64_decode_step(unsigned char *in, int len, unsigned char *out, int *state, unsigned int *save);

int base64_encode_step(unsigned char *in, int len, unsigned char *out, int *state, int *save);
int base64_encode_close(unsigned char *in, int inlen, unsigned char *out, int *state, int *save);

int uudecode_step (unsigned char *in, int len, unsigned char *out, int *state, guint32 *save, char *uulen);

int quoted_decode_step(unsigned char *in, int len, unsigned char *out, int *savestate, int *saveme);

int quoted_encode_step(unsigned char *in, int len, unsigned char *out, int *state, int *save);
int quoted_encode_close(unsigned char *in, int len, unsigned char *out, int *state, int *save);

#endif /* ! _CAMEL_MIME_UTILS_H */
