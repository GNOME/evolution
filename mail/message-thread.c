/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#include "camel/camel.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <ctype.h>

#include "message-thread.h"
#include "mail-tools.h"
#include "mail-threads.h"

#define d(x)
/*#define LEAKDEBUG*/

/* **************************************** */
/* mem leak debug stuff */

#ifdef LEAKDEBUG

static GHashTable *allocedht = NULL;

#define EXISTS (1 << 0)
#define WALKED (1 << 1)
#define FREED  (1 << 2)
#define AL_401 (1 << 3)
#define AL_541 (1 << 4)
#define AL_546 (1 << 5)
#define AL_569 (1 << 6)
#define LINKED (1 << 7)
#define EXCUSED (1 << 8)

#define GITP(x) GINT_TO_POINTER(x)
#define GPTI(x) GPOINTER_TO_INT(x)

static struct _container *
alloc_container (int where)
{
	struct _container *c;

	c = g_new0 (struct _container, 1);

	if (!allocedht)
		allocedht = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_hash_table_insert (allocedht, c, GITP(EXISTS|where));
	return c;
}

static void
free_container (struct _container **c)
{
	gpointer flags;

	memset ((*c), 0, sizeof (struct _container));
	if ((flags = g_hash_table_lookup (allocedht, (*c))) == NULL)
		printf ("** threading mem debug: freeing unalloced entry %p?\n", (*c));
	g_hash_table_insert (allocedht, (*c), GITP(GPTI(flags)|FREED));
	g_free ((*c));
	(*c) = NULL;
}

static void 
cont_print (gpointer key, gpointer value, gpointer user)
{
	struct _container *c = (struct _container *) key;
	char *line;

	if (GPTI(value) & FREED)
		return;

	if (GPTI(value) & AL_401)
		line = "401";
	else if (GPTI(value) & AL_541)
		line = "541";
	else if (GPTI(value) & AL_546)
		line = "546";
	else if (GPTI(value) & AL_569)
		line = "569";
	else
		line = "???";

	printf ("   %p : %s %s %s %s %s",
		c,
		GPTI(value) & FREED ? "freed" : "unfrd",
		GPTI(value) & WALKED ? "walked" : "unwlkd",
		line,
		GPTI(value) & LINKED ? "linked" : "unlnkd",
		GPTI(value) & EXCUSED ? "excused" : "unexcsd");

	if ((GPTI(value) & FREED) == 0) {
		gpointer oth_flags;

		printf (" : %p %p %p : \"%s\" \"%s\" %d %d", 
			c->next, c->parent, c->child,
			c->message ? c->message->subject : "(null message)",
			c->root_subject ? c->root_subject : "(null root-subject)",
			c->re, c->order);

		if (c->next) {
			oth_flags = g_hash_table_lookup (allocedht, c->next);

			printf ("\n     next : %p : %s %s %s",
				c->next,
				GPTI(oth_flags) & WALKED ? "walked" : "unwlkd",
				GPTI(oth_flags) & LINKED ? "linked" : "unlnkd",
				GPTI(oth_flags) & EXCUSED ? "excused" : "unexcsd");
		}

		if (c->parent) {
			oth_flags = g_hash_table_lookup (allocedht, c->parent);

			printf ("\n     prnt : %p : %s %s %s",
				c->parent,
				GPTI(oth_flags) & WALKED ? "walked" : "unwlkd",
				GPTI(oth_flags) & LINKED ? "linked" : "unlnkd",
				GPTI(oth_flags) & EXCUSED ? "excused" : "unexcsd");
		}

		if (c->child) {
			oth_flags = g_hash_table_lookup (allocedht, c->child);

			printf ("\n     chld : %p : %s %s %s",
				c->child,
				GPTI(oth_flags) & WALKED ? "walked" : "unwlkd",
				GPTI(oth_flags) & LINKED ? "linked" : "unlnkd",
				GPTI(oth_flags) & EXCUSED ? "excused" : "unexcsd");
		}
		
	}

	printf ("\n");
		      
}

static void 
make_excuses (gpointer key, gpointer value, gpointer user)
{
	struct _container *c;
	gpointer chldflags;
	gpointer nextflags;

	if (GPTI(value) & FREED)
		return;

	c = (struct _container *) key;

	if (c->next) {
		nextflags = g_hash_table_lookup (allocedht, c->next);

		if ((GPTI(nextflags) & EXCUSED) == 0) {
			g_hash_table_insert (allocedht, c->next, GITP(GPTI(nextflags)|EXCUSED));
			((gint *)user) = 1;
		}
	}

	if (c->child) {
		chldflags = g_hash_table_lookup (allocedht, c->child);

		if ((GPTI(chldflags) & EXCUSED) == 0) {
			g_hash_table_insert (allocedht, c->child, GITP(GPTI(chldflags)|EXCUSED));
			((gint *)user) = 1;
		}
	}
}

