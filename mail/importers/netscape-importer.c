/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* netscape-importer.c
 * 
 * Authors: 
 *    Iain Holmes  <iain@ximian.com>
 *    Christian Kreibich <cK@whoop.org> (email filter import)
 *
 * Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>

#include <glib.h>
#include <gnome.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <camel/camel-operation.h>

#include <bonobo/bonobo-control.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <importer/evolution-importer-client.h>

#include <filter/filter-context.h>
#include <filter/filter-filter.h>
#include <filter/filter-rule.h>
#include <filter/filter-option.h>
#include <filter/filter-folder.h>
#include <filter/filter-int.h>

#include "e-util/e-account-list.h"
#include "e-util/e-signature-list.h"

#include "mail/mail-mt.h"
#include "mail/mail-config.h"
#include "mail/em-utils.h"
#include "mail/mail-component.h"

#include "mail-importer.h"

/* FIXME: dont make this stuff global */
static char *nsmail_dir = NULL;
static GHashTable *user_prefs = NULL;

static char          *filter_name = N_("Priority Filter \"%s\"");

#define MAIL_CONFIG_IID "OAFIID:GNOME_Evolution_MailConfig:" BASE_VERSION

#define KEY "netscape-mail-imported"

/*#define SUPER_IMPORTER_DEBUG*/
#ifdef SUPER_IMPORTER_DEBUG
#define d(x) x
#else
#define d(x)
#endif

#define MAXLEN 4096

typedef struct {
	EvolutionIntelligentImporter *ii;

	GMutex *status_lock;
	char *status_what;
	int status_pc;
	int status_timeout_id;
	CamelOperation *cancel;	/* cancel/status port */

	GtkWidget *mail;
	GtkWidget *filters;
	GtkWidget *settings;

	gboolean do_mail;
	gboolean done_mail;

	gboolean do_filters;
	gboolean done_filters;
	gboolean do_settings;
	gboolean done_settings;

	/* GUI */
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *progressbar;
} NsImporter;


/* Email filter datastructures  ---------------------------------------------- */


typedef enum {
	MOVE_TO_FOLDER, CHANGE_PRIORITY, DELETE,
	MARK_READ, IGNORE_THREAD, WATCH_THREAD
} NsFilterActionType;

static char* ns_filter_action_types[] =
{
	"Move to folder", "Change priority", "Delete",
	"Mark read", "Ignore thread", "Watch thread"
};


typedef enum {
	HIGHEST, HIGH, NORMAL, LOW, LOWEST, FREE, NONE
} NsFilterActionValueType;

static char *ns_filter_action_value_types[] =
{
	"Highest", "High", "Normal", "Low", "Lowest"
};


typedef enum {
	FROM, SUBJECT, TO, CC, TO_OR_CC, BODY, DATE, PRIORITY,
	STATUS, AGE_IN_DAYS, X_MSG_HEADER
} NsFilterConditionType;

static char *ns_filter_condition_types[] =
{
	"from", "subject", "to", "CC", "to or CC", "body", "date",
	"priority", "age in days"
};


typedef enum {
	CONTAINS, CONTAINS_NOT, IS, IS_NOT, BEGINS_WITH, ENDS_WITH,
	IS_BEFORE, IS_AFTER, IS_GREATER_THAN, IS_LESS_THAN, READ,
	REPLIED, IS_HIGHER_THAN, IS_LOWER_THAN
} NsFilterConditionPropertyType;

static char *ns_filter_condition_property_types[] =
{
	"contains", "doesn't contain", "is", "isn't", "begins with",
	"ends with", "is before", "is after", "is greater than",
        "is less than", "read", "replied", "is higher than",
        "is lower than"
};


typedef struct
{
	NsFilterConditionType          type;
	NsFilterConditionPropertyType  prop;
	NsFilterActionValueType        prop_val_id;  /* for dealing with priority levels */	
	char                          *prop_val_str;
} NsFilterCondition;

typedef struct {
	char                     *name;
	char                     *description;

	gboolean                  enabled;

	NsFilterActionType        action;
	NsFilterActionValueType   action_val_id;
	char                     *action_val_str;

	enum _filter_grouping_t   grouping;
	GList                    *conditions; /* List of NSFilterConditions */
} NsFilter;

/* Prototypes ------------------------------------------------------------- */
static void  netscape_filter_cleanup (NsFilter *nsf);
static const char *fix_netscape_folder_names (const char *original_name);

/* Email filter stuff ----------------------------------------------------- */

static gboolean
netscape_filter_flatfile_get_entry (FILE *f, char *key, char *val)
{
	char line[MAXLEN];
	char *ptr = NULL;
	char *ptr2 = NULL;

	if (fgets (line, MAXLEN, f)) {
		
		ptr = strchr(line, '=');
		*ptr = '\0';

		memcpy (key, line, strlen(line)+1);

		ptr += 2; /* Skip '=' and '"' */
		ptr2 = strrchr (ptr, '"');
		*ptr2 = '\0';
		
		memcpy (val, ptr, strlen(ptr)+1);

		d(g_warning ("Parsing key/val '%s' '%s'", key, val));
		return TRUE;
		       
	}

	*key = '\0'; *val = '\0';
	return FALSE;
}

