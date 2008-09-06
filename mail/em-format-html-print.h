/*
 * Concrete class for formatting mails to displayed html
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
#ifndef _EM_FORMAT_HTML_PRINT_H
#define _EM_FORMAT_HTML_PRINT_H

#include "mail/em-format-html.h"

#define EM_TYPE_FORMAT_HTML_PRINT \
	(em_format_html_print_get_type ())

typedef struct _EMFormatHTMLPrint EMFormatHTMLPrint;
typedef struct _EMFormatHTMLPrintClass EMFormatHTMLPrintClass;

struct _EMFormatHTMLPrint {
	EMFormatHTML parent;

	GtkWidget *window;	/* used to realise the gtkhtml in a toplevel, i dont know why */
	EMFormatHTML *source; /* used for print_message */

	GtkPrintOperationAction action;
};

struct _EMFormatHTMLPrintClass {
	EMFormatHTMLClass parent_class;
};

GType      em_format_html_print_get_type      (void);
EMFormatHTMLPrint * em_format_html_print_new  (EMFormatHTML *source,
                                               GtkPrintOperationAction action);
void       em_format_html_print_message       (EMFormatHTMLPrint *efhp,
                                               CamelFolder *folder,
                                               const char *uid);
void        em_format_html_print_raw_message  (EMFormatHTMLPrint *efhp,
                                               CamelMimeMessage *msg);

#endif /* ! _EM_FORMAT_HTML_PRINT_H */
