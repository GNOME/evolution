#ifndef _HTML_STREAM_H_
#define _HTML_STREAM_H_ 1

#include <gtkhtml/gtkhtml.h>
#include "camel/camel-stream.h"

#define HTML_STREAM_TYPE     (html_stream_get_type ())
#define HTML_STREAM(obj)     (GTK_CHECK_CAST((obj), HTML_STREAM_TYPE, HTMLStream))
#define HTML_STREAM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), HTML_STREAM_TYPE, HTMLStreamClass))
#define IS_HTML_STREAM(o)    (GTK_CHECK_TYPE((o), HTML_STREAM_TYPE))

typedef struct {
	CamelStream parent_object;
	GtkHTML *gtk_html;
	GtkHTMLStreamHandle *gtk_html_stream;
} HTMLStream;

typedef struct {
	CamelStreamClass parent_class;
} HTMLStreamClass;


GtkType      html_stream_get_type (void);
CamelStream *html_stream_new      (GtkHTML *html);

#endif /* _HTML_STREAM_H_ */
