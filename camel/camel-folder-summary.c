/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
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

#include <unistd.h>
#include <netinet/in.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "camel-folder-summary.h"

#include <camel/camel-mime-message.h>

#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-index.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-save.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-mime-message.h>
#include "hash-table-utils.h"

/* this should probably be conditional on it existing */
#define USE_BSEARCH

#define d(x)
#define io(x)			/* io debug */

#if 0
extern int strdup_count, malloc_count, free_count;
#endif

#define CAMEL_FOLDER_SUMMARY_VERSION (7)

struct _CamelFolderSummaryPrivate {
	GHashTable *filter_charset;	/* CamelMimeFilterCharset's indexed by source charset */

	CamelMimeFilterIndex *filter_index;
	CamelMimeFilterBasic *filter_64;
	CamelMimeFilterBasic *filter_qp;
	CamelMimeFilterSave *filter_save;

	ibex *index;
};

#define _PRIVATE(o) (((CamelFolderSummary *)(o))->priv)

/* trivial lists, just because ... */
struct _node {
	struct _node *next;
};

static struct _node *my_list_append(struct _node **list, struct _node *n);
static int my_list_size(struct _node **list);

static int summary_header_load(CamelFolderSummary *, FILE *);
static int summary_header_save(CamelFolderSummary *, FILE *);

static CamelMessageInfo * message_info_new(CamelFolderSummary *, struct _header_raw *);
static CamelMessageInfo * message_info_new_from_parser(CamelFolderSummary *, CamelMimeParser *);
static CamelMessageInfo * message_info_load(CamelFolderSummary *, FILE *);
static int		  message_info_save(CamelFolderSummary *, FILE *, CamelMessageInfo *);
static void		  message_info_free(CamelFolderSummary *, CamelMessageInfo *);

static CamelMessageContentInfo * content_info_new(CamelFolderSummary *, struct _header_raw *);
static CamelMessageContentInfo * content_info_new_from_parser(CamelFolderSummary *, CamelMimeParser *);
static CamelMessageContentInfo * content_info_load(CamelFolderSummary *, FILE *);
static int		         content_info_save(CamelFolderSummary *, FILE *, CamelMessageContentInfo *);
static void		         content_info_free(CamelFolderSummary *, CamelMessageContentInfo *);

static CamelMessageContentInfo * summary_build_content_info(CamelFolderSummary *s, CamelMimeParser *mp);

static void camel_folder_summary_class_init (CamelFolderSummaryClass *klass);
static void camel_folder_summary_init       (CamelFolderSummary *obj);
static void camel_folder_summary_finalize   (CamelObject *obj);

static CamelObjectClass *camel_folder_summary_parent;

static void
camel_folder_summary_class_init (CamelFolderSummaryClass *klass)
{
	camel_folder_summary_parent = camel_type_get_global_classfuncs (camel_object_get_type ());

	klass->summary_header_load = summary_header_load;
	klass->summary_header_save = summary_header_save;

	klass->message_info_new  = message_info_new;
	klass->message_info_new_from_parser = message_info_new_from_parser;
	klass->message_info_load = message_info_load;
	klass->message_info_save = message_info_save;
	klass->message_info_free = message_info_free;

	klass->content_info_new  = content_info_new;
	klass->content_info_new_from_parser = content_info_new_from_parser;
	klass->content_info_load = content_info_load;
	klass->content_info_save = content_info_save;
	klass->content_info_free = content_info_free;
}

static void
camel_folder_summary_init (CamelFolderSummary *s)
{
	struct _CamelFolderSummaryPrivate *p;

	p = _PRIVATE(s) = g_malloc0(sizeof(*p));

	p->filter_charset = g_hash_table_new(g_strcase_hash, g_strcase_equal);

	s->message_info_size = sizeof(CamelMessageInfo);
	s->content_info_size = sizeof(CamelMessageContentInfo);

	s->version = CAMEL_FOLDER_SUMMARY_VERSION;
	s->flags = 0;
	s->time = 0;
	s->nextuid = 1;

	s->messages = g_ptr_array_new();
	s->messages_uid = g_hash_table_new(g_str_hash, g_str_equal);
}

static void free_o_name(void *key, void *value, void *data)
{
	camel_object_unref((CamelObject *)value);
	g_free(key);
}

static void
camel_folder_summary_finalize (CamelObject *obj)
{
	struct _CamelFolderSummaryPrivate *p;
	CamelFolderSummary *s = (CamelFolderSummary *)obj;

	p = _PRIVATE(obj);

	camel_folder_summary_clear(s);
	g_ptr_array_free(s->messages, TRUE);
	g_hash_table_destroy(s->messages_uid);

	g_hash_table_foreach(p->filter_charset, free_o_name, 0);
	g_hash_table_destroy(p->filter_charset);

	g_free(s->summary_path);

	if (p->filter_index)
		camel_object_unref ((CamelObject *)p->filter_index);
	if (p->filter_64)
		camel_object_unref ((CamelObject *)p->filter_64);
	if (p->filter_qp)
		camel_object_unref ((CamelObject *)p->filter_qp);
	if (p->filter_save)
		camel_object_unref ((CamelObject *)p->filter_save);

	g_free(p);
}

CamelType
camel_folder_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (), "CamelFolderSummary",
					    sizeof (CamelFolderSummary),
					    sizeof (CamelFolderSummaryClass),
					    (CamelObjectClassInitFunc) camel_folder_summary_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_folder_summary_init,
					    (CamelObjectFinalizeFunc) camel_folder_summary_finalize);
	}
	
	return type;
}

