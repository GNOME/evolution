/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <NotZed@Ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *  Copyright 2001 Ximian Inc. (www.ximian.com)
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

/* (from glibc headers:
   POSIX says that <sys/types.h> must be included (by the caller) before <regex.h>.  */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include <signal.h>
#include <sys/wait.h>

#include <e-util/e-sexp.h>

#include <gal/util/e-iconv.h>

#include "camel-mime-message.h"
#include "camel-provider.h"
#include "camel-session.h"
#include "camel-filter-search.h"
#include "camel-exception.h"
#include "camel-multipart.h"
#include "camel-stream-mem.h"
#include "camel-stream-fs.h"
#include "camel-search-private.h"
#include "camel-i18n.h"
#include "camel-url.h"

#define d(x)

typedef struct {
	CamelSession *session;
	CamelFilterSearchGetMessageFunc get_message;
	void *get_message_data;
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	const char *source;
	CamelException *ex;
} FilterMessageSearch;

/* ESExp callbacks */
static ESExpResult *header_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_matches (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_exists (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_soundex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_full_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *match_all (struct _ESExp *f, int argc, struct _ESExpTerm **argv, FilterMessageSearch *fms);
static ESExpResult *body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *body_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *user_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *user_tag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *system_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_sent_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_received_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_current_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *header_source (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *get_size (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *pipe_message (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);
static ESExpResult *junk_test (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms);

/* builtin functions */
static struct {
	char *name;
	ESExpFunc *func;
	int type;		/* set to 1 if a function can perform shortcut evaluation, or
				   doesn't execute everything, 0 otherwise */
} symbols[] = {
	{ "match-all",          (ESExpFunc *) match_all,          1 },
	{ "body-contains",      (ESExpFunc *) body_contains,      0 },
	{ "body-regex",         (ESExpFunc *) body_regex,         0 },
	{ "header-contains",    (ESExpFunc *) header_contains,    0 },
	{ "header-matches",     (ESExpFunc *) header_matches,     0 },
	{ "header-starts-with", (ESExpFunc *) header_starts_with, 0 },
	{ "header-ends-with",   (ESExpFunc *) header_ends_with,   0 },
	{ "header-exists",      (ESExpFunc *) header_exists,      0 },
	{ "header-soundex",     (ESExpFunc *) header_soundex,     0 },
	{ "header-regex",       (ESExpFunc *) header_regex,       0 },
	{ "header-full-regex",  (ESExpFunc *) header_full_regex,  0 },
	{ "user-tag",           (ESExpFunc *) user_tag,           0 },
	{ "user-flag",          (ESExpFunc *) user_flag,          0 },
	{ "system-flag",        (ESExpFunc *) system_flag,        0 },
	{ "get-sent-date",      (ESExpFunc *) get_sent_date,      0 },
	{ "get-received-date",  (ESExpFunc *) get_received_date,  0 },
	{ "get-current-date",   (ESExpFunc *) get_current_date,   0 },
	{ "header-source",      (ESExpFunc *) header_source,      0 },
	{ "get-size",           (ESExpFunc *) get_size,           0 },
	{ "pipe-message",       (ESExpFunc *) pipe_message,       0 },
	{ "junk-test",          (ESExpFunc *) junk_test,          0 },
};


static CamelMimeMessage *
camel_filter_search_get_message (FilterMessageSearch *fms, struct _ESExp *sexp)
{
	if (fms->message)
		return fms->message;
	
	fms->message = fms->get_message (fms->get_message_data, fms->ex);
	
	if (fms->message == NULL)
		e_sexp_fatal_error (sexp, _("Failed to retrieve message"));
	
	return fms->message;
}

static ESExpResult *
check_header (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms, camel_search_match_t how)
{
	gboolean matched = FALSE;
	ESExpResult *r;
	int i;
	
	if (argc > 1 && argv[0]->type == ESEXP_RES_STRING) {
		char *name = argv[0]->value.string;
		const char *header;
		camel_search_t type = CAMEL_SEARCH_TYPE_ENCODED;
		CamelContentType *ct;
		const char *charset = NULL;
		
		if (strcasecmp(name, "x-camel-mlist") == 0) {
			header = camel_message_info_mlist(fms->info);
			type = CAMEL_SEARCH_TYPE_MLIST;
		} else {
			CamelMimeMessage *message = camel_filter_search_get_message (fms, f);
			
			header = camel_medium_get_header (CAMEL_MEDIUM (message), argv[0]->value.string);
			/* FIXME: what about Resent-To, Resent-Cc and Resent-From? */
			if (strcasecmp("to", name) == 0 || strcasecmp("cc", name) == 0 || strcasecmp("from", name) == 0)
				type = CAMEL_SEARCH_TYPE_ADDRESS_ENCODED;
			else {
				ct = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));
				if (ct) {
					charset = camel_content_type_param (ct, "charset");
					charset = e_iconv_charset_name (charset);
				}
			}
		}
		
		if (header) {
			for (i=1; i<argc && !matched; i++) {
				if (argv[i]->type == ESEXP_RES_STRING)
					matched = camel_search_header_match(header, argv[i]->value.string, how, type, charset);
			}
		}
	}
	
	r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	r->value.bool = matched;
	
	return r;
}

static ESExpResult *
header_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header (f, argc, argv, fms, CAMEL_SEARCH_MATCH_CONTAINS);
}


static ESExpResult *
header_matches (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header (f, argc, argv, fms, CAMEL_SEARCH_MATCH_EXACT);
}

static ESExpResult *
header_starts_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header (f, argc, argv, fms, CAMEL_SEARCH_MATCH_STARTS);
}

