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

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define io(x)
#define d(x) (printf("%s(%d): ", __FILE__, __LINE__),(x))

#define CAMEL_LOCAL_SUMMARY_VERSION (0x100)

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
	camel_folder_summary_set_build_content(CAMEL_FOLDER_SUMMARY(new), TRUE);
	camel_folder_summary_set_filename(CAMEL_FOLDER_SUMMARY(new), filename);
	new->folder_path = g_strdup(local_name);
	new->index = index;
}

/* load/check the summary */
int
camel_local_summary_load(CamelLocalSummary *cls, int forceindex, CamelException *ex)
{
	if (forceindex || camel_folder_summary_load((CamelFolderSummary *)cls) == -1) {
		camel_folder_summary_clear((CamelFolderSummary *)cls);
	}
	return camel_local_summary_check(cls, NULL, ex);
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

int
camel_local_summary_check(CamelLocalSummary *cls, CamelFolderChangeInfo *changeinfo, CamelException *ex)
{
	return ((CamelLocalSummaryClass *)(CAMEL_OBJECT_GET_CLASS(cls)))->check(cls, changeinfo, ex);
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
		xev = camel_local_summary_encode_x_evolution(cls, mi);
		camel_medium_set_header((CamelMedium *)msg, "X-Evolution", xev);
		g_free(xev);
		camel_folder_change_info_add_uid(ci, mi->uid);
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
	char *ret, *p;
	guint32 uid;

	/* FIXME: work out what to do with uid's that aren't stored here? */
	/* FIXME: perhaps make that a mbox folder only issue?? */
	p = mi->uid;
	while (*p && isdigit(*p))
		p++;
	if (*p == 0 && sscanf(mi->uid, "%u", &uid) == 1) {
		g_string_sprintf(out, "%08x-%04x", uid, mi->flags & 0xffff);
	} else {
		g_string_sprintf(out, "%s-%04x", mi->uid, mi->flags & 0xffff);
	}

	if (flag || tag) {
		g_string_append(out, "; ");
		val = g_string_new("");

		if (flag) {
			while (flag) {
				g_string_append(val, flag->name);
				if (flag->next)
					g_string_append_c(out, ',');
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
					g_string_append_c(out, ',');
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
		sprintf(uidstr, "%u", uid);
		g_free(mi->uid);
		mi->uid = g_strdup(uidstr);
		mi->flags = flags;
	} else {
		g_free(header);
		return -1;
	}
	g_free(header);

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
			mi->uid = camel_folder_summary_next_uid_string(s);

			/* shortcut, no need to look it up in the index library */
			doindex = TRUE;
		}
		
		if (cls->index
		    && (doindex
			|| cls->index_force
			|| !ibex_contains_name(cls->index, mi->uid))) {
			d(printf("Am indexing message %s\n", mi->uid));
			camel_folder_summary_set_index(s, cls->index);
		} else {
			d(printf("Not indexing message %s\n", mi->uid));
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