static void 
print_containers (void)
{
	gint hit;

	do {
		hit = 0;
		g_hash_table_foreach (allocedht, make_excuses, &hit);
	} while (hit);

	printf ("List of container stats:\n");
	g_hash_table_foreach (allocedht, cont_print, NULL);
	printf ("End of list.\n");
}

static void
walk_containers (struct _container *head)
{
	gpointer flags;

	while (head) {
		if (head->child)
			walk_containers (head->child);
		if ((flags = g_hash_table_lookup (allocedht, head)) == NULL) {
			printf ("*** walk_containers : bad pointer %p\n", head);
		} else {
			g_hash_table_insert (allocedht, head, GITP(GPTI(flags)|WALKED));
		}
		
		head = head->next;
	}
}

static void
link_container (struct _container *c)
{
	gpointer flags;

	if ((flags = g_hash_table_lookup (allocedht, c)) == NULL)
		printf ("** threading mem debug: linking unalloced entry %p?\n", c);
	g_hash_table_insert (allocedht, c, GITP(GPTI(flags)|LINKED));
}

#else
#define alloc_container(w) (g_new0 (struct _container, 1))
#define free_container(c) g_free (*(c))
#define print_containers()
#define walk_containers(c)
#define link_container(c)
#endif

/* **************************************** */

static struct _container *thread_messages(CamelFolder *folder, GPtrArray *uids);
static void thread_messages_free(struct _container *);

/* for debug only */
int dump_tree(struct _container *c, int depth);

static void
container_add_child(struct _container *node, struct _container *child)
{
	d(printf("\nAdding child %p to parent %p \n", child, node));
	child->next = node->child;
	node->child = child;
	child->parent = node;
}

#if 0
static void
container_unparent_child(struct _container *child)
{
	struct _container *c, *node;

	/* are we unparented? */
	if (child->parent == NULL) {
		return;
	}

	/* else remove child from its existing parent, and reparent */
	node = child->parent;
	c = (struct _container *)&node->child;
	d(printf("scanning children:\n"));
	while (c->next) {
		d(printf(" %p\n", c));
	        if (c->next==child) {
			d(printf("found node %p\n", child));
			c->next = c->next->next;
			child->parent = NULL;
			return;
		}
		c = c->next;
	}

	printf("DAMN, we shouldn't  be here!\n");
}
#endif

static void
container_parent_child(struct _container *parent, struct _container *child)
{
	struct _container *c, *node, **prev;

	/* are we already the right parent? */
	if (child->parent == parent)
		return;

	/* are we unparented? */
	if (child->parent == NULL) {
		container_add_child(parent, child);
		return;
	}

	/* check for trying to make my child my parent */
	for (c = parent; c; c = c->parent) {
		if (c == child) {
			d(printf("AIIE: trying to lop off hunk of nodes!\n"));
			return;
		}
	}

	/* else remove child from its existing parent, and reparent */
	node = child->parent;

	/*	c = (struct _container *)&node->child;
	 *d(printf("scanning children:\n"));
	 *while (c->next) {
	 *	d(printf(" %p\n", c));
	 *       if (c->next==child) {
	 *		d(printf("found node %p\n", child));
	 *		c->next = c->next->next;
	 *		child->parent = NULL;
	 *		container_add_child(parent, child);
	 *		return;
	 *	}
	 *	c = c->next;
	 *}
	 */

	d(printf("PKGW deparent child"));
	c = node->child;
	prev = &(node->child);
	while (c) {
		d(printf ("   %p\n", c));

		if (c == child) {
			d(printf ("    hit child %p\n", child));
			(*prev) = c->next;
			c->next = NULL;
			c->parent = NULL;
			container_add_child (parent, child);
			return;
		}

		prev = &(c->next);
		c = c->next;
	}

	printf("DAMN, we shouldn't  be here!\n");
}

static void
prune_empty(struct _container **cp)
{
	struct _container *child, *next, *c, *lastc;

	/* yes, this is intentional */
	lastc = (struct _container *)cp;
	while (lastc->next) {
		c = lastc->next;

		d(printf("checking message %p %p (%s)\n", c,
			 c->message, c->message?c->message->message_id:"<empty>"));
		if (c->message == NULL) {
			if (c->child == NULL) {
				d(printf("removing empty node\n"));
				lastc->next = c->next;
				free_container (&c);
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
				free_container (&c);
				continue;
			}
		}
		prune_empty(&c->child);
		lastc = c;
	}
}

static void
hashloop(void *key, void *value, void *data)
{
	struct _container *c = value;
	struct _container *tail = data;

	if (c->parent == NULL) {
		c->next = tail->next;
		tail->next = c;
		link_container(c);
	}
}

