/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
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
 */

/* TODO: This could probably be made a camel object, but it isn't really required */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <glib.h>

#include "camel-folder-thread.h"
#include "e-util/e-memory.h"

#define d(x) 
#define m(x) 

/*#define TIMEIT*/

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

static void
container_add_child(CamelFolderThreadNode *node, CamelFolderThreadNode *child)
{
	d(printf("\nAdding child %p to parent %p \n", child, node));
	child->next = node->child;
	node->child = child;
	child->parent = node;
}

static void
container_parent_child(CamelFolderThreadNode *parent, CamelFolderThreadNode *child)
{
	CamelFolderThreadNode *c, *node;

	/* are we already the right parent? */
	if (child->parent == parent)
		return;

	/* would this create a loop? */
	node = parent->parent;
	while (node) {
		if (node == child)
			return;
		node = node->parent;
	}

	/* are we unparented? */
	if (child->parent == NULL) {
		container_add_child(parent, child);
		return;
	}

	/* else remove child from its existing parent, and reparent */
	node = child->parent;
	c = (CamelFolderThreadNode *)&node->child;
	d(printf("scanning children:\n"));
	while (c->next) {
		d(printf(" %p\n", c));
	        if (c->next==child) {
			d(printf("found node %p\n", child));
			c->next = c->next->next;
			child->parent = NULL;
			container_add_child(parent, child);
			return;
		}
		c = c->next;
	}

	printf("DAMN, we shouldn't  be here!\n");
}

static void
prune_empty(CamelFolderThread *thread, CamelFolderThreadNode **cp)
{
	CamelFolderThreadNode *child, *next, *c, *lastc;

	/* yes, this is intentional */
	lastc = (CamelFolderThreadNode *)cp;
	while (lastc->next) {
		c = lastc->next;

		d(printf("checking message %p %p (%08x%08x)\n", c,
			 c->message, c->message?c->message->message_id.id.part.hi:0,
			 c->message?c->message->message_id.id.part.lo:0));
		if (c->message == NULL) {
			if (c->child == NULL) {
				d(printf("removing empty node\n"));
				lastc->next = c->next;
				m(memset(c, 0xfe, sizeof(*c)));
				e_memchunk_free(thread->node_chunks, c);
				continue;
			}
			if (c->parent || c->child->next==0) {
				d(printf("promoting child\n"));
				lastc->next = c->next; /* remove us */
				child = c->child;
				while (child) {
					next = child->next;

					child->parent = c->parent;
					child->next = lastc->next;
					lastc->next = child;

					child = next;
				}
				continue;
			}
		}
		prune_empty(thread, &c->child);
		lastc = c;
	}
}

static void
hashloop(void *key, void *value, void *data)
{
	CamelFolderThreadNode *c = value;
	CamelFolderThreadNode *tail = data;

	if (c->parent == NULL) {
		c->next = tail->next;
		tail->next = c;
	}
}

static char *
get_root_subject(CamelFolderThreadNode *c)
{
	char *s, *p;
	CamelFolderThreadNode *scan;
	
	s = NULL;
	c->re = FALSE;
	if (c->message)
		s = (char *)camel_message_info_subject(c->message);
	else {
		/* one of the children will always have a message */
		scan = c->child;
		while (scan) {
			if (scan->message) {
				s = (char *)camel_message_info_subject(scan->message);
				break;
			}
			scan = scan->next;
		}
	}
	if (s != NULL) {
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
					c->re = TRUE;
					s = p+1;
				} else
					break;
			} else
				break;
		}
		if (*s)
			return s;
	}
	return NULL;
}