/* This function parses the filtering condition strings.
   Netscape describes the conditions that determine when
   to apply a filter through a string of the form

         " OR (type, property, value) OR (type, property, value) ... "

   or 
         " AND (type, property, value) AND (type, property, value) ... "

   where type can be "subject", "from", "to", "CC" etc, property
   is "contains" etc, and value is the according pattern.
*/
static void
netscape_filter_parse_conditions (NsFilter *nsf, FILE *f, char *condition)
{
	char *ptr = condition, *ptr2 = NULL;
	char  type[MAXLEN];
	char  prop[MAXLEN];
	char  val[MAXLEN];
	NsFilterCondition *cond;

	if ( (ptr = strstr (condition, "OR")) == NULL) {
		nsf->grouping = FILTER_GROUP_ALL;
	} else {
		nsf->grouping = FILTER_GROUP_ANY;
	}

	ptr = condition;
	while ( (ptr = strchr (ptr, '(')) != NULL) {

		/* Move ptr to start of type */
		ptr++; 

		/* Move ptr2 up to next comma: */
		if ( (ptr2 = strchr (ptr,  ',')) == NULL)
			continue;

		memcpy (type, ptr, ptr2-ptr);
		type[ptr2-ptr] = '\0';

		/* Move ptr to start of property */
		ptr = ptr2 + 1;

		/* Move ptr2 up to next comma: */
		if ( (ptr2 = strchr (ptr,  ',')) == NULL)
			continue;

		memcpy (prop, ptr, ptr2-ptr);
		prop[ptr2-ptr] = '\0';

		/* Move ptr to start of value */
		ptr = ptr2 + 1;

		/* Move ptr2 to end of value: */
		if ( (ptr2 = strchr (ptr,  ')')) == NULL)
			continue;
		
		memcpy (val, ptr, ptr2-ptr);
		val[ptr2-ptr] = '\0';

		cond = g_new0 (NsFilterCondition, 1);

		if (!strcmp (type, ns_filter_condition_types[FROM])) {
			cond->type = FROM;
		} else if (!strcmp (type, ns_filter_condition_types[SUBJECT])) {
			cond->type = SUBJECT;
		} else if (!strcmp (type, ns_filter_condition_types[TO])) {
			cond->type = TO;
		} else if (!strcmp (type, ns_filter_condition_types[CC])) {
			cond->type = CC;
		} else if (!strcmp (type, ns_filter_condition_types[TO_OR_CC])) {
			cond->type = TO_OR_CC;
		} else if (!strcmp (type, ns_filter_condition_types[BODY])) {
			cond->type = BODY;
		} else if (!strcmp (type, ns_filter_condition_types[DATE])) {
			cond->type = DATE;
		} else if (!strcmp (type, ns_filter_condition_types[PRIORITY])) {
			cond->type = PRIORITY;
		} else if (!strcmp (type, ns_filter_condition_types[STATUS])) {
			cond->type = STATUS;
		} else if (!strcmp (type, ns_filter_condition_types[AGE_IN_DAYS])) {
			cond->type = AGE_IN_DAYS;
		} else if (!strcmp (type, ns_filter_condition_types[X_MSG_HEADER])) {
			cond->type = X_MSG_HEADER;
		} else {
			d(g_warning ("Unknown condition type '%s' encountered -- skipping.", type));
			g_free (cond);
			continue;
		}


		if (!strcmp (prop, ns_filter_condition_property_types[CONTAINS])) {
			cond->prop = CONTAINS;
		} else if (!strcmp (prop, ns_filter_condition_property_types[CONTAINS_NOT])) {
			cond->prop = CONTAINS_NOT;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS])) {
			cond->prop = IS;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_NOT])) {
			cond->prop = IS_NOT;
		} else if (!strcmp (prop, ns_filter_condition_property_types[BEGINS_WITH])) {
			cond->prop = BEGINS_WITH;
		} else if (!strcmp (prop, ns_filter_condition_property_types[ENDS_WITH])) {
			cond->prop = ENDS_WITH;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_BEFORE])) {
			cond->prop = IS_BEFORE;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_AFTER])) {
			cond->prop = IS_AFTER;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_GREATER_THAN])) {
			cond->prop = IS_GREATER_THAN;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_LESS_THAN])) {
			cond->prop = IS_LESS_THAN;
		} else if (!strcmp (prop, ns_filter_condition_property_types[READ])) {
			cond->prop = READ;
		} else if (!strcmp (prop, ns_filter_condition_property_types[REPLIED])) {
			cond->prop = REPLIED;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_HIGHER_THAN])) {
			cond->prop = IS_HIGHER_THAN;
		} else if (!strcmp (prop, ns_filter_condition_property_types[IS_LOWER_THAN])) {
			cond->prop = IS_LOWER_THAN;
		} else {
			d(g_warning ("Unknown condition property '%s' encountered -- skipping.", prop));
			g_free (cond);
			continue;
		}

		cond->prop_val_id = FREE;

		if (!strcmp (val, ns_filter_action_value_types[LOWEST])) {
			cond->prop_val_id = LOWEST;
		} else if (!strcmp (val, ns_filter_action_value_types[LOW])) {
			cond->prop_val_id = LOW;
		} else if (!strcmp (val, ns_filter_action_value_types[NORMAL])) {
			cond->prop_val_id = NORMAL;
		} else if (!strcmp (val, ns_filter_action_value_types[HIGH])) {
			cond->prop_val_id = HIGH;
		} else if (!strcmp (val, ns_filter_action_value_types[HIGHEST])) {
			cond->prop_val_id = HIGHEST;
		}

		cond->prop_val_str = g_strdup (val);
		nsf->conditions = g_list_append (nsf->conditions, cond);		
	}
}

static NsFilter *
netscape_filter_read_next (FILE *mailrule_handle)
{
	NsFilter *nsf;
	char      key[MAXLEN];
	char      val[MAXLEN];	

	key[0] = '\0';

	for ( ; ; ) {

		/* Skip stuff at the beginning, until beginning of next filter
		   is read: */
	   
		do {	    
			if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
				return NULL;

		} while (strcmp(key, "name"));
	
		nsf = g_new0 (NsFilter, 1);
		nsf->name = g_strdup (val);


		/* Read value for "enabled" setting */
		
		if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
			goto cleanup;
		if (strcmp (key, "enabled")) {
			goto cleanup;
		}
		if (strcmp (val, "true"))
			nsf->enabled = TRUE;
		else
			nsf->enabled = FALSE;


		/* Read filter description */

		if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
			goto cleanup;
		if (strcmp (key, "description")) {
			goto cleanup;
		}
		nsf->description = g_strdup (val);


		/* Skip one line -- it's a "type" entry and always seems to be "1"? */

		if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
			goto cleanup;
		if (strcmp (key, "type")) {
			goto cleanup;
		}

		/* Read filter action and handle action value accordingly */
		
		if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
			goto cleanup;
		if (strcmp (key, "action")) {
			goto cleanup;
		}
		if (!strcmp (val, ns_filter_action_types[MOVE_TO_FOLDER])) {

			if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
				goto cleanup;
			if (strcmp (key, "actionValue")) {
				goto cleanup;
			}
			nsf->action = MOVE_TO_FOLDER;
			nsf->action_val_id = FREE;
			nsf->action_val_str = g_strdup(val);
		}	
		else if (!strcmp (val, ns_filter_action_types[CHANGE_PRIORITY])) {

			if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
				goto cleanup;
			if (strcmp (key, "actionValue")) {
				goto cleanup;
			}

			nsf->action = CHANGE_PRIORITY;

			if (!strcmp (val, ns_filter_action_value_types[LOWEST])) {
				nsf->action_val_id = LOWEST;
			} else if (!strcmp (val, ns_filter_action_value_types[LOW])) {
				nsf->action_val_id = LOW;
			} else if (!strcmp (val, ns_filter_action_value_types[NORMAL])) {
				nsf->action_val_id = NORMAL;
			} else if (!strcmp (val, ns_filter_action_value_types[HIGH])) {
				nsf->action_val_id = HIGH;
			} else if (!strcmp (val, ns_filter_action_value_types[HIGHEST])) {
				nsf->action_val_id = HIGHEST;
			} else {
				d(g_warning ("Unknown Netscape filter action value '%s' for action '%s'",
					     val, ns_filter_action_types[CHANGE_PRIORITY]));
				goto cleanup;
			}

			nsf->action_val_str = NULL;
			
		}	
		else if (!strcmp (val, ns_filter_action_types[DELETE])) {

			nsf->action = DELETE;
			nsf->action_val_id = NONE;
		}	
		else if (!strcmp (val, ns_filter_action_types[MARK_READ])) {

			nsf->action = MARK_READ;	
			nsf->action_val_id = NONE;
		}
		else if (!strcmp (val, ns_filter_action_types[IGNORE_THREAD])) {

			nsf->action = IGNORE_THREAD;	
			nsf->action_val_id = NONE;
		}
		else if (!strcmp (val, ns_filter_action_types[WATCH_THREAD])) {

			nsf->action = WATCH_THREAD;		 
			nsf->action_val_id = NONE;
		}
		else {
			d(g_warning ("Unknown Netscape filter action '%s'", val));
			goto cleanup;
		}
		        

		/* Read conditions, the fun part ... */

		if (!netscape_filter_flatfile_get_entry (mailrule_handle, key, val))
			goto cleanup;
		if (strcmp (key, "condition")) {
			goto cleanup;		
		}
		netscape_filter_parse_conditions (nsf, mailrule_handle, val);

		return nsf;

	cleanup:
		netscape_filter_cleanup (nsf);
	}

	return NULL;
}

