/*
  code for autogenerating rules or filters from a message
*/

#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <bonobo.h>

#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-popup-menu.h>

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include "folder-browser.h"
#include "mail-vfolder.h"
#include "mail-autofilter.h"

#include "camel/camel.h"

#include "filter/vfolder-context.h"
#include "filter/vfolder-rule.h"
#include "filter/vfolder-editor.h"

#include "filter/filter-context.h"
#include "filter/filter-filter.h"
#include "filter/filter-editor.h"
#include "filter/filter-option.h"

static void
rule_match_recipients(RuleContext *context, FilterRule *rule, CamelInternetAddress *iaddr)
{
	FilterPart *part;
	FilterElement *element;
	int i;
	const char *real, *addr;
	char *namestr;

	/* address types etc should handle multiple values */
	for (i=0;camel_internet_address_get(iaddr, i, &real, &addr);i++) {
		part = rule_context_create_part(context, "to");
		filter_rule_add_part((FilterRule *)rule, part);
		element = filter_part_find_element(part, "recipient-type");
		filter_option_set_current((FilterOption *)element, "contains");
		element = filter_part_find_element(part, "recipient");
		filter_input_set_value((FilterInput *)element, addr);

		namestr = g_strdup_printf("Mail to %s", real&&real[0]?real:addr);
		filter_rule_set_name(rule, namestr);
		g_free(namestr);
	}
}


/* remove 're' part of a subject */
static const char *
strip_re(const char *s)
{
	const char *p;

	while (*s) {
		while (isspace(*s))
			s++;
		if (s[0] == 0)
			break;
		if ((s[0] == 'r' || s[0]=='R')
		    && (s[1] == 'e' || s[1]=='E')) {
			p = s+2;
			while (isdigit(*p) || (ispunct(*p) && (*p != ':')))
				p++;
			if (*p==':') {
				s = p+1;
			} else
				break;
		} else
			break;
	}
	return s;
}

#if 0
int
reg_match(char *str, char *regstr)
{
	regex_t reg;
	int error;
	int ret;

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
rule_add_subject(RuleContext *context, FilterRule *rule, const char *text)
{
	FilterPart *part;
	FilterElement *element;

	/* dont match on empty strings ever */
	if (*text == 0)
		return;
	part = rule_context_create_part(context, "subject");
	filter_rule_add_part((FilterRule *)rule, part);
	element = filter_part_find_element(part, "subject-type");
	filter_option_set_current((FilterOption *)element, "contains");
	element = filter_part_find_element(part, "subject");
	filter_input_set_value((FilterInput *)element, text);
}

static void
rule_add_sender(RuleContext *context, FilterRule *rule, const char *text)
{
	FilterPart *part;
	FilterElement *element;

	/* dont match on empty strings ever */
	if (*text == 0)
		return;
	part = rule_context_create_part(context, "sender");
	filter_rule_add_part((FilterRule *)rule, part);
	element = filter_part_find_element(part, "sender-type");
	filter_option_set_current((FilterOption *)element, "contains");
	element = filter_part_find_element(part, "sender");
	filter_input_set_value((FilterInput *)element, text);
}

/* do a bunch of things on the subject to try and detect mailing lists, remove
   unneeded stuff, etc */
static void
rule_match_subject(RuleContext *context, FilterRule *rule, const char *subject)
{
	const char *s;
	const char *s1, *s2;
	char *tmp;

	s = strip_re(subject);
	/* dont match on empty subject */
	if (*s == 0)
		return;

	/* [blahblah] is probably a mailing list, match on it separately */
	s1 = strchr(s, '[');
	s2 = strchr(s, ']');
	if (s1 && s2 && s1<s2) {
		/* probably a mailing list, match on the mailing list name */
		tmp = alloca(s2-s1+2);
		memcpy(tmp, s1, s2-s1+1);
		tmp[s2-s1+1] = 0;
		g_strstrip(tmp);
		rule_add_subject(context, rule, tmp);
		s = s2+1;
	}
	/* Froblah: at the start is probably something important (e.g. bug number) */
	s1 = strchr(s, ':');
	if (s1) {
		tmp = alloca(s1-s+1);
		memcpy(tmp, s, s1-s);
		tmp[s1-s] = 0;
		g_strstrip(tmp);
		rule_add_subject(context, rule, tmp);
		s = s1+1;
	}

	/* just lump the rest together */
	tmp = alloca(strlen(s)+1);
	strcpy(tmp, s);
	g_strstrip(tmp);
	rule_add_subject(context, rule, tmp);
}

static void
rule_from_message(FilterRule *rule, RuleContext *context, CamelMimeMessage *msg, int flags)
{
	CamelInternetAddress *addr;

	rule->grouping = FILTER_GROUP_ANY;

	if (flags & AUTO_SUBJECT) {
		rule_match_subject(context, rule, msg->subject);
		filter_rule_set_name(rule, strip_re(msg->subject));
	}
	/* should parse the from address into an internet address? */
	if (flags & AUTO_FROM) {
		struct _header_address *haddr, *scan;
		char *name, *namestr;

		haddr = header_address_decode(msg->from);
		scan = haddr;
		while (scan) {
			if (scan->type == HEADER_ADDRESS_NAME) {
				rule_add_sender(context, rule, scan->v.addr);
				if (scan->name)
					name = scan->name;
				else
					name = scan->v.addr;
				namestr = g_strdup_printf("Mail from %s", name);
				filter_rule_set_name(rule, namestr);
				g_free(namestr);
			}
			scan = scan->next;
		}
		header_address_unref(haddr);
	}
	if (flags & AUTO_TO) {
		addr = (CamelInternetAddress *)camel_mime_message_get_recipients(msg, CAMEL_RECIPIENT_TYPE_TO);
		rule_match_recipients(context, rule, addr);
		addr = (CamelInternetAddress *)camel_mime_message_get_recipients(msg, CAMEL_RECIPIENT_TYPE_CC);
		rule_match_recipients(context, rule, addr);
	}
}

FilterRule *
vfolder_rule_from_message(VfolderContext *context, CamelMimeMessage *msg, int flags, const char *source)
{
	VfolderRule *rule;

	rule = vfolder_rule_new();
	vfolder_rule_add_source(rule, source);
	rule_from_message((FilterRule *)rule, (RuleContext *)context, msg, flags);

	return (FilterRule *)rule;
}

FilterRule *
filter_rule_from_message(FilterContext *context, CamelMimeMessage *msg, int flags)
{
	FilterFilter *rule;

	rule = filter_filter_new();
	rule_from_message((FilterRule *)rule, (RuleContext *)context, msg, flags);

	/* should we define the default action? */

	return (FilterRule *)rule;
}

void
filter_gui_add_from_message(CamelMimeMessage *msg, int flags)
{
	FilterContext *fc;
	char *userrules, *systemrules;
	FilterRule *rule;
	extern char *evolution_dir;

	fc = filter_context_new();
	userrules = g_strdup_printf("%s/filters.xml", evolution_dir);
	systemrules = g_strdup_printf("%s/evolution/filtertypes.xml", EVOLUTION_DATADIR);
	rule_context_load((RuleContext *)fc, systemrules, userrules);
	rule = filter_rule_from_message(fc, msg, flags);
	rule_context_add_rule_gui((RuleContext *)fc, rule, "Add Filter Rule", userrules);
	g_free (userrules);
	g_free (systemrules);
	gtk_object_unref((GtkObject *)fc);
}

