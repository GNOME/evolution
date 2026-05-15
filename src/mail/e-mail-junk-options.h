/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_JUNK_OPTIONS_H
#define E_MAIL_JUNK_OPTIONS_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_JUNK_OPTIONS \
	(e_mail_junk_options_get_type ())
#define E_MAIL_JUNK_OPTIONS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_JUNK_OPTIONS, EMailJunkOptions))
#define E_MAIL_JUNK_OPTIONS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_JUNK_OPTIONS, EMailJunkOptionsClass))
#define E_IS_MAIL_JUNK_OPTIONS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_JUNK_OPTIONS))
#define E_IS_MAIL_JUNK_OPTIONS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_JUNK_OPTIONS))
#define E_MAIL_JUNK_OPTIONS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_JUNK_OPTIONS, EMailJunkOptionsClass))

G_BEGIN_DECLS

typedef struct _EMailJunkOptions EMailJunkOptions;
typedef struct _EMailJunkOptionsClass EMailJunkOptionsClass;
typedef struct _EMailJunkOptionsPrivate EMailJunkOptionsPrivate;

struct _EMailJunkOptions {
	GtkGrid parent;
	EMailJunkOptionsPrivate *priv;
};

struct _EMailJunkOptionsClass {
	GtkGridClass parent_class;
};

GType		e_mail_junk_options_get_type	(void);
GtkWidget *	e_mail_junk_options_new		(EMailSession *session);
EMailSession *	e_mail_junk_options_get_session	(EMailJunkOptions *options);
void		e_mail_junk_options_set_session	(EMailJunkOptions *options,
						 EMailSession *session);

G_END_DECLS

#endif /* E_MAIL_JUNK_OPTIONS_H */