static ESExpResult *
header_ends_with (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header (f, argc, argv, fms, CAMEL_SEARCH_MATCH_ENDS);
}

static ESExpResult *
header_soundex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	return check_header (f, argc, argv, fms, CAMEL_SEARCH_MATCH_SOUNDEX);
}

static ESExpResult *
header_exists (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	CamelMimeMessage *message;
	gboolean matched = FALSE;
	ESExpResult *r;
	int i;
	
	message = camel_filter_search_get_message (fms, f);
	
	for (i = 0; i < argc && !matched; i++) {
		if (argv[i]->type == ESEXP_RES_STRING)
			matched = camel_medium_get_header (CAMEL_MEDIUM (message), argv[i]->value.string) != NULL;
	}
	
	r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	r->value.bool = matched;
	
	return r;
}

static ESExpResult *
header_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	CamelMimeMessage *message;
	regex_t pattern;
	const char *contents;
	
	message = camel_filter_search_get_message (fms, f);
	
	if (argc > 1 && argv[0]->type == ESEXP_RES_STRING
	    && (contents = camel_medium_get_header (CAMEL_MEDIUM (message), argv[0]->value.string))
	    && camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_REGEX|CAMEL_SEARCH_MATCH_ICASE, argc-1, argv+1, fms->ex) == 0) {
		r->value.bool = regexec (&pattern, contents, 0, NULL, 0) == 0;
		regfree (&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static gchar *
get_full_header (CamelMimeMessage *message)
{
	CamelMimePart *mp = CAMEL_MIME_PART (message);
	GString *str = g_string_new ("");
	char   *ret;
	struct _camel_header_raw *h;
	
	for (h = mp->headers; h; h = h->next) {
		if (h->value != NULL) {
			g_string_append (str, h->name);
			if (isspace (h->value[0]))
				g_string_append (str, ":");
			else
				g_string_append (str, ": ");
			g_string_append (str, h->value);
			g_string_append_c(str, '\n');
		}
	}
	
	ret = str->str;
	g_string_free (str, FALSE);
	
	return ret;
}

static ESExpResult *
header_full_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	CamelMimeMessage *message;
	regex_t pattern;
	char *contents;
	
	if (camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_REGEX|CAMEL_SEARCH_MATCH_ICASE|CAMEL_SEARCH_MATCH_NEWLINE,
					   argc, argv, fms->ex) == 0) {
		message = camel_filter_search_get_message (fms, f);
		contents = get_full_header (message);
		r->value.bool = regexec (&pattern, contents, 0, NULL, 0) == 0;
		g_free (contents);
		regfree (&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
match_all (struct _ESExp *f, int argc, struct _ESExpTerm **argv, FilterMessageSearch *fms)
{
	/* match-all: when dealing with single messages is a no-op */
	ESExpResult *r;
	
	if (argc > 0)
		return e_sexp_term_eval (f, argv[0]);
	
	r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	r->value.bool = TRUE;
	
	return r;
}

static ESExpResult *
body_contains (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	CamelMimeMessage *message;
	regex_t pattern;
	
	if (camel_search_build_match_regex (&pattern, CAMEL_SEARCH_MATCH_ICASE, argc, argv, fms->ex) == 0) {
		message = camel_filter_search_get_message (fms, f);
		r->value.bool = camel_search_message_body_contains ((CamelDataWrapper *) message, &pattern);
		regfree (&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
body_regex (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	CamelMimeMessage *message;
	regex_t pattern;
	
	if (camel_search_build_match_regex(&pattern, CAMEL_SEARCH_MATCH_ICASE|CAMEL_SEARCH_MATCH_REGEX|CAMEL_SEARCH_MATCH_NEWLINE,
					   argc, argv, fms->ex) == 0) {
		message = camel_filter_search_get_message (fms, f);
		r->value.bool = camel_search_message_body_contains ((CamelDataWrapper *) message, &pattern);
		regfree (&pattern);
	} else
		r->value.bool = FALSE;
	
	return r;
}

static ESExpResult *
user_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	gboolean truth = FALSE;
	int i;
	
	/* performs an OR of all words */
	for (i = 0; i < argc && !truth; i++) {
		if (argv[i]->type == ESEXP_RES_STRING
		    && camel_flag_get (&fms->info->user_flags, argv[i]->value.string)) {
			truth = TRUE;
			break;
		}
	}
	
	r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	r->value.bool = truth;
	
	return r;
}

static ESExpResult *
system_flag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	if (argc != 1 || argv[0]->type != ESEXP_RES_STRING)
		e_sexp_fatal_error(f, _("Invalid arguments to (system-flag)"));
	
	r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	r->value.bool = camel_system_flag_get (fms->info->flags, argv[0]->value.string);
	
	return r;
}

static ESExpResult *
user_tag (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	const char *tag;

	if (argc != 1 || argv[0]->type != ESEXP_RES_STRING)
		e_sexp_fatal_error(f, _("Invalid arguments to (user-tag)"));
	
	tag = camel_tag_get (&fms->info->user_tags, argv[0]->value.string);
	
	r = e_sexp_result_new (f, ESEXP_RES_STRING);
	r->value.string = g_strdup (tag ? tag : "");
	
	return r;
}

static ESExpResult *
get_sent_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	CamelMimeMessage *message;
	ESExpResult *r;
	
	message = camel_filter_search_get_message (fms, f);
	r = e_sexp_result_new (f, ESEXP_RES_INT);
	r->value.number = camel_mime_message_get_date (message, NULL);
	
	return r;
}

static ESExpResult *
get_received_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	CamelMimeMessage *message;
	ESExpResult *r;
	
	message = camel_filter_search_get_message (fms, f);
	r = e_sexp_result_new (f, ESEXP_RES_INT);
	r->value.number = camel_mime_message_get_date_received (message, NULL);
	
	return r;
}

static ESExpResult *
get_current_date (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new (f, ESEXP_RES_INT);
	r->value.number = time (NULL);
	
	return r;
}

static ESExpResult *
header_source (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	CamelMimeMessage *message;
	ESExpResult *r;
	const char *src;
	int truth = FALSE, i;
	CamelProvider *provider;
	CamelURL *uria, *urib;

	if (fms->source) {
		src = fms->source;
	} else {
		message = camel_filter_search_get_message(fms, f);
		src = camel_mime_message_get_source(message);
	}

	if (src
	    && (provider = camel_provider_get(src, NULL))
	    && provider->url_equal) {
		uria = camel_url_new(src, NULL);
		if (uria) {
			for (i=0;i<argc && !truth;i++) {
				if (argv[i]->type == ESEXP_RES_STRING
				    && (urib = camel_url_new(argv[i]->value.string, NULL))) {
					truth = provider->url_equal(uria, urib);
					camel_url_free(urib);
				}
			}
			camel_url_free(uria);
		}
	}

	r = e_sexp_result_new(f, ESEXP_RES_BOOL);
	r->value.bool = truth;
	
	return r;
}

/* remember, the size comparisons are done at Kbytes */
static ESExpResult *
get_size (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	
	r = e_sexp_result_new(f, ESEXP_RES_INT);
	r->value.number = fms->info->size / 1024;

	return r;
}

static int
run_command (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	CamelMimeMessage *message;
	CamelStream *stream;
	int result, status;
	int in_fds[2];
	pid_t pid;
	
	if (argc < 1 || argv[0]->value.string[0] == '\0')
		return 0;
	
	if (pipe (in_fds) == -1) {
		camel_exception_setv (fms->ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to create pipe to '%s': %s"),
				      argv[0]->value.string, g_strerror (errno));
		return -1;
	}
	
	if (!(pid = fork ())) {
		/* child process */
		GPtrArray *args;
		int maxfd, fd, i;
		
		fd = open ("/dev/null", O_WRONLY);
		
		if (dup2 (in_fds[0], STDIN_FILENO) < 0 ||
		    dup2 (fd, STDOUT_FILENO) < 0 ||
		    dup2 (fd, STDERR_FILENO) < 0)
			_exit (255);
		
		setsid ();
		
		maxfd = sysconf (_SC_OPEN_MAX);
		for (fd = 3; fd < maxfd; fd++)
			fcntl (fd, F_SETFD, FD_CLOEXEC);
		
		args = g_ptr_array_new ();
		for (i = 0; i < argc; i++)
			g_ptr_array_add (args, argv[i]->value.string);
		g_ptr_array_add (args, NULL);
		
		execvp (argv[0]->value.string, (char **) args->pdata);
		
		g_ptr_array_free (args, TRUE);
		
		d(printf ("Could not execute %s: %s\n", argv[0]->value.string, g_strerror (errno)));
		_exit (255);
	} else if (pid < 0) {
		camel_exception_setv (fms->ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to create create child process '%s': %s"),
				      argv[0]->value.string, g_strerror (errno));
		return -1;
	}
	
	/* parent process */
	close (in_fds[0]);
	
	message = camel_filter_search_get_message (fms, f);
	
	stream = camel_stream_fs_new_with_fd (in_fds[1]);
	camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), stream);
	camel_stream_flush (stream);
	camel_object_unref (stream);
	
	result = waitpid (pid, &status, 0);
	
	if (result == -1 && errno == EINTR) {
		/* child process is hanging... */
		kill (pid, SIGTERM);
		sleep (1);
		result = waitpid (pid, &status, WNOHANG);
		if (result == 0) {
			/* ...still hanging, set phasers to KILL */
			kill (pid, SIGKILL);
			sleep (1);
			result = waitpid (pid, &status, WNOHANG);
		}
	}
	
	if (result != -1 && WIFEXITED (status))
		return WEXITSTATUS (status);
	else
		return -1;
}

