/*
 * SPDX-FileCopyrightText: (C) 2015 Red Hat Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_PROPERTIES_H
#define E_MAIL_PROPERTIES_H

#include <glib-object.h>

#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PROPERTIES \
	(e_mail_properties_get_type ())
#define E_MAIL_PROPERTIES(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PROPERTIES, EMailProperties))
#define E_MAIL_PROPERTIES_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PROPERTIES, EMailPropertiesClass))
#define E_IS_MAIL_PROPERTIES(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PROPERTIES))
#define E_IS_MAIL_PROPERTIES_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PROPERTIES))
#define E_MAIL_PROPERTIES_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PROPERTIES, EMailPropertiesClass))

G_BEGIN_DECLS

typedef struct _EMailProperties EMailProperties;
typedef struct _EMailPropertiesClass EMailPropertiesClass;
typedef struct _EMailPropertiesPrivate EMailPropertiesPrivate;

struct _EMailProperties {
	GObject parent;
	EMailPropertiesPrivate *priv;
};

struct _EMailPropertiesClass {
	GObjectClass parent_class;
};

GType		e_mail_properties_get_type	(void) G_GNUC_CONST;
EMailProperties *
		e_mail_properties_new		(const gchar *config_filename);
void		e_mail_properties_set_for_folder
						(EMailProperties *properties,
						 CamelFolder *folder,
						 const gchar *key,
						 const gchar *value);
void		e_mail_properties_set_for_folder_uri
						(EMailProperties *properties,
						 const gchar *folder_uri,
						 const gchar *key,
						 const gchar *value);
gchar *		e_mail_properties_get_for_folder
						(EMailProperties *properties,
						 CamelFolder *folder,
						 const gchar *key);
gchar *		e_mail_properties_get_for_folder_uri
						(EMailProperties *properties,
						 const gchar *folder_uri,
						 const gchar *key);
G_END_DECLS

#endif /* E_MAIL_PROPERTIES_H */