/**
 * camel_folder_summary_new:
 *
 * Create a new CamelFolderSummary object.
 * 
 * Return value: A new CamelFolderSummary widget.
 **/
CamelFolderSummary *
camel_folder_summary_new (void)
{
	CamelFolderSummary *new = CAMEL_FOLDER_SUMMARY ( camel_object_new (camel_folder_summary_get_type ()));
	return new;
}


void camel_folder_summary_set_filename(CamelFolderSummary *s, const char *name)
{
	g_free(s->summary_path);
	s->summary_path = g_strdup(name);
}

void camel_folder_summary_set_index(CamelFolderSummary *s, ibex *index)
{
	struct _CamelFolderSummaryPrivate *p = _PRIVATE(s);

	p->index = index;
}

void camel_folder_summary_set_build_content(CamelFolderSummary *s, gboolean state)
{
	s->build_content = state;
}

int
camel_folder_summary_count(CamelFolderSummary *s)
{
	return s->messages->len;
}

CamelMessageInfo *
camel_folder_summary_index(CamelFolderSummary *s, int i)
{
	if (i<s->messages->len)
		return g_ptr_array_index(s->messages, i);
	return NULL;
}

CamelMessageInfo *
camel_folder_summary_uid(CamelFolderSummary *s, const char *uid)
{
	return g_hash_table_lookup(s->messages_uid, uid);
}

guint32 camel_folder_summary_next_uid(CamelFolderSummary *s)
{
	guint32 uid = s->nextuid++;

	/* FIXME: sync this to disk */
/*	summary_header_save(s);*/
	return uid;
}

char *
camel_folder_summary_next_uid_string (CamelFolderSummary *s)
{
	return g_strdup_printf ("%u", camel_folder_summary_next_uid (s));
}

/* loads the content descriptions, recursively */
static CamelMessageContentInfo *
perform_content_info_load(CamelFolderSummary *s, FILE *in)
{
	int i;
	guint32 count;
	CamelMessageContentInfo *ci, *part;

	ci = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->content_info_load(s, in);
	camel_folder_summary_decode_uint32(in, &count);
	for (i=0;i<count;i++) {
		part = perform_content_info_load(s, in);
		if (part) {
			my_list_append((struct _node **)&ci->childs, (struct _node *)part);
			part->parent = ci;
		} else {
			g_warning("Summary file format messed up?");
		}
	}
	return ci;
}

int
camel_folder_summary_load(CamelFolderSummary *s)
{
	FILE *in;
	int i;
	CamelMessageInfo *mi;

	g_assert(s->summary_path);

	in = fopen(s->summary_path, "r");
	if ( in == NULL ) {
		return -1;
	}

	if ( ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_load(s, in) == -1) {
		fclose(in);
		return -1;
	}

	/* now read in each message ... */
	/* FIXME: check returns */
	for (i=0;i<s->saved_count;i++) {
		mi = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->message_info_load(s, in);

		if (s->build_content) {
			mi->content = perform_content_info_load(s, in);
		}

		camel_folder_summary_add(s, mi);
	}
	
	if (fclose(in) == -1)
		return -1;

	s->flags &= ~CAMEL_SUMMARY_DIRTY;

	return 0;
}

/* saves the content descriptions, recursively */
static int
perform_content_info_save(CamelFolderSummary *s, FILE *out, CamelMessageContentInfo *ci)
{
	CamelMessageContentInfo *part;

	((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->content_info_save(s, out, ci);
	camel_folder_summary_encode_uint32(out, my_list_size((struct _node **)&ci->childs));
	part = ci->childs;
	while (part) {
		perform_content_info_save(s, out, part);
		part = part->next;
	}
	return 0;
}

int
camel_folder_summary_save(CamelFolderSummary *s)
{
	FILE *out;
	int fd;
	int i;
	guint32 count;
	CamelMessageInfo *mi;

	g_assert(s->summary_path);

	if ((s->flags & CAMEL_SUMMARY_DIRTY) == 0)
		return 0;

	fd = open(s->summary_path, O_RDWR|O_CREAT, 0600);
	if (fd == -1)
		return -1;
	out = fdopen(fd, "w");
	if ( out == NULL ) {
		close(fd);
		return -1;
	}

	io(printf("saving header\n"));

	if ( ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->summary_header_save(s, out) == -1) {
		fclose(out);
		return -1;
	}

	/* now write out each message ... */
	/* FIXME: check returns */
	count = camel_folder_summary_count(s);
	for (i=0;i<count;i++) {
		mi = camel_folder_summary_index(s, i);
		((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->message_info_save(s, out, mi);

		if (s->build_content) {
			perform_content_info_save(s, out, mi->content);
		}
	}
	if (fclose(out) == -1)
		return -1;

	s->flags &= ~CAMEL_SUMMARY_DIRTY;
	return 0;
}

void camel_folder_summary_add(CamelFolderSummary *s, CamelMessageInfo *info)
{
	if (info == NULL)
		return;
retry:
	if (info->uid == NULL) {
		info->uid = camel_folder_summary_next_uid_string (s);
	}
	if (g_hash_table_lookup(s->messages_uid, info->uid)) {
		g_warning("Trying to insert message with clashing uid (%s).  new uid re-assigned", info->uid);
		g_free(info->uid);
		info->uid = NULL;
		info->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED;
		goto retry;
	}

	g_ptr_array_add(s->messages, info);
	g_hash_table_insert(s->messages_uid, info->uid, info);
	s->flags |= CAMEL_SUMMARY_DIRTY;
}

CamelMessageInfo *camel_folder_summary_add_from_header(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *info = NULL;

	info = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s))) -> message_info_new(s, h);
	camel_folder_summary_add(s, info);

	return info;
}

CamelMessageInfo *camel_folder_summary_add_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *info = NULL;
	char *buffer;
	int len;
	struct _CamelFolderSummaryPrivate *p = _PRIVATE(s);

	/* should this check the parser is in the right state, or assume it is?? */

	if (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_EOF) {
		info = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->message_info_new_from_parser(s, mp);

		camel_mime_parser_unstep(mp);

		camel_folder_summary_add(s, info);

		if (p->index) {
			if (p->filter_index == NULL)
				p->filter_index = camel_mime_filter_index_new_ibex(p->index);
			camel_mime_filter_index_set_name(p->filter_index, info->uid);
			ibex_unindex(p->index, info->uid);
		}

		/* build the content info, if we're supposed to */
		if (s->build_content) {
			info->content = summary_build_content_info(s, mp);
			if (info->content->pos != -1)
				info->size = info->content->endpos - info->content->pos;
		} else {
			camel_mime_parser_drop_step(mp);
		}
	}
	return info;
}

