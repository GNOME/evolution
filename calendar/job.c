/* GNOME personal calendar server - job manager
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "job.h"



/* The job list */

typedef struct {
	JobFunc func;
	gpointer data;
} Job;

static GSList *jobs_head;
static GSList *jobs_tail;

static guint jobs_idle_id;



/* Runs a job and dequeues it */
static gboolean
run_job (gpointer data)
{
	Job *job;
	GSList *l;

	g_assert (jobs_head != NULL);

	job = jobs_head->data;
	(* job->func) (job->data);
	g_free (job);

	l = jobs_head;
	jobs_head = g_slist_remove_link (jobs_head, jobs_head);
	g_slist_free_1 (l);

	if (!jobs_head) {
		jobs_tail = NULL;
		jobs_idle_id = 0;
		return FALSE;
	} else
		return TRUE;
}

/**
 * job_add:
 * @func: Function to run the job.
 * @data: Data to pass to @function.
 * 
 * Adds a job to the queue.  The job will automatically be run asynchronously.
 **/
void
job_add (JobFunc func, gpointer data)
{
	Job *job;

	g_return_if_fail (func != NULL);

	job = g_new (Job, 1);
	job->func = func;
	job->data = data;

	if (!jobs_head) {
		g_assert (jobs_tail == NULL);
		g_assert (jobs_idle_id == 0);

		jobs_head = g_slist_append (NULL, job);
		jobs_tail = jobs_head;

		jobs_idle_id = g_idle_add (run_job, NULL);
	} else {
		g_assert (jobs_tail != NULL);
		g_assert (jobs_idle_id != 0);

		jobs_tail = g_slist_append (jobs_tail, job)->next;
	}
}
