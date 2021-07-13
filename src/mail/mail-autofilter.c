/*
 * Code for autogenerating rules or filters from a message.
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#include "evolution-config.h"

#include <ctype.h>
#include <string.h>
#include <glib/gi18n.h>

#include "mail-vfolder-ui.h"
#include "mail-autofilter.h"
#include "em-utils.h"
#include "e-util/e-util-private.h"

#include "em-vfolder-editor-context.h"
#include "em-vfolder-editor-rule.h"
#include "em-vfolder-editor.h"

#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-filter-editor.h"

#define d(x)

static void
rule_match_recipients (ERuleContext *context,
                       EFilterRule *rule,
                       CamelInternetAddress *iaddr)
{
	EFilterPart *part;
	EFilterElement *element;
	gint i;
	const gchar *real, *addr;
	gchar *namestr;

	/* address types etc should handle multiple values */
	for (i = 0; camel_internet_address_get (iaddr, i, &real, &addr); i++) {
		part = e_rule_context_create_part (context, "to");
		e_filter_rule_add_part ((EFilterRule *) rule, part);
		element = e_filter_part_find_element (part, "recipient-type");
		e_filter_option_set_current ((EFilterOption *) element, "contains");
		element = e_filter_part_find_element (part, "recipient");
		e_filter_input_set_value ((EFilterInput *) element, addr);

		namestr = g_strdup_printf (_("Mail to %s"), real && real[0] ? real : addr);
		e_filter_rule_set_name (rule, namestr);
		g_free (namestr);
	}
}

/* remove 're' part of a subject */
static const gchar *
strip_re (const gchar *subject)
{
	const guchar *s, *p;

	s = (guchar *) subject;

	while (*s) {
		while (isspace (*s))
			s++;
		if (s[0] == 0)
			break;
		if ((s[0] == 'r' || s[0] == 'R')
		    && (s[1] == 'e' || s[1] == 'E')) {
			p = s + 2;
			while (isdigit (*p) || (ispunct (*p) && (*p != ':')))
				p++;
			if (*p == ':') {
				s = p + 1;
			} else
				break;
		} else
			break;
	}

	return (gchar *) s;
}

static void
rule_add_subject (ERuleContext *context,
                  EFilterRule *rule,
                  const gchar *text)
{
	EFilterPart *part;
	EFilterElement *element;

	/* don't match on empty strings ever */
	if (*text == 0)
		return;
	part = e_rule_context_create_part (context, "subject");
	e_filter_rule_add_part ((EFilterRule *) rule, part);
	element = e_filter_part_find_element (part, "subject-type");
	e_filter_option_set_current ((EFilterOption *) element, "contains");
	element = e_filter_part_find_element (part, "subject");
	e_filter_input_set_value ((EFilterInput *) element, text);
}

static void
rule_add_sender (ERuleContext *context,
                 EFilterRule *rule,
                 const gchar *text)
{
	EFilterPart *part;
	EFilterElement *element;

	/* don't match on empty strings ever */
	if (*text == 0)
		return;
	part = e_rule_context_create_part (context, "sender");
	e_filter_rule_add_part ((EFilterRule *) rule, part);
	element = e_filter_part_find_element (part, "sender-type");
	e_filter_option_set_current ((EFilterOption *) element, "contains");
	element = e_filter_part_find_element (part, "sender");
	e_filter_input_set_value ((EFilterInput *) element, text);
}

/* do a bunch of things on the subject to try and detect mailing lists, remove
 * unneeded stuff, etc */
