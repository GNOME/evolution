/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : Bertrand Guiheneuf <bertrand@helixcode.com> 
 *
 * Copyright (C) 1999 Helix Code (http://www.helixcode.com).
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

#ifndef MBOX_SUMMARY_H
#define MBOX_SUMMARY_H 1

#include <camel-folder-summary.h>

#define CAMEL_MBOX_SUMMARY_TYPE     (camel_mbox_summary_get_type ())
#define CAMEL_MBOX_SUMMARY(obj)     (GTK_CHECK_CAST((obj), CAMEL_MBOX_SUMMARY_TYPE, CamelMboxSummary))
#define CAMEL_MBOX_SUMMARY_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MBOX_SUMMARY_TYPE, CamelMboxSummaryClass))
#define CAMEL_IS_MBOX_SUMMARY(o)    (GTK_CHECK_TYPE((o), CAMEL_MBOX_SUMMARY_TYPE))


#define CAMEL_MBOX_SUMMARY_VERSION 1


typedef struct {
	CamelMessageInfo headers;

	guint32  position;
	guint    size;
	guint    x_evolution_offset;
	guint32  uid;
	guchar   status;

} CamelMboxSummaryInformation;


/* this contains informations about the whole mbox file */
typedef struct {
	CamelFolderSummary parent_object;

	guint nb_message;	/* number of messages in the summary	*/
	guint32 next_uid;
	guint32 mbox_file_size;
	guint32 mbox_modtime;

	GArray *message_info;	/* array of CamelMboxSummaryInformation	*/

} CamelMboxSummary;

typedef struct {
	CamelFolderSummaryClass parent_class;

} CamelMboxSummaryClass;


GtkType camel_mbox_summary_get_type (void);

void camel_mbox_summary_save (CamelMboxSummary *summary,
			      const gchar *filename, CamelException *ex);
CamelMboxSummary *camel_mbox_summary_load (const gchar *filename,
					   CamelException *ex);

gboolean camel_mbox_summary_check_sync (gchar *summary_filename,
					gchar *mbox_filename,
					CamelException *ex);

void camel_mbox_summary_append_entries (CamelMboxSummary *summary,
					GArray *entries);


#endif /* MBOX_SUMMARY_H */
