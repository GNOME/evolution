/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-destination.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include <config.h>
#include "e-destination.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "e-book.h"
#include "e-book-util.h"
#include <gal/widgets/e-unicode.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>


struct _EDestinationPrivate {

	gchar *card_uri;
	ECard *card;
	gint card_email_num;

	gchar *name;
	gchar *email;
	gchar *addr;

	gboolean html_mail_override;
	gboolean wants_html_mail;

	GList *list_dests;
};

static void e_destination_clear_card    (EDestination *);
static void e_destination_clear_strings (EDestination *);

static GtkObjectClass *parent_class;

static void
e_destination_destroy (GtkObject *obj)
{
	EDestination *dest = E_DESTINATION (obj);

	e_destination_clear (dest);
	g_free (dest->priv);

	if (parent_class->destroy)
		parent_class->destroy (obj);
}

static void
e_destination_class_init (EDestinationClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (GTK_TYPE_OBJECT));

	object_class->destroy = e_destination_destroy;
}

static void
e_destination_init (EDestination *dest)
{
	dest->priv = g_new0 (struct _EDestinationPrivate, 1);
}

GtkType
e_destination_get_type (void)
{
	static GtkType dest_type = 0;

	if (!dest_type) {
		GtkTypeInfo dest_info = {
			"EDestination",
			sizeof (EDestination),
			sizeof (EDestinationClass),
			(GtkClassInitFunc) e_destination_class_init,
			(GtkObjectInitFunc) e_destination_init,
			NULL, NULL, /* reserved */
			(GtkClassInitFunc) NULL
		};

		dest_type = gtk_type_unique (gtk_object_get_type (), &dest_info);
	}

	return dest_type;
}

EDestination *
e_destination_new (void)
{
	return E_DESTINATION (gtk_type_new (E_TYPE_DESTINATION));
}

EDestination *
e_destination_copy (EDestination *dest)
{
	EDestination *new_dest;
	GList *iter;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	new_dest = e_destination_new ();

	new_dest->priv->card_uri       = g_strdup (dest->priv->card_uri);
	new_dest->priv->name           = g_strdup (dest->priv->name);
	new_dest->priv->email          = g_strdup (dest->priv->email);
	new_dest->priv->addr           = g_strdup (dest->priv->addr);
	new_dest->priv->card_email_num = dest->priv->card_email_num;

	new_dest->priv->card     = dest->priv->card;
	if (new_dest->priv->card)
		gtk_object_ref (GTK_OBJECT (new_dest->priv->card));

	new_dest->priv->html_mail_override = dest->priv->html_mail_override;
	new_dest->priv->wants_html_mail    = dest->priv->wants_html_mail;

	for (iter = dest->priv->list_dests; iter != NULL; iter = g_list_next (iter)) {
		new_dest->priv->list_dests = g_list_append (new_dest->priv->list_dests,
							    e_destination_copy (E_DESTINATION (iter->data)));
	}

	return new_dest;
}

static void
e_destination_clear_card (EDestination *dest)
{
	g_free (dest->priv->card_uri);
	dest->priv->card_uri = NULL;

	if (dest->priv->card)
		gtk_object_unref (GTK_OBJECT (dest->priv->card));
	dest->priv->card = NULL;

	dest->priv->card_email_num = -1;

	g_list_foreach (dest->priv->list_dests, (GFunc) gtk_object_unref, NULL);
	g_list_free (dest->priv->list_dests);
	dest->priv->list_dests = NULL;
}

static void
e_destination_clear_strings (EDestination *dest)
{
	g_free (dest->priv->name);
	dest->priv->name = NULL;

	g_free (dest->priv->email);
	dest->priv->email = NULL;

	g_free (dest->priv->addr);
	dest->priv->addr = NULL;
}

void
e_destination_clear (EDestination *dest)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	e_destination_clear_card (dest);
	e_destination_clear_strings (dest);
}

