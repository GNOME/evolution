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

#ifndef _CAMEL_LIST_UTILS_H
#define _CAMEL_LIST_UTILS_H

/* This is a copy of Amiga's Exec lists, the head and tail nodes are
 * overlapped and merged into a single list header.  All operations
 * are O(1), including node removal and addition from/to either end of
 * the list.  It can be used to implement O(1) queues, fifo's and
 * normal lists.  You don't need the list header to remove the node. */

typedef struct _CamelDList CamelDList;
typedef struct _CamelDListNode CamelDListNode;

/**
 * struct _CamelDListNode - A double-linked list node.
 * 
 * @next: The next node link.
 * @prev: The previous node link.
 * 
 * A double-linked list node header.  Put this at the start of the
 * list node structure.  Data is stored in the list by subclassing the
 * node header rather than using a pointer.  Or more normally by just
 * duplicating the next and previous pointers for better type safety.
 **/
struct _CamelDListNode {
	struct _CamelDListNode *next;
	struct _CamelDListNode *prev;
};

/**
 * struct _CamelDList - A double-linked list header.
 * 
 * @head: The head node's next pointer.
 * @tail: The tail node's next pointer.
 * @tailpred: The previous node to the tail node.
 * 
 * This is the merging of two separate head and tail nodes into a
 * single structure.  i.e. if you ahve a NULL terminated head and tail
 * node such as head = { first, NULL } and tail = { NULL, last } then
 * overlap them at the common NULL, you get this structure.
 *
 * The list header must be initialised with camel_dlist_init, or by
 * using the static CAMEL_DLIST_INITIALISER macro.
 **/
struct _CamelDList {
	struct _CamelDListNode *head;
	struct _CamelDListNode *tail;
	struct _CamelDListNode *tailpred;
};

#define CAMEL_DLIST_INITIALISER(l) { (CamelDListNode *)&l.tail, 0, (CamelDListNode *)&l.head }

void camel_dlist_init(CamelDList *v);
CamelDListNode *camel_dlist_addhead(CamelDList *l, CamelDListNode *n);
CamelDListNode *camel_dlist_addtail(CamelDList *l, CamelDListNode *n);
CamelDListNode *camel_dlist_remove(CamelDListNode *n);
CamelDListNode *camel_dlist_remhead(CamelDList *l);
CamelDListNode *camel_dlist_remtail(CamelDList *l);
int camel_dlist_empty(CamelDList *l);
int camel_dlist_length(CamelDList *l);

/* This is provided mostly for orthogonality with the dlist structure.
 * By making the nodes contain all of the data themselves it
 * simplifies memory management.  Removing and adding from/to the head
 * of the list is O(1), the rest of the operations are O(n). */

typedef struct _CamelSListNode CamelSListNode;
typedef struct _CamelSList CamelSList;

/**
 * struct _CamelSListNode - A single-linked list node.
 * 
 * @next: The next node in the list.
 *
 * A single-linked list node header.  Put this at hte start of the
 * actual list node structure, or more commonly, just a next pointer.
 * Data is stored in the list node by subclassing the node-header
 * rather than using a pointer.
 **/
struct _CamelSListNode {
	struct _CamelSListNode *next;
};

/**
 * struct _CamelSList - A single-linked list header.
 * 
 * @head: The head of the list.
 * 
 * This is the header of a single-linked list.
 **/
struct _CamelSList {
	struct _CamelSListNode *head;
};

#define CAMEL_SLIST_INITIALISER(l) { 0 }

void camel_slist_init(CamelSList *l);
CamelSListNode *camel_slist_addhead(CamelSList *l, CamelSListNode *n);
CamelSListNode *camel_slist_addtail(CamelSList *l, CamelSListNode *n);
CamelSListNode *camel_slist_remove(CamelSList *l, CamelSListNode *n);
CamelSListNode *camel_slist_remhead(CamelSList *l);
CamelSListNode *camel_slist_remtail(CamelSList *l);
int camel_slist_empty(CamelSList *l);
int camel_slist_length(CamelSList *l);

#endif
