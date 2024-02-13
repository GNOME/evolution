/*
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
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "em-vfolder-editor-context.h"

#include <string.h>

#include "shell/e-shell.h"

#include "em-filter-editor-folder-element.h"
#include "em-utils.h"
#include "em-vfolder-editor-rule.h"

struct _EMVFolderEditorContextPrivate {
	EMailSession *session;
};

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE_WITH_PRIVATE (EMVFolderEditorContext, em_vfolder_editor_context, EM_TYPE_VFOLDER_CONTEXT)

static void
vfolder_editor_context_set_session (EMVFolderEditorContext *context,
                             EMailSession *session)
{
	if (session == NULL) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailBackend *backend;

		shell = e_shell_get_default ();
		shell_backend = e_shell_get_backend_by_name (shell, "mail");

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);
	}

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (context->priv->session == NULL);

	context->priv->session = g_object_ref (session);
}

static void
vfolder_editor_context_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			vfolder_editor_context_set_session (
				EM_VFOLDER_EDITOR_CONTEXT (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_editor_context_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				em_vfolder_editor_context_get_session (
				EM_VFOLDER_EDITOR_CONTEXT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
vfolder_editor_context_dispose (GObject *object)
{
	EMVFolderEditorContext *self = EM_VFOLDER_EDITOR_CONTEXT (object);

	g_clear_object (&self->priv->session);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_vfolder_editor_context_parent_class)->dispose (object);
}

static EFilterElement *
vfolder_editor_context_new_element (ERuleContext *context,
                             const gchar *type)
{
	EMVFolderEditorContext *self = EM_VFOLDER_EDITOR_CONTEXT (context);

	if (strcmp (type, "system-flag") == 0)
		return e_filter_option_new ();

	if (strcmp (type, "score") == 0)
		return e_filter_int_new_type ("score", -3, 3);

	if (strcmp (type, "folder") == 0)
		return em_filter_editor_folder_element_new (self->priv->session);

	/* XXX Legacy type name.  Same as "folder" now. */
	if (strcmp (type, "folder-curi") == 0)
		return em_filter_editor_folder_element_new (self->priv->session);

	return E_RULE_CONTEXT_CLASS (em_vfolder_editor_context_parent_class)->new_element (context, type);
}

static void
em_vfolder_editor_context_class_init (EMVFolderEditorContextClass *class)
{
	GObjectClass *object_class;
	ERuleContextClass *rule_context_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = vfolder_editor_context_set_property;
	object_class->get_property = vfolder_editor_context_get_property;
	object_class->dispose = vfolder_editor_context_dispose;

	rule_context_class = E_RULE_CONTEXT_CLASS (class);
	rule_context_class->new_element = vfolder_editor_context_new_element;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_vfolder_editor_context_init (EMVFolderEditorContext *context)
{
	context->priv = em_vfolder_editor_context_get_instance_private (context);

	e_rule_context_add_part_set (
		E_RULE_CONTEXT (context), "partset", E_TYPE_FILTER_PART,
		(ERuleContextPartFunc) e_rule_context_add_part,
		(ERuleContextNextPartFunc) e_rule_context_next_part);

	e_rule_context_add_rule_set (
		E_RULE_CONTEXT (context), "ruleset", EM_TYPE_VFOLDER_EDITOR_RULE,
		(ERuleContextRuleFunc) e_rule_context_add_rule,
		(ERuleContextNextRuleFunc) e_rule_context_next_rule);

	E_RULE_CONTEXT (context)->flags =
		E_RULE_CONTEXT_THREADING | E_RULE_CONTEXT_GROUPING;
}

EMVFolderEditorContext *
em_vfolder_editor_context_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		EM_TYPE_VFOLDER_EDITOR_CONTEXT, "session", session, NULL);
}

EMailSession *
em_vfolder_editor_context_get_session (EMVFolderEditorContext *context)
{
	g_return_val_if_fail (EM_IS_VFOLDER_EDITOR_CONTEXT (context), NULL);

	return context->priv->session;
}
