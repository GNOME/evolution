/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
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

#include "camel-local-summary.h"
#include <camel/camel-mime-message.h>

#include <ctype.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define io(x)
#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

#define CAMEL_LOCAL_SUMMARY_VERSION (0x200)

struct _CamelLocalSummaryPrivate {
};

#define _PRIVATE(o) (((CamelLocalSummary *)(o))->priv)

#if 0
static int summary_header_load (CamelFolderSummary *, FILE *);
static int summary_header_save (CamelFolderSummary *, FILE *);
#endif

static CamelMessageInfo * message_info_new (CamelFolderSummary *, struct _header_raw *);
static CamelMessageInfo * message_info_new_from_parser (CamelFolderSummary *, CamelMimeParser *);
static CamelMessageInfo * message_info_new_from_message(CamelFolderSummary *s, CamelMimeMessage *msg);
#if 0
static CamelMessageInfo * message_info_load (CamelFolderSummary *, FILE *);
static int		  message_info_save (CamelFolderSummary *, FILE *, CamelMessageInfo *);
#endif
/*static void		  message_info_free (CamelFolderSummary *, CamelMessageInfo *);*/

static int local_summary_decode_x_evolution(CamelLocalSummary *cls, const char *xev, CamelMessageInfo *mi);
static char *local_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelMessageInfo *mi);

static int local_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex);
static int local_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static int local_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex);
static CamelMessageInfo *local_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *, CamelException *ex);

static void camel_local_summary_class_init (CamelLocalSummaryClass *klass);
static void camel_local_summary_init       (CamelLocalSummary *obj);
static void camel_local_summary_finalise   (CamelObject *obj);

static CamelFolderSummaryClass *camel_local_summary_parent;

CamelType
camel_local_summary_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(camel_folder_summary_get_type(), "CamelLocalSummary",
					   sizeof (CamelLocalSummary),
					   sizeof (CamelLocalSummaryClass),
					   (CamelObjectClassInitFunc) camel_local_summary_class_init,
					   NULL,
					   (CamelObjectInitFunc) camel_local_summary_init,
					   (CamelObjectFinalizeFunc) camel_local_summary_finalise);
	}
	
	return type;
}

static void
camel_local_summary_class_init(CamelLocalSummaryClass *klass)
{
	CamelFolderSummaryClass *sklass = (CamelFolderSummaryClass *) klass;
	
	camel_local_summary_parent = CAMEL_FOLDER_SUMMARY_CLASS(camel_type_get_global_classfuncs(camel_folder_summary_get_type()));

	/*sklass->summary_header_load = summary_header_load;
	  sklass->summary_header_save = summary_header_save;*/

	sklass->message_info_new  = message_info_new;
	sklass->message_info_new_from_parser = message_info_new_from_parser;
	sklass->message_info_new_from_message = message_info_new_from_message;

	/*sklass->message_info_load = message_info_load;
	  sklass->message_info_save = message_info_save;*/
	/*sklass->message_info_free = message_info_free;*/

	klass->load = local_summary_load;
	klass->check = local_summary_check;
	klass->sync = local_summary_sync;
	klass->add = local_summary_add;

	klass->encode_x_evolution = local_summary_encode_x_evolution;
	klass->decode_x_evolution = local_summary_decode_x_evolution;
}

static void
camel_local_summary_init(CamelLocalSummary *obj)
{
	struct _CamelLocalSummaryPrivate *p;
	struct _CamelFolderSummary *s = (CamelFolderSummary *)obj;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));

	/* subclasses need to set the right instance data sizes */
	s->message_info_size = sizeof(CamelMessageInfo);
	s->content_info_size = sizeof(CamelMessageContentInfo);

	/* and a unique file version */
	s->version += CAMEL_LOCAL_SUMMARY_VERSION;
}

static void
camel_local_summary_finalise(CamelObject *obj)
{
	CamelLocalSummary *mbs = CAMEL_LOCAL_SUMMARY(obj);

	g_free(mbs->folder_path);
}

void
camel_local_summary_construct(CamelLocalSummary *new, const char *filename, const char *local_name, ibex *index)
{
	camel_folder_summary_set_build_content(CAMEL_FOLDER_SUMMARY(new), FALSE);
	camel_folder_summary_set_filename(CAMEL_FOLDER_SUMMARY(new), filename);
	new->folder_path = g_strdup(local_name);
	new->index = index;
}