static void
netscape_filter_cleanup (NsFilter *nsf)
{
	GList *l;

	g_free (nsf->name);
	g_free (nsf->description);
	g_free (nsf->action_val_str);

	for (l = nsf->conditions; l; l = l->next) {

		NsFilterCondition *cond = (NsFilterCondition *)l->data;

		g_free (cond->prop_val_str);
		g_free (cond);
	}

	g_list_free (nsf->conditions);
	g_free (nsf);
}

static gboolean
netscape_filter_set_opt_for_cond (NsFilterCondition *cond, FilterOption* op)
{
	switch (cond->prop) {
	case CONTAINS:
		filter_option_set_current (op, "contains");
		break;
	case CONTAINS_NOT:
		filter_option_set_current (op, "does not contain");
		break;
	case IS:
		filter_option_set_current (op, "is");
		break;
	case IS_NOT:
		filter_option_set_current (op, "is not");
		break;
	case BEGINS_WITH:
		filter_option_set_current (op, "starts with");
		break;
	case ENDS_WITH:
		filter_option_set_current (op, "ends with");
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

/* Translates a string of the form   
   folder1.sbd/folder2.sbd/.../folderN.sbd/folder

   into one that looks like this:

   folder1/folder2/.../folderN/folder
*/
static char*
netscape_filter_strip_sbd (char *ns_folder)
{
	char *folder_copy;
	char s[MAXLEN];
	char *ptr, *ptr2;
	const char *fixed_folder;
	
	folder_copy = g_strdup (ns_folder);
	ptr = folder_copy;
	s[0] = '\0';

	while (ptr) {
		if ( (ptr2 = strstr (ptr, ".sbd")) == NULL)
			break;

		*ptr2 = '\0';
		strcat (s, ptr);

		ptr = ptr2 + 4; /* skip ".sbd" */
	}

	fixed_folder = fix_netscape_folder_names (ptr);
	strcat (s, fixed_folder);
	g_free (folder_copy);

	d(g_warning ("Stripped '%s' to '%s'", ns_folder, s));
	
	return g_strdup (s);
}


static char *
netscape_filter_map_folder_to_uri (char *folder)
{
	char *folder_copy;
	char *ptr, *ptr2;
	GString *s;

	folder_copy = g_strdup (folder);
	ptr = folder_copy;

	/* FIXME: this should use account-relative uri's */

	s = g_string_new("mbox:");
	g_string_append(s, g_get_home_dir());
	g_string_append(s, "/mail/local#");

	while (ptr) {
		if ( (ptr2 = strchr (ptr, '/')) == NULL)
			break;

		*ptr2 = '\0';
		g_string_append(s, ptr);
		g_string_append_c(s, '/');

		ptr = ptr2 + 1;
	}

	g_string_append(s, ptr);
	g_free (folder_copy);

	d(g_warning ("Mapped '%s' to '%s'", folder, s));
	
	ptr = s->str;
	g_string_free(s, FALSE);

	return ptr;
}


static void
netscape_filter_change_priority_warning (void)
{
	GtkWidget *dialog;
	static gboolean already_shown = FALSE;

	if (!already_shown) {
		already_shown = TRUE;
		dialog = gnome_ok_dialog (_("Some of your Netscape email filters are based on\n"
					    "email priorities, which are not used in Evolution.\n"
					    "Instead, Evolution provides scores in the range of\n"
					    "-3 to 3 that can be assigned to emails and filtered\n"
					    "accordingly.\n"
					    "\n"
					    "As a workaround, a set of filters called \"Priority Filter\"\n"
					    "was added that converts Netscape's email priorities into\n"
					    "Evolution's scores, and the affected filters use scores instead\n"
					    "of priorities. Check the imported filters to make sure\n"
					    "everything still works as intended."));
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
}


static void
netscape_filter_threads_action_not_supported (void)
{
	GtkWidget *dialog;
	static gboolean already_shown = FALSE;

	if (!already_shown) {
		already_shown = TRUE;
		dialog = gnome_ok_dialog (_("Some of your Netscape email filters use\n"
					    "the \"Ignore Thread\" or \"Watch Thread\"\n"
					    "feature, which is not supported in Evolution.\n"
					    "These filters will be dropped."));
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
}


static void
netscape_filter_body_is_not_supported (void)
{
	GtkWidget *dialog;
	static gboolean already_shown = FALSE;

	if (!already_shown) {
		already_shown = TRUE;
		dialog = gnome_ok_dialog (_("Some of your Netscape email filters test the\n"
					    "body of emails for (in)equality to a given string,\n"
					    "which is not supported in Evolution. Those filters\n"
					    "were modified to test whether that string is or is not\n"
					    "contained in the message body."));
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
}


static FilterRule*
netscape_create_priority_converter (FilterContext *fc, NsFilterActionValueType priority)
{
	FilterFilter *ff;
	FilterPart   *fp;
	FilterRule   *fr;
	FilterElement *el;
	char           s[MAXLEN];
	int v;

	ff = filter_filter_new ();
	fr = FILTER_RULE(ff);
	
	g_snprintf (s, MAXLEN, filter_name, ns_filter_action_value_types[priority]);
	filter_rule_set_name (fr, s);
	filter_rule_set_source (fr, FILTER_SOURCE_INCOMING);

	fp = rule_context_create_part (RULE_CONTEXT(fc), "header");
	filter_rule_add_part (fr, fp);
	el = filter_part_find_element (fp, "header-field");
	filter_input_set_value ((FilterInput*)el, "X-Priority");
	el = filter_part_find_element (fp, "header-type");
	filter_option_set_current ((FilterOption*)el, "contains");
	el = filter_part_find_element (fp, "word");
	filter_input_set_value ((FilterInput*)el,
				ns_filter_action_value_types[priority]);
	
	fp = filter_context_create_action (fc, "score");
	el = filter_part_find_element (fp, "score");

	switch (priority) {
	case LOWEST:		
		v = -2;
		break;
	case LOW:
		v = -1;
		break;
	case NORMAL:
		v = 0;
		break;
	case HIGH:
		v = 1;
		break;
	case HIGHEST:
		v = 2;
		break;
	default:
		g_object_unref((ff));
		return NULL;
	}

	filter_int_set_value((FilterInt *)el, v);
	filter_filter_add_action (ff, fp);

	return FILTER_RULE(ff);
}


static void
netscape_add_priority_workaround_filters (FilterContext *fc)
{
	FilterRule *fr;

	fr = netscape_create_priority_converter (fc, LOWEST);
	rule_context_add_rule (RULE_CONTEXT(fc), FILTER_RULE(fr));
	rule_context_rank_rule (RULE_CONTEXT(fc), FILTER_RULE(fr), FILTER_SOURCE_INCOMING, 0);

	fr = netscape_create_priority_converter (fc, LOW);
	rule_context_add_rule (RULE_CONTEXT(fc), FILTER_RULE(fr));
	rule_context_rank_rule (RULE_CONTEXT(fc), FILTER_RULE(fr), FILTER_SOURCE_INCOMING, 1);

	fr = netscape_create_priority_converter (fc, HIGH);
	rule_context_add_rule (RULE_CONTEXT(fc), FILTER_RULE(fr));
	rule_context_rank_rule (RULE_CONTEXT(fc), FILTER_RULE(fr), FILTER_SOURCE_INCOMING, 2);

	fr = netscape_create_priority_converter (fc, HIGHEST);
	rule_context_add_rule (RULE_CONTEXT(fc), FILTER_RULE(fr));
	rule_context_rank_rule (RULE_CONTEXT(fc), FILTER_RULE(fr), FILTER_SOURCE_INCOMING, 3);
}


static gboolean
netscape_filter_score_set (NsFilterCondition *cond, FilterInt *el)
{
	int v;

	switch (cond->prop_val_id) {
	case LOWEST:
		v = -2;
		break;
	case LOW:
		v = -1;
		break;
	case NORMAL:
		v = 0;
		break;
	case HIGH:
		v = 1;
		break;
	case HIGHEST:
		v = 2;
		break;
	default:
		return FALSE;
	}

	filter_int_set_value(el, v);

	return TRUE;
}


static FilterFilter *
netscape_filter_to_evol_filter (FilterContext *fc, NsFilter *nsf, gboolean *priority_needed)
{
	RuleContext  *rc = RULE_CONTEXT(fc);
	FilterFilter *ff = NULL;
	FilterPart   *fp;
	FilterRule   *fr;
	FilterElement *el;
	GList        *l;
	gboolean      part_added = FALSE, action_added = FALSE;


	ff = filter_filter_new ();
	fr = FILTER_RULE(ff);

	filter_rule_set_name (fr, nsf->name);
	filter_rule_set_source (fr, FILTER_SOURCE_INCOMING);
	fr->grouping = nsf->grouping;
	

	/* build and add partset */
	
	for (l = nsf->conditions; l; l = l->next) {
		
		NsFilterCondition *cond = (NsFilterCondition*) l->data;

		fp = NULL;

		switch (cond->type) {
		case FROM:
			fp = rule_context_create_part (rc, "sender");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "sender-type");

			if (!netscape_filter_set_opt_for_cond (cond, (FilterOption*)el)) {
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}

			el = filter_part_find_element (fp, "sender");
			filter_input_set_value ((FilterInput *)el, cond->prop_val_str);
			part_added = TRUE;
			break;

		case SUBJECT:
			fp = rule_context_create_part (rc, "subject");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "subject-type");

			if (!netscape_filter_set_opt_for_cond (cond, (FilterOption*)el)) {
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}

			el = filter_part_find_element (fp, "subject");
			filter_input_set_value ((FilterInput *)el, cond->prop_val_str);
			part_added = TRUE;
			break;
		case TO:
		case CC:
		case TO_OR_CC:
			fp = rule_context_create_part (rc, "to");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "recipient-type");

			if (!netscape_filter_set_opt_for_cond (cond, (FilterOption*)el)) {
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}

			el = filter_part_find_element (fp, "recipient");
			filter_input_set_value ((FilterInput *)el, cond->prop_val_str);
			part_added = TRUE;
			break;
		case BODY:
			fp = rule_context_create_part (rc, "body");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "body-type");

			switch (cond->prop) {
			case CONTAINS:
				filter_option_set_current ((FilterOption*)el, "contains");
				break;
			case CONTAINS_NOT:
				filter_option_set_current ((FilterOption*)el, "not contains");
				break;
			case IS:
				netscape_filter_body_is_not_supported ();
				filter_option_set_current ((FilterOption*)el, "contains");
				break;
			case IS_NOT:
				netscape_filter_body_is_not_supported ();
				filter_option_set_current ((FilterOption*)el, "not contains");
				break;
			default:
				g_warning("Body rule dropped");
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}

			el = filter_part_find_element (fp, "word");
			filter_input_set_value ((FilterInput *)el, cond->prop_val_str);
			part_added = TRUE;
			break;
		case DATE:
			fp = rule_context_create_part (rc, "sent-date");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "date-spec-type");

			switch (cond->prop) {
			case IS:
				filter_option_set_current ((FilterOption*)el, "is");
				break;
			case IS_NOT:
				filter_option_set_current ((FilterOption*)el, "is-not");
				break;
			case IS_BEFORE:
				filter_option_set_current ((FilterOption*)el, "before");
				break;
			case IS_AFTER:
				filter_option_set_current ((FilterOption*)el, "after");
				break;
			default:
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}

			el = filter_part_find_element (fp, "versus");
			filter_input_set_value ((FilterInput *)el, cond->prop_val_str);
			part_added = TRUE;
			break;
		case PRIORITY:
			switch (cond->prop) {
			case IS:
				*priority_needed = TRUE;
				fp = rule_context_create_part (rc, "score");
				filter_rule_add_part (fr, fp);
				el = filter_part_find_element (fp, "score-type");
				filter_option_set_current ((FilterOption*)el, "is");
				el = filter_part_find_element (fp, "versus");

				if (!netscape_filter_score_set(cond, (FilterInt*)el)) {
					filter_rule_remove_part (fr, fp);
					g_object_unref((fp));
					continue;
				}

				break;
			case IS_NOT:
				*priority_needed = TRUE;
				fp = rule_context_create_part (rc, "score");
				filter_rule_add_part (fr, fp);
				el = filter_part_find_element (fp, "score-type");
				filter_option_set_current ((FilterOption*)el, "is-not");
				el = filter_part_find_element (fp, "versus");

				if (!netscape_filter_score_set(cond, (FilterInt*)el)) {
					filter_rule_remove_part (fr, fp);
					g_object_unref((fp));
					continue;
				}

				break;
			case IS_HIGHER_THAN:
				*priority_needed = TRUE;
				fp = rule_context_create_part (rc, "score");
				filter_rule_add_part (fr, fp);
				el = filter_part_find_element (fp, "score-type");
				filter_option_set_current ((FilterOption*)el, "greater-than");
				el = filter_part_find_element (fp, "versus");

				if (!netscape_filter_score_set(cond, (FilterInt*)el)) {
					filter_rule_remove_part (fr, fp);
					g_object_unref((fp));
					continue;
				}

				break;
			case IS_LOWER_THAN:
				*priority_needed = TRUE;
				fp = rule_context_create_part (rc, "score");
				filter_rule_add_part (fr, fp);
				el = filter_part_find_element (fp, "score-type");
				filter_option_set_current ((FilterOption*)el, "less-than");
				el = filter_part_find_element (fp, "versus");

				if (!netscape_filter_score_set(cond, (FilterInt*)el)) {
					filter_rule_remove_part (fr, fp);
					g_object_unref((fp));
					continue;
				}
				break;
			default:
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}
			part_added = TRUE;
			break;

		case STATUS:
			fp = rule_context_create_part (rc, "status");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "match-type");

			switch (cond->prop) {
			case IS:
				filter_option_set_current ((FilterOption*)el, "is");
				el = filter_part_find_element (fp, "flag");

				if (!strcmp (cond->prop_val_str,
					    ns_filter_condition_property_types[READ])) {
					filter_option_set_current ((FilterOption*)el, "Seen");
				} else if (!strcmp (cond->prop_val_str,
					   ns_filter_condition_property_types[REPLIED])) {
					filter_option_set_current ((FilterOption*)el, "Answered");
				}
				break;
			case IS_NOT:
				filter_option_set_current ((FilterOption*)el, "is not");
				el = filter_part_find_element (fp, "flag");

				if (!strcmp (cond->prop_val_str,
					    ns_filter_condition_property_types[READ])) {
					filter_option_set_current ((FilterOption*)el, "Seen");
				} else if (!strcmp (cond->prop_val_str,
					   ns_filter_condition_property_types[REPLIED])) {
					filter_option_set_current ((FilterOption*)el, "Answered");
				}
			default:
				filter_rule_remove_part (fr, fp);
				g_object_unref((fp));
				continue;
			}
			part_added = TRUE;
			break;
		case AGE_IN_DAYS:
			/* I guess we can skip that -- Netscape crashes anyway
			   whenever you try to use that setting ... :) */
			break;
		case X_MSG_HEADER:
			fp = rule_context_create_part (rc, "header");
			filter_rule_add_part (fr, fp);
			el = filter_part_find_element (fp, "header-field");
			filter_input_set_value ((FilterInput *)el, cond->prop_val_str);
			el = filter_part_find_element (fp, "header-type");
			filter_option_set_current ((FilterOption*)el, "exists");			
			part_added = TRUE;
			break;
		default:
			continue;
		}
	}

	if (!part_added) {
		g_object_unref((ff));
		return NULL;
	}
	
	/* build and add actionset */

	switch (nsf->action) {
	case MOVE_TO_FOLDER:
		{
			char *evol_folder;
			char *evol_folder_uri;

			fp = filter_context_create_action (fc, "move-to-folder");
			filter_filter_add_action (ff, fp);
			el = filter_part_find_element (fp, "folder");

			evol_folder = netscape_filter_strip_sbd (nsf->action_val_str);
			evol_folder_uri = netscape_filter_map_folder_to_uri (evol_folder);
			filter_folder_set_value ((FilterFolder *)el, evol_folder_uri);
			g_free (evol_folder);
			g_free (evol_folder_uri);

			action_added = TRUE;
		}
		break;
	case CHANGE_PRIORITY:
		fp = filter_context_create_action (fc, "score");
		el = filter_part_find_element (fp, "score");

		switch (nsf->action_val_id) {
		case LOWEST:
			filter_int_set_value((FilterInt *)el, -2);
			action_added = TRUE;
			break;
		case LOW:
			filter_int_set_value((FilterInt *)el, -1);
			action_added = TRUE;
			break;
		case NORMAL:
			filter_int_set_value((FilterInt *)el, 0);
			action_added = TRUE;
			break;
		case HIGH:
			filter_int_set_value((FilterInt *)el, 1);
			action_added = TRUE;
			break;
		case HIGHEST:
			filter_int_set_value((FilterInt *)el, 2);
			action_added = TRUE;
			break;
		default:
			g_object_unref((fp));
		}
		if (action_added) {
			*priority_needed = TRUE;
			filter_filter_add_action (ff, fp);
		}
		break;
	case DELETE:
		fp = filter_context_create_action (fc, "delete");
		filter_filter_add_action (ff, fp);
		action_added = TRUE;
		break;
	case MARK_READ:
		fp = filter_context_create_action (fc, "set-status");
		el = filter_part_find_element (fp, "flag");
		filter_option_set_current ((FilterOption *)el, "Seen");
		filter_filter_add_action (ff, fp);
		action_added = TRUE;
		break;
	case IGNORE_THREAD:
	case WATCH_THREAD:
		netscape_filter_threads_action_not_supported ();
		break;
	default:
		break;
	}

	if (!action_added) {
		g_object_unref((ff));
		return NULL;
	}

	return ff;
}


