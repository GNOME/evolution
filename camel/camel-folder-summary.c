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

#include "camel-folder-summary.h"

#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-index.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-save.h>
#include <camel/camel-mime-filter-basic.h>
#include "hash-table-utils.h"

#define d(x)

#define CAMEL_FOLDER_SUMMARY_VERSION (3)

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
static void camel_folder_summary_finalise   (GtkObject *obj);

static GtkObjectClass *camel_folder_summary_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_folder_summary_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelFolderSummary",
			sizeof (CamelFolderSummary),
			sizeof (CamelFolderSummaryClass),
			(GtkClassInitFunc) camel_folder_summary_class_init,
			(GtkObjectInitFunc) camel_folder_summary_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
camel_folder_summary_class_init (CamelFolderSummaryClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	camel_folder_summary_parent = gtk_type_class (gtk_object_get_type ());

	object_class->finalize = camel_folder_summary_finalise;

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

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
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

static void
camel_folder_summary_finalise (GtkObject *obj)
{
	struct _CamelFolderSummaryPrivate *p;
	CamelFolderSummary *s = (CamelFolderSummary *)obj;

	p = _PRIVATE(obj);

	/* FIXME: free contents */
	g_ptr_array_free(s->messages, TRUE);

	g_hash_table_destroy(s->messages_uid);

	/* FIXME: free contents */
	g_hash_table_destroy(p->filter_charset);
	g_free(p);

	((GtkObjectClass *)(camel_folder_summary_parent))->finalize((GtkObject *)obj);
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
	CamelFolderSummary *new = CAMEL_FOLDER_SUMMARY ( gtk_type_new (camel_folder_summary_get_type ()));
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

	if (p->index)
		ibex_write(p->index);
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

void
camel_folder_summary_set_uid(CamelFolderSummary *s, guint32 base)
{
	if (s->nextuid <= base)
		s->nextuid = base+1;
}

guint32 camel_folder_summary_next_uid(CamelFolderSummary *s)
{
	guint32 uid = s->nextuid++;

	/* FIXME: sync this to disk */
/*	summary_header_save(s);*/
	return uid;
}

/* loads the content descriptions, recursively */
static CamelMessageContentInfo *
perform_content_info_load(CamelFolderSummary *s, FILE *in)
{
	int i;
	guint32 count;
	CamelMessageContentInfo *ci, *part;

	ci = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->content_info_load(s, in);
	camel_folder_summary_decode_uint32(in, &count);
	for (i=0;i<count;i++) {
		part = perform_content_info_load(s, in);
		if (part) {
			my_list_append((struct _node **)&ci->childs, (struct _node *)ci);
			ci->parent = ci;
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

	if ( ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->summary_header_load(s, in) == -1) {
		fclose(in);
		return -1;
	}

	/* now read in each message ... */
	/* FIXME: check returns */
	for (i=0;i<s->saved_count;i++) {
		mi = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->message_info_load(s, in);

		if (s->build_content) {
			mi->content = content_info_load(s, in);
		}
	}

	/* FIXME: check error return */
	return 0;
}

/* saves the content descriptions, recursively */
static int
perform_content_info_save(CamelFolderSummary *s, FILE *out, CamelMessageContentInfo *ci)
{
	CamelMessageContentInfo *part;

	((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->content_info_save(s, out, ci);
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

	fd = open(s->summary_path, O_RDWR|O_CREAT, 0600);
	if (fd == -1)
		return -1;
	out = fdopen(fd, "w");
	if ( out == NULL ) {
		close(fd);
		return -1;
	}

	if ( ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->summary_header_save(s, out) == -1) {
		fclose(out);
		return -1;
	}

	/* now write out each message ... */
	/* FIXME: check returns */
	count = camel_folder_summary_count(s);
	for (i=0;i<count;i++) {
		mi = camel_folder_summary_index(s, i);
		((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->message_info_save(s, out, mi);

		if (s->build_content) {
			perform_content_info_save(s, out, mi->content);
		}
	}
	return fclose(out);
}

void camel_folder_summary_add(CamelFolderSummary *s, CamelMessageInfo *info)
{
	if (info == NULL)
		return;
retry:
	if (info->uid == NULL) {
		info->uid = g_strdup_printf("%u", s->nextuid++);
	}
	if (g_hash_table_lookup(s->messages_uid, info->uid)) {
		g_warning("Trying to insert message with clashing uid.  new uid re-assigned");
		g_free(info->uid);
		info->uid = NULL;
		goto retry;
	}

	g_ptr_array_add(s->messages, info);
	g_hash_table_insert(s->messages_uid, info->uid, info);
	s->flags |= CAMEL_SUMMARY_DIRTY;
}

void camel_folder_summary_add_from_header(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *info;

	info = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->message_info_new(s, h);
	camel_folder_summary_add(s, info);
}

void camel_folder_summary_add_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *info;
	char *buffer;
	int len;
	struct _CamelFolderSummaryPrivate *p = _PRIVATE(s);

	/* should this check the parser is in the right state, or assume it is?? */

	if (camel_mime_parser_step(mp, &buffer, &len) != HSCAN_EOF) {
		info = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->message_info_new_from_parser(s, mp);

		camel_mime_parser_unstep(mp);

		/* FIXME: better uid assignment method? */
		if (info->uid == NULL) {
			info->uid = g_strdup_printf("%u", s->nextuid++);
		}

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

		camel_folder_summary_add(s, info);
	}
}

int
camel_folder_summary_encode_uint32(FILE *out, guint32 value)
{
	int i;

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
        return 0;
}

int
camel_folder_summary_encode_fixed_int32(FILE *out, gint32 value)
{
	guint32 save;

	save = htonl(value);
	return fwrite(&save, sizeof(save), 1, out);
}

int
camel_folder_summary_decode_fixed_int32(FILE *in, gint32 *dest)
{
	guint32 save;

	if (fread(&save, sizeof(save), 1, in) != -1) {
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
	"quoted-printable",
	"rfc822",
	"text",
	"us-ascii",		/* 23 words */
};

#define tokens_len (sizeof(tokens)/sizeof(tokens[0]))

/* baiscally ...
    0 = null
    1-tokens_len == tokens[id-1]
    >=32 string, length = n-32
*/

int
camel_folder_summary_encode_token(FILE *out, char *str)
{
	if (str == NULL) {
		return camel_folder_summary_encode_uint32(out, 0);
	} else {
		int len = strlen(str);
		int i, token=-1;

		if (len <= 16) {
			char lower[32];

			for (i=0;i<len;i++)
				lower[i] = tolower(str[i]);
			lower[i] = 0;
			for (i=0;i<tokens_len;i++) {
				if (!strcmp(tokens[i], lower)) {
					token = i;
					break;
				}
			}
		}
		if (token != -1) {
			return camel_folder_summary_encode_uint32(out, token+1);
		} else {
			if (camel_folder_summary_encode_uint32(out, len+32) == -1)
				return -1;
			return fwrite(str, len, 1, out);
		}
	}
	return 0;
}

int
camel_folder_summary_decode_token(FILE *in, char **str)
{
	char *ret;
	int len;
	
	if (camel_folder_summary_decode_uint32(in, &len) == -1) {
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
		if (fread(ret, len, 1, in) == -1) {
			g_free(ret);
			*str = NULL;
			return -1;
		}
		ret[len]=0;
	}

	*str = ret;
	return 0;
}

int
camel_folder_summary_encode_string(FILE *out, char *str)
{
	register int len;

	if (str == NULL)
		return camel_folder_summary_encode_uint32(out, 0);

	len = strlen(str);
	if (camel_folder_summary_encode_uint32(out, len+1) == -1)
		return -1;
	return fwrite(str, len, 1, out);	
}


int
camel_folder_summary_decode_string(FILE *in, char **str)
{
	int len;
	register char *ret;

	if (camel_folder_summary_decode_uint32(in, &len) == -1) {
		*str = NULL;
		return -1;
	}

	len--;
	if (len < 0) {
		*str = NULL;
		return -1;
	}

	ret = g_malloc(len+1);
	if (fread(ret, len, 1, in) == -1) {
		g_free(ret);
		*str = NULL;
		return -1;
	}

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
	guint32 version, flags, nextuid, count;
	time_t time;

	fseek(in, 0, SEEK_SET);

	if (camel_folder_summary_decode_fixed_int32(in, &version) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &flags) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &nextuid) == -1
	    || camel_folder_summary_decode_fixed_int32(in, &time) == -1	/* TODO: yes i know this warns, to be fixed later */
	    || camel_folder_summary_decode_fixed_int32(in, &count) == -1) {
		return -1;
	}

	s->nextuid = nextuid;
	s->flags = flags;
	s->time = time;
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
		mi = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->message_info_new(s, camel_mime_parser_headers_raw(mp));
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
		ci = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->content_info_new(s, camel_mime_parser_headers_raw(mp));
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

static char *
summary_format_address(struct _header_raw *h, const char *name)
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

static CamelMessageInfo *
message_info_new(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *mi;

	mi = g_malloc0(s->message_info_size);

	mi->subject = header_decode_string(header_raw_find(&h, "subject", NULL));
	mi->from = summary_format_address(h, "from");
	mi->to = summary_format_address(h, "to");
	mi->date_sent = header_decode_date(header_raw_find(&h, "date", NULL), NULL);
	mi->date_received = 0;

	return mi;
}


static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	guint32 tmp;
	CamelMessageInfo *mi;

	mi = g_malloc0(s->message_info_size);

	camel_folder_summary_decode_uint32(in, &tmp);
	mi->uid = g_strdup_printf("%u", tmp);
	camel_folder_summary_decode_uint32(in, &mi->flags);
	camel_folder_summary_decode_uint32(in, &mi->date_sent);	/* warnings, leave them here */
	camel_folder_summary_decode_uint32(in, &mi->date_received);
/*	ms->xev_offset = camel_folder_summary_decode_uint32(in);*/
	camel_folder_summary_decode_string(in, &mi->subject);
	camel_folder_summary_decode_string(in, &mi->from);
	camel_folder_summary_decode_string(in, &mi->to);
	mi->content = NULL;

	return mi;
}

static int
message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *mi)
{
	camel_folder_summary_encode_uint32(out, strtoul(mi->uid, NULL, 10));
	camel_folder_summary_encode_uint32(out, mi->flags);
	camel_folder_summary_encode_uint32(out, mi->date_sent);
	camel_folder_summary_encode_uint32(out, mi->date_received);
/*	camel_folder_summary_encode_uint32(out, ms->xev_offset);*/
	camel_folder_summary_encode_string(out, mi->subject);
	camel_folder_summary_encode_string(out, mi->from);
	return camel_folder_summary_encode_string(out, mi->to);
}

static void
message_info_free(CamelFolderSummary *s, CamelMessageInfo *mi)
{
	g_free(mi->uid);
	g_free(mi->subject);
	g_free(mi->from);
	g_free(mi->to);
	g_free(mi);
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
	guint32 count, i;
	struct _header_content_type *ct;

	ci = g_malloc0(s->content_info_size);

/*	bs->pos = decode_int(in);
	bs->bodypos = bs->pos + decode_int(in);
	bs->endpos = bs->pos + decode_int(in);*/

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

/*	camel_folder_summary_encode_uint32(out, bs->pos);
	camel_folder_summary_encode_uint32(out, bs->bodypos - bs->pos);
	camel_folder_summary_encode_uint32(out, bs->endpos - bs->pos);*/

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
	int start, body;
	int enc_id = -1, chr_id = -1, idx_id = -1;
	struct _CamelFolderSummaryPrivate *p = _PRIVATE(s);
	CamelMimeFilterCharset *mfc;
	CamelMessageContentInfo *part;

	/* start of this part */
	start = camel_mime_parser_tell(mp);
	state = camel_mime_parser_step(mp, &buffer, &len);
	body = camel_mime_parser_tell(mp);

	info = ((CamelFolderSummaryClass *)((GtkObject *)s)->klass)->content_info_new_from_parser(s, mp);

	info->pos = start;
	info->bodypos = body;

	switch(state) {
	case HSCAN_HEADER:
		/* check content type for indexing, then read body */
		ct = camel_mime_parser_content_type(mp);
		if (p->index && header_content_type_is(ct, "text", "*")) {
			char *encoding;
			const char *charset;
				
			encoding = header_content_encoding_decode(camel_mime_parser_header(mp, "content-transfer-encoding", NULL));
			if (encoding) {
				if (!strcasecmp(encoding, "base64")) {
					if (p->filter_64 == NULL)
						p->filter_64 = camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
					enc_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)p->filter_64);
				} else if (!strcasecmp(encoding, "quoted-printable")) {
					if (p->filter_qp == NULL)
						p->filter_qp = camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_DEC);
					enc_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)p->filter_qp);
				}
				g_free(encoding);
			}
				
			charset = header_content_type_param(ct, "charset");
			if (charset!=NULL
			    && !(strcasecmp(charset, "us-ascii")==0
				 || strcasecmp(charset, "utf-8")==0)) {
				d(printf("Adding conversion filter from %s to utf-8\n", charset));
				mfc = g_hash_table_lookup(p->filter_charset, charset);
				if (mfc == NULL) {
					mfc = camel_mime_filter_charset_new_convert(charset, "utf-8");
					if (mfc)
						g_hash_table_insert(p->filter_charset, g_strdup(charset), mfc);
				}
				if (mfc) {
					chr_id = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)mfc);
				} else {
					g_warning("Cannot convert '%s' to 'utf-8', message index may be corrupt", charset);
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
		part = summary_build_content_info(s, mp);
		if (part) {
			part->parent = info;
			my_list_append((struct _node **)&info->childs, (struct _node *)part);
		} else {
			g_error("Parsing failed: no content of a message?");
		}
		if (!(state == HSCAN_MESSAGE_END)) {
			g_error("Bad parser state: Expecing MESSAGE_END or MESSAGE_EOF, got: %d", state);
			camel_mime_parser_unstep(mp);
		}
		break;
	}

	info->endpos = camel_mime_parser_tell(mp);

	return info;
}
