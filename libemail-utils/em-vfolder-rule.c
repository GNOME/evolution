/*
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
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libevolution-utils/e-alert.h>

#include <libemail-engine/e-mail-folder-utils.h>

#include "em-vfolder-context.h"
#include "em-vfolder-rule.h"

#define EM_VFOLDER_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_RULE, EMVFolderRulePrivate))

#define EM_VFOLDER_RULE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_VFOLDER_RULE, EMVFolderRulePrivate))

struct _EMVFolderRulePrivate {
	gint placeholder;
};

static gint validate (EFilterRule *, EAlert **alert);
static gint vfolder_eq (EFilterRule *fr, EFilterRule *cm);
static xmlNodePtr xml_encode (EFilterRule *);
static gint xml_decode (EFilterRule *, xmlNodePtr, ERuleContext *f);
static void rule_copy (EFilterRule *dest, EFilterRule *src);
static GtkWidget *get_widget (EFilterRule *fr, ERuleContext *f);

/* DO NOT internationalise these strings */
static const gchar *with_names[] = {
	"specific",
	"local_remote_active",
	"remote_active",
	"local"
};

G_DEFINE_TYPE (
	EMVFolderRule,
	em_vfolder_rule,
	E_TYPE_FILTER_RULE)

static void
vfolder_rule_finalize (GObject *object)
{
	EMVFolderRule *rule = EM_VFOLDER_RULE (object);
	gchar *uri;

	while ((uri = g_queue_pop_head (&rule->sources)) != NULL)
		g_free (uri);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_vfolder_rule_parent_class)->finalize (object);
}

static void
em_vfolder_rule_class_init (EMVFolderRuleClass *class)
{
	GObjectClass *object_class;
	EFilterRuleClass *filter_rule_class;

	g_type_class_add_private (class, sizeof (EMVFolderRulePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = vfolder_rule_finalize;

	filter_rule_class = E_FILTER_RULE_CLASS (class);
	filter_rule_class->validate = validate;
	filter_rule_class->eq = vfolder_eq;
	filter_rule_class->xml_encode = xml_encode;
	filter_rule_class->xml_decode = xml_decode;
	filter_rule_class->copy = rule_copy;
	filter_rule_class->get_widget = get_widget;
}

static void
em_vfolder_rule_init (EMVFolderRule *rule)
{
	rule->priv = EM_VFOLDER_RULE_GET_PRIVATE (rule);
	rule->with = EM_VFOLDER_RULE_WITH_SPECIFIC;
	rule->rule.source = g_strdup ("incoming");
}

EFilterRule *
em_vfolder_rule_new ()
{
	return g_object_new (
		EM_TYPE_VFOLDER_RULE, NULL);
}

void
em_vfolder_rule_add_source (EMVFolderRule *rule,
                            const gchar *uri)
{
	g_return_if_fail (EM_IS_VFOLDER_RULE (rule));
	g_return_if_fail (uri);

	g_queue_push_tail (&rule->sources, g_strdup (uri));

	e_filter_rule_emit_changed (E_FILTER_RULE (rule));
}

const gchar *
em_vfolder_rule_find_source (EMVFolderRule *rule,
                             const gchar *uri)
{
	GList *link;

	g_return_val_if_fail (EM_IS_VFOLDER_RULE (rule), NULL);

	/* only does a simple string or address comparison, should
	 * probably do a decoded url comparison */
	link = g_queue_find_custom (
		&rule->sources, uri, (GCompareFunc) strcmp);

	return (link != NULL) ? link->data : NULL;
}

void
em_vfolder_rule_remove_source (EMVFolderRule *rule,
                               const gchar *uri)
{
	gchar *found;

	g_return_if_fail (EM_IS_VFOLDER_RULE (rule));

	found =(gchar *) em_vfolder_rule_find_source (rule, uri);
	if (found != NULL) {
		g_queue_remove (&rule->sources, found);
		g_free (found);
		e_filter_rule_emit_changed (E_FILTER_RULE (rule));
	}
}

const gchar *
em_vfolder_rule_next_source (EMVFolderRule *rule,
                             const gchar *last)
{
	GList *link;

	if (last == NULL) {
		link = g_queue_peek_head_link (&rule->sources);
	} else {
		link = g_queue_find (&rule->sources, last);
		if (link == NULL)
			link = g_queue_peek_head_link (&rule->sources);
		else
			link = g_list_next (link);
	}

	return (link != NULL) ? link->data : NULL;
}

static gint
validate (EFilterRule *fr,
          EAlert **alert)
{
	g_return_val_if_fail (fr != NULL, 0);
	g_warn_if_fail (alert == NULL || *alert == NULL);

	if (!fr->name || !*fr->name) {
		if (alert)
			*alert = e_alert_new ("mail:no-name-vfolder", NULL);
		return 0;
	}

	/* We have to have at least one source set in the "specific" case.
	 * Do not translate this string! */
	if (((EMVFolderRule *) fr)->with == EM_VFOLDER_RULE_WITH_SPECIFIC &&
		g_queue_is_empty (&((EMVFolderRule *) fr)->sources)) {
		if (alert)
			*alert = e_alert_new ("mail:vfolder-no-source", NULL);
		return 0;
	}

	return E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->validate (fr, alert);
}

static gint
queue_eq (GQueue *queue_a,
          GQueue *queue_b)
{
	GList *link_a;
	GList *link_b;
	gint truth = TRUE;

	link_a = g_queue_peek_head_link (queue_a);
	link_b = g_queue_peek_head_link (queue_b);

	while (truth && link_a != NULL && link_b != NULL) {
		gchar *uri_a = link_a->data;
		gchar *uri_b = link_b->data;

		truth = (strcmp (uri_a, uri_b)== 0);

		link_a = g_list_next (link_a);
		link_b = g_list_next (link_b);
	}

	return truth && link_a == NULL && link_b == NULL;
}

static gint
vfolder_eq (EFilterRule *fr,
            EFilterRule *cm)
{
	return E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->eq (fr, cm)
		&& queue_eq (
			&((EMVFolderRule *) fr)->sources,
			&((EMVFolderRule *) cm)->sources);
}

static xmlNodePtr
xml_encode (EFilterRule *fr)
{
	EMVFolderRule *vr =(EMVFolderRule *) fr;
	xmlNodePtr node, set, work;
	GList *head, *link;

	node = E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->xml_encode (fr);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (vr->with < G_N_ELEMENTS (with_names), NULL);

	set = xmlNewNode(NULL, (const guchar *)"sources");
	xmlAddChild (node, set);
	xmlSetProp(set, (const guchar *)"with", (guchar *)with_names[vr->with]);

	head = g_queue_peek_head_link (&vr->sources);
	for (link = head; link != NULL; link = g_list_next (link)) {
		const gchar *uri = link->data;

		work = xmlNewNode (NULL, (const guchar *) "folder");
		xmlSetProp (work, (const guchar *) "uri", (guchar *) uri);
		xmlAddChild (set, work);
	}

	return node;
}

static void
set_with (EMVFolderRule *vr,
          const gchar *name)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (with_names); i++) {
		if (!strcmp (name, with_names[i])) {
			vr->with = i;
			return;
		}
	}

	vr->with = 0;
}