static void
netscape_import_filters (NsImporter *importer)
{
	FilterContext *fc;
	char *user, *system;
	FILE *mailrule_handle;
	char *ns_mailrule;
	NsFilter      *nsf;
	FilterFilter  *ff;
	gboolean       priority_needed = FALSE;

	ns_mailrule = gnome_util_prepend_user_home (".netscape/mailrule");
	mailrule_handle = fopen (ns_mailrule, "r");
	g_free (ns_mailrule);

	if (mailrule_handle == NULL) {
		d(g_warning ("No .netscape/mailrule found."));
		user_prefs = NULL;
		return;
	}

	fc = filter_context_new ();
	user = g_build_filename(g_get_home_dir (), "evolution/filters.xml");
	system = EVOLUTION_PRIVDATADIR "/filtertypes.xml";

	if (rule_context_load ((RuleContext *)fc, system, user) < 0) {
		g_warning ("Could not load rule context.");
		goto exit;
	}

	while ( (nsf = netscape_filter_read_next (mailrule_handle)) != NULL) {

		if ( (ff = netscape_filter_to_evol_filter (fc, nsf, &priority_needed)) != NULL)
			rule_context_add_rule (RULE_CONTEXT(fc), FILTER_RULE(ff));
		netscape_filter_cleanup (nsf);
	}
	
	if (priority_needed) {
		netscape_filter_change_priority_warning ();
		netscape_add_priority_workaround_filters (fc);
	}

	if (rule_context_save(RULE_CONTEXT(fc), user) < 0) {
		g_warning ("Could not save user's rule context.");
	}

 exit:
	g_free(user);
	g_object_unref((fc));

}

