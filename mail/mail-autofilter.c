/*
 * Code for autogenerating rules or filters from a message.
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-session.h"
#include "em-utils.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-util-private.h"

#include "em-vfolder-context.h"
#include "em-vfolder-rule.h"
#include "em-vfolder-editor.h"

#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-filter-editor.h"
#include "filter/e-filter-option.h"
#include "filter/e-filter-input.h"

#define d(x)

static void
rule_match_recipients (ERuleContext *context, EFilterRule *rule, CamelInternetAddress *iaddr)
{
	EFilterPart *part;
	EFilterElement *element;
	gint i;
	const gchar *real, *addr;
	gchar *namestr;

	/* address types etc should handle multiple values */
	for (i = 0; camel_internet_address_get (iaddr, i, &real, &addr); i++) {
		part = e_rule_context_create_part (context, "to");
		e_filter_rule_add_part ((EFilterRule *)rule, part);
		element = e_filter_part_find_element (part, "recipient-type");
		e_filter_option_set_current ((EFilterOption *)element, "contains");
		element = e_filter_part_find_element (part, "recipient");
		e_filter_input_set_value ((EFilterInput *)element, addr);

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
			p = s+2;
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

#if 0
gint
reg_match (gchar *str, gchar *regstr)
{
	regex_t reg;
	gint error;
	gint ret;

	error = regcomp(&reg, regstr, REG_EXTENDED|REG_ICASE|REG_NOSUB);
	if (error != 0) {
		return 0;
	}
	error = regexec(&reg, str, 0, NULL, 0);
	regfree(&reg);
	return (error == 0);
}
#endif

static void
rule_add_subject (ERuleContext *context, EFilterRule *rule, const gchar *text)
{
	EFilterPart *part;
	EFilterElement *element;

	/* dont match on empty strings ever */
	if (*text == 0)
		return;
	part = e_rule_context_create_part (context, "subject");
	e_filter_rule_add_part ((EFilterRule *)rule, part);
	element = e_filter_part_find_element (part, "subject-type");
	e_filter_option_set_current ((EFilterOption *)element, "contains");
	element = e_filter_part_find_element (part, "subject");
	e_filter_input_set_value ((EFilterInput *)element, text);
}

static void
rule_add_sender (ERuleContext *context, EFilterRule *rule, const gchar *text)
{
	EFilterPart *part;
	EFilterElement *element;

	/* dont match on empty strings ever */
	if (*text == 0)
		return;
	part = e_rule_context_create_part (context, "sender");
	e_filter_rule_add_part ((EFilterRule *)rule, part);
	element = e_filter_part_find_element (part, "sender-type");
	e_filter_option_set_current ((EFilterOption *)element, "contains");
	element = e_filter_part_find_element (part, "sender");
	e_filter_input_set_value ((EFilterInput *)element, text);
}

/* do a bunch of things on the subject to try and detect mailing lists, remove
   unneeded stuff, etc */
static void
rule_match_subject (ERuleContext *context, EFilterRule *rule, const gchar *subject)
{
	const gchar *s;
	const gchar *s1, *s2;
	gchar *tmp;

	s = strip_re (subject);
	/* dont match on empty subject */
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
		memcpy (tmp, s, s1-s);
		tmp[s1 - s] = 0;
		g_strstrip (tmp);
		rule_add_subject (context, rule, tmp);
		s = s1+1;
	}

	/* just lump the rest together */
	tmp = g_alloca (strlen (s) + 1);
	strcpy (tmp, s);
	g_strstrip (tmp);
	rule_add_subject (context, rule, tmp);
}

static void
rule_match_mlist(ERuleContext *context, EFilterRule *rule, const gchar *mlist)
{
	EFilterPart *part;
	EFilterElement *element;

	if (mlist[0] == 0)
		return;

	part = e_rule_context_create_part(context, "mlist");
	e_filter_rule_add_part(rule, part);

	element = e_filter_part_find_element(part, "mlist-type");
	e_filter_option_set_current((EFilterOption *)element, "is");

	element = e_filter_part_find_element (part, "mlist");
	e_filter_input_set_value((EFilterInput *)element, mlist);
}

static void
rule_from_address (EFilterRule *rule, ERuleContext *context, CamelInternetAddress* addr, gint flags)
{
	rule->grouping = E_FILTER_GROUP_ANY;

	if (flags & AUTO_FROM) {
		const gchar *name, *address;
		gchar *namestr;

		camel_internet_address_get (addr, 0, &name, &address);
		rule_add_sender (context, rule, address);
		if (name == NULL || name[0] == '\0')
			name = address;
		namestr = g_strdup_printf(_("Mail from %s"), name);
		e_filter_rule_set_name (rule, namestr);
		g_free (namestr);
	}
	if (flags & AUTO_TO) {
		rule_match_recipients (context, rule, addr);
	}

}

static void
rule_from_message (EFilterRule *rule, ERuleContext *context, CamelMimeMessage *msg, gint flags)
{
	CamelInternetAddress *addr;

	rule->grouping = E_FILTER_GROUP_ANY;

	if (flags & AUTO_SUBJECT) {
		const gchar *subject = msg->subject ? msg->subject : "";
		gchar *namestr;

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
		for (i = 0; from && camel_internet_address_get (from, i, &name, &address); i++) {
			rule_add_sender(context, rule, address);
			if (name == NULL || name[0] == '\0')
				name = address;
			namestr = g_strdup_printf(_("Mail from %s"), name);
			e_filter_rule_set_name (rule, namestr);
			g_free (namestr);
		}
	}
	if (flags & AUTO_TO) {
		addr = (CamelInternetAddress *)camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_TO);
		if (addr)
			rule_match_recipients (context, rule, addr);
		addr = (CamelInternetAddress *)camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_CC);
		if (addr)
			rule_match_recipients (context, rule, addr);
	}
	if (flags & AUTO_MLIST) {
		gchar *name, *mlist;

		mlist = camel_header_raw_check_mailing_list (&((CamelMimePart *)msg)->headers);
		if (mlist) {
			rule_match_mlist(context, rule, mlist);
			name = g_strdup_printf (_("%s mailing list"), mlist);
			e_filter_rule_set_name(rule, name);
			g_free(name);
		}
		g_free(mlist);
	}
}

