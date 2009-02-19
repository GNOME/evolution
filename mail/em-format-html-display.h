/*
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
  Concrete class for formatting mails to displayed html
*/

#ifndef EM_FORMAT_HTML_DISPLAY_H
#define EM_FORMAT_HTML_DISPLAY_H

#include "mail/em-format-html.h"
#include "widgets/misc/e-attachment-bar.h"

/* Standard GObject macros */
#define EM_TYPE_FORMAT_HTML_DISPLAY \
	(em_format_html_display_get_type ())
#define EM_FORMAT_HTML_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FORMAT_HTML_DISPLAY, EMFormatHTMLDisplay))
#define EM_FORMAT_HTML_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FORMAT_HTML_DISPLAY, EMFormatHTMLDisplayClass))
#define EM_IS_FORMAT_HTML_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FORMAT_HTML_DISPLAY))
#define EM_IS_FORMAT_HTML_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FORMAT_HTML_DISPLAY))
#define EM_FORMAT_HTML_DISPLAY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FORMAT_HTML_DISPLAY, EMFormatHTMLDisplayClass))

/* Search options */
#define EM_FORMAT_HTML_DISPLAY_SEARCH_PRIMARY		(0)
#define EM_FORMAT_HTML_DISPLAY_SEARCH_SECONDARY		(1)
#define EM_FORMAT_HTML_DISPLAY_SEARCH_ICASE		(1 << 8)

G_BEGIN_DECLS

typedef struct _EMFormatHTMLDisplay EMFormatHTMLDisplay;
typedef struct _EMFormatHTMLDisplayClass EMFormatHTMLDisplayClass;
typedef struct _EMFormatHTMLDisplayPrivate EMFormatHTMLDisplayPrivate;

struct _CamelMimePart;

struct _EMFormatHTMLDisplay {
	EMFormatHTML parent;
	EMFormatHTMLDisplayPrivate *priv;

	struct _ESearchingTokenizer *search_tok;

	unsigned int nobar:1;
};

struct _EMFormatHTMLDisplayClass {
	EMFormatHTMLClass parent_class;

	/* a link clicked normally */
	void (*link_clicked)(EMFormatHTMLDisplay *efhd, const char *uri);
	/* a part or a link button pressed event */
	int (*popup_event)(EMFormatHTMLDisplay *efhd, GdkEventButton *event, const char *uri, struct _CamelMimePart *part);
	/* the mouse is over a link */
	void (*on_url)(EMFormatHTMLDisplay *efhd, const char *uri);
};

GType		em_format_html_display_get_type	(void);
EMFormatHTMLDisplay *
		em_format_html_display_new	(void);
gboolean	em_format_html_display_get_animate
						(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_set_animate
						(EMFormatHTMLDisplay *efhd,
						 gboolean animate);
gboolean	em_format_html_display_get_caret_mode
						(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_set_caret_mode
						(EMFormatHTMLDisplay *efhd,
						 gboolean caret_mode);
void		em_format_html_display_set_search
						(EMFormatHTMLDisplay *efhd,
						 int type,
						 GSList *strings);
void		em_format_html_display_search	(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_search_with
						(EMFormatHTMLDisplay *efhd,
						 char *word);
void		em_format_html_display_search_close
						(EMFormatHTMLDisplay *efhd);
GtkWidget *	em_format_html_get_search_dialog(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_cut	(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_copy	(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_paste	(EMFormatHTMLDisplay *efhd);

void		em_format_html_display_zoom_in	(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_zoom_out	(EMFormatHTMLDisplay *efhd);
void		em_format_html_display_zoom_reset
						(EMFormatHTMLDisplay *efhd);
EAttachmentBar *em_format_html_display_get_bar	(EMFormatHTMLDisplay *efhd);

gboolean	em_format_html_display_popup_menu
						(EMFormatHTMLDisplay *efhd);

G_END_DECLS

#endif /* EM_FORMAT_HTML_DISPLAY_H */
