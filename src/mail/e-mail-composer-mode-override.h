/*
 * SPDX-FileCopyrightText: (C) 2026 Red Hat <www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_COMPOSER_MODE_OVERRIDE_H
#define E_MAIL_COMPOSER_MODE_OVERRIDE_H

#include <glib-object.h>
#include <camel/camel.h>
#include <e-util/e-util.h>

G_BEGIN_DECLS

#define E_TYPE_MAIL_COMPOSER_MODE_OVERRIDE e_mail_composer_mode_override_get_type ()

typedef struct _EMailComposerModeOverrideEntry {
	gchar *key;
	EContentEditorMode mode;
} EMailComposerModeOverrideEntry;

G_DECLARE_FINAL_TYPE (EMailComposerModeOverride, e_mail_composer_mode_override, E, MAIL_COMPOSER_MODE_OVERRIDE, GObject)

EMailComposerModeOverride *
		e_mail_composer_mode_override_new
					(const gchar *config_filename);
void		e_mail_composer_mode_override_set_config_filename
					(EMailComposerModeOverride *self,
					 const gchar *config_filename);
gchar *		e_mail_composer_mode_override_dup_config_filename
					(EMailComposerModeOverride *self);
void		e_mail_composer_mode_override_set_prefer_folder
					(EMailComposerModeOverride *self,
					 gboolean prefer_folder);
gboolean	e_mail_composer_mode_override_get_prefer_folder
					(EMailComposerModeOverride *self);
EContentEditorMode
		e_mail_composer_mode_override_get_mode
					(EMailComposerModeOverride *self,
					 const gchar *folder_uri,
					 CamelInternetAddress *recipients_to,
					 CamelInternetAddress *recipients_cc,
					 CamelInternetAddress *recipients_bcc);
EContentEditorMode
		e_mail_composer_mode_override_get_for_folder
					(EMailComposerModeOverride *self,
					 const gchar *folder_uri);
void		e_mail_composer_mode_override_set_for_folder
					(EMailComposerModeOverride *self,
					 const gchar *folder_uri,
					 EContentEditorMode mode);
void		e_mail_composer_mode_override_remove_for_folder
					(EMailComposerModeOverride *self,
					 const gchar *folder_uri);
EContentEditorMode
		e_mail_composer_mode_override_get_for_recipient
					(EMailComposerModeOverride *self,
					 CamelInternetAddress *recipients);
void		e_mail_composer_mode_override_set_for_recipient
					(EMailComposerModeOverride *self,
					 const gchar *recipient,
					 EContentEditorMode mode);
void		e_mail_composer_mode_override_remove_for_recipient
					(EMailComposerModeOverride *self,
					 const gchar *recipient);
GPtrArray *	e_mail_composer_mode_override_list_all_folders
					(EMailComposerModeOverride *self);
GPtrArray *	e_mail_composer_mode_override_list_all_recipients
					(EMailComposerModeOverride *self);
void		e_mail_composer_mode_override_freeze_save
					(EMailComposerModeOverride *self);
void		e_mail_composer_mode_override_thaw_save
					(EMailComposerModeOverride *self);

G_END_DECLS

#endif /* E_MAIL_COMPOSER_MODE_OVERRIDE_H */