/* this can be pretty slow, but not used often */
/* clast cannot be null */
static void
remove_node(CamelFolderThreadNode **list, CamelFolderThreadNode *node, CamelFolderThreadNode **clast)
{
	CamelFolderThreadNode *c;

	/* this is intentional, even if it looks funny */
	/* if we have a parent, then we should remove it from the parent list,
	   otherwise we remove it from the root list */
	if (node->parent) {
		c = (CamelFolderThreadNode *)&node->parent->child;
	} else {
		c = (CamelFolderThreadNode *)list;
	}
	while (c->next) {
		if (c->next == node) {
			if (*clast == c->next)
				*clast = c;
			c->next = c->next->next;
			return;
		}
		c = c->next;
	}

	printf("ERROR: removing node %p failed\n", node);
}

static void
group_root_set(CamelFolderThread *thread, CamelFolderThreadNode **cp)
{
	GHashTable *subject_table = g_hash_table_new(g_str_hash, g_str_equal);
	CamelFolderThreadNode *c, *clast, *scan, *container;

	/* gather subject lines */ 
	d(printf("gathering subject lines\n"));
	clast = (CamelFolderThreadNode *)cp;
	c = clast->next;
	while (c) {
		c->root_subject = get_root_subject(c);
		if (c->root_subject) {
			container = g_hash_table_lookup(subject_table, c->root_subject);
			if (container == NULL
			    || (container->message == NULL && c->message)
			    || (container->re == TRUE && !c->re)) {
				g_hash_table_insert(subject_table, c->root_subject, c);
			}
		}
		c = c->next;
	}

	/* merge common subjects? */
	clast = (CamelFolderThreadNode *)cp;
	while (clast->next) {
		c = clast->next;
		d(printf("checking %p %s\n", c, c->root_subject));
		if (c->root_subject
		    && (container = g_hash_table_lookup(subject_table, c->root_subject))
		    && (container != c)) {
			d(printf(" matching %p %s\n", container, container->root_subject));
			if (c->message == NULL && container->message == NULL) {
				d(printf("merge containers children\n"));
				/* steal the children from c onto container, and unlink c */
				scan = (CamelFolderThreadNode *)&container->child;
				while (scan->next)
					scan = scan->next;
				scan->next = c->child;
				clast->next = c->next;
				m(memset(c, 0xee, sizeof(*c)));
				e_memchunk_free(thread->node_chunks, c);
				continue;
			} if (c->message == NULL && container->message != NULL) {
				d(printf("container is non-empty parent\n"));
				remove_node(cp, container, &clast);
				container_add_child(c, container);
			} else if (c->message != NULL && container->message == NULL) {
				d(printf("container is empty child\n"));
				clast->next = c->next;
				container_add_child(container, c);
				continue;
			} else if (c->re && !container->re) {
				d(printf("container is re\n"));
				clast->next = c->next;
				container_add_child(container, c);
				continue;
			} else if (!c->re && container->re) {
				d(printf("container is not re\n"));
				remove_node(cp, container, &clast);
				container_add_child(c, container);
			} else if (c->re && container->re) {
				d(printf("subjects are common %p and %p\n", c, container));

				/* build a phantom node */
				remove_node(cp, container, &clast);
				remove_node(cp, c, &clast);

				scan = e_memchunk_alloc0(thread->node_chunks);

				scan->root_subject = c->root_subject;
				scan->re = c->re && container->re;
				scan->next = c->next;
				clast->next = scan;
				container_add_child(scan, c);
				container_add_child(scan, container);
				clast = scan;
				g_hash_table_insert(subject_table, scan->root_subject, scan);
				continue;
			}
		}
		clast = c;
	}
	g_hash_table_destroy(subject_table);
}

struct _tree_info {
	GHashTable *visited;
};

static int
dump_tree_rec(struct _tree_info *info, CamelFolderThreadNode *c, int depth)
{
	char *p;
	int count=0;

	p = alloca(depth*2+1);
	memset(p, ' ', depth*2);
	p[depth*2] = 0;

	while (c) {
		if (g_hash_table_lookup(info->visited, c)) {
			printf("WARNING: NODE REVISITED: %p\n", c);
		} else {
			g_hash_table_insert(info->visited, c, c);
		}
		if (c->message) {
			printf("%s %p Subject: %s <%.8s>\n", p, c, camel_message_info_subject(c->message), camel_message_info_message_id(c->message)->id.hash);
			count += 1;
		} else {
			printf("%s %p <empty>\n", p, c);
		}
		if (c->child)
			count += dump_tree_rec(info, c->child, depth+1);
		c = c->next;
	}
	return count;
}