gboolean
e_destination_is_empty (EDestination *dest)
{
	struct _EDestinationPrivate *p;
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), TRUE);
	p = dest->priv;

	return !(p->card != NULL
		 || (p->card_uri && *p->card_uri)
		 || (p->name && *p->name)
		 || (p->email && *p->email)
		 || (p->addr && *p->addr)
		 || (p->list_dests != NULL));
}

void
e_destination_set_card (EDestination *dest, ECard *card, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (card && E_IS_CARD (card));

	e_destination_clear (dest);

	dest->priv->card = card;
	gtk_object_ref (GTK_OBJECT (dest->priv->card));

	dest->priv->card_email_num = email_num;
}

void
e_destination_set_card_uri (EDestination *dest, const gchar *uri, gint email_num)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (uri != NULL);
	
	g_free (dest->priv->card_uri);
	dest->priv->card_uri = g_strdup (uri);
	dest->priv->card_email_num = email_num;

	/* If we already have a card, remove it unless it's uri matches the one
	   we just set. */
	if (dest->priv->card && strcmp (uri, e_card_get_uri (dest->priv->card))) {
		gtk_object_unref (GTK_OBJECT (dest->priv->card));
		dest->priv->card = NULL;
	}
}

void
e_destination_set_name (EDestination *dest, const gchar *name)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (name != NULL);

	g_free (dest->priv->name);
	dest->priv->name = g_strdup (name);

	if (dest->priv->addr) {
		g_free (dest->priv->addr);
		dest->priv->addr = NULL;
	}
}

void
e_destination_set_email (EDestination *dest, const gchar *email)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (email != NULL);

	g_free (dest->priv->email);
	dest->priv->email = g_strdup (email);

	if (dest->priv->addr) {
		g_free (dest->priv->addr);
		dest->priv->addr = NULL;
	}
}


/* This function takes a free-form string and tries to do something
   intelligent with it. */
void
e_destination_set_string (EDestination *dest, const gchar *str)
{
	gchar *name = NULL;
	gchar *email = NULL;
	gchar *lt, *gt;

	g_return_if_fail (dest && E_IS_DESTINATION (dest));
	g_return_if_fail (str != NULL);

	/* This turned out to be an overly-clever approach... */
#if 0
	/* Look for something of the form Jane Smith <jane@assbarn.com> */
	if ( (lt = strrchr (str, '<')) && (gt = strrchr (str, '>')) && lt+1 < gt) {
		name  = g_strndup (str, lt-str);
		email = g_strndup (lt+1, gt-lt-1);

		/* I love using goto.  It makes me feel so wicked. */
		goto finished;
	}

	/* If it contains '@', assume it is an e-mail address. */
	if (strchr (str, '@')) {
		email = g_strdup (str);
		goto finished;
	}

	/* If we contain whitespace, that is very suggestive of being a name. */
	if (strchr (str, ' ')) {
		name = g_strdup (str);
		goto finished;
	}
#endif

	/* Default: Just treat it as a name address. */
	name = g_strdup (str);

 finished:
	if (name) {
		g_message ("name: [%s]", name);
		if (*name)
			e_destination_set_name (dest, name);
		g_free (name);
	}

	if (email) {
		g_message ("email: [%s]", email);
		if (*email)
			e_destination_set_email (dest, email);
		g_free (email);
	}
}

void
e_destination_set_html_mail_pref (EDestination *dest, gboolean x)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	dest->priv->html_mail_override = TRUE;
	dest->priv->wants_html_mail = x;
}

gboolean
e_destination_contains_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->card != NULL;
}

gboolean
e_destination_from_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	return dest->priv->card != NULL || dest->priv->card_uri != NULL;
}


typedef struct _UseCard UseCard;
struct _UseCard {
	EDestination *dest;
	EDestinationCardCallback cb;
	gpointer closure;
};

