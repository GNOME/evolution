/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-mail.h
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 *
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_MAIL_H__
#define __E_SUMMARY_MAIL_H__

#include <Evolution.h>

#include "e-summary-type.h"
#include "e-summary-table.h"

typedef enum _ESummaryMailMode ESummaryMailMode;
enum _ESummaryMailMode {
	E_SUMMARY_MAIL_MODE_ONLY,
	E_SUMMARY_MAIL_MODE_EXCLUDING
};

typedef struct _ESummaryMailRowData ESummaryMailRowData;	
typedef struct _ESummaryMail ESummaryMail;

struct _ESummaryMailRowData {
	char *name;
	char *uri;
};

const char *e_summary_mail_get_html (ESummary *summary);
void e_summary_mail_init (ESummary *summary);
void e_summary_mail_reconfigure (void);
void e_summary_mail_free (ESummary *summary);
const char *e_summary_mail_uri_to_name (const char *uri);
void e_summary_mail_fill_list (ESummaryTable *est);

/* Folder stuff */
gboolean e_summary_folder_init_folder_store (GNOME_Evolution_Shell shell);
gboolean e_summary_folder_clear_folder_store (void);
#endif
