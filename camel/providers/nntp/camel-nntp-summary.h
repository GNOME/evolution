/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-utils.c : summary support for nntp groups. */

/* 
 *
 * Author : Chris Toshok <toshok@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code (http://www.helixcode.com).
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef NNTP_SUMMARY_H
#define NNTP_SUMMARY_H 1

#include <camel-folder-summary.h>

#define CAMEL_NNTP_SUMMARY_TYPE     (camel_nntp_summary_get_type ())
#define CAMEL_NNTP_SUMMARY(obj)     (GTK_CHECK_CAST((obj), CAMEL_NNTP_SUMMARY_TYPE, CamelNNTPSummary))
#define CAMEL_NNTP_SUMMARY_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_NNTP_SUMMARY_TYPE, CamelNNTPSummaryClass))
#define CAMEL_IS_NNTP_SUMMARY(o)    (GTK_CHECK_TYPE((o), CAMEL_NNTP_SUMMARY_TYPE))


#define CAMEL_NNTP_SUMMARY_VERSION 1


typedef struct {
	CamelMessageInfo headers;

	guint    size;
	guchar   status;

} CamelNNTPSummaryInformation;


/* this contains informations about the whole nntp file */
typedef struct {
	CamelFolderSummary parent_object;

	guint first_message;    /* the first message number in the summary */
	guint nb_message;	/* number of messages in the summary	*/

	GArray *message_info;	/* array of CamelNNTPSummaryInformation	*/

} CamelNNTPSummary;

typedef struct {
	CamelFolderSummaryClass parent_class;

} CamelNNTPSummaryClass;


GtkType camel_nntp_summary_get_type (void);

void camel_nntp_summary_save (CamelNNTPSummary *summary,
			      const gchar *filename, CamelException *ex);
CamelNNTPSummary *
camel_nntp_summary_load (const gchar *newsgroup, const gchar *filename, CamelException *ex);

gboolean camel_nntp_summary_check_sync (gchar *summary_filename,
					gchar *nntp_filename,
					CamelException *ex);

void camel_nntp_summary_append_entries (CamelNNTPSummary *summary,
					GArray *entries);


#endif /* NNTP_SUMMARY_H */