static void
use_card_cb (ECard *card, gpointer closure)
{
	UseCard *uc = (UseCard *) closure;

	if (card != NULL && uc->dest->priv->card == NULL) {

		uc->dest->priv->card = card;
		gtk_object_ref (GTK_OBJECT (uc->dest->priv->card));

	}

	if (uc->cb) {
		uc->cb (uc->dest, uc->dest->priv->card, uc->closure);
	}

	/* We held a copy of the destination during the callback. */
	gtk_object_unref (GTK_OBJECT (uc->dest));
	g_free (uc);
}

void
e_destination_use_card (EDestination *dest, EDestinationCardCallback cb, gpointer closure)
{
	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	if (dest->priv->card) {

		if (cb) {
			cb (dest, dest->priv->card, closure);
		}

	} else if (dest->priv->card_uri) {

		UseCard *uc = g_new (UseCard, 1);
		uc->dest = dest;
		/* Hold a reference to the destination during the callback. */
		gtk_object_ref (GTK_OBJECT (uc->dest));
		uc->cb = cb;
		uc->closure = closure;
		e_card_load_uri (dest->priv->card_uri, use_card_cb, uc);
	}
}

ECard *
e_destination_get_card (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	return dest->priv->card;
}

const gchar *
e_destination_get_card_uri (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	if (dest->priv->card_uri)
		return dest->priv->card_uri;
	
	if (dest->priv->card)
		return e_card_get_uri (dest->priv->card);

	return NULL;
}

gint
e_destination_get_email_num (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), -1);

	if (dest->priv->card == NULL && dest->priv->card_uri == NULL)
		return -1;

	return dest->priv->card_email_num;
}

const gchar *
e_destination_get_name (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->name == NULL && priv->card != NULL)
		priv->name = e_card_name_to_string (priv->card->name);
	
	return priv->name;
	
}

const gchar *
e_destination_get_email (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */

	if (priv->email == NULL) {

		if (priv->card) { /* Pull the address out of the card. */

			EIterator *iter = e_list_get_iterator (priv->card->email);
			gint n = priv->card_email_num;

			if (n >= 0) {
				while (n > 0) {
					e_iterator_next (iter);
					--n;
				}

				if (e_iterator_is_valid (iter)) {
					gconstpointer ptr = e_iterator_get (iter);
					priv->email = g_strdup ((gchar *) ptr);
				}
			}

		} else if (priv->name) {
			gchar *lt = strchr (priv->name, '<');
			gchar *gt = strchr (priv->name, '>');
			
			if (lt && gt && lt+1 < gt) {
				priv->email = g_strndup (lt+1, gt-lt-1);
			}
		}
	}

	return priv->email;
}

const gchar *
e_destination_get_address (const EDestination *dest)
{
	struct _EDestinationPrivate *priv;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);
	
	priv = (struct _EDestinationPrivate *)dest->priv; /* cast out const */
	
	if (priv->addr == NULL) {
		if (e_destination_is_evolution_list (dest)) {
			gchar **strv = g_new0 (gchar *, g_list_length (priv->list_dests) + 1);
			gint i = 0;
			GList *iter = dest->priv->list_dests;
			
			while (iter) {
				EDestination *list_dest = E_DESTINATION (iter->data);
				if (!e_destination_is_empty (list_dest)) {
					strv[i++] = (gchar *) e_destination_get_address (list_dest);
				}
				iter = g_list_next (iter);
			}
			
			priv->addr = g_strjoinv (", ", strv);
			
			g_message ("List address is [%s]", priv->addr);
			
			g_free (strv);
		} else {
			const gchar *name     = e_destination_get_name (dest);
			const gchar *email    = e_destination_get_email (dest);
			
			/* If this isn't set, we return NULL */
			if (email) {
				if (name) {
					/* uhm, yea... this'll work. NOT!!! */
					/* what about ','? or any of the other chars that require quoting?? */
					const gchar *lt = strchr (name, '<');
					gchar *namecpy = lt ? g_strndup (name, lt-name) : g_strdup (name);
					gboolean needs_quotes = (strchr (namecpy, '.') != NULL);
					
					g_strstrip (namecpy);
					
					priv->addr = g_strdup_printf ("%s%s%s <%s>",
								      needs_quotes ? "\"" : "",
								      namecpy,
								      needs_quotes ? "\"" : "",
								      email);
					g_free (namecpy);
				} else {
					priv->addr = g_strdup (email);
				}
			} else {
				/* Just use the name, which is the best we can do. */
				priv->addr = g_strdup (name);
			}
		}
	}
	
	return priv->addr;
}