static void
perform_content_info_free(CamelFolderSummary *s, CamelMessageContentInfo *ci)
{
	CamelMessageContentInfo *pw, *pn;

	pw = ci->childs;
	((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->content_info_free(s, ci);
	while (pw) {
		pn = pw->next;
		perform_content_info_free(s, pw);
		pw = pn;
	}
}

void
camel_folder_summary_touch(CamelFolderSummary *s)
{
	s->flags |= CAMEL_SUMMARY_DIRTY;
}

void
camel_folder_summary_clear(CamelFolderSummary *s)
{
	int i;

	if (camel_folder_summary_count(s) == 0)
		return;

	for (i=0;i<camel_folder_summary_count(s);i++) {
		CamelMessageInfo *mi = camel_folder_summary_index(s, i);
		CamelMessageContentInfo *ci = mi->content;

		((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->message_info_free(s, mi);		
		if (s->build_content && ci) {
			perform_content_info_free(s, ci);
		}
	}

	g_ptr_array_set_size(s->messages, 0);
	g_hash_table_destroy(s->messages_uid);
	s->messages_uid = g_hash_table_new(g_str_hash, g_str_equal);
	s->flags |= CAMEL_SUMMARY_DIRTY;
}

void camel_folder_summary_remove(CamelFolderSummary *s, CamelMessageInfo *info)
{
	CamelMessageContentInfo *ci = info->content;

	g_hash_table_remove(s->messages_uid, info->uid);
	g_ptr_array_remove(s->messages, info);
	((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->message_info_free(s, info);
	if (s->build_content && ci) {
		perform_content_info_free(s, ci);
	}
	s->flags |= CAMEL_SUMMARY_DIRTY;
}

void camel_folder_summary_remove_uid(CamelFolderSummary *s, const char *uid)
{
        CamelMessageInfo *oldinfo;
        char *olduid;

        if (g_hash_table_lookup_extended(s->messages_uid, uid, (void *)&olduid, (void *)&oldinfo)) {
		camel_folder_summary_remove(s, oldinfo);
		g_free(olduid);
        }
}

int
camel_folder_summary_encode_uint32(FILE *out, guint32 value)
{
	int i;

	io(printf("Encoding int %u\n", value));

	for (i=28;i>0;i-=7) {
		if (value >= (1<<i)) {
			unsigned int c = (value>>i) & 0x7f;
			if (fputc(c, out) == -1)
				return -1;
		}
	}
	return fputc(value | 0x80, out);
}

int
camel_folder_summary_decode_uint32(FILE *in, guint32 *dest)
{
        gint32 value=0, v;

        /* until we get the last byte, keep decoding 7 bits at a time */
        while ( ((v = fgetc(in)) & 0x80) == 0 && v!=EOF) {
                value |= v;
                value <<= 7;
        }
	if (v == EOF) {
		*dest = value>>7;
		return 01;
	}
	*dest = value | (v&0x7f);

	io(printf("Decoding int %u\n", *dest));

        return 0;
}

int
camel_folder_summary_encode_fixed_int32(FILE *out, gint32 value)
{
	guint32 save;

	save = htonl(value);
	if (fwrite(&save, sizeof(save), 1, out) != 1)
		return -1;
	return 0;
}

int
camel_folder_summary_decode_fixed_int32(FILE *in, gint32 *dest)
{
	guint32 save;

	if (fread(&save, sizeof(save), 1, in) == 1) {
		*dest = ntohl(save);
		return 0;
	} else {
		return -1;
	}
}

/* should be sorted, for binary search */
/* This is a tokenisation mechanism for strings written to the
   summary - to save space.
   This list can have at most 31 words. */
static char * tokens[] = {
	"7bit",
	"8bit",
	"alternative",
	"application",
	"base64",
	"boundary",
	"charset",
	"filename",
	"html",
	"image",
	"iso-8859-1",
	"iso-8859-8",
	"message",
	"mixed",
	"multipart",
	"name",
	"octet-stream",
	"parallel",
	"plain",
	"postscript",
	"quoted-printable",
	"related",
	"rfc822",
	"text",
	"us-ascii",		/* 25 words */
};

#define tokens_len (sizeof(tokens)/sizeof(tokens[0]))

/* baiscally ...
    0 = null
    1-tokens_len == tokens[id-1]
    >=32 string, length = n-32
*/

#ifdef USE_BSEARCH
static int
token_search_cmp(char *key, char **index)
{
	d(printf("comparing '%s' to '%s'\n", key, *index));
	return strcmp(key, *index);
}
#endif

int
camel_folder_summary_encode_token(FILE *out, char *str)
{
	io(printf("Encoding token: '%s'\n", str));

	if (str == NULL) {
		return camel_folder_summary_encode_uint32(out, 0);
	} else {
		int len = strlen(str);
		int i, token=-1;

		if (len <= 16) {
			char lower[32];
			char **match;

			for (i=0;i<len;i++)
				lower[i] = tolower(str[i]);
			lower[i] = 0;
#ifdef USE_BSEARCH
			match = bsearch(lower, tokens, tokens_len, sizeof(char *), (int (*)(void *, void *))token_search_cmp);
			if (match)
				token = match-tokens;
#else
			for (i=0;i<tokens_len;i++) {
				if (!strcmp(tokens[i], lower)) {
					token = i;
					break;
				}
			}
#endif
		}
		if (token != -1) {
			return camel_folder_summary_encode_uint32(out, token+1);
		} else {
			if (camel_folder_summary_encode_uint32(out, len+32) == -1)
				return -1;
			if (fwrite(str, len, 1, out) != 1)
				return -1;
		}
	}
	return 0;
}

int
camel_folder_summary_decode_token(FILE *in, char **str)
{
	char *ret;
	guint32 len;

	io(printf("Decode token ...\n"));
	
	if (camel_folder_summary_decode_uint32(in, &len) == -1) {
		g_warning("Could not decode token from file");
		*str = NULL;
		return -1;
	}

	if (len<32) {
		if (len <= 0) {
			ret = NULL;
		} else if (len<= tokens_len) {
			ret = g_strdup(tokens[len-1]);
		} else {
			g_warning("Invalid token encountered: %d", len);
			*str = NULL;
			return -1;
		}
	} else if (len > 10240) {
		g_warning("Got broken string header length: %d bytes", len);
		*str = NULL;
		return -1;
	} else {
		len -= 32;
		ret = g_malloc(len+1);
		if (fread(ret, len, 1, in) != 1) {
			g_free(ret);
			*str = NULL;
			return -1;
		}
		ret[len]=0;
	}

	io(printf("Token = '%s'\n", ret));

	*str = ret;
	return 0;
}

int
camel_folder_summary_encode_string(FILE *out, char *str)
{
	register int len;

	io(printf("Encoding string: '%s'\n", str));

	if (str == NULL)
		return camel_folder_summary_encode_uint32(out, 0);

	len = strlen(str);
	if (camel_folder_summary_encode_uint32(out, len+1) == -1)
		return -1;
	if (fwrite(str, len, 1, out) == 1)
		return 0;
	return -1;
}


int
camel_folder_summary_decode_string(FILE *in, char **str)
{
	gint32 len;
	register char *ret;

	io(printf("Decode string ...\n", str));

	if (camel_folder_summary_decode_uint32(in, &len) == -1) {
		*str = NULL;
		return -1;
	}

	len--;
	if (len < 0) {
		*str = NULL;
		io(printf("String = '%s'\n", *str));
		return -1;
	}

	ret = g_malloc(len+1);
	if (fread(ret, len, 1, in) != 1) {
		g_free(ret);
		*str = NULL;
		return -1;
	}

	io(printf("String = '%s'\n", ret));

	ret[len] = 0;
	*str = ret;
	return 0;
}

void
camel_folder_summary_offset_content(CamelMessageContentInfo *content, off_t offset)
{
	content->pos += offset;
	content->bodypos += offset;
	content->endpos += offset;
	content = content->childs;
	while (content) {
		camel_folder_summary_offset_content(content, offset);
		content = content->next;
	}
}

static struct _node *
my_list_append(struct _node **list, struct _node *n)
{
	struct _node *ln = (struct _node *)list;
	while (ln->next)
		ln = ln->next;
	n->next = 0;
	ln->next = n;
	return n;
}

static int
my_list_size(struct _node **list)
{
	int len = 0;
	struct _node *ln = (struct _node *)list;
	while (ln->next) {
		ln = ln->next;
		len++;
	}
	return len;
}

static int
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	gint32 version, flags, nextuid, count, utime;

	fseek(in, 0, SEEK_SET);

	io(printf("Loading header\n"));

	if (camel_folder_summary_decode_fixed_int32(in, &version) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &flags) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &nextuid) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &utime) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &count) == -1) {
		return -1;
	}

	s->nextuid = nextuid;
	s->flags = flags;
	s->time = (time_t) utime;
	s->saved_count = count;
	if (s->version != version) {
		g_warning("Summary header version mismatch");
		return -1;
	}
	return 0;
}

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	fseek(out, 0, SEEK_SET);

	io(printf("Savining header\n"));

	camel_folder_summary_encode_fixed_int32(out, s->version);
	camel_folder_summary_encode_fixed_int32(out, s->flags);
	camel_folder_summary_encode_fixed_int32(out, s->nextuid);
	camel_folder_summary_encode_fixed_int32(out, s->time);
	return camel_folder_summary_encode_fixed_int32(out, camel_folder_summary_count(s));
}