static char *
get_root_subject(struct _container *c, int *re)
{
	char *s, *p;
	struct _container *scan;
	
	s = NULL;
	*re = FALSE;
	if (c->message)
		s = c->message->subject;
	else {
		/* one of the children will always have a message */
		scan = c->child;
		while (scan) {
			if (scan->message) {
				s = scan->message->subject;
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
					*re = TRUE;
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

/* this is pretty slow, but not used often */
static void
remove_node(struct _container **list, struct _container *node, struct _container **clast)
{
	struct _container *c;

	/* this is intentional, even if it looks funny */
	c = (struct _container *)list;
	while (c->next) {
		if (c->next == node) {
			if (clast && *clast == c->next)
				*clast = c;
			c->next = c->next->next;
			break;
		}
		c = c->next;
	}
}

static void
group_root_set(struct _container **cp)
{
	GHashTable *subject_table = g_hash_table_new(g_str_hash, g_str_equal);
	struct _container *c, *clast, *scan, *container;

	/* gather subject lines */ 
	d(printf("gathering subject lines\n"));
	clast = (struct _container *)cp;
	c = clast->next;
	while (c) {
		c->root_subject = get_root_subject(c, &c->re);
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
	clast = (struct _container *)cp;
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
				scan = (struct _container *)&container->child;
				while (scan->next)
					scan = scan->next;
				scan->next = c->child;
				clast->next = c->next;
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

				remove_node(cp, container, &clast);
				remove_node(cp, c, &clast);

				scan = alloc_container(AL_401);
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

int
dump_tree(struct _container *c, int depth)
{
	char *p;
	int count=0;

	p = alloca(depth*2+1);
	memset(p, ' ', depth*2);
	p[depth*2] = 0;

	while (c) {
		if (c->message) {
			printf("%s %p Subject: %s <%s>\n", p, c, c->message->subject, c->message->message_id);
			count += 1;
		} else {
			printf("%s %p <empty>\n", p, c);
		}
		if (c->child)
			count += dump_tree(c->child, depth+1);
		c = c->next;
	}
	return count;
}

static void thread_messages_free(struct _container *c)
{
	struct _container *n;

	/* FIXME: ok, for some reason this doesn't work .. investigate later ... */

	while (c) {
		n = c->next;
		if (c->child)
			thread_messages_free(c->child); /* free's children first */
		free_container (&c);
		c = n;
	}
}

static int
sort_node(const void *a, const void *b)
{
	const struct _container *a1 = ((struct _container **)a)[0];
	const struct _container *b1 = ((struct _container **)b)[0];

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
		return 1;
	else
		return -1;
}

static void
sort_thread(struct _container **cp)
{
	struct _container *c, *head, **carray;
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
	carray = alloca(size*sizeof(struct _container *));
	c = *cp;
	size=0;
	while (c) {
		carray[size] = c;
		c = c->next;
		size++;
	}
	qsort(carray, size, sizeof(struct _container *), sort_node);
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

static struct _container *
thread_messages(CamelFolder *folder, GPtrArray *uids)
{
	GHashTable *id_table, *no_id_table;
	int i;
	struct _container *c, *p, *child, *head, *container;
	struct _header_references *ref;

	id_table = g_hash_table_new(g_str_hash, g_str_equal);
	no_id_table = g_hash_table_new(NULL, NULL);
	for (i=0;i<uids->len;i++) {
		const CamelMessageInfo *mi;
		mail_tool_camel_lock_up ();
		mi = camel_folder_get_message_info (folder, uids->pdata[i]);
		mail_tool_camel_lock_down ();

		if (mi == NULL) {
			g_warning("Folder doesn't contain uid %s", (char *)uids->pdata[i]);
			continue;
		}

		if (mi->message_id) {
			d(printf("doing : %s\n", mi->message_id));
			c = g_hash_table_lookup(id_table, mi->message_id);
			if (!c) {
				c = alloc_container(AL_541);
				g_hash_table_insert(id_table, mi->message_id, c);
			}
		} else {
			d(printf("doing : (no message id)\n"));
			c = alloc_container(AL_546);
			g_hash_table_insert(no_id_table, (void *)mi, c);
		}

		c->message = mi;
		c->order = i;
		container = c;
		ref = mi->references;
		p = NULL;
		child = container;
		head = NULL;
		d(printf("references:\n"));
		while (ref) {
			if (ref->id == NULL) {
				printf("ref missing id!?\n");
				ref = ref->next;
				continue;
			}

			d(printf("looking up reference: %s\n", ref->id));
			c = g_hash_table_lookup(id_table, ref->id);
			if (c == NULL) {
				d(printf("not found\n"));
				c = alloc_container(AL_569);
				g_hash_table_insert(id_table, ref->id, c);
			}
			if (c!=child)
				container_parent_child(c, child);
			child = c;
			if (head == NULL)
				head = c;
			ref = ref->next;
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
	prune_empty(&head);

	/* find any siblings which missed out */
	group_root_set(&head);

#if 0
	printf("finished\n");
	i = dump_tree(head, 0);
	printf("%d count, %d items in tree\n", uids->len, i);
#endif

	sort_thread(&head);
	return head;
}

/* ** THREAD MESSAGES ***************************************************** */

typedef struct thread_messages_input_s {
	MessageList *ml;
	GPtrArray *uids;
	gboolean use_camel_uidfree;
	void (*build) (MessageList *, struct _container *);
} thread_messages_input_t;

typedef struct thread_messages_data_s {
	struct _container *container;
} thread_messages_data_t;

static gchar *describe_thread_messages (gpointer in_data, gboolean gerund);
static void setup_thread_messages   (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_thread_messages      (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex);

static gchar *describe_thread_messages (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup ("Threading message list");
	else
		return g_strdup ("Thread message list");
}

static void setup_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	thread_messages_input_t *input = (thread_messages_input_t *) in_data;

	if (!IS_MESSAGE_LIST (input->ml)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No messagelist to thread was provided to thread_messages");
		return;
	}

	if (!input->uids) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No uids were provided to thread_messages");
		return;
	}

	if (!input->build) {
		camel_exception_set (ex, CAMEL_EXCEPTION_INVALID_PARAM,
				     "No build callback provided to thread_messages");
		return;
	}

	gtk_object_ref (GTK_OBJECT (input->ml));
}

static void do_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	thread_messages_input_t *input = (thread_messages_input_t *) in_data;
	thread_messages_data_t *data = (thread_messages_data_t *) op_data;

	data->container = thread_messages (input->ml->folder, input->uids);
}

static void cleanup_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	thread_messages_input_t *input = (thread_messages_input_t *) in_data;
	thread_messages_data_t *data = (thread_messages_data_t *) op_data;

	(input->build) (input->ml, data->container);
	walk_containers (data->container);
	thread_messages_free (data->container);

	print_containers();

	if (input->use_camel_uidfree) {
		mail_tool_camel_lock_up ();
		camel_folder_free_uids (input->ml->folder, input->uids);
		mail_tool_camel_lock_down ();
	} else {
		g_ptr_array_add (input->uids, NULL);
		g_strfreev ((char **)input->uids->pdata);
		g_ptr_array_free (input->uids, FALSE);
	}

	gtk_object_unref (GTK_OBJECT (input->ml));
}

static const mail_operation_spec op_thread_messages =
{
	describe_thread_messages,
	sizeof (thread_messages_data_t),
	setup_thread_messages,
	do_thread_messages,
	cleanup_thread_messages
};

void mail_do_thread_messages (MessageList *ml, GPtrArray *uids, 
			      gboolean use_camel_uidfree,
			      void (*build) (MessageList *,
					     struct _container *))
{
	thread_messages_input_t *input;

	input = g_new (thread_messages_input_t, 1);
	input->ml = ml;
	input->uids = uids;
	input->use_camel_uidfree = use_camel_uidfree;
	input->build = build;

	mail_operation_queue (&op_thread_messages, input, TRUE);
}

/* ************************************************************************ */

#ifdef STANDALONE

static char *
auth_callback(char *prompt, gboolean secret,
	      CamelService *service, char *item,
	      CamelException *ex)
{
	printf ("auth_callback called: %s\n", prompt);
	return NULL;
}

int
main (int argc, char**argv)
{
	CamelSession *session;
	CamelException *ex;
	CamelStore *store;
	gchar *store_url = "mbox:///home/notzed/evolution/local/Inbox";
	CamelFolder *folder;
	CamelMimeMessage *message;
	GList *uid_list;
	GPtrArray *summary;

	gtk_init (&argc, &argv);
	camel_init ();		
	ex = camel_exception_new ();
	
	session = camel_session_new (auth_callback);
	store = camel_session_get_store (session, store_url, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_session_get_store\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

	folder = camel_store_get_folder (store, "mbox", TRUE, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught in camel_store_get_folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}

#if 0
	camel_folder_open (folder, FOLDER_OPEN_RW, ex);
	if (camel_exception_get_id (ex)) {
		printf ("Exception caught when trying to open the folder\n"
			"Full description : %s\n", camel_exception_get_description (ex));
		return -1;
	}
#endif

	summary = camel_folder_get_summary(folder);
	thread_messages((CamelMessageInfo **)summary->pdata, summary->len);

	return 0;
}

#endif

/*

  msgid: d
  references: a b c

  msgid: f
  references: c d

  msgid: e
  references: c

  a
   \
    b
     \
      c
       \
        d
        |\
        e f
 */
/*
  lookup d
    create new node d
  child = d
  loop on c b a
    lookup node?
    if no node, create node
    add child to node
    child = node
  endloop

 */