/* Email folder & accounts stuff ----------------------------------------------- */

static GtkWidget *
create_importer_gui (NsImporter *importer)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("Evolution is importing your old Netscape data"), GNOME_MESSAGE_BOX_INFO, NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Importing..."));

	importer->label = gtk_label_new (_("Please wait"));
	importer->progressbar = gtk_progress_bar_new ();
	gtk_progress_set_activity_mode (GTK_PROGRESS (importer->progressbar), TRUE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    importer->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    importer->progressbar, FALSE, FALSE, 0);

	return dialog;
}

static void
netscape_store_settings (NsImporter *importer)
{
	GConfClient *gconf = gconf_client_get_default();

	gconf_client_set_bool(gconf, "/apps/evolution/importer/netscape/mail", importer->done_mail, NULL);
	gconf_client_set_bool(gconf, "/apps/evolution/importer/netscape/settings", importer->done_settings, NULL);
	gconf_client_set_bool(gconf, "/apps/evolution/importer/netscape/filters", importer->done_filters, NULL);
}

static void
netscape_restore_settings (NsImporter *importer)
{
	GConfClient *gconf = gconf_client_get_default();

	importer->done_mail = gconf_client_get_bool(gconf, "/apps/evolution/importer/netscape/mail", NULL);
	importer->done_settings = gconf_client_get_bool(gconf, "/apps/evolution/importer/netscape/settings", NULL);
	importer->done_filters = gconf_client_get_bool(gconf, "/apps/evolution/importer/netscape/filters", NULL);
}

