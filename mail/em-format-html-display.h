
/*
  Concrete class for formatting mails to displayed html
*/

#ifndef _EM_FORMAT_HTML_DISPLAY_H
#define _EM_FORMAT_HTML_DISPLAY_H

#include "em-format-html.h"

typedef struct _EMFormatHTMLDisplay EMFormatHTMLDisplay;
typedef struct _EMFormatHTMLDisplayClass EMFormatHTMLDisplayClass;

struct _CamelMimePart;

struct _EMFormatHTMLDisplay {
	EMFormatHTML formathtml;

	struct _EMFormatHTMLDisplayPrivate *priv;

	struct _ESearchingTokenizer *search_tok;

	unsigned int animate:1;
	unsigned int caret_mode:1;
};

#define EM_FORMAT_HTML_DISPLAY_SEARCH_PRIMARY (0)
#define EM_FORMAT_HTML_DISPLAY_SEARCH_SECONDARY (1)
#define EM_FORMAT_HTML_DISPLAY_SEARCH_ICASE (1<<8)

struct _EMFormatHTMLDisplayClass {
	EMFormatHTMLClass formathtml_class;

	/* a link clicked normally */
	void (*link_clicked)(EMFormatHTMLDisplay *efhd, const char *uri);
	/* a part or a link button pressed event */
	int (*popup_event)(EMFormatHTMLDisplay *efhd, GdkEventButton *event, const char *uri, struct _CamelMimePart *part);
	/* the mouse is over a link */
	void (*on_url)(EMFormatHTMLDisplay *efhd, const char *uri);
};

GType em_format_html_display_get_type(void);
EMFormatHTMLDisplay *em_format_html_display_new(void);

void em_format_html_display_goto_anchor(EMFormatHTMLDisplay *efhd, const char *name);

void em_format_html_display_set_animate(EMFormatHTMLDisplay *efhd, gboolean state);
void em_format_html_display_set_caret_mode(EMFormatHTMLDisplay *efhd, gboolean state);

void em_format_html_display_set_search(EMFormatHTMLDisplay *efhd, int type, GSList *strings);
void em_format_html_display_search(EMFormatHTMLDisplay *efhd);

void em_format_html_display_cut (EMFormatHTMLDisplay *efhd);
void em_format_html_display_copy (EMFormatHTMLDisplay *efhd);
void em_format_html_display_paste (EMFormatHTMLDisplay *efhd);

void em_format_html_display_zoom_in (EMFormatHTMLDisplay *efhd);
void em_format_html_display_zoom_out (EMFormatHTMLDisplay *efhd);
void em_format_html_display_zoom_reset (EMFormatHTMLDisplay *efhd);

gboolean em_format_html_display_popup_menu (EMFormatHTMLDisplay *efhd);

/* experimental */
struct _EPopupExtension;
void em_format_html_display_set_popup(EMFormatHTMLDisplay *, struct _EPopupExtension *);

#endif /* !_EM_FORMAT_HTML_DISPLAY_H */
