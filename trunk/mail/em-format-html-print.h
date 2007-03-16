
/*
  Concrete class for formatting mails to displayed html
*/

#ifndef _EM_FORMAT_HTML_PRINT_H
#define _EM_FORMAT_HTML_PRINT_H

#include "mail/em-format-html.h"

struct GtkPrintSettings;
typedef struct _EMFormatHTMLPrint EMFormatHTMLPrint;
typedef struct _EMFormatHTMLPrintClass EMFormatHTMLPrintClass;

struct _CamelFolder;

struct _EMFormatHTMLPrint {
	EMFormatHTML formathtml;

	struct _GtkWidget *window;	/* used to realise the gtkhtml in a toplevel, i dont know why */
	struct _GtkPrintSettings *settings;
	struct _EMFormatHTML *source; /* used for print_message */

	guint preview:1;
};

struct _EMFormatHTMLPrintClass {
	EMFormatHTMLClass formathtml_class;
};

GType em_format_html_print_get_type(void);

EMFormatHTMLPrint *em_format_html_print_new(void);

int em_format_html_print_print(EMFormatHTMLPrint *efhp, EMFormatHTML *source, struct GtkPrintSettings *settings, int preview);
int em_format_html_print_message(EMFormatHTMLPrint *efhp, EMFormatHTML *source, struct GtkPrintSettings *settings, struct _CamelFolder *folder, const char *uid, int preview);
int em_format_html_print_raw_message(EMFormatHTMLPrint *efhp, struct _GtkPrintSettings *settings, struct _CamelMimeMessage *msg, int preview);

#endif /* ! _EM_FORMAT_HTML_PRINT_H */