int
camel_folder_threaded_messages_dump(CamelFolderThreadNode *c)
{
	int count;
	struct _tree_info info;

	info.visited = g_hash_table_new(g_direct_hash, g_direct_equal);
	count = dump_tree_rec(&info, c, 0);
	g_hash_table_destroy(info.visited);
	return count;
}

static int
sort_node(const void *a, const void *b)
{
	const CamelFolderThreadNode *a1 = ((CamelFolderThreadNode **)a)[0];
	const CamelFolderThreadNode *b1 = ((CamelFolderThreadNode **)b)[0];

	/* if we have no message, it must be a dummy node, which 
	   also means it must have a child, just use that as the
	   sort data (close enough?) */
	if (a1->message == NULL)
		a1 = a1->child;
	if (b1->message == NULL)
		b1 = b1->child;
	if (a1->order == b1->order)
		return 0;
	if (a1->order < b1->order)
		return -1;
	else
		return 1;
}

static void
sort_thread(CamelFolderThreadNode **cp)
{
	CamelFolderThreadNode *c, *head, **carray;
	int size=0;

	c = *cp;
	while (c) {
		/* sort the children while we're at it */
		if (c->child)
			sort_thread(&c->child);
		size++;
		c = c->next;
	}
	if (size<2)
		return;
	carray = alloca(size*sizeof(CamelFolderThreadNode *));
	c = *cp;
	size=0;
	while (c) {
		carray[size] = c;
		c = c->next;
		size++;
	}
	qsort(carray, size, sizeof(CamelFolderThreadNode *), sort_node);
	size--;
	head = carray[size];
	head->next = NULL;
	size--;
	do {
		c = carray[size];
		c->next = head;
		head = c;
		size--;
	} while (size>=0);
	*cp = head;
}

static guint id_hash(void *key)
{
	CamelSummaryMessageID *id = (CamelSummaryMessageID *)key;

	return id->id.part.lo;
}

static gint id_equal(void *a, void *b)
{
	return ((CamelSummaryMessageID *)a)->id.id == ((CamelSummaryMessageID *)b)->id.id;
}