/* are these even useful for anything??? */
static CamelMessageInfo * message_info_new_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *mi = NULL;
	int state;

	state = camel_mime_parser_state(mp);
	switch (state) {
	case HSCAN_HEADER:
	case HSCAN_MESSAGE:
	case HSCAN_MULTIPART:
		mi = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->message_info_new(s, camel_mime_parser_headers_raw(mp));
		break;
	default:
		g_error("Invalid parser state");
	}

	return mi;
}

static CamelMessageContentInfo * content_info_new_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageContentInfo *ci = NULL;

	switch (camel_mime_parser_state(mp)) {
	case HSCAN_HEADER:
	case HSCAN_MESSAGE:
	case HSCAN_MULTIPART:
		ci = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->content_info_new(s, camel_mime_parser_headers_raw(mp));
		if (ci) {
			ci->type = camel_mime_parser_content_type(mp);
			header_content_type_ref(ci->type);
		}
		break;
	default:
		g_error("Invalid parser state");
	}

	return ci;
}

char *
camel_summary_format_address(struct _header_raw *h, const char *name)
{
	struct _header_address *addr;
	const char *text;
	char *ret;

	text = header_raw_find(&h, name, NULL);
	addr = header_address_decode(text);
	if (addr) {
		ret = header_address_list_format(addr);
		header_address_list_clear(&addr);
	} else {
		ret = g_strdup(text);
	}
	return ret;
}

