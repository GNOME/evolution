/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright(C) 2000 Ximian Inc.
 *
 *  Authors:
 *    Michael Zucchi <notzed@ximian.com>
 *    Dan Winship <danw@ximian.com>
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

#include "camel-imapp-summary.h"
#include <camel/camel-file-utils.h>

#define CAMEL_IMAPP_SUMMARY_VERSION (1)

static int summary_header_load(CamelFolderSummary *, FILE *);
static int summary_header_save(CamelFolderSummary *, FILE *);

static CamelMessageInfo *message_info_load(CamelFolderSummary *s, FILE *in);
static int message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *info);

static void camel_imapp_summary_class_init(CamelIMAPPSummaryClass *klass);
static void camel_imapp_summary_init      (CamelIMAPPSummary *obj);

static CamelFolderSummaryClass *camel_imapp_summary_parent;

CamelType
camel_imapp_summary_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(
			camel_folder_summary_get_type(), "CamelIMAPPSummary",
			sizeof(CamelIMAPPSummary),
			sizeof(CamelIMAPPSummaryClass),
			(CamelObjectClassInitFunc) camel_imapp_summary_class_init,
			NULL,
			(CamelObjectInitFunc) camel_imapp_summary_init,
			NULL);
	}

	return type;
}

static void
camel_imapp_summary_class_init(CamelIMAPPSummaryClass *klass)
{
	CamelFolderSummaryClass *cfs_class =(CamelFolderSummaryClass *) klass;

	camel_imapp_summary_parent = CAMEL_FOLDER_SUMMARY_CLASS(camel_type_get_global_classfuncs(camel_folder_summary_get_type()));

	cfs_class->summary_header_load = summary_header_load;
	cfs_class->summary_header_save = summary_header_save;
	cfs_class->message_info_load = message_info_load;
	cfs_class->message_info_save = message_info_save;
}

static void
camel_imapp_summary_init(CamelIMAPPSummary *obj)
{
	CamelFolderSummary *s =(CamelFolderSummary *)obj;

	/* subclasses need to set the right instance data sizes */
	s->message_info_size = sizeof(CamelIMAPPMessageInfo);
	s->content_info_size = sizeof(CamelMessageContentInfo);

	/* and a unique file version */
	s->version += CAMEL_IMAPP_SUMMARY_VERSION;
}

/**
 * camel_imapp_summary_new:
 * @filename: the file to store the summary in.
 *
 * This will create a new CamelIMAPPSummary object and read in the
 * summary data from disk, if it exists.
 *
 * Return value: A new CamelIMAPPSummary object.
 **/
CamelFolderSummary *
camel_imapp_summary_new(void)
{
	CamelFolderSummary *summary = CAMEL_FOLDER_SUMMARY(camel_object_new(camel_imapp_summary_get_type()));

	return summary;
}


static int
summary_header_load(CamelFolderSummary *s, FILE *in)
{
	CamelIMAPPSummary *ims = CAMEL_IMAPP_SUMMARY(s);

	if (camel_imapp_summary_parent->summary_header_load(s, in) == -1)
		return -1;

	/* Legacy version */
	if (s->version == 0x100c)
		return camel_file_util_decode_uint32(in, &ims->uidvalidity);

	if (camel_file_util_decode_fixed_int32(in, &ims->version) == -1
	    || camel_file_util_decode_fixed_int32(in, &ims->uidvalidity) == -1)
		return -1;

	if (ims->version > CAMEL_IMAPP_SUMMARY_VERSION) {
		g_warning("Unkown summary version\n");
		errno = EINVAL;
		return -1;
	}

	return 0;
}	

static int
summary_header_save(CamelFolderSummary *s, FILE *out)
{
	CamelIMAPPSummary *ims = CAMEL_IMAPP_SUMMARY(s);

	if (camel_imapp_summary_parent->summary_header_save(s, out) == -1)
		return -1;

	if (camel_file_util_encode_fixed_int32(out, CAMEL_IMAPP_SUMMARY_VERSION) == -1
	    || camel_file_util_encode_fixed_int32(out, ims->uidvalidity) == -1)
		return -1;

	return 0;
}


static CamelMessageInfo *
message_info_load(CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *info;
	CamelIMAPPMessageInfo *iinfo;

	info = camel_imapp_summary_parent->message_info_load(s, in);
	if (info) {
		iinfo =(CamelIMAPPMessageInfo *)info;

		if (camel_file_util_decode_uint32(in, &iinfo->server_flags) == -1)
			goto error;
	}

	return info;
error:
	camel_message_info_free(info);
	return NULL;
}

static int
message_info_save(CamelFolderSummary *s, FILE *out, CamelMessageInfo *info)
{
	CamelIMAPPMessageInfo *iinfo =(CamelIMAPPMessageInfo *)info;

	if (camel_imapp_summary_parent->message_info_save(s, out, info) == -1)
		return -1;

	return camel_file_util_encode_uint32(out, iinfo->server_flags);
}