const gchar *
e_destination_get_textrep (const EDestination *dest)
{
	const gchar *txt;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	txt = e_destination_get_name (dest);
	if (txt)
		return txt;

	txt = e_destination_get_email (dest);
	if (txt)
		return txt;

	return "";
}

gboolean
e_destination_is_evolution_list (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);

	if (dest->priv->list_dests == NULL
	    && dest->priv->card != NULL
	    && dest->priv->card->email != NULL
	    && e_card_evolution_list (dest->priv->card)) {

		EIterator *iter = e_list_get_iterator (dest->priv->card->email);
		e_iterator_reset (iter);
		while (e_iterator_is_valid (iter)) {
			const gchar *dest_xml = (const gchar *) e_iterator_get (iter);
			EDestination *list_dest = e_destination_import (dest_xml);
			if (list_dest)
				dest->priv->list_dests = g_list_append (dest->priv->list_dests, list_dest);
			e_iterator_next (iter);
		}
	}

	return dest->priv->list_dests != NULL;
}

gboolean
e_destination_get_html_mail_pref (const EDestination *dest)
{
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);

	if (dest->priv->html_mail_override || dest->priv->card == NULL)
		return dest->priv->wants_html_mail;

	return dest->priv->card->wants_html;
}

gchar *
e_destination_get_address_textv (EDestination **destv)
{
	gint i, j, len = 0;
	gchar **strv;
	gchar *str;
	
	g_return_val_if_fail (destv, NULL);

	/* FIXME: please tell me this is only for assertion
           reasons. If this is considered to be ok behavior then you
           shouldn't use g_return's. Just a reminder ;-) */
	while (destv[len]) {
		g_return_val_if_fail (E_IS_DESTINATION (destv[len]), NULL);
		++len;
	}
	
	strv = g_new0 (gchar *, len+1);
	for (i = 0, j = 0; destv[i]; i++) {
		if (!e_destination_is_empty (destv[i])) {
			const gchar *addr = e_destination_get_address (destv[i]);
			strv[j++] = addr ? (gchar *) addr : "";
		}
	}
	
	str = g_strjoinv (", ", strv);
	
	g_free (strv);
	
	return str;
}

xmlNodePtr
e_destination_xml_encode (const EDestination *dest)
{
	xmlNodePtr dest_node;
	const gchar *str;

	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	dest_node = xmlNewNode (NULL, "destination");

	str = e_destination_get_name (dest);
	if (str)
		xmlNewTextChild (dest_node, NULL, "name", str);

	if (! e_destination_is_evolution_list (dest)) {
		str = e_destination_get_email (dest);
		if (str)
			xmlNewTextChild (dest_node, NULL, "email", str);
	} else {
		GList *iter = dest->priv->list_dests;
		
		while (iter) {
			EDestination *list_dest = E_DESTINATION (iter->data);
			xmlNodePtr list_node = xmlNewNode (NULL, "list_entry");
			
			str = e_destination_get_name (list_dest);
			if (str)
				xmlNewTextChild (list_node, NULL, "name", str);
			
			str = e_destination_get_email (list_dest);
			if (str)
				xmlNewTextChild (list_node, NULL, "email", str);

			xmlAddChild (dest_node, list_node);
			
			iter = g_list_next (iter);
		}

		xmlNewProp (dest_node, "is_list", "yes");
	}

	str = e_destination_get_card_uri (dest);
	if (str) {
		gchar buf[16];
		xmlNodePtr uri_node = xmlNewTextChild (dest_node, NULL, "card_uri", str);
		g_snprintf (buf, 16, "%d", e_destination_get_email_num (dest));
		xmlNewProp (uri_node, "email_num", buf);
	}

	xmlNewProp (dest_node, "html_mail", e_destination_get_html_mail_pref (dest) ? "yes" : "no");

	return dest_node;
}