char *
camel_summary_format_string(struct _header_raw *h, const char *name)
{
	const char *text;

	text = header_raw_find(&h, name, NULL);
	if (text) {
		while (isspace(*text))
			text++;
		return header_decode_string(text);
	} else {
		return NULL;
	}
}

static CamelMessageInfo *
message_info_new(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	const char *received;

	mi = g_malloc0(s->message_info_size);

	mi->subject = camel_summary_format_string(h, "subject");
	mi->from = camel_summary_format_address(h, "from");
	mi->to = camel_summary_format_address(h, "to");
	mi->cc = camel_summary_format_address(h, "cc");
	mi->user_flags = NULL;
	mi->user_tags = NULL;
	mi->date_sent = header_decode_date(header_raw_find(&h, "date", NULL), NULL);
	received = header_raw_find(&h, "received", NULL);
	if (received)
		received = strrchr(received, ';');
	if (received)
		mi->date_received = header_decode_date(received + 1, NULL);
	else
		mi->date_received = 0;
	mi->message_id = header_msgid_decode(header_raw_find(&h, "message-id", NULL));
	/* if we have a references, use that, otherwise, see if we have an in-reply-to
	   header, with parsable content, otherwise *shrug* */
	mi->references = header_references_decode(header_raw_find(&h, "references", NULL));
	if (mi->references == NULL)
		mi->references = header_references_decode(header_raw_find(&h, "in-reply-to", NULL));
	return mi;
}


static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;
	guint32 udate_sent, udate_received;
	guint count;
	int i;

	mi = g_malloc0(s->message_info_size);

	io(printf("Loading message info\n"));

	camel_folder_summary_decode_string(in, &mi->uid);
	camel_folder_summary_decode_uint32(in, &mi->flags);
	camel_folder_summary_decode_uint32(in, &udate_sent);	/* warnings, leave them here */
	camel_folder_summary_decode_uint32(in, &udate_received);
/*	ms->xev_offset = camel_folder_summary_decode_uint32(in);*/
	camel_folder_summary_decode_string(in, &mi->subject);
	camel_folder_summary_decode_string(in, &mi->from);
	camel_folder_summary_decode_string(in, &mi->to);
	camel_folder_summary_decode_string(in, &mi->cc);
	mi->content = NULL;

	mi->date_sent = (time_t) udate_sent;
	mi->date_received = (time_t) udate_received;

	camel_folder_summary_decode_string(in, &mi->message_id);

	camel_folder_summary_decode_uint32(in, &count);
	for (i=0;i<count;i++) {
		char *id;
		camel_folder_summary_decode_string(in, &id);
		header_references_list_append_asis(&mi->references, id);
	}

	camel_folder_summary_decode_uint32(in, &count);
	for (i=0;i<count;i++) {
		char *name;
		camel_folder_summary_decode_string(in, &name);
		camel_flag_set(&mi->user_flags, name, TRUE);
		g_free(name);
	}

	camel_folder_summary_decode_uint32(in, &count);
	for (i=0;i<count;i++) {
		char *name, *value;
		camel_folder_summary_decode_string(in, &name);
		camel_folder_summary_decode_string(in, &value);
		camel_tag_set(&mi->user_tags, name, value);
		g_free(name);
		g_free(value);
	}

	return mi;
}

static int
message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *mi)
{
	guint32 count;
	CamelFlag *flag;
	CamelTag *tag;
	struct _header_references *refs;

	io(printf("Saving message info\n"));

	camel_folder_summary_encode_string(out, mi->uid);
	camel_folder_summary_encode_uint32(out, mi->flags);
	camel_folder_summary_encode_uint32(out, mi->date_sent);
	camel_folder_summary_encode_uint32(out, mi->date_received);
/*	camel_folder_summary_encode_uint32(out, ms->xev_offset);*/
	camel_folder_summary_encode_string(out, mi->subject);
	camel_folder_summary_encode_string(out, mi->from);
	camel_folder_summary_encode_string(out, mi->to);
	camel_folder_summary_encode_string(out, mi->cc);

	camel_folder_summary_encode_string(out, mi->message_id);

	count = header_references_list_size(&mi->references);
	camel_folder_summary_encode_uint32(out, count);
	refs = mi->references;
	while (refs) {
		camel_folder_summary_encode_string(out, refs->id);
		refs = refs->next;
	}

	count = camel_flag_list_size(&mi->user_flags);
	camel_folder_summary_encode_uint32(out, count);
	flag = mi->user_flags;
	while (flag) {
		camel_folder_summary_encode_string(out, flag->name);
		flag = flag->next;
	}

	count = camel_tag_list_size(&mi->user_tags);
	camel_folder_summary_encode_uint32(out, count);
	tag = mi->user_tags;
	while (tag) {
		camel_folder_summary_encode_string(out, tag->name);
		camel_folder_summary_encode_string(out, tag->value);
		tag = tag->next;
	}

	return ferror(out);
}