static void
rule_match_subject (ERuleContext *context,
                    EFilterRule *rule,
                    const gchar *subject)
{
	const gchar *s;
	const gchar *s1, *s2;
	gchar *tmp;

	s = strip_re (subject);
	/* don't match on empty subject */
	if (*s == 0)
		return;

	/* [blahblah] is probably a mailing list, match on it separately */
	s1 = strchr (s, '[');
	s2 = strchr (s, ']');
	if (s1 && s2 && s1 < s2) {
		/* probably a mailing list, match on the mailing list name */
		tmp = g_alloca (s2 - s1 + 2);
		memcpy (tmp, s1, s2 - s1 + 1);
		tmp[s2 - s1 + 1] = 0;
		g_strstrip (tmp);
		rule_add_subject (context, rule, tmp);
		s = s2 + 1;
	}
	/* Froblah: at the start is probably something important (e.g. bug number) */
	s1 = strchr (s, ':');
	if (s1) {
		tmp = g_alloca (s1 - s + 1);
		memcpy (tmp, s, s1 - s);
		tmp[s1 - s] = 0;
		g_strstrip (tmp);
		rule_add_subject (context, rule, tmp);
		s = s1 + 1;
	}

	/* just lump the rest together */
	tmp = g_alloca (strlen (s) + 1);
	strcpy (tmp, s);
	g_strstrip (tmp);
	rule_add_subject (context, rule, tmp);
}

static void
rule_match_mlist (ERuleContext *context,
                  EFilterRule *rule,
                  const gchar *mlist)
{
	EFilterPart *part;
	EFilterElement *element;

	if (mlist[0] == 0)
		return;

	part = e_rule_context_create_part (context, "mlist");
	e_filter_rule_add_part (rule, part);

	element = e_filter_part_find_element (part, "mlist-type");
	e_filter_option_set_current ((EFilterOption *) element, "is");

	element = e_filter_part_find_element (part, "mlist");
	e_filter_input_set_value ((EFilterInput *) element, mlist);
}

static void
rule_from_address (EFilterRule *rule,
                   ERuleContext *context,
                   CamelInternetAddress *addr,
                   gint flags)
{
	rule->grouping = E_FILTER_GROUP_ALL;

	if (flags & AUTO_FROM) {
		const gchar *name = NULL, *address = NULL;
		gchar *namestr;

		if (camel_internet_address_get (addr, 0, &name, &address)) {
			rule_add_sender (context, rule, address);
			if (name == NULL || name[0] == '\0')
				name = address;
			namestr = g_strdup_printf (_("Mail from %s"), name);
			e_filter_rule_set_name (rule, namestr);
			g_free (namestr);
		}
	}

	if (flags & AUTO_TO) {
		rule_match_recipients (context, rule, addr);
	}
}

static void
rule_from_message (EFilterRule *rule,
                   ERuleContext *context,
                   CamelMimeMessage *msg,
                   gint flags)
{
	CamelInternetAddress *addr;

	rule->grouping = E_FILTER_GROUP_ALL;

	if (flags & AUTO_SUBJECT) {
		const gchar *subject;
		gchar *namestr;

		subject = camel_mime_message_get_subject (msg);
		if (!subject)
			subject = "";

		rule_match_subject (context, rule, subject);

		namestr = g_strdup_printf (_("Subject is %s"), strip_re (subject));
		e_filter_rule_set_name (rule, namestr);
		g_free (namestr);
	}
	/* should parse the from address into an internet address? */
	if (flags & AUTO_FROM) {
		CamelInternetAddress *from;
		gint i;
		const gchar *name, *address;
		gchar *namestr;

		from = camel_mime_message_get_from (msg);
		for (i = 0; from && camel_internet_address_get (
				from, i, &name, &address); i++) {
			rule_add_sender (context, rule, address);
			if (name == NULL || name[0] == '\0')
				name = address;
			namestr = g_strdup_printf (_("Mail from %s"), name);
			e_filter_rule_set_name (rule, namestr);
			g_free (namestr);
		}
	}
	if (flags & AUTO_TO) {
		addr = (CamelInternetAddress *)
			camel_mime_message_get_recipients (
			msg, CAMEL_RECIPIENT_TYPE_TO);
		if (addr)
			rule_match_recipients (context, rule, addr);
		addr = (CamelInternetAddress *)
			camel_mime_message_get_recipients (
			msg, CAMEL_RECIPIENT_TYPE_CC);
		if (addr)
			rule_match_recipients (context, rule, addr);
	}
	if (flags & AUTO_MLIST) {
		gchar *name, *mlist;

		mlist = camel_headers_dup_mailing_list (camel_medium_get_headers (CAMEL_MEDIUM (msg)));
		if (mlist) {
			rule_match_mlist (context, rule, mlist);
			name = g_strdup_printf (_("%s mailing list"), mlist);
			e_filter_rule_set_name (rule, name);
			g_free (name);
		}
		g_free (mlist);
	}
}

