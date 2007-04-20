
/*
  Concrete class for formatting mails to displayed html
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