static void
message_info_free(CamelFolderSummary *s, CamelMessageInfo *mi)
{
	camel_message_info_free(mi);
}

static CamelMessageContentInfo *
content_info_new(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageContentInfo *ci;

	ci = g_malloc0(s->content_info_size);

	ci->id = header_msgid_decode(header_raw_find(&h, "content-id", NULL));
	ci->description = header_decode_string(header_raw_find(&h, "content-description", NULL));
	ci->encoding = header_content_encoding_decode(header_raw_find(&h, "content-transfer-encoding", NULL));

	ci->pos = -1;
	ci->bodypos = -1;
	ci->endpos = -1;
	return ci;
}

static CamelMessageContentInfo *
content_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageContentInfo *ci;
	char *type, *subtype;
	guint32 count, i, upos, ubodypos, uendpos;
	struct _header_content_type *ct;

	io(printf("Loading content info\n"));

	ci = g_malloc0(s->content_info_size);

	camel_folder_summary_decode_uint32(in, &upos);
	camel_folder_summary_decode_uint32(in, &ubodypos);
	camel_folder_summary_decode_uint32(in, &uendpos);

	ci->pos = (off_t) upos;
        ci->bodypos = (off_t) ubodypos;
	ci->endpos = (off_t) uendpos;
	
	camel_folder_summary_decode_token(in, &type);
	camel_folder_summary_decode_token(in, &subtype);
	ct = header_content_type_new(type, subtype);
	g_free(type);		/* can this be removed? */
	g_free(subtype);
	camel_folder_summary_decode_uint32(in, &count);
	for (i=0;i<count;i++) {
		char *name, *value;
		camel_folder_summary_decode_token(in, &name);
		camel_folder_summary_decode_token(in, &value);
		header_content_type_set_param(ct, name, value);
		/* TODO: do this so we dont have to double alloc/free */
		g_free(name);
		g_free(value);
	}
	ci->type = ct;

	camel_folder_summary_decode_token(in, &ci->id);
	camel_folder_summary_decode_token(in, &ci->description);
	camel_folder_summary_decode_token(in, &ci->encoding);

	ci->childs = NULL;
	return ci;
}

static int
content_info_save(CamelFolderSummary *s, FILE *out, CamelMessageContentInfo *ci)
{
	struct _header_content_type *ct;
	struct _header_param *hp;

	io(printf("Saving content info\n"));

	camel_folder_summary_encode_uint32(out, ci->pos);
	camel_folder_summary_encode_uint32(out, ci->bodypos);
	camel_folder_summary_encode_uint32(out, ci->endpos);

	ct = ci->type;
	if (ct) {
		camel_folder_summary_encode_token(out, ct->type);
		camel_folder_summary_encode_token(out, ct->subtype);
		camel_folder_summary_encode_uint32(out, my_list_size((struct _node **)&ct->params));
		hp = ct->params;
		while (hp) {
			camel_folder_summary_encode_token(out, hp->name);
			camel_folder_summary_encode_token(out, hp->value);
			hp = hp->next;
		}
	} else {
		camel_folder_summary_encode_token(out, NULL);
		camel_folder_summary_encode_token(out, NULL);
		camel_folder_summary_encode_uint32(out, 0);
	}
	camel_folder_summary_encode_token(out, ci->id);
	camel_folder_summary_encode_token(out, ci->description);
	return camel_folder_summary_encode_token(out, ci->encoding);
}

static void
content_info_free(CamelFolderSummary *s, CamelMessageContentInfo *ci)
{
	header_content_type_unref(ci->type);
	g_free(ci->id);
	g_free(ci->description);
	g_free(ci->encoding);
	g_free(ci);
}

/*
  OK
  Now this is where all the "smarts" happen, where the content info is built,
  and any indexing and what not is performed
*/