gboolean
e_destination_xml_decode (EDestination *dest, xmlNodePtr node)
{
	gchar *name = NULL, *email = NULL, *card_uri = NULL;
	gint email_num = -1;
	gboolean html_mail = FALSE;
	gboolean is_list = FALSE;
	gchar *tmp;
	GList *list_dests = NULL;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);
	
	if (strcmp (node->name, "destination"))
		return FALSE;
	
	tmp = xmlGetProp (node, "html_mail");
	if (tmp) {
		html_mail = !strcmp (tmp, "yes");
		xmlFree (tmp);
	}
	
	tmp = xmlGetProp (node, "is_list");
	if (tmp) {
		is_list = !strcmp (tmp, "yes");
		xmlFree (tmp);
	}
	
	node = node->xmlChildrenNode;
	while (node) {
		if (!strcmp (node->name, "name")) {
			tmp = xmlNodeGetContent (node);
			g_free (name);
			name = g_strdup (tmp);
			xmlFree (tmp);
		} else if (!is_list && !strcmp (node->name, "email")) {
			tmp = xmlNodeGetContent (node);
			g_free (email);
			email = g_strdup (tmp);
			xmlFree (tmp);
		} else if (is_list && !strcmp (node->name, "list_entry")) {
			xmlNodePtr subnode = node->xmlChildrenNode;
			gchar *list_name = NULL, *list_email = NULL;
			
			while (subnode) {
				if (!strcmp (subnode->name, "name")) {
					tmp = xmlNodeGetContent (subnode);
					g_free (list_name);
					list_name = g_strdup (tmp);
					xmlFree (tmp);
				} else if (!strcmp (subnode->name, "email")) {
					tmp = xmlNodeGetContent (subnode);
					g_free (list_email);
					list_email = g_strdup (tmp);
					xmlFree (tmp);
				}
				
				subnode = subnode->next;
			}
			
			if (list_name || list_email) {
				EDestination *list_dest = e_destination_new ();
				if (list_name)
					e_destination_set_name (list_dest, list_name);
				if (list_email)
					e_destination_set_email (list_dest, list_email);
				
				list_dests = g_list_append (list_dests, list_dest);
			}
		} else if (!strcmp (node->name, "card_uri")) {
			tmp = xmlNodeGetContent (node);
			g_free (card_uri);
			card_uri = g_strdup (tmp);
			xmlFree (tmp);
			
			tmp = xmlGetProp (node, "email_num");
			email_num = atoi (tmp);
			xmlFree (tmp);
		}
		
		node = node->next;
	}
	
	e_destination_clear (dest);
	
	if (name)
		e_destination_set_name (dest, name);
	if (email)
		e_destination_set_email (dest, email);
	if (card_uri)
		e_destination_set_card_uri (dest, card_uri, email_num);
	if (list_dests)
		dest->priv->list_dests = list_dests;
	
	return TRUE;
}

/* FIXME: Make utf-8 safe */
static gchar *
null_terminate_and_remove_extra_whitespace (xmlChar *xml_in, gint size)
{
	gchar *xml;
	gchar *r, *w;
	gboolean skip_white = FALSE;

	if (xml_in == NULL || size <= 0) 
		return NULL;

	xml = g_strndup (xml_in, size);
	r = w = xml;

	while (*r) {
		if (*r == '\n' || *r == '\r') {
			skip_white = TRUE;
		} else {
			gboolean is_space = isspace (*r);

			*w = *r;

			if (! (skip_white && is_space))
				++w;
			if (! is_space)
				skip_white = FALSE;
		}
		++r;
	}

	*w = '\0';

	return xml;
}