EFilterRule *
em_vfolder_rule_from_message (EMVFolderContext *context,
                              CamelMimeMessage *msg,
                              gint flags,
                              CamelFolder *folder)
{
	EFilterRule *rule;
	EMailSession *session;
	gchar *uri;

	g_return_val_if_fail (EM_IS_VFOLDER_CONTEXT (context), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (msg), NULL);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	uri = e_mail_folder_uri_from_folder (folder);

	session = em_vfolder_editor_context_get_session ((EMVFolderEditorContext *) context);

	rule = em_vfolder_editor_rule_new (session);
	em_vfolder_rule_add_source (EM_VFOLDER_RULE (rule), uri);
	rule_from_message (rule, E_RULE_CONTEXT (context), msg, flags);

	g_free (uri);

	return rule;
}

EFilterRule *
em_vfolder_rule_from_address (EMVFolderContext *context,
                              CamelInternetAddress *addr,
                              gint flags,
                              CamelFolder *folder)
{
	EFilterRule *rule;
	EMailSession *session;
	gchar *uri;

	g_return_val_if_fail (EM_IS_VFOLDER_CONTEXT (context), NULL);
	g_return_val_if_fail (CAMEL_IS_INTERNET_ADDRESS (addr), NULL);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	uri = e_mail_folder_uri_from_folder (folder);

	session = em_vfolder_editor_context_get_session ((EMVFolderEditorContext *) context);

	rule = em_vfolder_editor_rule_new (session);
	em_vfolder_rule_add_source (EM_VFOLDER_RULE (rule), uri);
	rule_from_address (rule, E_RULE_CONTEXT (context), addr, flags);

	g_free (uri);

	return rule;
}

EFilterRule *
filter_rule_from_message (EMFilterContext *context,
                          CamelMimeMessage *msg,
                          gint flags)
{
	EFilterRule *rule;
	EFilterPart *part;

	g_return_val_if_fail (EM_IS_FILTER_CONTEXT (context), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (msg), NULL);

	rule = em_filter_rule_new ();
	rule_from_message (rule, E_RULE_CONTEXT (context), msg, flags);

	part = em_filter_context_next_action (context, NULL);

	em_filter_rule_add_action (
		EM_FILTER_RULE (rule), e_filter_part_clone (part));

	return rule;
}

void
filter_gui_add_from_message (EMailSession *session,
                             CamelMimeMessage *msg,
                             const gchar *source,
                             gint flags)
{
	EMFilterContext *fc;
	const gchar *config_dir;
	gchar *user, *system;
	EFilterRule *rule;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (msg));

	fc = em_filter_context_new (session);
	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *) fc, system, user);
	g_free (system);

	rule = filter_rule_from_message (fc, msg, flags);

	e_filter_rule_set_source (rule, source);

	e_rule_context_add_rule_gui (
		(ERuleContext *) fc, rule, _("Add Filter Rule"), user);
	g_free (user);
	g_object_unref (fc);
}

void
mail_filter_rename_folder (CamelStore *store,
                           const gchar *old_folder_name,
                           const gchar *new_folder_name)
{
	CamelSession *session;
	EMFilterContext *fc;
	const gchar *config_dir;
	gchar *user, *system;
	GList *changed;
	gchar *old_uri;
	gchar *new_uri;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (old_folder_name != NULL);
	g_return_if_fail (new_folder_name != NULL);

	session = camel_service_ref_session (CAMEL_SERVICE (store));

	old_uri = e_mail_folder_uri_build (store, old_folder_name);
	new_uri = e_mail_folder_uri_build (store, new_folder_name);

	fc = em_filter_context_new (E_MAIL_SESSION (session));
	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *) fc, system, user);
	g_free (system);

	changed = e_rule_context_rename_uri (
		(ERuleContext *) fc, old_uri, new_uri, g_str_equal);
	if (changed) {
		if (e_rule_context_save ((ERuleContext *) fc, user) == -1)
			g_warning ("Could not write out changed filter rules\n");
		e_rule_context_free_uri_list ((ERuleContext *) fc, changed);
	}

	g_free (user);
	g_object_unref (fc);

	g_free (old_uri);
	g_free (new_uri);

	g_object_unref (session);
}

