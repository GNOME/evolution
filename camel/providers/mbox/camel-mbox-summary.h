/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : Bertrand Guiheneuf <bertrand@helixcode.com> 
 *
 * Copyright (C) 1999 Helix Code .
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

#include <glib.h>



typedef struct {

	guint32  position;
	guint    x_evolution_offset;
	guint32  uid;
	guchar   status;
	gchar   *subject;
	gchar   *sender;
	gchar   *to;
	gchar   *date;

} CamelMboxSummaryInformation;


typedef struct {
	
	guint nb_message;      /* number of messages in the summary    */
	guchar md5_digest[16];   /* md5 signature of the mbox file     */
	guint32 next_uid;
	guint32 mbox_file_size;
	
	GArray *message_info;    /* array of CamelMboxSummaryInformation */
	
} CamelMboxSummary;


void 
camel_mbox_save_summary (CamelMboxSummary *summary, const gchar *filename, CamelException *ex);

CamelMboxSummary *
camel_mbox_load_summary (const gchar *filename, CamelException *ex);

gboolean
camel_mbox_check_summary_sync (gchar *summary_filename,
			       gchar *mbox_filename,
			       CamelException *ex);

void
camel_summary_append_entries (CamelMboxSummary *summary, GArray *entries);


#endif /* MH_SUMMARY_H */
