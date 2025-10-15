/*
 * Copyright (C) 2019 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_FOLDER_TWEAKS_H
#define E_MAIL_FOLDER_TWEAKS_H

#include <glib-object.h>
#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FOLDER_TWEAKS \
	(e_mail_folder_tweaks_get_type ())
#define E_MAIL_FOLDER_TWEAKS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FOLDER_TWEAKS, EMailFolderTweaks))
#define E_MAIL_FOLDER_TWEAKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FOLDER_TWEAKS, EMailFolderTweaksClass))
#define E_IS_MAIL_FOLDER_TWEAKS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FOLDER_TWEAKS))
#define E_IS_MAIL_FOLDER_TWEAKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FOLDER_TWEAKS))
#define E_MAIL_FOLDER_TWEAKS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FOLDER_TWEAKS, EMailFolderTweaksClass))

G_BEGIN_DECLS

typedef struct _EMailFolderTweaks EMailFolderTweaks;
typedef struct _EMailFolderTweaksClass EMailFolderTweaksClass;
typedef struct _EMailFolderTweaksPrivate EMailFolderTweaksPrivate;

struct _EMailFolderTweaks {
	/*< private >*/
	GObject parent;
	EMailFolderTweaksPrivate *priv;
};

struct _EMailFolderTweaksClass {
	/*< private >*/
	GObjectClass parent_class;

	void	(* changed)	(EMailFolderTweaks *tweaks,
				 const gchar *folder_uri);
};

GType		e_mail_folder_tweaks_get_type	(void);

EMailFolderTweaks *
		e_mail_folder_tweaks_new	(void);
void		e_mail_folder_tweaks_remove_for_folders
						(EMailFolderTweaks *tweaks,
						 const gchar *top_folder_uri);
gboolean	e_mail_folder_tweaks_get_color	(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri,
						 GdkRGBA *out_rgba);
void		e_mail_folder_tweaks_set_color	(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri,
						 const GdkRGBA *rgba);
gchar *		e_mail_folder_tweaks_dup_icon_filename
						(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri);
void		e_mail_folder_tweaks_set_icon_filename
						(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri,
						 const gchar *icon_filename);
guint		e_mail_folder_tweaks_get_sort_order
						(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri);
void		e_mail_folder_tweaks_set_sort_order
						(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri,
						 guint sort_order);
void		e_mail_folder_tweaks_remove_sort_order_for_folders
						(EMailFolderTweaks *tweaks,
						 const gchar *top_folder_uri);
void		e_mail_folder_tweaks_folder_renamed
						(EMailFolderTweaks *tweaks,
						 const gchar *old_folder_uri,
						 const gchar *new_folder_uri);
void		e_mail_folder_tweaks_folder_deleted
						(EMailFolderTweaks *tweaks,
						 const gchar *folder_uri);

G_END_DECLS

#endif /* E_MAIL_FOLDER_TWEAKS_H */