static CamelMessageContentInfo *
summary_build_content_info(CamelFolderSummary *s, CamelMimeParser *mp)
{
	int state, len;
	char *buffer;
	CamelMessageContentInfo *info = NULL;
	struct _header_content_type *ct;
	int body;
	int enc_id = -1, chr_id = -1, idx_id = -1;
	struct _CamelFolderSummaryPrivate *p = _PRIVATE(s);
	CamelMimeFilterCharset *mfc;
	CamelMessageContentInfo *part;

	d(printf("building content info\n"));

	/* start of this part */
	state = camel_mime_parser_step(mp, &buffer, &len);
	body = camel_mime_parser_tell(mp);

	info = ((CamelFolderSummaryClass *)(CAMEL_OBJECT_GET_CLASS(s)))->content_info_new_from_parser(s, mp);

	info->pos = camel_mime_parser_tell_start_headers(mp);
	info->bodypos = body;

	switch(state) {
	case HSCAN_HEADER:
		/* check content type for indexing, then read body */
		ct = camel_mime_parser_content_type(mp);
		if (p->index && header_content_type_is(ct, "text", "*")) {
			char *encoding;
			const char *charset;
			
			d(printf("generating index:\n"));
			
			encoding = header_content_encoding_decode(camel_mime_parser_header(mp, "content-transfer-encoding", NULL));
			if (encoding) {
				if (!strcasecmp(encoding, "base64")) {
					d(printf(" decoding base64\n"));
					if (p->filter_64 == NULL)
						p->filter_64 = camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
					enc_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)p->filter_64);
				} else if (!strcasecmp(encoding, "quoted-printable")) {
					d(printf(" decoding quoted-printable\n"));
					if (p->filter_qp == NULL)
						p->filter_qp = camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_DEC);
					enc_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)p->filter_qp);
				} else {
					d(printf(" ignoring encoding %s\n", encoding));
				}
				g_free(encoding);
			}
				
			charset = header_content_type_param(ct, "charset");
			if (charset!=NULL
			    && !(strcasecmp(charset, "us-ascii")==0
				 || strcasecmp(charset, "iso-8859-1")==0)) {
				d(printf(" Adding conversion filter from %s to iso-8859-1\n", charset));
				mfc = g_hash_table_lookup(p->filter_charset, charset);
				if (mfc == NULL) {
					mfc = camel_mime_filter_charset_new_convert(charset, "iso-8859-1");
					if (mfc)
						g_hash_table_insert(p->filter_charset, g_strdup(charset), mfc);
				}
				if (mfc) {
					chr_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)mfc);
				} else {
					g_warning("Cannot convert '%s' to 'iso-8859-1', message index may be corrupt", charset);
				}
			}

			/* and this filter actually does the indexing */
			idx_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)p->filter_index);
		}
		/* and scan/index everything */
		while (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_BODY_END)
			;
		/* and remove the filters */
		camel_mime_parser_filter_remove(mp, enc_id);
		camel_mime_parser_filter_remove(mp, chr_id);
		camel_mime_parser_filter_remove(mp, idx_id);
		break;
	case HSCAN_MULTIPART:
		d(printf("Summarising multipart\n"));
		while (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_MULTIPART_END) {
			camel_mime_parser_unstep(mp);
			part = summary_build_content_info(s, mp);
			if (part) {
				part->parent = info;
				my_list_append((struct _node **)&info->childs, (struct _node *)part);
			} else {
				g_error("Parsing failed: could not build part of a multipart");
			}
		}
		break;
	case HSCAN_MESSAGE:
		d(printf("Summarising message\n"));
		part = summary_build_content_info(s, mp);
		if (part) {
			part->parent = info;
			my_list_append((struct _node **)&info->childs, (struct _node *)part);
		} else {
			g_error("Parsing failed: no content of a message?");
		}
		state = camel_mime_parser_step(mp, &buffer, &len);
		if (state != HSCAN_MESSAGE_END) {
			g_error("Bad parser state: Expecing MESSAGE_END or MESSAGE_EOF, got: %d", state);
			camel_mime_parser_unstep(mp);
		}
		break;
	}

	info->endpos = camel_mime_parser_tell(mp);

	d(printf("finished building content info\n"));

	return info;
}

gboolean
camel_flag_get(CamelFlag **list, const char *name)
{
	CamelFlag *flag;
	flag = *list;
	while (flag) {
		if (!strcmp(flag->name, name))
			return TRUE;
		flag = flag->next;
	}
	return FALSE;
}

void
camel_flag_set(CamelFlag **list, const char *name, gboolean value)
{
	CamelFlag *flag, *tmp;

	/* this 'trick' works because flag->next is the first element */
	flag = (CamelFlag *)list;
	while (flag->next) {
		tmp = flag->next;
		if (!strcmp(flag->next->name, name)) {
			if (!value) {
				flag->next = tmp->next;
				g_free(tmp);
			}
			return;
		}
		flag = tmp;
	}

	if (value) {
		tmp = g_malloc(sizeof(*tmp) + strlen(name));
		strcpy(tmp->name, name);
		tmp->next = 0;
		flag->next = tmp;
	}
}

int
camel_flag_list_size(CamelFlag **list)
{
	int count=0;
	CamelFlag *flag;

	flag = *list;
	while (flag) {
		count++;
		flag = flag->next;
	}
	return count;
}

void
camel_flag_list_free(CamelFlag **list)
{
	CamelFlag *flag, *tmp;
	flag = *list;
	while (flag) {
		tmp = flag->next;
		g_free(flag);
		flag = tmp;
	}
	*list = NULL;
}

const char	*camel_tag_get(CamelTag **list, const char *name)
{
	CamelTag *tag;

	tag = *list;
	while (tag) {
		if (!strcmp(tag->name, name))
			return (const char *)tag->value;
		tag = tag->next;
	}
	return NULL;
}

void		camel_tag_set(CamelTag **list, const char *name, const char *value)
{
	CamelTag *tag, *tmp;

	/* this 'trick' works because tag->next is the first element */
	tag = (CamelTag *)list;
	while (tag->next) {
		tmp = tag->next;
		if (!strcmp(tmp->name, name)) {
			if (value == NULL) { /* clear it? */
				tag->next = tmp->next;
				g_free(tmp->value);
				g_free(tmp);
			} else if (strcmp(tmp->value, value)) { /* has it changed? */
				g_free(tmp->value);
				tmp->value = g_strdup(value);
			}
			return;
		}
		tag = tmp;
	}

	if (value) {
		tmp = g_malloc(sizeof(*tmp)+strlen(name));
		strcpy(tmp->name, name);
		tmp->value = g_strdup(value);
		tmp->next = 0;
		tag->next = tmp;
	}
}

int		camel_tag_list_size(CamelTag **list)
{
	int count=0;
	CamelTag *tag;

	tag = *list;
	while (tag) {
		count++;
		tag = tag->next;
	}
	return count;
}

void		camel_tag_list_free(CamelTag **list)
{
	CamelTag *tag, *tmp;
	tag = *list;
	while (tag) {
		tmp = tag->next;
		g_free(tag->value);
		g_free(tag);
		tag = tmp;
	}
	*list = NULL;
}

/**
 * camel_message_info_dup_to:
 * @from: source message info
 * @to: destination message info
 *
 * Duplicates the contents of one CamelMessageInfo structure into another.
 * (The destination is assumed to be empty: its contents are not freed.)
 * The slightly odd interface is to allow this to be used to initialize
 * "subclasses" of CamelMessageInfo.
 **/
