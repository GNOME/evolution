/*
 * e-mail-config-format-html.h
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
 */

#ifndef E_MAIL_CONFIG_FORMAT_HTML_H
#define E_MAIL_CONFIG_FORMAT_HTML_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_FORMAT_HTML \
	(e_mail_config_format_html_get_type ())
#define E_MAIL_CONFIG_FORMAT_HTML(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_FORMAT_HTML, EMailConfigFormatHTML))
#define E_MAIL_CONFIG_FORMAT_HTML_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_FORMAT_HTML, EMailConfigFormatHTMLClass))
#define E_IS_MAIL_CONFIG_FORMAT_HTML(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_FORMAT_HTML))
#define E_IS_MAIL_CONFIG_FORMAT_HTML_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_FORMAT_HTML))
#define E_MAIL_CONFIG_FORMAT_HTML_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_FORMAT_HTML, EMailConfigFormatHTMLClass))

G_BEGIN_DECLS

typedef struct _EMailConfigFormatHTML EMailConfigFormatHTML;
typedef struct _EMailConfigFormatHTMLClass EMailConfigFormatHTMLClass;
typedef struct _EMailConfigFormatHTMLPrivate EMailConfigFormatHTMLPrivate;

struct _EMailConfigFormatHTML {
	EExtension parent;
	EMailConfigFormatHTMLPrivate *priv;
};

struct _EMailConfigFormatHTMLClass {
	EExtensionClass parent_class;
};

GType		e_mail_config_format_html_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_format_html_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_CONFIG_FORMAT_HTML_H */

