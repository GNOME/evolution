/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jon Trowbridge <trow@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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


#ifndef _MAIL_SEARCH_H_
#define _MAIL_SEARCH_H_

#ifdef _cplusplus
extern "C" {
#pragma }
#endif /* _cplusplus */

#include <gtk/gtkdialog.h>
#include "mail-display.h"

#define MAIL_SEARCH_TYPE        (mail_search_get_type ())
#define MAIL_SEARCH(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_SEARCH_TYPE, MailSearch))
#define MAIL_SEARCH_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), MAIL_SEARCH_TYPE, MailSearch))
#define IS_MAIL_SEARCH(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_SEARCH_TYPE))
#define IS_MAIL_SEARCH_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_SEARCH_TYPE))

typedef struct _MailSearch      MailSearch;
typedef struct _MailSearchClass MailSearchClass;

struct _MailSearch {
	GtkDialog parent;
	
	MailDisplay *mail;
	
	GtkWidget *entry;
	GtkWidget *msg_frame;
	GtkWidget *count_label;
	
	gboolean search_forward, case_sensitive;
	char *last_search;
	
	guint begin_handler;
	guint match_handler;
};

struct _MailSearchClass {
	GtkDialogClass parent_class;
	
};

GtkType    mail_search_get_type (void);

void       mail_search_construct (MailSearch *, MailDisplay *);
GtkWidget *mail_search_new       (MailDisplay *);


#ifdef _cplusplus
}
#endif /* _cplusplus */

#endif /* _MAIL_SEARCH_H_ */