static int
local_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex)
{
	return camel_folder_summary_load((CamelFolderSummary *)cls);
}

/* load/check the summary */
int
camel_local_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex)
{
	struct stat st;
	CamelFolderSummary *s = (CamelFolderSummary *)cls;

	d(printf("Loading summary ...\n"));

	if (forceindex
	    || stat(s->summary_path, &st) == -1
	    || ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->load(cls, forceindex, ex) == -1) {
		camel_folder_summary_clear((CamelFolderSummary *)cls);
	}

	if (camel_local_summary_check(cls, NULL, ex) == 0) {
		if (camel_folder_summary_save(s) == -1)
			g_warning("Could not save summary for %s: %s", cls->folder_path, strerror(errno));
		if (cls->index && ibex_save(cls->index) == -1)
			g_warning("Could not sync index for %s: %s", cls->folder_path, strerror(errno));

		return 0;
	}
	return -1;
}

char *
camel_local_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelMessageInfo *info)
{
	return ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->encode_x_evolution(cls, info);
}

int
camel_local_summary_decode_x_evolution(CamelLocalSummary *cls, const char *xev, CamelMessageInfo *info)
{
	return ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->decode_x_evolution(cls, xev, info);
}

/*#define DOSTATS*/
#ifdef DOSTATS
struct _stat_info {
	int mitotal;
	int micount;
	int citotal;
	int cicount;
	int msgid;
	int msgcount;
};

static void
do_stat_ci(CamelLocalSummary *cls, struct _stat_info *info, CamelMessageContentInfo *ci)
{
	info->cicount++;
	info->citotal += ((CamelFolderSummary *)cls)->content_info_size /*+ 4 memchunks are 1/4 byte overhead per mi */;
	if (ci->id)
		info->citotal += strlen(ci->id) + 4;
	if (ci->description)
		info->citotal += strlen(ci->description) + 4;
	if (ci->encoding)
		info->citotal += strlen(ci->encoding) + 4;
	if (ci->type) {
		struct _header_content_type *ct = ci->type;
		struct _header_param *param;

		info->citotal += sizeof(*ct) + 4;
		if (ct->type)
			info->citotal += strlen(ct->type) + 4;
		if (ct->subtype)
			info->citotal += strlen(ct->subtype) + 4;
		param = ct->params;
		while (param) {
			info->citotal += sizeof(*param) + 4;
			if (param->name)
				info->citotal += strlen(param->name)+4;
			if (param->value)
				info->citotal += strlen(param->value)+4;
			param = param->next;
		}
	}
	ci = ci->childs;
	while (ci) {
		do_stat_ci(cls, info, ci);
		ci = ci->next;
	}
}

static void
do_stat_mi(CamelLocalSummary *cls, struct _stat_info *info, CamelMessageInfo *mi)
{
	info->micount++;
	info->mitotal += ((CamelFolderSummary *)cls)->content_info_size /*+ 4*/;

	if (mi->subject)
		info->mitotal += strlen(mi->subject) + 4;
	if (mi->to)
		info->mitotal += strlen(mi->to) + 4;
	if (mi->from)
		info->mitotal += strlen(mi->from) + 4;
	if (mi->cc)
		info->mitotal += strlen(mi->cc) + 4;
	if (mi->uid)
		info->mitotal += strlen(mi->uid) + 4;

	if (mi->references) {
		info->mitotal += (mi->references->size-1) * sizeof(CamelSummaryMessageID) + sizeof(CamelSummaryReferences) + 4;
		info->msgid += (mi->references->size) * sizeof(CamelSummaryMessageID);
		info->msgcount += mi->references->size;
	}

	/* dont have any user flags yet */

	if (mi->content) {
		do_stat_ci(cls, info, mi->content);
	}
}

#endif

int
camel_local_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	int ret;

	ret = ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->check(cls, changeinfo, ex);