static ESExpResult *
pipe_message (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	int retval, i;
	
	/* make sure all args are strings */
	for (i = 0; i < argc; i++) {
		if (argv[i]->type != ESEXP_RES_STRING) {
			retval = -1;
			goto done;
		}
	}
	
	retval = run_command (f, argc, argv, fms);
	
 done:
	r = e_sexp_result_new (f, ESEXP_RES_INT);
	r->value.number = retval;
	
	return r;
}

static ESExpResult *
junk_test (struct _ESExp *f, int argc, struct _ESExpResult **argv, FilterMessageSearch *fms)
{
	ESExpResult *r;
	gboolean retval = FALSE;

	if (fms->session->junk_plugin != NULL) {
		retval = camel_junk_plugin_check_junk (fms->session->junk_plugin, camel_filter_search_get_message (fms, f));
		
		printf("junk filter => %s\n", retval ? "*JUNK*" : "clean");
	}

	r = e_sexp_result_new (f, ESEXP_RES_BOOL);
	r->value.number = retval;

	return r;
}

/**
 * camel_filter_search_match:
 * @session:
 * @get_message: function to retrieve the message if necessary
 * @data: data for above
 * @info:
 * @source:
 * @expression:
 * @ex:
 *
 * Returns one of CAMEL_SEARCH_MATCHED, CAMEL_SEARCH_NOMATCH, or CAMEL_SEARCH_ERROR.
 **/