gchar *
e_destination_export (const EDestination *dest)
{
	xmlNodePtr dest_node;
	xmlDocPtr  dest_doc;
	xmlChar   *buffer = NULL;
	gint       size = -1;
	gchar     *str;
	
	g_return_val_if_fail (dest && E_IS_DESTINATION (dest), NULL);

	dest_node = e_destination_xml_encode (dest);
	if (dest_node == NULL)
		return NULL;

	dest_doc = xmlNewDoc (XML_DEFAULT_VERSION);
	xmlDocSetRootElement (dest_doc, dest_node);

	xmlDocDumpMemory (dest_doc, &buffer, &size);
	xmlFreeDoc (dest_doc);

	str = null_terminate_and_remove_extra_whitespace (buffer, size);
	xmlFree (buffer);

	return str;
}

EDestination *
e_destination_import (const gchar *str)
{
	EDestination *dest = NULL;
	xmlDocPtr dest_doc;

	if (! (str && *str))
		return NULL;

	dest_doc = xmlParseMemory ((gchar *) str, strlen (str));
	if (dest_doc && dest_doc->xmlRootNode) {
		dest = e_destination_new ();
		if (! e_destination_xml_decode (dest, dest_doc->xmlRootNode)) {
			gtk_object_unref (GTK_OBJECT (dest));
			dest = NULL;
		}
	}
	xmlFreeDoc (dest_doc);

	return dest;
}

gchar *
e_destination_exportv (EDestination **destv)
{
	xmlDocPtr   destv_doc;
	xmlNodePtr  destv_node;
	xmlChar    *buffer = NULL;
	gint        size = -1;
	gchar      *str;
	gint        i;

	if (destv == NULL || *destv == NULL)
		return NULL;

	destv_doc  = xmlNewDoc (XML_DEFAULT_VERSION);
	destv_node = xmlNewNode (NULL, "destinations");
	xmlDocSetRootElement (destv_doc, destv_node);

	for (i=0; destv[i]; ++i) {
		if (! e_destination_is_empty (destv[i])) {
			xmlNodePtr dest_node = e_destination_xml_encode (destv[i]);
			if (dest_node)
				xmlAddChild (destv_node, dest_node);
		}
	}

	xmlDocDumpMemory (destv_doc, &buffer, &size);
	xmlFreeDoc (destv_doc);

	str = null_terminate_and_remove_extra_whitespace (buffer, size);
	xmlFree (buffer);

	return str;
}

EDestination **
e_destination_importv (const gchar *str)
{
	GPtrArray *dest_array = NULL;
	xmlDocPtr destv_doc;
	xmlNodePtr node;
	EDestination **destv = NULL;
	
	if (!(str && *str))
		return NULL;
	
	destv_doc = xmlParseMemory ((gchar *)str, strlen (str));
	node = destv_doc->xmlRootNode;
	
	if (strcmp (node->name, "destinations"))
		goto finished;
	
	node = node->xmlChildrenNode;
	
	dest_array = g_ptr_array_new ();
	
	while (node) {
		EDestination *dest;
		
		dest = e_destination_new ();
		if (e_destination_xml_decode (dest, node)) {
			g_ptr_array_add (dest_array, dest);
		} else {
			gtk_object_unref (GTK_OBJECT (dest));
		}
		
		node = node->next;
	}
	
	/* we need destv to be NULL terminated */
	g_ptr_array_add (dest_array, NULL);
	
	destv = (EDestination **) dest_array->pdata;
	g_ptr_array_free (dest_array, FALSE);
	
 finished:
	xmlFreeDoc (destv_doc);
	
	return destv;
}

static void
touch_cb (EBook *book, const gchar *addr, ECard *card, gpointer closure)
{
	if (book != NULL && card != NULL) {
		e_card_touch (card);
		g_message ("Use score for \"%s\" is now %f", addr, e_card_get_use_score (card));
		e_book_commit_card (book, card, NULL, NULL);
	}
}

void
e_destination_touch (EDestination *dest)
{
	const gchar *email;

	g_return_if_fail (dest && E_IS_DESTINATION (dest));

	email = e_destination_get_email (dest);

	if (email) {
		e_book_query_address_locally (email, touch_cb, NULL);
	}
}