#ifdef DOSTATS
	if (ret != -1) {
		int i;
		CamelFolderSummary *s = (CamelFolderSummary *)cls;
		struct _stat_info stats = { 0 };

		for (i=0;i<camel_folder_summary_count(s);i++) {
			CamelMessageInfo *info = camel_folder_summary_index(s, i);
			do_stat_mi(cls, &stats, info);
		}

		printf("\nMemory used by summary:\n\n");
		printf("Total of %d messages\n", camel_folder_summary_count(s)); 
		printf("Total: %d bytes (ave %f)\n", stats.citotal + stats.mitotal,
		       (double)(stats.citotal+stats.mitotal)/(double)camel_folder_summary_count(s));
		printf("Message Info: %d (ave %f)\n", stats.mitotal, (double)stats.mitotal/(double)stats.micount);
		printf("Content Info; %d (ave %f) count %d\n", stats.citotal, (double)stats.citotal/(double)stats.cicount, stats.cicount);
		printf("message id's: %d (ave %f) count %d\n", stats.msgid, (double)stats.msgid/(double)stats.msgcount, stats.msgcount);
	}
#endif
	return ret;
}

int
camel_local_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	return ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->sync(cls, expunge, changeinfo, ex);
}

CamelMessageInfo *
camel_local_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, CamelException *ex)
{
	return ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->add(cls, msg, info, ci, ex);
}

/**
 * camel_local_summary_write_headers:
 * @fd: 
 * @header: 
 * @xevline: 
 * 
 * Write a bunch of headers to the file @fd.  IF xevline is non NULL, then
 * an X-Evolution header line is created at the end of all of the headers.
 * The headers written are termianted with a blank line.
 * 
 * Return value: -1 on error, otherwise the number of bytes written.
 **/
int
camel_local_summary_write_headers(int fd, struct _header_raw *header, char *xevline)
{
	int outlen = 0, len;
	int newfd;
	FILE *out;

	/* dum de dum, maybe the whole sync function should just use stdio for output */
	newfd = dup(fd);
	if (newfd == -1)
		return -1;

	out = fdopen(newfd, "w");
	if (out == NULL) {
		close(newfd);
		errno = EINVAL;
		return -1;
	}

	while (header) {
		if (strcmp(header->name, "X-Evolution")) {
			len = fprintf(out, "%s:%s\n", header->name, header->value);
			if (len == -1) {
				fclose(out);
				return -1;
			}
			outlen += len;
		}
		header = header->next;
	}

	if (xevline) {
		len = fprintf(out, "X-Evolution: %s\n\n", xevline);
		if (len == -1) {
			fclose(out);
			return -1;
		}
		outlen += len;
	}

	if (fclose(out) == -1)
		return -1;

	return outlen;
}

#if 0
static int
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	CamelLocalSummary *mbs = CAMEL_LOCAL_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_local_summary_parent)->summary_header_load(s, in) == -1)
		return -1;

	return camel_folder_summary_decode_uint32(in, &mbs->folder_size);
}

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	CamelLocalSummary *mbs = CAMEL_LOCAL_SUMMARY(s);

	if (((CamelFolderSummaryClass *)camel_local_summary_parent)->summary_header_save(s, out) == -1)
		return -1;

	return camel_folder_summary_encode_uint32(out, mbs->folder_size);
}

static int
header_evolution_decode(const char *in, guint32 *uid, guint32 *flags)
{
        char *header;
	
        if (in && (header = header_token_decode(in))) {
                if (strlen (header) == strlen ("00000000-0000")
                    && sscanf (header, "%08x-%04x", uid, flags) == 2) {
                        g_free(header);
                        return *uid;
                }
                g_free(header);
        }

        return -1;
}

static char *
header_evolution_encode(guint32 uid, guint32 flags)
{
	return g_strdup_printf("%08x-%04x", uid, flags & 0xffff);
}
#endif

static int
local_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	/* FIXME: sync index here */
	return 0;
}

static int
local_summary_sync(CamelLocalSummary *cls, gboolean expunge, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	return 0;
}