static gint
xml_decode (EFilterRule *fr,
            xmlNodePtr node,
            ERuleContext *f)
{
	xmlNodePtr set, work;
	gint result;
	EMVFolderRule *vr =(EMVFolderRule *) fr;
	gchar *tmp;

	result = E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->
		xml_decode (fr, node, f);
	if (result != 0)
		return result;

	/* handle old format file, vfolder source is in filterrule */
	if (strcmp(fr->source, "incoming")!= 0) {
		set_with (vr, fr->source);
		g_free (fr->source);
		fr->source = g_strdup("incoming");
	}

	set = node->children;
	while (set) {
		if (!strcmp((gchar *)set->name, "sources")) {
			tmp = (gchar *)xmlGetProp(set, (const guchar *)"with");
			if (tmp) {
				set_with (vr, tmp);
				xmlFree (tmp);
			}
			work = set->children;
			while (work) {
				if (!strcmp((gchar *)work->name, "folder")) {
					tmp = (gchar *)xmlGetProp(work, (const guchar *)"uri");
					if (tmp) {
						g_queue_push_tail (&vr->sources, g_strdup (tmp));
						xmlFree (tmp);
					}
				}
				work = work->next;
			}
		}
		set = set->next;
	}
	return 0;
}

static void
rule_copy (EFilterRule *dest,
           EFilterRule *src)
{
	EMVFolderRule *vdest, *vsrc;
	GList *head, *link;
	gchar *uri;

	vdest =(EMVFolderRule *) dest;
	vsrc =(EMVFolderRule *) src;

	while ((uri = g_queue_pop_head (&vdest->sources)) != NULL)
		g_free (uri);

	head = g_queue_peek_head_link (&vsrc->sources);
	for (link = head; link != NULL; link = g_list_next (link)) {
		const gchar *uri = link->data;
		g_queue_push_tail (&vdest->sources, g_strdup (uri));
	}

	vdest->with = vsrc->with;

	E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->copy (dest, src);
}

static GtkWidget *
get_widget (EFilterRule *fr,
            ERuleContext *rc)
{
	GtkWidget *widget;

	widget = E_FILTER_RULE_CLASS (em_vfolder_rule_parent_class)->
		get_widget (fr, rc);

	return widget;
}
