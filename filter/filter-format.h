
#ifndef _FILTER_FORMAT_H
#define _FILTER_FORMAT_H

#include <glib.h>

char *filter_description_text(GList *description, GList *args);
void filter_description_html_write(GList *description, GList *args,
				   GtkHTML *html, GtkHTMLStream *stream);

#endif /* _FILTER_FORMAT_H */