void
camel_message_info_dup_to(const CamelMessageInfo *from, CamelMessageInfo *to)
{
	CamelFlag *flag;
	CamelTag *tag;

	/* Copy numbers */
	to->flags = from->flags;
	to->size = from->size;
	to->date_sent = from->date_sent;
	to->date_received = from->date_received;

	/* Copy strings */
	to->subject = g_strdup(from->subject);
	to->from = g_strdup(from->from);
	to->to = g_strdup(from->to);
	to->cc = g_strdup(from->cc);
	to->uid = g_strdup(from->uid);
	to->message_id = g_strdup(from->message_id);

	/* Copy structures */
	to->references = header_references_dup(from->references);
	flag = from->user_flags;
	while (flag) {
		camel_flag_set(&to->user_flags, flag->name, TRUE);
		flag = flag->next;
	}

	tag = from->user_tags;
	while (tag) {
		camel_tag_set(&to->user_tags, tag->name, tag->value);
		tag = tag->next;
	}

	/* FIXME some day */
	to->content = NULL;
}

/**
 * camel_message_info_free:
 * @mi: the message info
 *
 * Frees a CamelMessageInfo and its contents.
 **/
void
camel_message_info_free(CamelMessageInfo *mi)
{
	g_free(mi->uid);
	g_free(mi->subject);
	g_free(mi->from);
	g_free(mi->to);
	g_free(mi->cc);
	g_free(mi->message_id);
	header_references_list_clear(&mi->references);
	camel_flag_list_free(&mi->user_flags);
	camel_tag_list_free(&mi->user_tags);
	/* FIXME: content info? */
	g_free(mi);
}

#if 0
static void
content_info_dump(CamelMessageContentInfo *ci, int depth)
{
	char *p;

	p = alloca(depth*4+1);
	memset(p, ' ', depth*4);
	p[depth*4] = 0;

	if (ci == NULL) {
		printf("%s<empty>\n", p);
		return;
	}

	printf("%sconent-type: %s/%s\n", p, ci->type->type, ci->type->subtype);
	printf("%sontent-transfer-encoding: %s\n", p, ci->encoding);
	printf("%scontent-description: %s\n", p, ci->description);
	printf("%sbytes: %d %d %d\n", p, (int)ci->pos, (int)ci->bodypos, (int)ci->endpos);
	ci = ci->childs;
	while (ci) {
		content_info_dump(ci, depth+1);
		ci = ci->next;
	}
}

static void
message_info_dump(CamelMessageInfo *mi)
{
	if (mi == NULL) {
		printf("No message?\n");
		return;
	}

	printf("Subject: %s\n", mi->subject);
	printf("To: %s\n", mi->to);
	printf("Cc: %s\n", mi->cc);
	printf("From: %s\n", mi->from);
	printf("UID: %s\n", mi->uid);
	printf("Flags: %04x\n", mi->flags & 0xffff);
	content_info_dump(mi->content, 0);
}

int main(int argc, char **argv)
{
	CamelMimeParser *mp;
	int fd;
	CamelFolderSummary *s;
	char *buffer;
	int len;
	int i;
	ibex *index;

	/*g_tk_init(&argc, &argv);*/

#if 0
	{
		int i;
		char *s;
		char buf[1024];

		for (i=0;i<434712;i++) {
			memcpy(buf, "                                                         ", 50);
			buf[50] = 0;
#if 0
			s = g_strdup(buf);
			g_free(s);
#endif
		}
		return 0;
	}
#endif

	if (argc < 2 ) {
		printf("usage: %s mbox\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDONLY);

	index = ibex_open("index.ibex", O_CREAT|O_RDWR, 0600);

	mp = camel_mime_parser_new();
	camel_mime_parser_scan_from(mp, TRUE);
/*	camel_mime_parser_set_header_regex(mp, "^(content-[^:]*|subject|from|to|date):");*/
	camel_mime_parser_init_with_fd(mp, fd);

	s = camel_folder_summary_new();
	camel_folder_summary_set_build_content(s, TRUE);
/*	camel_folder_summary_set_index(s, index);*/

	while (camel_mime_parser_step(mp, &buffer, &len) == HSCAN_FROM) {
		/*printf("Parsing message ...\n");*/
		camel_folder_summary_add_from_parser(s, mp);
		if (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_FROM_END) {
			g_warning("Uknown state encountered, excpecting %d, got %d\n", HSCAN_FROM_END, camel_mime_parser_state(mp));
			break;
		}
	}

	printf("Printing summary\n");
	for (i=0;i<camel_folder_summary_count(s);i++) {
		message_info_dump(camel_folder_summary_index(s, i));
	}

	printf("Saivng summary\n");
	camel_folder_summary_set_filename(s, "index.summary");
	camel_folder_summary_save(s);

	{
		CamelFolderSummary *n;

		printf("\nLoading summary\n");
		n = camel_folder_summary_new();
		camel_folder_summary_set_build_content(n, TRUE);
		camel_folder_summary_set_filename(n, "index.summary");
		camel_folder_summary_load(n);

		printf("Printing summary\n");
		for (i=0;i<camel_folder_summary_count(n);i++) {
			message_info_dump(camel_folder_summary_index(n, i));
		}
		camel_object_unref(n);		
	}


	camel_object_unref(mp);
	camel_object_unref(s);

	printf("summarised %d messages\n", camel_folder_summary_count(s));
#if 0
	printf("g_strdup count = %d\n", strdup_count);
	printf("g_malloc count = %d\n", malloc_count);
	printf("g_free count = %d\n", free_count);
#endif
	return 0;
}

#endif
