/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors:
 *    Michael Zucchi <notzed@helixcode.com>
 *    Dan Winship <danw@helixcode.com>
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

#include "camel-imap-summary.h"
#include <camel/camel-mime-message.h>

#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define CAMEL_IMAP_SUMMARY_VERSION (0x300)

static int summary_header_load (CamelFolderSummary *, FILE *);
static int summary_header_save (CamelFolderSummary *, FILE *);

static CamelMessageInfo *message_info_load (CamelFolderSummary *s, FILE *in);
static int message_info_save (CamelFolderSummary *s, FILE *out,
			      CamelMessageInfo *info);
static CamelMessageContentInfo *content_info_load (CamelFolderSummary *s, FILE *in);
static int content_info_save (CamelFolderSummary *s, FILE *out,
			      CamelMessageContentInfo *info);

static void camel_imap_summary_class_init (CamelImapSummaryClass *klass);
static void camel_imap_summary_init       (CamelImapSummary *obj);

static CamelFolderSummaryClass *camel_imap_summary_parent;

CamelType
camel_imap_summary_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register(
			camel_folder_summary_get_type(), "CamelImapSummary",
			sizeof (CamelImapSummary),
			sizeof (CamelImapSummaryClass),
			(CamelObjectClassInitFunc) camel_imap_summary_class_init,
			NULL,
			(CamelObjectInitFunc) camel_imap_summary_init,
			NULL);
	}

	return type;
}

static void
camel_imap_summary_class_init (CamelImapSummaryClass *klass)
{
	CamelFolderSummaryClass *cfs_class = (CamelFolderSummaryClass *) klass;

	camel_imap_summary_parent = CAMEL_FOLDER_SUMMARY_CLASS (camel_type_get_global_classfuncs (camel_folder_summary_get_type()));

	cfs_class->summary_header_load = summary_header_load;
	cfs_class->summary_header_save = summary_header_save;
	cfs_class->message_info_load = message_info_load;
	cfs_class->message_info_save = message_info_save;
	cfs_class->content_info_load = content_info_load;
	cfs_class->content_info_save = content_info_save;
}

static void
camel_imap_summary_init (CamelImapSummary *obj)
{
	CamelFolderSummary *s = (CamelFolderSummary *)obj;

	/* subclasses need to set the right instance data sizes */
	s->message_info_size = sizeof(CamelImapMessageInfo);
	s->content_info_size = sizeof(CamelImapMessageContentInfo);

	/* and a unique file version */
	s->version += CAMEL_IMAP_SUMMARY_VERSION;
}

/**
 * camel_imap_summary_new:
 * @filename: the file to store the summary in.
 * @validity: the current UIDVALIDITY value of the folder
 *
 * This will create a new CamelImapSummary object and read in the
 * summary data from disk, if it exists and has the right UIDVALIDITY
 * value.
 *
 * Return value: A new CamelImapSummary object.
 **/
CamelFolderSummary *
camel_imap_summary_new (const char *filename, guint32 validity)
{
	CamelFolderSummary *summary = CAMEL_FOLDER_SUMMARY (
		camel_object_new (camel_imap_summary_get_type ()));
	CamelImapSummary *imap_summary = (CamelImapSummary *)summary;

	camel_folder_summary_set_build_content (summary, TRUE);
	camel_folder_summary_set_filename (summary, filename);

	if (camel_folder_summary_load (summary) == -1) {
		if (errno == ENOENT) {
			imap_summary->validity = validity;
			return summary;
		} else {
			/* FIXME: are there error conditions where this won't work? */
			camel_folder_summary_clear (summary);
			camel_folder_summary_touch (summary);
			
			return summary;
		}
	}
	
	if (imap_summary->validity != validity) {
		camel_folder_summary_clear (summary);
		imap_summary->validity = validity;
	}

	return summary;
}


static int
summary_header_load (CamelFolderSummary *s, FILE *in)
{
	CamelImapSummary *ims = CAMEL_IMAP_SUMMARY (s);

	if (camel_imap_summary_parent->summary_header_load (s, in) == -1)
		return -1;

	return camel_folder_summary_decode_uint32 (in, &ims->validity);
}

static int
summary_header_save (CamelFolderSummary *s, FILE *out)
{
	CamelImapSummary *ims = CAMEL_IMAP_SUMMARY(s);

	if (camel_imap_summary_parent->summary_header_save (s, out) == -1)
		return -1;

	return camel_folder_summary_encode_uint32 (out, ims->validity);
}


static CamelMessageInfo *
message_info_load (CamelFolderSummary *s, FILE *in)
{
	CamelMessageInfo *info;
	CamelImapMessageInfo *iinfo;

	info = camel_imap_summary_parent->message_info_load (s, in);
	if (info) {
		iinfo = (CamelImapMessageInfo *)info;

		if (camel_folder_summary_decode_uint32 (in, &iinfo->server_flags) == -1)
			goto error;
	}

	return info;
error:
	camel_folder_summary_info_free (s, info);
	return NULL;
}

static int
message_info_save (CamelFolderSummary *s, FILE *out, CamelMessageInfo *info)
{
	CamelImapMessageInfo *iinfo = (CamelImapMessageInfo *)info;

	if (camel_imap_summary_parent->message_info_save (s, out, info) == -1)
		return -1;

	return camel_folder_summary_encode_uint32 (out, iinfo->server_flags);
}


static CamelMessageContentInfo *
content_info_load (CamelFolderSummary *s, FILE *in)
{
	if (fgetc (in))
		return camel_imap_summary_parent->content_info_load (s, in);
	else
		return camel_folder_summary_content_info_new (s);
}

static int
content_info_save (CamelFolderSummary *s, FILE *out,
		   CamelMessageContentInfo *info)
{
	if (info->type) {
		fputc (1, out);
		return camel_imap_summary_parent->content_info_save (s, out, info);
	} else
		return fputc (0, out);
}
