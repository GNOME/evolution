/*
 * e-settings-mail-part-headers.h
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

#ifndef E_SETTINGS_MAIL_PART_HEADERS_H
#define E_SETTINGS_MAIL_PART_HEADERS_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_MAIL_PART_HEADERS \
	(e_settings_mail_part_headers_get_type ())
#define E_SETTINGS_MAIL_PART_HEADERS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_MAIL_PART_HEADERS, ESettingsMailPartHeaders))
#define E_SETTINGS_MAIL_PART_HEADERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_MAIL_PART_HEADERS, ESettingsMailPartHeadersClass))
#define E_IS_SETTINGS_MAIL_PART_HEADERS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_MAIL_PART_HEADERS))
#define E_IS_SETTINGS_MAIL_PART_HEADERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_MAIL_PART_HEADERS))
#define E_SETTINGS_MAIL_PART_HEADERS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_MAIL_PART_HEADERS, ESettingsMailPartHeadersClass))

G_BEGIN_DECLS

typedef struct _ESettingsMailPartHeaders ESettingsMailPartHeaders;
typedef struct _ESettingsMailPartHeadersClass ESettingsMailPartHeadersClass;
typedef struct _ESettingsMailPartHeadersPrivate ESettingsMailPartHeadersPrivate;

struct _ESettingsMailPartHeaders {
	EExtension parent;
	ESettingsMailPartHeadersPrivate *priv;
};

struct _ESettingsMailPartHeadersClass {
	EExtensionClass parent_class;
};

GType		e_settings_mail_part_headers_get_type
						(void) G_GNUC_CONST;
void		e_settings_mail_part_headers_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_MAIL_PART_HEADERS_H */