int
camel_filter_search_match (CamelSession *session,
			   CamelFilterSearchGetMessageFunc get_message, void *data,
			   CamelMessageInfo *info, const char *source,
			   const char *expression, CamelException *ex)
{
	FilterMessageSearch fms;
	ESExp *sexp;
	ESExpResult *result;
	gboolean retval;
	int i;

	fms.session = session;
	fms.get_message = get_message;
	fms.get_message_data = data;
	fms.message = NULL;
	fms.info = info;
	fms.source = source;
	fms.ex = ex;
	
	sexp = e_sexp_new ();
	
	for (i = 0; i < sizeof (symbols) / sizeof (symbols[0]); i++) {
		if (symbols[i].type == 1)
			e_sexp_add_ifunction (sexp, 0, symbols[i].name, (ESExpIFunc *)symbols[i].func, &fms);
		else
			e_sexp_add_function (sexp, 0, symbols[i].name, symbols[i].func, &fms);
	}
	
	e_sexp_input_text (sexp, expression, strlen (expression));
	if (e_sexp_parse (sexp) == -1) {
		if (!camel_exception_is_set (ex))
			/* A filter search is a search through your filters, ie. your filters is the corpus being searched thru. */
			camel_exception_setv (ex, 1, _("Error executing filter search: %s: %s"),
					      e_sexp_error (sexp), expression);
		goto error;
	}
	
	result = e_sexp_eval (sexp);
	if (result == NULL) {
		if (!camel_exception_is_set (ex))
			camel_exception_setv (ex, 1, _("Error executing filter search: %s: %s"),
					      e_sexp_error (sexp), expression);
		goto error;
	}
	
	if (result->type == ESEXP_RES_BOOL)
		retval = result->value.bool ? CAMEL_SEARCH_MATCHED : CAMEL_SEARCH_NOMATCH;
	else
		retval = CAMEL_SEARCH_NOMATCH;
	
	e_sexp_result_free (sexp, result);
	e_sexp_unref (sexp);
	
	if (fms.message)
		camel_object_unref (fms.message);
	
	return retval;
	
 error:
	if (fms.message)
		camel_object_unref (fms.message);
	
	e_sexp_unref (sexp);
	
	return CAMEL_SEARCH_ERROR;
}
