/*
 * e-mail-config-web-view.h
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

#ifndef E_MAIL_CONFIG_WEB_VIEW_H
#define E_MAIL_CONFIG_WEB_VIEW_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_WEB_VIEW \
	(e_mail_config_web_view_get_type ())
#define E_MAIL_CONFIG_WEB_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_WEB_VIEW, EMailConfigWebView))
#define E_MAIL_CONFIG_WEB_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_WEB_VIEW, EMailConfigWebViewClass))
#define E_IS_MAIL_CONFIG_WEB_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_WEB_VIEW))
#define E_IS_MAIL_CONFIG_WEB_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_WEB_VIEW))
#define E_MAIL_CONFIG_WEB_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_WEB_VIEW, EMailConfigWebViewClass))

G_BEGIN_DECLS

typedef struct _EMailConfigWebView EMailConfigWebView;
typedef struct _EMailConfigWebViewClass EMailConfigWebViewClass;
typedef struct _EMailConfigWebViewPrivate EMailConfigWebViewPrivate;

struct _EMailConfigWebView {
	EExtension parent;
	EMailConfigWebViewPrivate *priv;
};

struct _EMailConfigWebViewClass {
	EExtensionClass parent_class;
};

GType		e_mail_config_web_view_get_type	(void) G_GNUC_CONST;
void		e_mail_config_web_view_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_CONFIG_WEB_VIEW_H */
