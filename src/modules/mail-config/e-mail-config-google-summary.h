/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_GOOGLE_SUMMARY_H
#define E_MAIL_CONFIG_GOOGLE_SUMMARY_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_GOOGLE_SUMMARY \
	(e_mail_config_google_summary_get_type ())
#define E_MAIL_CONFIG_GOOGLE_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_GOOGLE_SUMMARY, EMailConfigGoogleSummary))
#define E_MAIL_CONFIG_GOOGLE_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_GOOGLE_SUMMARY, EMailConfigGoogleSummaryClass))
#define E_IS_MAIL_CONFIG_GOOGLE_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_GOOGLE_SUMMARY))
#define E_IS_MAIL_CONFIG_GOOGLE_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_GOOGLE_SUMMARY))
#define E_MAIL_CONFIG_GOOGLE_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_GOOGLE_SUMMARY, EMailConfigGoogleSummaryClass))

G_BEGIN_DECLS

typedef struct _EMailConfigGoogleSummary EMailConfigGoogleSummary;
typedef struct _EMailConfigGoogleSummaryClass EMailConfigGoogleSummaryClass;
typedef struct _EMailConfigGoogleSummaryPrivate EMailConfigGoogleSummaryPrivate;

struct _EMailConfigGoogleSummary {
	EExtension parent;
	EMailConfigGoogleSummaryPrivate *priv;
};

struct _EMailConfigGoogleSummaryClass {
	EExtensionClass parent_class;
};

GType		e_mail_config_google_summary_get_type
					(void) G_GNUC_CONST;
void		e_mail_config_google_summary_type_register
					(GTypeModule *type_module);
gboolean	e_mail_config_google_summary_get_applicable
					(EMailConfigGoogleSummary *extension);

G_END_DECLS

#endif /* E_MAIL_CONFIG_GOOGLE_SUMMARY_H */