/* perform actual threading */
static void
thread_summary(CamelFolderThread *thread, GPtrArray *summary)
{
	GHashTable *id_table, *no_id_table;
	int i;
	CamelFolderThreadNode *c, *child, *head;
#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	gettimeofday(&start, NULL);
#endif

	id_table = g_hash_table_new((GHashFunc)id_hash, (GCompareFunc)id_equal);
	no_id_table = g_hash_table_new(NULL, NULL);
	for (i=0;i<summary->len;i++) {
		CamelMessageInfo *mi = summary->pdata[i];
		const CamelSummaryMessageID *mid = camel_message_info_message_id(mi);
		const CamelSummaryReferences *references = camel_message_info_references(mi);

		if (mid->id.id) {
			c = g_hash_table_lookup(id_table, mid);
			/* check for duplicate messages */
			if (c && c->order) {
				/* if duplicate, just make out it is a no-id message,  but try and insert it
				   into the right spot in the tree */
				d(printf("doing: (duplicate message id)\n"));
				c = e_memchunk_alloc0(thread->node_chunks);
				g_hash_table_insert(no_id_table, (void *)mi, c);
			} else if (!c) {
				d(printf("doing : %08x%08x (%s)\n", mid->id.part.hi, mid->id.part.lo, camel_message_info_subject(mi)));
				c = e_memchunk_alloc0(thread->node_chunks);
				g_hash_table_insert(id_table, (void *)mid, c);
			}
		} else {
			d(printf("doing : (no message id)\n"));
			c = e_memchunk_alloc0(thread->node_chunks);
			g_hash_table_insert(no_id_table, (void *)mi, c);
		}

		c->message = mi;
		c->order = i+1;
		child = c;
		if (references) {
			int j;

			d(printf("references:\n"));
			for (j=0;j<references->size;j++) {
				/* should never be empty, but just incase */
				if (references->references[j].id.id == 0)
					continue;

				c = g_hash_table_lookup(id_table, &references->references[j]);
				if (c == NULL) {
					d(printf("not found\n"));
					c = e_memchunk_alloc0(thread->node_chunks);
					g_hash_table_insert(id_table, (void *)&references->references[j], c);
				}
				if (c!=child)
					container_parent_child(c, child);
				child = c;
			}
		}
	}

	d(printf("\n\n"));
	/* build a list of root messages (no parent) */
	head = NULL;
	g_hash_table_foreach(id_table, hashloop, &head);
	g_hash_table_foreach(no_id_table, hashloop, &head);

	g_hash_table_destroy(id_table);
	g_hash_table_destroy(no_id_table);

	/* remove empty parent nodes */
	prune_empty(thread, &head);
	
	/* find any siblings which missed out - but only if we are allowing threading by subject */
	if (thread->subject)
		group_root_set (thread, &head);

#if 0
	printf("finished\n");
	i = camel_folder_thread_messages_dump(head);
	printf("%d count, %d items in tree\n", uids->len, i);
#endif

	sort_thread(&head);

	/* remove any phantom nodes, this could possibly be put in group_root_set()? */
	c = (CamelFolderThreadNode *)&head;
	while (c && c->next) {
		CamelFolderThreadNode *scan, *newtop;

		child = c->next;
		if (child->message == NULL) {
			newtop = child->child;
			newtop->parent = NULL;
			/* unlink pseudo node */
			c->next = newtop;

			/* link its siblings onto the end of its children, fix all parent pointers */
			scan = (CamelFolderThreadNode *)&newtop->child;
			while (scan->next) {
				scan = scan->next;
			}
			scan->next = newtop->next;
			while (scan->next) {
				scan = scan->next;
				scan->parent = newtop;
			}

			/* and link the now 'real' node into the list */
			newtop->next = child->next;
			c = newtop;
			m(memset(child, 0xde, sizeof(*child)));
			e_memchunk_free(thread->node_chunks, child);
		} else {
			c = child;
		}
	}

	/* this is only debug assertion stuff */
	c = (CamelFolderThreadNode *)&head;
	while (c->next) {
		c = c->next;
		if (c->message == NULL)
			g_warning("threading missed removing a pseudo node: %s\n", c->root_subject);
		if (c->parent != NULL)
			g_warning("base node has a non-null parent: %s\n", c->root_subject);
	}

	thread->tree = head;

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Message threading %d messages took %ld.%03ld seconds\n",
	       summary->len, diff / 1000, diff % 1000);
#endif
}

/**
 * camel_folder_thread_messages_new:
 * @folder: 
 * @uids: The subset of uid's to thread.  If NULL. then thread all
 * uid's in @folder.
 * @thread_subject: thread based on subject also
 * 
 * Thread a (subset) of the messages in a folder.  And sort the result
 * in summary order.
 *
 * If @thread_subject is %TRUE, messages with
 * related subjects will also be threaded. The default behaviour is to
 * only thread based on message-id.
 * 
 * This function is probably to be removed soon.
 *
 * Return value: A CamelFolderThread contianing a tree of CamelFolderThreadNode's
 * which represent the threaded structure of the messages.
 **/
