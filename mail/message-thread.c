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

#define TIMEIT

#ifdef TIMEIT
#include <sys/time.h>
#include <unistd.h>
#endif

/* for debug only */
int dump_tree(struct _container *c);

static void
container_add_child(struct _container *node, struct _container *child)
{
	d(printf("\nAdding child %p to parent %p \n", child, node));
	child->next = node->child;
	node->child = child;
	child->parent = node;
}

static void
container_parent_child(struct _container *parent, struct _container *child)
{
	struct _container *c, *node;

	/* are we already the right parent? */
	if (child->parent == parent)
		return;

	/* are we unparented? */
	if (child->parent == NULL) {
		container_add_child(parent, child);
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
			container_add_child(parent, child);
			return;
		}
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
				g_free(c);
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

/* this can be pretty slow, but not used often */
/* clast cannot be null */
static void
remove_node(struct _container **list, struct _container *node, struct _container **clast)
{
	struct _container *c;

	/* this is intentional, even if it looks funny */
	/* if we have a parent, then we should remove it from the parent list,
	   otherwise we remove it from the root list */
	if (node->parent) {
		c = (struct _container *)&node->parent->child;
	} else {
		c = (struct _container *)list;
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
				g_free(c);
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

				scan = g_malloc0(sizeof(*scan));
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
dump_tree_rec(struct _tree_info *info, struct _container *c, int depth)
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
			printf("%s %p Subject: %s <%s>\n", p, c, c->message->subject, c->message->message_id);
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
dump_tree(struct _container *c)
{
	int count;
	struct _tree_info info;

	info.visited = g_hash_table_new(g_direct_hash, g_direct_equal);
	count = dump_tree_rec(&info, c, 0);
	g_hash_table_destroy(info.visited);
	return count;
}

static void
container_free(struct _container *c)
{
	struct _container *n;

	while (c) {
		n = c->next;
		if (c->child)
			container_free(c->child); /* free's children first */
		g_free(c);
		c = n;
	}
}

void
thread_messages_free(struct _thread_messages *thread)
{
	container_free(thread->tree);
	g_free(thread);
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
		return -1;
	else
		return 1;
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

/* NOTE: This function assumes you have obtained the relevant locks for
   the folder, BEFORE calling it */
struct _thread_messages *
thread_messages(CamelFolder *folder, GPtrArray *uids)
{
	GHashTable *id_table, *no_id_table;
	int i;
	struct _container *c, *p, *child, *head;
	struct _header_references *ref;
	struct _thread_messages *thread;

#ifdef TIMEIT
	struct timeval start, end;
	unsigned long diff;

	gettimeofday(&start, NULL);
#endif

	id_table = g_hash_table_new(g_str_hash, g_str_equal);
	no_id_table = g_hash_table_new(NULL, NULL);
	for (i=0;i<uids->len;i++) {
		const CamelMessageInfo *mi;
		mi = camel_folder_get_message_info (folder, uids->pdata[i]);

		if (mi == NULL) {
			g_warning("Folder doesn't contain uid %s", (char *)uids->pdata[i]);
			continue;
		}

		if (mi->message_id) {
			c = g_hash_table_lookup(id_table, mi->message_id);
			/* check for duplicate messages */
			if (c) {
				/* if duplicate, just make out it is a no-id message,  but try and insert it
				   into the right spot in the tree */
				d(printf("doing: (duplicate message id)\n"));
				c = g_malloc0(sizeof(*c));
				g_hash_table_insert(no_id_table, (void *)mi, c);
			} else {
				d(printf("doing : %s\n", mi->message_id));
				c = g_malloc0(sizeof(*c));
				g_hash_table_insert(id_table, mi->message_id, c);
			}
		} else {
			d(printf("doing : (no message id)\n"));
			c = g_malloc0(sizeof(*c));
			g_hash_table_insert(no_id_table, (void *)mi, c);
		}

		c->message = mi;
		c->order = i;
		child = c;
		ref = mi->references;
		p = NULL;
		head = NULL;
		d(printf("references:\n"));
		while (ref) {
			if (ref->id == NULL) {
				/* this shouldn't actually happen, and indicates
				   some problems in camel */
				d(printf("ref missing id!?\n"));
				ref = ref->next;
				continue;
			}

			d(printf("looking up reference: %s\n", ref->id));
			c = g_hash_table_lookup(id_table, ref->id);
			if (c == NULL) {
				d(printf("not found\n"));
				c = g_malloc0(sizeof(*c));
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
	i = dump_tree(head);
	printf("%d count, %d items in tree\n", uids->len, i);
#endif

	sort_thread(&head);

	thread = g_malloc(sizeof(*thread));
	thread->tree = head;

#ifdef TIMEIT
	gettimeofday(&end, NULL);
	diff = end.tv_sec * 1000 + end.tv_usec/1000;
	diff -= start.tv_sec * 1000 + start.tv_usec/1000;
	printf("Message threading %d messages took %ld.%03ld seconds\n",
	       uids->len, diff / 1000, diff % 1000);
#endif
	return thread;
}

/* intended for incremental update.  Not implemented yet as, well, its probbaly
   not worth it (memory overhead vs speed, may as well just rethread the whole
   lot?)

   But it might be implemented at a later date.
*/
void
thread_messages_add(struct _thread_messages *thread, CamelFolder *folder, GPtrArray *uids)
{
	
}

void
thread_messages_remove(struct _thread_messages *thread, CamelFolder *folder, GPtrArray *uids)
{
	
}

/* ** THREAD MESSAGES ***************************************************** */

typedef struct thread_messages_input_s {
	MessageList *ml;
	GPtrArray *uids;
	gboolean use_camel_uidfree;
	void (*build) (MessageList *, struct _container *);
} thread_messages_input_t;

typedef struct thread_messages_data_s {
	struct _thread_messages *thread;
} thread_messages_data_t;

static gchar *describe_thread_messages (gpointer in_data, gboolean gerund);
static void setup_thread_messages   (gpointer in_data, gpointer op_data, CamelException *ex);
static void do_thread_messages      (gpointer in_data, gpointer op_data, CamelException *ex);
static void cleanup_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex);

static gchar *describe_thread_messages (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup (_("Threading message list"));
	else
		return g_strdup (_("Thread message list"));
}

static void setup_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	thread_messages_input_t *input = (thread_messages_input_t *) in_data;

	gtk_object_ref (GTK_OBJECT (input->ml));
}

static void do_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	thread_messages_input_t *input = (thread_messages_input_t *) in_data;
	thread_messages_data_t *data = (thread_messages_data_t *) op_data;

	mail_tool_camel_lock_up ();
	data->thread = thread_messages (input->ml->folder, input->uids);
	mail_tool_camel_lock_down ();
}

static void cleanup_thread_messages (gpointer in_data, gpointer op_data, CamelException *ex)
{
	thread_messages_input_t *input = (thread_messages_input_t *) in_data;
	thread_messages_data_t *data = (thread_messages_data_t *) op_data;

	(input->build) (input->ml, data->thread->tree);
	thread_messages_free (data->thread);

	if (input->use_camel_uidfree) {
		mail_tool_camel_lock_up ();
		camel_folder_free_uids (input->ml->folder, input->uids);
		mail_tool_camel_lock_down ();
	} else {
		g_ptr_array_add(input->uids, 0);
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

	folder = camel_store_get_folder (store, "mbox", 0, ex);
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
