/*
 * e-summary-mail.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef __E_SUMMARY_MAIL_H__
#define __E_SUMMARY_MAIL_H__

#include "e-summary-type.h"
#include <Evolution.h>
#include <gtk/gtkclist.h>

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
void e_summary_mail_init (ESummary *summary,
			  GNOME_Evolution_Shell corba_shell);
void e_summary_mail_reconfigure (ESummary *summary);
void e_summary_mail_free (ESummary *summary);
void e_summary_mail_fill_list (GtkCList *clist, 
			       ESummary *summary);
const char *e_summary_mail_uri_to_name (ESummary *summary,
					const char *uri);
#endif