CamelFolderThread *
camel_folder_thread_messages_new (CamelFolder *folder, GPtrArray *uids, gboolean thread_subject)
{
	CamelFolderThread *thread;
	GHashTable *wanted = NULL;
	GPtrArray *summary;
	GPtrArray *fsummary;
	int i;

	thread = g_malloc(sizeof(*thread));
	thread->refcount = 1;
	thread->subject = thread_subject;
	thread->tree = NULL;
	thread->node_chunks = e_memchunk_new(32, sizeof(CamelFolderThreadNode));
	thread->folder = folder;
	camel_object_ref((CamelObject *)folder);

	/* get all of the summary items of interest in summary order */
	if (uids) {
		wanted = g_hash_table_new(g_str_hash, g_str_equal);
		for (i=0;i<uids->len;i++)
			g_hash_table_insert(wanted, uids->pdata[i], uids->pdata[i]);
	}

	fsummary = camel_folder_get_summary(folder);
	thread->summary = summary = g_ptr_array_new();

	for (i=0;i<fsummary->len;i++) {
		CamelMessageInfo *info = fsummary->pdata[i];

		if (wanted == NULL || g_hash_table_lookup(wanted, camel_message_info_uid(info)) != NULL) {
			camel_folder_ref_message_info(folder, info);
			g_ptr_array_add(summary, info);
		}
	}

	camel_folder_free_summary(folder, fsummary);

	thread_summary(thread, summary);

	if (wanted)
		g_hash_table_destroy(wanted);

	return thread;
}

/* add any still there, in the existing order */
static void
add_present_rec(CamelFolderThread *thread, GHashTable *have, GPtrArray *summary, CamelFolderThreadNode *node)
{
	while (node) {
		const char *uid = camel_message_info_uid(node->message);

		if (g_hash_table_lookup(have, (char *)uid)) {
			g_hash_table_remove(have, (char *)camel_message_info_uid(node->message));
			g_ptr_array_add(summary, (void *) node->message);
		} else {
			camel_folder_free_message_info(thread->folder, (CamelMessageInfo *)node->message);
		}

		if (node->child)
			add_present_rec(thread, have, summary, node->child);
		node = node->next;
	}
}

void
camel_folder_thread_messages_apply(CamelFolderThread *thread, GPtrArray *uids)
{
	int i;
	GPtrArray *all;
	GHashTable *table;
	CamelMessageInfo *info;

	all = g_ptr_array_new();
	table = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<uids->len;i++)
		g_hash_table_insert(table, uids->pdata[i], uids->pdata[i]);

	add_present_rec(thread, table, all, thread->tree);

	/* add any new ones, in supplied order */
	for (i=0;i<uids->len;i++)
		if (g_hash_table_lookup(table, uids->pdata[i]) && (info = camel_folder_get_message_info(thread->folder, uids->pdata[i])))
			g_ptr_array_add(all, info);

	g_hash_table_destroy(table);

	thread->tree = NULL;
	e_memchunk_destroy(thread->node_chunks);
	thread->node_chunks = e_memchunk_new(32, sizeof(CamelFolderThreadNode));
	thread_summary(thread, all);

	g_ptr_array_free(thread->summary, TRUE);
	thread->summary = all;
}

void
camel_folder_thread_messages_ref(CamelFolderThread *thread)
{
	thread->refcount++;
}

/**
 * camel_folder_thread_messages_unref:
 * @thread: 
 * 
 * Free all memory associated with the thread descriptor @thread.
 **/
void
camel_folder_thread_messages_unref(CamelFolderThread *thread)
{
	if (thread->refcount > 1) {
		thread->refcount--;
		return;
	}

	if (thread->folder) {
		int i;

		for (i=0;i<thread->summary->len;i++)
			camel_folder_free_message_info(thread->folder, thread->summary->pdata[i]);
		g_ptr_array_free(thread->summary, TRUE);
		camel_object_unref((CamelObject *)thread->folder);
	}
	e_memchunk_destroy(thread->node_chunks);
	g_free(thread);
}