EFilterRule *
em_vfolder_rule_from_message (EMVFolderContext *context, CamelMimeMessage *msg, gint flags, const gchar *source)
{
	EMVFolderRule *rule;
	gchar *euri = em_uri_from_camel(source);

	rule = em_vfolder_rule_new ();
	em_vfolder_rule_add_source (rule, euri);
	rule_from_message ((EFilterRule *)rule, (ERuleContext *)context, msg, flags);
	g_free(euri);

	return (EFilterRule *)rule;
}

EFilterRule *
em_vfolder_rule_from_address (EMVFolderContext *context, CamelInternetAddress *addr, gint flags, const gchar *source)
{
	EMVFolderRule *rule;
	gchar *euri = em_uri_from_camel(source);

	rule = em_vfolder_rule_new ();
	em_vfolder_rule_add_source (rule, euri);
	rule_from_address ((EFilterRule *)rule, (ERuleContext *)context, addr, flags);
	g_free(euri);

	return (EFilterRule *)rule;
}

EFilterRule *
filter_rule_from_message (EMFilterContext *context, CamelMimeMessage *msg, gint flags)
{
	EMFilterRule *rule;
	EFilterPart *part;

	rule = em_filter_rule_new ();
	rule_from_message ((EFilterRule *)rule, (ERuleContext *)context, msg, flags);

	part = em_filter_context_next_action (context, NULL);
	em_filter_rule_add_action (rule, e_filter_part_clone (part));

	return (EFilterRule *)rule;
}

void
filter_gui_add_from_message (CamelMimeMessage *msg, const gchar *source, gint flags)
{
	EMFilterContext *fc;
	const gchar *config_dir;
	gchar *user, *system;
	EFilterRule *rule;

	g_return_if_fail (msg != NULL);

	fc = em_filter_context_new ();
	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *)fc, system, user);
	g_free (system);

	rule = filter_rule_from_message (fc, msg, flags);

	e_filter_rule_set_source (rule, source);

	e_rule_context_add_rule_gui ((ERuleContext *)fc, rule, _("Add Filter Rule"), user);
	g_free (user);
	g_object_unref (fc);
}

void
mail_filter_rename_uri(CamelStore *store, const gchar *olduri, const gchar *newuri)
{
	EMFilterContext *fc;
	const gchar *config_dir;
	gchar *user, *system;
	GList *changed;
	gchar *eolduri, *enewuri;

	eolduri = em_uri_from_camel(olduri);
	enewuri = em_uri_from_camel(newuri);

	fc = em_filter_context_new ();
	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *)fc, system, user);
	g_free (system);

	changed = e_rule_context_rename_uri((ERuleContext *)fc, eolduri, enewuri, g_str_equal);
	if (changed) {
		d(printf("Folder rename '%s' -> '%s' changed filters, resaving\n", olduri, newuri));
		if (e_rule_context_save((ERuleContext *)fc, user) == -1)
			g_warning("Could not write out changed filter rules\n");
		e_rule_context_free_uri_list((ERuleContext *)fc, changed);
	}

	g_free(user);
	g_object_unref(fc);

	g_free(enewuri);
	g_free(eolduri);
}

void
mail_filter_delete_uri(CamelStore *store, const gchar *uri)
{
	EMFilterContext *fc;
	const gchar *config_dir;
	gchar *user, *system;
	GList *deleted;
	gchar *euri;

	euri = em_uri_from_camel(uri);

	fc = em_filter_context_new ();
	config_dir = mail_session_get_config_dir ();
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *)fc, system, user);
	g_free (system);

	deleted = e_rule_context_delete_uri ((ERuleContext *) fc, euri, g_str_equal);
	if (deleted) {
		GtkWidget *dialog;
		GString *s;
		guint s_count;
		gchar *info;
		GList *l;

		s = g_string_new("");
		s_count = 0;
		for (l = deleted; l; l = l->next) {
			const gchar *name = (const gchar *)l->data;

			if (s_count == 0) {
				g_string_append (s, name);
			} else {
				if (s_count == 1) {
					g_string_prepend (s, "    ");
					g_string_append (s, "\n");
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
			"The filter rule \"%s\" has been modified to account "
			"for the deleted folder\n\"%s\".",
			"The following filter rules\n%s have been modified "
			"to account for the deleted folder\n\"%s\".",
			s_count), s->str, euri);
		dialog = e_alert_dialog_new_for_args (e_shell_get_active_window (NULL), "mail:filter-updated", info, NULL);
		em_utils_show_info_silent (dialog);
		g_string_free (s, TRUE);
		g_free (info);

		d(printf("Folder delete/rename '%s' changed filters, resaving\n", euri));
		if (e_rule_context_save ((ERuleContext *) fc, user) == -1)
			g_warning ("Could not write out changed filter rules\n");
		e_rule_context_free_uri_list ((ERuleContext *) fc, deleted);
	}

	g_free(user);
	g_object_unref(fc);
	g_free(euri);
}
