#ifndef _MAIL_DISPLAY_H_
#define _MAIL_DISPLAY_H_

#include <gtk/gtktable.h>
#include "camel/camel-stream.h"

#define MAIL_DISPLAY_TYPE        (mail_display_get_type ())
#define MAIL_DISPLAY(o)          (GTK_CHECK_CAST ((o), MAIL_DISPLAY_TYPE, MailDisplay))
#define MAIL_DISPLAY_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_DISPLAY_TYPE, MailDisplayClass))
#define IS_MAIL_DISPLAY(o)       (GTK_CHECK_TYPE ((o), MAIL_DISPLAY_TYPE))
#define IS_MAIL_DISPLAY_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_DISPLAY_TYPE))

typedef struct {
	GtkTable parent;

	GtkHTML *html;
} MailDisplay;

typedef struct {
	GtkTableClass parent_class;
} MailDisplayClass;

GtkType        mail_display_get_type (void);
GtkWidget     *mail_display_new      (void);

CamelStream   *mail_display_get_stream (MailDisplay *display);

#endif /* _MAIL_DISPLAY_H_ */
