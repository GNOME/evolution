/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "camel-vee-summary.h"
#include "camel-folder.h"

#define d(x)

static CamelFolderSummaryClass *camel_vee_summary_parent;

static void
vee_message_info_free(CamelFolderSummary *s, CamelMessageInfo *info)
{
	CamelVeeMessageInfo *mi = (CamelVeeMessageInfo *)info;

	g_free(info->uid);
	camel_message_info_free(mi->real);
}

static CamelMessageInfo *
vee_message_info_clone(CamelFolderSummary *s, const CamelMessageInfo *mi)
{
	CamelVeeMessageInfo *to;
	const CamelVeeMessageInfo *from = (const CamelVeeMessageInfo *)mi;

	to = (CamelVeeMessageInfo *)camel_message_info_new(s);

	to->real = camel_message_info_clone(from->real);
	to->info.summary = s;

	return (CamelMessageInfo *)to;
}

static const void *
vee_info_ptr(const CamelMessageInfo *mi, int id)
{
	return camel_message_info_ptr(((CamelVeeMessageInfo *)mi)->real, id);
}

static guint32
vee_info_uint32(const CamelMessageInfo *mi, int id)
{
	return camel_message_info_uint32(((CamelVeeMessageInfo *)mi)->real, id);
}

static time_t
vee_info_time(const CamelMessageInfo *mi, int id)
{
	return camel_message_info_time(((CamelVeeMessageInfo *)mi)->real, id);
}

static gboolean
vee_info_user_flag(const CamelMessageInfo *mi, const char *id)
{
	return camel_message_info_user_flag(((CamelVeeMessageInfo *)mi)->real, id);
}

static const char *
vee_info_user_tag(const CamelMessageInfo *mi, const char *id)
{
	return camel_message_info_user_tag(((CamelVeeMessageInfo *)mi)->real, id);
}

static gboolean
vee_info_set_user_flag(CamelMessageInfo *mi, const char *name, gboolean value)
{
	int res = FALSE;

	if (mi->uid)
		res = camel_message_info_set_user_flag(((CamelVeeMessageInfo *)mi)->real, name, value);

	return res;
}

static gboolean
vee_info_set_user_tag(CamelMessageInfo *mi, const char *name, const char *value)
{
	int res = FALSE;

	if (mi->uid)
		res = camel_message_info_set_user_tag(((CamelVeeMessageInfo *)mi)->real, name, value);

	return res;
}

static gboolean
vee_info_set_flags(CamelMessageInfo *mi, guint32 flags, guint32 set)
{
	int res = FALSE;

	if (mi->uid)
		res = camel_message_info_set_flags(((CamelVeeMessageInfo *)mi)->real, flags, set);

	return res;
}

static void
camel_vee_summary_class_init (CamelVeeSummaryClass *klass)
{
	((CamelFolderSummaryClass *)klass)->message_info_clone = vee_message_info_clone;
	((CamelFolderSummaryClass *)klass)->message_info_free = vee_message_info_free;

	((CamelFolderSummaryClass *)klass)->info_ptr = vee_info_ptr;
	((CamelFolderSummaryClass *)klass)->info_uint32 = vee_info_uint32;
	((CamelFolderSummaryClass *)klass)->info_time = vee_info_time;
	((CamelFolderSummaryClass *)klass)->info_user_flag = vee_info_user_flag;
	((CamelFolderSummaryClass *)klass)->info_user_tag = vee_info_user_tag;

#if 0
	((CamelFolderSummaryClass *)klass)->info_set_string = vee_info_set_string;
	((CamelFolderSummaryClass *)klass)->info_set_uint32 = vee_info_set_uint32;
	((CamelFolderSummaryClass *)klass)->info_set_time = vee_info_set_time;
	((CamelFolderSummaryClass *)klass)->info_set_references = vee_info_set_references;
#endif
	((CamelFolderSummaryClass *)klass)->info_set_user_flag = vee_info_set_user_flag;
	((CamelFolderSummaryClass *)klass)->info_set_user_tag = vee_info_set_user_tag;

	((CamelFolderSummaryClass *)klass)->info_set_flags = vee_info_set_flags;
}

static void
camel_vee_summary_init (CamelVeeSummary *obj)
{
	CamelFolderSummary *s = (CamelFolderSummary *)obj;

	s->message_info_size = sizeof(CamelVeeMessageInfo);
	s->content_info_size = 0;
}

CamelType
camel_vee_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		camel_vee_summary_parent = (CamelFolderSummaryClass *)camel_folder_summary_get_type();

		type = camel_type_register(
			camel_folder_summary_get_type(), "CamelVeeSummary",
			sizeof (CamelVeeSummary),
			sizeof (CamelVeeSummaryClass),
			(CamelObjectClassInitFunc) camel_vee_summary_class_init,
			NULL,
			(CamelObjectInitFunc) camel_vee_summary_init,
			NULL);
	}

	return type;
}

/**
 * camel_vee_summary_new:
 * @parent: Folder its attached to.
 *
 * This will create a new CamelVeeSummary object and read in the
 * summary data from disk, if it exists.
 *
 * Return value: A new CamelVeeSummary object.
 **/
CamelFolderSummary *
camel_vee_summary_new(CamelFolder *parent)
{
	CamelVeeSummary *s;

	s = (CamelVeeSummary *)camel_object_new(camel_vee_summary_get_type());
	s->summary.folder = parent;

	return &s->summary;
}

CamelVeeMessageInfo *
camel_vee_summary_add(CamelVeeSummary *s, CamelMessageInfo *info, const char hash[8])
{
	CamelVeeMessageInfo *mi;
	char *vuid;
	const char *uid;

	uid = camel_message_info_uid(info);
	vuid = g_malloc(strlen(uid)+9);
	memcpy(vuid, hash, 8);
	strcpy(vuid+8, uid);
	mi = (CamelVeeMessageInfo *)camel_folder_summary_uid(&s->summary, vuid);
	if (mi) {
		d(printf("w:clash, we already have '%s' in summary\n", vuid));
		camel_message_info_free((CamelMessageInfo *)mi);
		g_free(vuid);
		return NULL;
	}

	mi = (CamelVeeMessageInfo *)camel_message_info_new(&s->summary);
	mi->real = info;
	camel_message_info_ref(info);
	mi->info.uid = vuid;

	camel_folder_summary_add(&s->summary, (CamelMessageInfo *)mi);

	return mi;
}
