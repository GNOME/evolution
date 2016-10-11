/*
 * e-mail-config-yahoo-summary.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_CONFIG_YAHOO_SUMMARY_H
#define E_MAIL_CONFIG_YAHOO_SUMMARY_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_YAHOO_SUMMARY \
	(e_mail_config_yahoo_summary_get_type ())
#define E_MAIL_CONFIG_YAHOO_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_YAHOO_SUMMARY, EMailConfigYahooSummary))
#define E_MAIL_CONFIG_YAHOO_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_YAHOO_SUMMARY, EMailConfigYahooSummaryClass))
#define E_IS_MAIL_CONFIG_YAHOO_SUMMARY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_YAHOO_SUMMARY))
#define E_IS_MAIL_CONFIG_YAHOO_SUMMARY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_YAHOO_SUMMARY))
#define E_MAIL_CONFIG_YAHOO_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_YAHOO_SUMMARY, EMailConfigYahooSummaryClass))

G_BEGIN_DECLS

typedef struct _EMailConfigYahooSummary EMailConfigYahooSummary;
typedef struct _EMailConfigYahooSummaryClass EMailConfigYahooSummaryClass;
typedef struct _EMailConfigYahooSummaryPrivate EMailConfigYahooSummaryPrivate;

struct _EMailConfigYahooSummary {
	EExtension parent;
	EMailConfigYahooSummaryPrivate *priv;
};

struct _EMailConfigYahooSummaryClass {
	EExtensionClass parent_class;
};

GType		e_mail_config_yahoo_summary_get_type
					(void) G_GNUC_CONST;
void		e_mail_config_yahoo_summary_type_register
					(GTypeModule *type_module);
gboolean	e_mail_config_yahoo_summary_get_applicable
					(EMailConfigYahooSummary *extension);

G_END_DECLS

#endif /* E_MAIL_CONFIG_YAHOO_SUMMARY_H */