static const char *
netscape_get_string (const char *strname)
{
	return g_hash_table_lookup (user_prefs, strname);
}

static int
netscape_get_integer (const char *strname)
{
	char *intstr;

	intstr = g_hash_table_lookup (user_prefs, strname);
	if (intstr == NULL) {
		return 0;
	} else {
		return atoi (intstr);
	}
}

static gboolean
netscape_get_boolean (const char *strname)
{
	char *boolstr;

	boolstr = g_hash_table_lookup (user_prefs, strname);

	if (boolstr == NULL) {
		return FALSE;
	} else {
		if (strcasecmp (boolstr, "false") == 0) {
			return FALSE;
		} else if (strcasecmp (boolstr, "true") == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static char *
netscape_get_key (const char *line)
{
	char *line_dup;
	char *start, *end;
	char *key;
	
	line_dup = g_strdup (line);
	start = strchr (line_dup, '\"');
	if (start == NULL)
		goto die;
	start++;
	if (*start == '\0')
		goto die;

	end = strchr (start, '\"');
	if (end == NULL)
		goto die;
	*end = '\0';

	key = g_strdup (start);
	g_free (line_dup);

	d(g_warning ("Found key: %s", key));
	return key;

 die:
	g_free (line_dup);
	g_warning ("Broken line: %s", line);
	return NULL;
}

static char *
netscape_get_value (const char *line)
{
	char *line_dup;
	char *start, *end;
	char *value;

	line_dup = g_strdup (line);
	start = strchr (line_dup, ',');
	if (start == NULL)
		goto die;
	start++;
	if (*start == '\0')
		goto die;

	if (*start == ' ')
		start++;
	if (*start == '\0')
		goto die;

	if (*start == '\"')
		start++;
	if (*start == '\0')
		goto die;

	/* Start should now be the start of the value */
	end = strrchr (start, ')');
	if (end == NULL)
		goto die;
	*end = '\0';
	if (*(end - 1) == '\"')
		*(end - 1) = '\0';

	if (start == (end - 1)) {
		g_free (line_dup);
		return NULL;
	}

	value = g_strdup (start);
	g_free (line_dup);

	d(g_warning ("Found value: %s", value));
	return value;

 die:
	g_free (line_dup);
	g_warning ("Broken line: %s", line);
	return NULL;
}

static void
netscape_init_prefs (void)
{
	FILE *prefs_handle;
	char *nsprefs;
	char line[MAXLEN];

	user_prefs = g_hash_table_new (g_str_hash, g_str_equal);

	nsprefs = gnome_util_prepend_user_home (".netscape/preferences.js");
	prefs_handle = fopen (nsprefs, "r");
	g_free (nsprefs);

	if (prefs_handle == NULL) {
		d(g_warning ("No .netscape/preferences.js"));
		g_hash_table_destroy (user_prefs);
		user_prefs = NULL;
		return;
	}

	/* Find the user mail dir */
	while (fgets (line, MAXLEN, prefs_handle)) {
		char *key, *value;

		if (*line == 0) {
			continue;
		}

		if (*line == '/' && line[1] == '/') {
			continue;
		}

		key = netscape_get_key (line);
		value = netscape_get_value (line);

		if (key == NULL)
			continue;

		g_hash_table_insert (user_prefs, key, value);
	}

	return;
}

static char *
get_user_fullname (void)
{
	char *uname, *gecos, *special;
	struct passwd *pwd;

	uname = getenv ("USER");
	pwd = getpwnam (uname);

	if (strcmp (pwd->pw_gecos, "") == 0)
		return g_strdup (uname);

	special = strchr (pwd->pw_gecos, ',');
	if (special == NULL)
		gecos = g_strdup (pwd->pw_gecos);
	else
		gecos = g_strndup (pwd->pw_gecos, special - pwd->pw_gecos);

	special = strchr (gecos, '&');
	if (special == NULL) {
		return gecos;
	} else {
		char *capname, *expanded, *noamp;

		capname = g_strdup (uname);
		capname[0] = toupper ((int) capname[0]);
		noamp = g_strndup (gecos, special - gecos - 1);
		expanded = g_strconcat (noamp, capname, NULL);

		g_free (noamp);
		g_free (capname);
		g_free (gecos);

		return expanded;
	}
}

/* Needs to run in gui thread */
static void
netscape_import_accounts (NsImporter *importer)
{
	char *username;
	const char *nstr;
	const char *imap;
	EAccount *account;

	if (user_prefs == NULL) {
		netscape_init_prefs ();
		if (user_prefs == NULL)
			return;
	}

	/* Create identify structure */
	nstr = netscape_get_string ("mail.identity.username");
	if (nstr != NULL)
		username = g_strdup (nstr);
	else
		username = get_user_fullname ();

	account = e_account_new();

	account->id->name = g_strdup(username);
	account->id->address = g_strdup(netscape_get_string("mail.identity.useremail"));
	account->id->organization = g_strdup(netscape_get_string("mail.identity.organization"));

	nstr = netscape_get_string ("mail.signature_file");
	if (nstr != NULL) {
		ESignature *sig;
		char *cmd;
		
		sig = mail_config_signature_new (NULL, FALSE, FALSE);
		mail_config_add_signature (sig);
		account->id->sig_uid = g_strdup (sig->uid);
		
		/* HACK: yeah this is a hack, who cares? */
		cmd = g_strdup_printf("cp \'%s\' \'%s\'", nstr, sig->filename);
		system(cmd);
		g_free(cmd);
	}

	/* Create transport */
	nstr = netscape_get_string ("network.hosts.smtp_server");
	if (nstr != NULL) {
		const char *nstr2;

		nstr2 = netscape_get_string ("mail.smtp_name");
		if (nstr2)
			account->transport->url = g_strconcat ("smtp://", nstr2, "@", nstr, NULL);
		else
			account->transport->url = g_strconcat ("smtp://", nstr, NULL);
	} else {
		account->transport->url = g_strdup("");
	}

	account->transport->keep_on_server = FALSE;
	account->transport->auto_check = FALSE;
	account->transport->auto_check_time = 10;
	account->transport->save_passwd = FALSE;

	/*transport.enabled = FALSE;*/

	/* Create account */
	account->drafts_folder_uri = em_uri_from_camel(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_DRAFTS));
	account->sent_folder_uri = em_uri_from_camel(mail_component_get_folder_uri(NULL, MAIL_COMPONENT_FOLDER_SENT));

	/* Create POP3 source */
	nstr = netscape_get_string ("network.hosts.pop_server");
	if (nstr != NULL && *nstr != 0) {
		const char *nstr2;

		nstr2 = netscape_get_string ("mail.pop_name");
		if (nstr2)
			account->source->url = g_strconcat ("pop://", nstr2, "@", nstr, NULL);
		else
			account->source->url = g_strconcat ("pop://", nstr, NULL);
		account->source->keep_on_server = netscape_get_boolean ("mail.leave_on_server");
		account->source->auto_check = TRUE;
		account->source->auto_check_time = 10;
		account->source->save_passwd = netscape_get_boolean ("mail.remember_password");
		account->enabled = TRUE;
	} else {
		/* Are there IMAP accounts? */
		imap = netscape_get_string ("network.hosts.imap_servers");
		if (imap != NULL) {
			char **servers;
			int i;

			servers = g_strsplit (imap, ",", 0);
			for (i = 0; servers[i] != NULL; i++) {
				char *serverstr, *name;
				const char *username;
				EAccount *imap;

				imap = e_account_new();
				e_account_import(imap, account);

				/* Create a server for each of these */
				serverstr = g_strdup_printf ("mail.imap.server.%s.", servers[i]);
				name = g_strconcat (serverstr, "userName", NULL);
				username = netscape_get_string (name);
				g_free (name);

				if (username)
					imap->source->url = g_strconcat ("imap://", username, "@", servers[i], NULL);
				else
					imap->source->url = g_strconcat ("imap://", servers[i], NULL);

				imap->source->keep_on_server = netscape_get_boolean ("mail.leave_on_server");
				
				name = g_strconcat (serverstr, "check_new_mail", NULL);
				imap->source->auto_check = netscape_get_boolean (name);
				g_free (name);

				name = g_strconcat (serverstr, "check_time", NULL);
				imap->source->auto_check_time = netscape_get_integer (name);
				g_free (name);

				name = g_strconcat (serverstr, "remember_password", NULL);
				imap->source->save_passwd = netscape_get_boolean (name);
				g_free (name);

				imap->enabled = TRUE;

				mail_config_add_account(imap);
				g_free (serverstr);
			}

			g_object_unref(account);
			g_strfreev (servers);
			return;
		} else {
			/* Using Movemail */
			account->source->url = g_strconcat ("mbox://", getenv("MAIL"), NULL);
			account->source->keep_on_server = netscape_get_boolean ("mail.leave_on_server");
			account->source->auto_check = TRUE;
			account->source->auto_check_time = 10;
			account->source->save_passwd = netscape_get_boolean ("mail.remember_password");
			account->enabled = TRUE;
		}
	}

	mail_config_add_account(account);
	g_free(username);
}

static gboolean
is_dir_empty (const char *path)
{
	DIR *base;
	struct stat buf;
	struct dirent *contents;

	base = opendir (path);
	if (base == NULL) {
		return TRUE; /* Can't open dir */
	}
	
	while ((contents = readdir(base)) != NULL) {
		char *fullpath;
		
		if (strcmp (contents->d_name, ".") == 0 ||
		    strcmp (contents->d_name, "..") == 0) {
			continue;
		}

		fullpath = g_build_filename(path, contents->d_name, NULL);
		if (lstat (fullpath, &buf) == -1) {
			g_free(fullpath);
			continue;
		}

		if ((S_ISDIR (buf.st_mode) && !is_dir_empty (fullpath))
		    || (S_ISREG(buf.st_mode) && buf.st_size != 0)) {
			g_free (fullpath);
			closedir (base);
			return FALSE;
		}

		g_free (fullpath);
	}

	closedir (base);
	return TRUE;
}

static gboolean
netscape_can_import(EvolutionIntelligentImporter *ii, void *data)
{
	int can;
	NsImporter *importer = data;

	if (user_prefs == NULL)
		netscape_init_prefs ();

	can = user_prefs
		&& (nsmail_dir = g_hash_table_lookup (user_prefs, "mail.directory")) != NULL
		&& !is_dir_empty(nsmail_dir);

	importer->do_mail = can && !importer->done_mail;
	importer->do_settings = can && !importer->done_settings;
	importer->do_filters = importer->do_mail && !importer->done_filters;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (importer->mail), importer->do_mail);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (importer->settings), importer->do_settings);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (importer->mail), importer->do_filters);

	return can;
}

