/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-digest-summary.h"

#define CAMEL_DIGEST_SUMMARY_VERSION 0

static void camel_digest_summary_class_init (CamelDigestSummaryClass *klass);
static void camel_digest_summary_init       (CamelDigestSummary *obj);
static void camel_digest_summary_finalise   (CamelObject *obj);


static CamelFolderSummaryClass *parent_class = NULL;


CamelType
camel_digest_summary_get_type(void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (
			camel_folder_summary_get_type (),
			"CamelDigestSummary",
			sizeof (CamelDigestSummary),
			sizeof (CamelDigestSummaryClass),
			(CamelObjectClassInitFunc) camel_digest_summary_class_init,
			NULL,
			(CamelObjectInitFunc) camel_digest_summary_init,
			(CamelObjectFinalizeFunc) camel_digest_summary_finalise);
	}
	
	return type;
}

static void
camel_digest_summary_class_init (CamelDigestSummaryClass *klass)
{
	parent_class = CAMEL_FOLDER_SUMMARY_CLASS (camel_type_get_global_classfuncs (camel_folder_summary_get_type ()));
}

static void
camel_digest_summary_init (CamelDigestSummary *summary)
{
	CamelFolderSummary *s = (CamelFolderSummary *) summary;
	
	/* subclasses need to set the right instance data sizes */
	s->message_info_size = sizeof (CamelMessageInfo);
	s->content_info_size = sizeof (CamelMessageContentInfo);
	
	/* and a unique file version */
	s->version += CAMEL_DIGEST_SUMMARY_VERSION;
}

static void
camel_digest_summary_finalise (CamelObject *object)
{
	
}


CamelFolderSummary *
camel_digest_summary_new (void)
{
	return (CamelFolderSummary *) camel_object_new (camel_digest_summary_get_type ());
}