static CamelMessageInfo *
local_summary_add(CamelLocalSummary *cls, CamelMimeMessage *msg, const CamelMessageInfo *info, CamelFolderChangeInfo *ci, CamelException *ex)
{
	CamelMessageInfo *mi;
	char *xev;

	d(printf("Adding message to summary\n"));
	
	mi = camel_folder_summary_add_from_message((CamelFolderSummary *)cls, msg);
	if (mi) {
		d(printf("Added, uid = %s\n", mi->uid));
		if (info) {
			CamelTag *tag = info->user_tags;
			CamelFlag *flag = info->user_flags;

			while (flag) {
				camel_flag_set(&mi->user_flags, flag->name, TRUE);
				flag = flag->next;
			}
			
			while (tag) {
				camel_tag_set(&mi->user_tags, tag->name, tag->value);
				tag = tag->next;
			}

			mi->flags = mi->flags | (info->flags & 0xffff);
		}
		mi->flags &= ~(CAMEL_MESSAGE_FOLDER_NOXEV|CAMEL_MESSAGE_FOLDER_FLAGGED);
		xev = camel_local_summary_encode_x_evolution(cls, mi);
		camel_medium_set_header((CamelMedium *)msg, "X-Evolution", xev);
		g_free(xev);
		camel_folder_change_info_add_uid(ci, camel_message_info_uid(mi));
	} else {
		d(printf("Failed!\n"));
		camel_exception_set(ex, 1, "Unable to add message to summary: unknown reason");
	}
	return mi;
}