#if 0
/**
 * camel_folder_thread_messages_new_summary:
 * @summary: Array of CamelMessageInfo's to thread.
 * 
 * Thread a list of MessageInfo's.  The summary must remain valid for the
 * life of the CamelFolderThread created by this function, and it is upto the
 * caller to ensure this.
 * 
 * Return value: A CamelFolderThread contianing a tree of CamelFolderThreadNode's
 * which represent the threaded structure of the messages.
 **/
CamelFolderThread *
camel_folder_thread_messages_new_summary(GPtrArray *summary)
{
	CamelFolderThread *thread;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	gettimeofday(&start, NULL);
#endif

	thread = g_malloc(sizeof(*thread));
	thread->refcount = 1;
	thread->tree = NULL;
	thread->node_chunks = e_memchunk_new(32, sizeof(CamelFolderThreadNode));
	thread->folder = NULL;
	thread->summary = NULL;

	thread_summary(thread, summary);

	return thread;
}

/* scan the list in depth-first fashion */
static void
build_summary_rec(GHashTable *have, GPtrArray *summary, CamelFolderThreadNode *node)
{
	while (node) {
		if (node->message)
			g_hash_table_insert(have, (char *)camel_message_info_uid(node->message), node->message);
		g_ptr_array_add(summary, node);
		if (node->child)
			build_summary_rec(have, summary, node->child);
		node = node->next;
	}
}

void
camel_folder_thread_messages_add(CamelFolderThread *thread, GPtrArray *summary)
{
	GPtrArray *all;
	int i;
	GHashTable *table;

	/* Instead of working out all the complex in's and out's of
	   trying to do an incremental summary generation, just redo the whole
	   thing with the summary in the current order - so it comes out
	   in the same order */

	all = g_ptr_array_new();
	table = g_hash_table_new(g_str_hash, g_str_equal);
	build_summary_rec(table, all, thread->tree);
	for (i=0;i<summary->len;i++) {
		CamelMessageInfo *info = summary->pdata[i];

		/* check its not already there, we dont want duplicates */
		if (g_hash_table_lookup(table, camel_message_info_uid(info)) == NULL)
			g_ptr_array_add(all, info);
	}
	g_hash_table_destroy(table);

	/* reset the tree, and rebuild fully */
	thread->tree = NULL;
	e_memchunk_empty(thread->node_chunks);
	thread_summary(thread, all);
}

static void
remove_uid_node_rec(CamelFolderThread *thread, GHashTable *table, CamelFolderThreadNode **list, CamelFolderThreadNode *parent)
{
	CamelFolderThreadNode *prev = NULL;
	CamelFolderThreadNode *node, *next, *child, *rest;

	node = (CamelFolderThreadNode *)list;
	next = node->next;
	while (next) {

		if (next->child)
			remove_uid_node_rec(thread, table, &next->child, next);

		/* do we have a node to remove? */
		if (next->message && g_hash_table_lookup(table, (char *)camel_message_info_uid(node->message))) {
			child = next->child;
			if (child) {
				/*
				  node
				  next
				   child
				   lchild
				  rest

				  becomes:
				  node
				  child
				  lchild
				  rest
				*/

				rest = next->next;
				node->next = child;
				e_memchunk_free(thread->node_chunks, next);
				next = child;
				do {
					lchild = child;
					child->parent = parent;
					child = child->next;
				} while (child);
				lchild->next = rest;
			} else {
				/*
				  node
				  next
				  rest
				  becomes:
				  node
				  rest */
				node->next = next->next;
				e_memchunk_free(thread->node_chunks, next);
				next = node->next;
			}
		} else {
			node = next;
			next = node->next;
		}
	}
}

void
camel_folder_thread_messages_remove(CamelFolderThread *thread, GPtrArray *uids)
{
	GHashTable *table;
	int i;

	table = g_hash_table_new(g_str_hash, g_str_equal);
	for (i=0;i<uids->len;i++)
		g_hash_table_insert(table, uids->pdata[i], uids->pdata[i]);

	remove_uid_node_rec(thread, table, &thread->tree, NULL);
	g_hash_table_destroy(table);
}

#endif