struct _netscape_import_msg {
	struct _mail_msg msg;

	NsImporter *importer;
};

static char *
netscape_import_describe (struct _mail_msg *mm, int complete)
{
	return g_strdup (_("Importing Netscape data"));
}

/* Translations ? */
static MailImporterSpecial netscape_special_folders[] = {
	{ "Trash", "Netscape-Trash" },
	{ "Calendar", "Netscape-Calendar" },
	{ "Contacts", "Netscape-Contacts" },
	{ "Tasks", "Netscape-Tasks" },
	{ "Unsent Messages", "Outbox" },
	{ 0 },
};

static const char *fix_netscape_folder_names (const char *original_name)
{
	int i;

	for (i=0;netscape_special_folders[i].orig;i++)
		if (strcmp(netscape_special_folders[i].orig, original_name) == 0)
			return netscape_special_folders[i].new;

	return original_name;
}

static void
netscape_import_import(struct _mail_msg *mm)
{
	struct _netscape_import_msg *m = (struct _netscape_import_msg *) mm;

	if (m->importer->do_mail)
		mail_importer_import_folders_sync(nsmail_dir, netscape_special_folders, MAIL_IMPORTER_MOZFMT, m->importer->cancel);
}

static void
netscape_import_imported(struct _mail_msg *mm)
{
}

