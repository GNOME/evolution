
/*
  Concrete class for formatting mails to displayed html
*/

#ifndef _EM_FORMAT_HTML_PRINT_H
#define _EM_FORMAT_HTML_PRINT_H

#include "em-format-html.h"

struct _GnomePrintConfig;

typedef struct _EMFormatHTMLPrint EMFormatHTMLPrint;
typedef struct _EMFormatHTMLPrintClass EMFormatHTMLPrintClass;

struct _CamelFolder;

struct _EMFormatHTMLPrint {
	EMFormatHTML formathtml;

	struct _GtkWidget *window;	/* used to realise the gtkhtml in a toplevel, i dont know why */
	struct _GnomePrintConfig *config;
	struct _EMFormatHTML *source; /* used for print_message */

	int preview:1;
};

struct _EMFormatHTMLPrintClass {
	EMFormatHTMLClass formathtml_class;
};

GType em_format_html_print_get_type(void);

EMFormatHTMLPrint *em_format_html_print_new(void);

int em_format_html_print_print(EMFormatHTMLPrint *efhp, EMFormatHTML *source, struct _GnomePrintConfig *print_config, int preview);
int em_format_html_print_message(EMFormatHTMLPrint *efhp, EMFormatHTML *source, struct _GnomePrintConfig *print_config, struct _CamelFolder *folder, const char *uid, int preview);

#endif /* ! _EM_FORMAT_HTML_PRINT_H */