static void
mail_autofilter_open_filters_clicked_cb (GtkWidget *button,
					 gpointer user_data)
{
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view = NULL;
	EMailSession *session;
	EShell *shell;
	GList *windows, *link;

	shell = e_shell_get_default ();

	windows = gtk_application_get_windows (GTK_APPLICATION (shell));

	for (link = windows; link && !shell_view; link = g_list_next (link)) {
		GtkWindow *window = link->data;

		if (E_IS_SHELL_WINDOW (window)) {
			shell_window = E_SHELL_WINDOW (window);
			shell_view = e_shell_window_peek_shell_view (shell_window, "mail");
		}
	}

	if (!shell_view)
		return;

	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	em_utils_edit_filters (
		session,
		E_ALERT_SINK (shell_content),
		GTK_WINDOW (shell_window));
}

void
mail_filter_delete_folder (CamelStore *store,
                           const gchar *folder_name,
                           EAlertSink *alert_sink)
{
	CamelSession *session;
	EMFilterContext *fc;
	const gchar *config_dir;
	gchar *user, *system;
	GList *deleted;
	gchar *uri;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);
	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));

	session = camel_service_ref_session (CAMEL_SERVICE (store));

	uri = e_mail_folder_uri_build (store, folder_name);

	fc = em_filter_context_new (E_MAIL_SESSION (session));
	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *) fc, system, user);
	g_free (system);

	deleted = e_rule_context_delete_uri (
		(ERuleContext *) fc, uri, g_str_equal);
	if (deleted) {
		EAlert *alert;
		GtkWidget *button;
		GString *s;
		guint s_count;
		gchar *info;
		GList *l;

		s = g_string_new ("");
		s_count = 0;
		for (l = deleted; l; l = l->next) {
			const gchar *name = (const gchar *) l->data;

			if (s_count == 0) {
				g_string_append (s, name);
			} else {
				if (s_count == 1) {
					g_string_prepend (s, "    ");
					g_string_append_c (s, '\n');
				}
				g_string_append_printf (s, "    %s\n", name);
			}
			s_count++;
		}

		info = g_strdup_printf (ngettext (
			/* Translators: The first %s is name of the affected
			 * filter rule(s), the second %s is URI of the removed
			 * folder. For more than one filter rule is each of
			 * them on a separate line, with four spaces in front
			 * of its name, without quotes. */
			"The filter rule “%s” has been modified to account "
			"for the deleted folder\n“%s”.",
			"The following filter rules\n%s have been modified "
			"to account for the deleted folder\n“%s”.",
			s_count), s->str, folder_name);

		alert = e_alert_new ("mail:filter-updated", info, NULL);

		button = gtk_button_new_with_mnemonic (_("Open Message Filters"));
		gtk_widget_show (button);
		g_signal_connect (button, "clicked",
			G_CALLBACK (mail_autofilter_open_filters_clicked_cb), NULL);

		e_alert_add_widget (alert, button);

		e_alert_sink_submit_alert (alert_sink, alert);
		g_object_unref (alert);
		g_string_free (s, TRUE);
		g_free (info);

		if (e_rule_context_save ((ERuleContext *) fc, user) == -1)
			g_warning ("Could not write out changed filter rules\n");
		e_rule_context_free_uri_list ((ERuleContext *) fc, deleted);
	}

	g_free (user);
	g_object_unref (fc);
	g_free (uri);

	g_object_unref (session);
}