static char *
local_summary_encode_x_evolution(CamelLocalSummary *cls, const CamelMessageInfo *mi)
{
	GString *out = g_string_new("");
	struct _header_param *params = NULL;
	GString *val = g_string_new("");
	CamelFlag *flag = mi->user_flags;
	CamelTag *tag = mi->user_tags;
	char *ret;
	const char *p, *uidstr;
	guint32 uid;

	/* FIXME: work out what to do with uid's that aren't stored here? */
	/* FIXME: perhaps make that a mbox folder only issue?? */
	p = uidstr = camel_message_info_uid(mi);
	while (*p && isdigit(*p))
		p++;
	if (*p == 0 && sscanf(uidstr, "%u", &uid) == 1) {
		g_string_sprintf(out, "%08x-%04x", uid, mi->flags & 0xffff);
	} else {
		g_string_sprintf(out, "%s-%04x", uidstr, mi->flags & 0xffff);
	}

	if (flag || tag) {
		val = g_string_new("");

		if (flag) {
			while (flag) {
				g_string_append(val, flag->name);
				if (flag->next)
					g_string_append_c(val, ',');
				flag = flag->next;
			}
			header_set_param(&params, "flags", val->str);
			g_string_truncate(val, 0);
		}
		if (tag) {
			while (tag) {
				g_string_append(val, tag->name);
				g_string_append_c(val, '=');
				g_string_append(val, tag->value);
				if (tag->next)
					g_string_append_c(val, ',');
				tag = tag->next;
			}
			header_set_param(&params, "tags", val->str);
		}
		g_string_free(val, TRUE);
		header_param_list_format_append(out, params);
		header_param_list_free(params);
	}
	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

static int
local_summary_decode_x_evolution(CamelLocalSummary *cls, const char *xev, CamelMessageInfo *mi)
{
	struct _header_param *params, *scan;
	guint32 uid, flags;
	char *header;
	int i;

	/* check for uid/flags */
	header = header_token_decode(xev);
	if (header && strlen(header) == strlen("00000000-0000")
	    && sscanf(header, "%08x-%04x", &uid, &flags) == 2) {
		char uidstr[20];
		if (mi) {
			sprintf(uidstr, "%u", uid);
			camel_message_info_set_uid(mi, g_strdup(uidstr));
			mi->flags = flags;
		}
	} else {
		g_free(header);
		return -1;
	}
	g_free(header);

	if (mi == NULL)
		return 0;

	/* check for additional data */	
	header = strchr(xev, ';');
	if (header) {
		params = header_param_list_decode(header+1);
		scan = params;
		while (scan) {
			if (!strcasecmp(scan->name, "flags")) {
				char **flagv = g_strsplit(scan->value, ",", 1000);

				for (i=0;flagv[i];i++) {
					camel_flag_set(&mi->user_flags, flagv[i], TRUE);
				}
				g_strfreev(flagv);
			} else if (!strcasecmp(scan->name, "tags")) {
				char **tagv = g_strsplit(scan->value, ",", 10000);
				char *val;

				for (i=0;tagv[i];i++) {
					val = strchr(tagv[i], '=');
					if (val) {
						*val++ = 0;
						camel_tag_set(&mi->user_tags, tagv[i], val);
						val[-1]='=';
					}
				}
				g_strfreev(tagv);
			}
			scan = scan->next;
		}
		header_param_list_free(params);
	}
	return 0;
}

static CamelMessageInfo *
message_info_new(CamelFolderSummary *s, struct _header_raw *h)
{
	CamelMessageInfo *mi;
	CamelLocalSummary *cls = (CamelLocalSummary *)s;

	mi = ((CamelFolderSummaryClass *)camel_local_summary_parent)->message_info_new(s, h);
	if (mi) {
		const char *xev;
		int doindex = FALSE;

		xev = header_raw_find(&h, "X-Evolution", NULL);
		if (xev==NULL || camel_local_summary_decode_x_evolution(cls, xev, mi) == -1) {
			/* to indicate it has no xev header */
			mi->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED | CAMEL_MESSAGE_FOLDER_NOXEV;
			camel_message_info_set_uid(mi, camel_folder_summary_next_uid_string(s));

			/* shortcut, no need to look it up in the index library */
			doindex = TRUE;
		}
		
		if (cls->index
		    && (doindex
			|| cls->index_force
			|| !ibex_contains_name(cls->index, (char *)camel_message_info_uid(mi)))) {
			d(printf("Am indexing message %s\n", camel_message_info_uid(mi)));
			camel_folder_summary_set_index(s, cls->index);
		} else {
			d(printf("Not indexing message %s\n", camel_message_info_uid(mi)));
			camel_folder_summary_set_index(s, NULL);
		}
	}
	
	return mi;
}

static CamelMessageInfo * message_info_new_from_message(CamelFolderSummary *s, CamelMimeMessage *msg)
{
	CamelMessageInfo *mi;
	/*CamelLocalSummary *cls = (CamelLocalSummary *)s;*/

	mi = ((CamelFolderSummaryClass *)camel_local_summary_parent)->message_info_new_from_message(s, msg);
#if 0
	if (mi) {

		if (mi->uid == NULL) {
			d(printf("no uid assigned yet, assigning one\n"));
			mi->uid = camel_folder_summary_next_uid_string(s);
			mi->flags |= CAMEL_MESSAGE_FOLDER_FLAGGED | CAMEL_MESSAGE_FOLDER_NOXEV;
		}

		if (cls->index
		    && (cls->index_force
			|| !ibex_contains_name(cls->index, mi->uid))) {
			d(printf("Am indexing message %s\n", mi->uid));
			camel_folder_summary_set_index(s, cls->index);
		} else {
			d(printf("Not indexing message %s\n", mi->uid));
			camel_folder_summary_set_index(s, NULL);
		}
	}
#endif
	return mi;
}

static CamelMessageInfo *
message_info_new_from_parser(CamelFolderSummary *s, CamelMimeParser *mp)
{
	CamelMessageInfo *mi;
	/*CamelLocalSummary *mbs = CAMEL_LOCAL_SUMMARY(s);*/

	mi = ((CamelFolderSummaryClass *)camel_local_summary_parent)->message_info_new_from_parser(s, mp);
	if (mi) {
#if 0
		/* do we want to index this message as we add it, as well? */
		if (mbs->index
		    && (mbs->index_force
			|| !ibex_contains_name(mbs->index, mi->uid))) {
			d(printf("Am indexing message %s\n", mi->uid));
			camel_folder_summary_set_index(s, mbs->index);
		} else {
			d(printf("Not indexing message %s\n", mi->uid));
			camel_folder_summary_set_index(s, NULL);
		}
#endif
	}
	
	return mi;
}

#if 0
static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *mi;

	io(printf("loading local message info\n"));

	mi = ((CamelFolderSummaryClass *)camel_local_summary_parent)->message_info_load(s, in);
	if (mi) {
		guint32 position;
		CamelLocalMessageInfo *mbi = (CamelLocalMessageInfo *)mi;

		camel_folder_summary_decode_uint32(in, &position);
		mbi->frompos = position;
	}
	
	return mi;
}

static int
message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *mi)
{
	CamelLocalMessageInfo *mbi = (CamelLocalMessageInfo *)mi;

	io(printf("saving local message info\n"));

	((CamelFolderSummaryClass *)camel_local_summary_parent)->message_info_save(s, out, mi);

	return camel_folder_summary_encode_uint32(out, mbi->frompos);
}
#endif