static void
netscape_import_free(struct _mail_msg *mm)
{
}

static struct _mail_msg_op netscape_import_op = {
	netscape_import_describe,
	netscape_import_import,
	netscape_import_imported,
	netscape_import_free,
};

static int
mail_importer_netscape_import(NsImporter *importer)
{
	struct _netscape_import_msg *m;
	int id;

	m = mail_msg_new(&netscape_import_op, NULL, sizeof (*m));
	m->importer = importer;

	id = m->msg.seq;

	/* Need to do these in gui thread, and should be quick anyway */
	if (m->importer->do_mail && m->importer->do_filters)
		netscape_import_filters(m->importer);
	if (m->importer->do_settings)
		netscape_import_accounts(m->importer);
	
	e_thread_put(mail_thread_queued, (EMsg *) m);

	return id;
}

static void
netscape_status(CamelOperation *op, const char *what, int pc, void *data)
{
	NsImporter *importer = data;

	if (pc == CAMEL_OPERATION_START)
		pc = 0;
	else if (pc == CAMEL_OPERATION_END)
		pc = 100;

	g_mutex_lock(importer->status_lock);
	g_free(importer->status_what);
	importer->status_what = g_strdup(what);
	importer->status_pc = pc;
	g_mutex_unlock(importer->status_lock);
}

static gboolean
netscape_status_timeout(void *data)
{
	NsImporter *importer = data;
	int pc;
	char *what;

	if (!importer->status_what)
		return TRUE;

	g_mutex_lock(importer->status_lock);
	what = importer->status_what;
	importer->status_what = NULL;
	pc = importer->status_pc;
	g_mutex_unlock(importer->status_lock);

	gtk_progress_bar_set_fraction((GtkProgressBar *)importer->progressbar, (gfloat)(pc/100.0));
	gtk_progress_bar_set_text((GtkProgressBar *)importer->progressbar, what);
	
	return TRUE;
}

static void
netscape_create_structure (EvolutionIntelligentImporter *ii, void *closure)
{
	NsImporter *importer = closure;

	if (importer->do_settings || importer->do_mail) {
		importer->dialog = create_importer_gui (importer);
		gtk_widget_show_all (importer->dialog);
		importer->status_timeout_id = g_timeout_add(100, netscape_status_timeout, importer);
		importer->cancel = camel_operation_new(netscape_status, importer);

		mail_msg_wait(mail_importer_netscape_import(importer));

		camel_operation_unref(importer->cancel);
		g_source_remove(importer->status_timeout_id);
		importer->status_timeout_id = 0;

		if (importer->do_settings)
			importer->done_settings = TRUE;
		if (importer->do_mail)
			importer->done_mail = TRUE;
		if (importer->do_filters)
			importer->done_filters = TRUE;
	}

	netscape_store_settings(importer);

	bonobo_object_unref(BONOBO_OBJECT(ii));
}

static void
netscape_destroy_cb (NsImporter *importer, GObject *object)
{
	netscape_store_settings (importer);

	if (importer->status_timeout_id)
		g_source_remove(importer->status_timeout_id);
	g_free(importer->status_what);
	g_mutex_free(importer->status_lock);

	if (importer->dialog)
		gtk_widget_destroy(importer->dialog);

	g_free(importer);
}

/* Fun initialisation stuff */

/* Fun with aggregation */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    NsImporter *importer)
{
	/* Some extra logic here to make the filters choice
	   depending on the mail choice */
	if (GTK_WIDGET(tb) == importer->mail) {
		importer->do_mail = gtk_toggle_button_get_active (tb);
		
		if (importer->do_mail == FALSE) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(importer->filters), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(importer->filters), FALSE);
			importer->do_filters = FALSE;
		} else {
			gtk_widget_set_sensitive(GTK_WIDGET(importer->filters), TRUE);
		}

	} else if (GTK_WIDGET(tb) == importer->settings) {
		importer->do_settings = gtk_toggle_button_get_active (tb);

	} else if (GTK_WIDGET(tb) == importer->filters) {
		importer->do_filters = gtk_toggle_button_get_active (tb);

	}
	/* *do_item = gtk_toggle_button_get_active (tb); */
}

static BonoboControl *
create_checkboxes_control (NsImporter *importer)
{
	GtkWidget *hbox;
	BonoboControl *control;

	hbox = gtk_hbox_new (FALSE, 2);

	importer->mail = gtk_check_button_new_with_label (_("Mail"));
	g_signal_connect((importer->mail), "toggled",
			    G_CALLBACK (checkbox_toggle_cb),
			    importer);

	importer->settings = gtk_check_button_new_with_label (_("Settings"));
	g_signal_connect((importer->settings), "toggled",
			    G_CALLBACK (checkbox_toggle_cb),
			    importer);

	importer->filters = gtk_check_button_new_with_label (_("Mail Filters"));
	gtk_widget_set_sensitive(GTK_WIDGET(importer->filters), FALSE);
	g_signal_connect((importer->filters), "toggled",
			    G_CALLBACK (checkbox_toggle_cb),
			    importer);

	gtk_box_pack_start (GTK_BOX (hbox), importer->mail, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), importer->settings, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), importer->filters, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);
	control = bonobo_control_new (hbox);
	return control;
}
	
BonoboObject *
netscape_intelligent_importer_new(void)
{
	EvolutionIntelligentImporter *importer;
	BonoboControl *control;
	NsImporter *netscape;
	char *message = N_("Evolution has found Netscape mail files.\n"
			   "Would you like them to be imported into Evolution?");
	
	netscape = g_new0 (NsImporter, 1);
	netscape->status_lock = g_mutex_new();
	netscape_restore_settings (netscape);
	importer = evolution_intelligent_importer_new (netscape_can_import,
						       netscape_create_structure,
						       "Netscape", 
						       _(message), netscape);
	g_object_weak_ref(G_OBJECT(importer), (GWeakNotify)netscape_destroy_cb, netscape);
	netscape->ii = importer;

	control = create_checkboxes_control(netscape);
	bonobo_object_add_interface(BONOBO_OBJECT(importer), BONOBO_OBJECT(control));

	return BONOBO_OBJECT(importer);
}
