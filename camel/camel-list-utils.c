/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2004 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "camel-list-utils.h"

/**
 * camel_dlist_init:
 * @v: 
 * 
 * Initialise a double-linked list header.  All list headers must be
 * initialised before use.
 **/
void camel_dlist_init(CamelDList *v)
{
        v->head = (CamelDListNode *)&v->tail;
        v->tail = 0;
        v->tailpred = (CamelDListNode *)&v->head;
}

/**
 * camel_dlist_addhead:
 * @l: An initialised list header.
 * @n: A node, the next and prev pointers will be overwritten.
 * 
 * Add the list node @n to the head (start) of the list @l.
 * 
 * Return value: @n.
 **/
CamelDListNode *camel_dlist_addhead(CamelDList *l, CamelDListNode *n)
{
        n->next = l->head;
        n->prev = (CamelDListNode *)&l->head;
        l->head->prev = n;
        l->head = n;
        return n;
}

/**
 * camel_dlist_addtail:
 * @l: An intialised list header.
 * @n: A node, the next and prev pointers will be overwritten.
 * 
 * Add the list onde @n to the tail (end) of the list @l.
 * 
 * Return value: @n.
 **/
CamelDListNode *camel_dlist_addtail(CamelDList *l, CamelDListNode *n)
{
        n->next = (CamelDListNode *)&l->tail;
        n->prev = l->tailpred;
        l->tailpred->next = n;
        l->tailpred = n;
        return n;
}

/**
 * camel_dlist_remove:
 * @n: A node which is part of a list.
 * 
 * Remove @n from the list it's in.  @n must belong to a list.
 * 
 * Return value: @n.
 **/
CamelDListNode *camel_dlist_remove(CamelDListNode *n)
{
        n->next->prev = n->prev;
        n->prev->next = n->next;
        return n;
}

/**
 * camel_dlist_remhead:
 * @l: An initialised list, maybe containing items.
 * 
 * Remove the head node (start) of the list.
 * 
 * xReturn value: The previously first-node in the list, or NULLif @l
 * is an empty list.
 **/
CamelDListNode *camel_dlist_remhead(CamelDList *l)
{
	CamelDListNode *n, *nn;

	n = l->head;
	nn = n->next;
	if (nn) {
		nn->prev = n->prev;
		l->head = nn;
		return n;
	}
	return NULL;
}

/**
 * camel_dlist_remtail:
 * @l: An initialised list, maybe containing items.
 * 
 * Remove the last node in the list.
 * 
 * Return value: The previously last-node in the list, or NULL if @l
 * is an empty list.
 **/
CamelDListNode *camel_dlist_remtail(CamelDList *l)
{
	CamelDListNode *n, *np;

	n = l->tailpred;
	np = n->prev;
	if (np) {
		np->next = n->next;
		l->tailpred = np;
		return n;
	}
	return NULL;
}

/**
 * camel_dlist_empty:
 * @l: An initialised list header.
 * 
 * Returns %TRUE if @l is an empty list.
 * 
 * Return value: %TRUE if @l is an empty list, %FALSE otherwise.
 **/
int camel_dlist_empty(CamelDList *l)
{
	return (l->head == (CamelDListNode *)&l->tail);
}

/**
 * camel_dlist_length:
 * @l: An initialised list header.
 * 
 * Returns the number of nodes in the list @l.
 * 
 * Return value: The number of nodes.
 **/
int camel_dlist_length(CamelDList *l)
{
	CamelDListNode *n, *nn;
	int count = 0;

	n = l->head;
	nn = n->next;
	while (nn) {
		count++;
		n = nn;
		nn = n->next;
	}

	return count;
}

/* This is just for orthogonal completeness */

void camel_slist_init(CamelSList *v)
{
	v->head = NULL;
}

CamelSListNode *camel_slist_addhead(CamelSList *l, CamelSListNode *n)
{
        n->next = l->head;
	l->head = n;

	return n;
}

CamelSListNode *camel_slist_addtail(CamelSList *l, CamelSListNode *n)
{
	CamelSListNode *p;

	p = (CamelSListNode *)l;
	while (p->next)
		p = p->next;
	n->next = NULL;
	p->next = n;

        return n;
}

CamelSListNode *camel_slist_remove(CamelSList *l, CamelSListNode *n)
{
	CamelSListNode *p, *q;

	p = (CamelSListNode *)l;
	while ( (q = p->next) ) {
		if (q == n) {
			p->next = n->next;
			return n;
		}
		p = q;
	}

	g_warning("Trying to remove SList node not present in SList");

	return NULL;
}

CamelSListNode *camel_slist_remhead(CamelSList *l)
{
	CamelSListNode *n;

	n = l->head;
	if (n)
		l->head = n->next;

	return n;
}

CamelSListNode *camel_slist_remtail(CamelSList *l)
{
	CamelSListNode *n, *p;

	n = l->head;
	if (l->head == NULL)
		return NULL;
	p = (CamelSListNode *)l;
	while (n->next) {
		p = n;
		n = n->next;
	}
	p->next = NULL;

	return n;
}

int camel_slist_empty(CamelSList *l)
{
	return (l->head == NULL);
}

int camel_slist_length(CamelSList *l)
{
	CamelSListNode *n;
	int count = 0;

	n = l->head;
	while (n) {
		count++;
		n = n->next;
	}

	return count;
}

