/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>

static int
update_file (const gchar *path,
	     const gchar *lang,
	     const gchar *name,
	     const gchar *options)
{
	gchar *filename;
	int res = 0;

	filename = g_build_filename (path, name, NULL);

	if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		gchar *contents = NULL;
		GError *local_error = NULL;

		if (g_file_get_contents (filename, &contents, NULL, &local_error)) {
			#define HEAD_END "</head>"
			#define HEADER_START "<header><div class=\"inner pagewide\">"

			const gchar *head_end, *header_start;

			head_end = strstr (contents, HEAD_END);
			header_start = strstr (contents, HEADER_START);

			if (head_end && header_start) {
				GString *tmp = g_string_new (contents);
				gchar *script;
				const gchar *relative_path = "..";

				if (g_strcmp0 (lang, "C") == 0)
					relative_path = ".";

				script = g_strdup_printf (
					"<script type=\"text/javascript\">\n"
					"helpLanguageChanged = function() {\n"
					"	var selector = document.getElementById(\"helpLanguage\");\n"
					"	if (selector && selector.value) {\n"
					"		if (selector.value == 'C')\n"
					"			window.location.href = '%s/%s';\n"
					"		else\n"
					"			window.location.href = '%s/' + selector.value + '/%s';\n"
					"	}\n"
					"}\n"
					"</script>\n",
					relative_path, name,
					relative_path, name);

				g_string_insert (tmp, head_end - contents, script);

				g_free (script);

				header_start = strstr (tmp->str, HEADER_START);
				g_assert (HEADER_START != NULL);

				g_string_insert (tmp, header_start - tmp->str + strlen (HEADER_START), options);

				if (!g_file_set_contents (filename, tmp->str, tmp->len, &local_error)) {
					g_printerr ("Failed to overwrite '%s': %s\n", filename, local_error ? local_error->message : "Unknown error");
					res = 6;
				}

				g_string_free (tmp, TRUE);
			} else {
				if (!head_end)
					g_printerr ("Cannot find '" HEAD_END "' in '%s'\n", filename);
				if (!header_start)
					g_printerr ("Cannot find '" HEADER_START "' in '%s'\n", filename);
				res = 5;
			}

			g_free (contents);

			#undef HEAD_END
			#undef HEADER_START
		} else {
			g_printerr ("Failed to read '%s': %s\n", filename, local_error ? local_error->message : "Unknown error");
			res = 4;
		}

		g_clear_error (&local_error);
	}

	g_free (filename);

	return res;
}

static gchar *
gen_options (const gchar *lang,
	     GPtrArray *langs)
{
	guint ii;
	GString *opts;

	opts = g_string_new ("<span style=\"float:right; margin-top:5px;\" lang=\"en\"><select id=\"helpLanguage\" onchange=\"helpLanguageChanged()\">");

	for (ii = 0; ii < langs->len; ii++) {
		const gchar *opt = g_ptr_array_index (langs, ii);
		g_string_append_printf (opts, "<option value=\"%s\"%s>%s</option>",
			opt,
			g_strcmp0 (opt, lang) == 0 ? " selected" : "",
			g_strcmp0 (opt, "C") == 0 ? "en_US" : opt);
	}

	g_string_append (opts, "</select></span>");

	return g_string_free (opts, FALSE);
}

static int
traverse_lang (const gchar *parent_dir,
	       const gchar *lang,
	       GPtrArray *langs)
{
	GDir *dir;
	gchar *path;
	gchar *options;
	GError *local_error = NULL;
	int res = 0;

	options = gen_options (lang, langs);
	/* "C", alias "en_US", is in the root help dir */
	path = g_build_filename (parent_dir, g_strcmp0 (lang, "C") == 0 ? NULL : lang, NULL);

	dir = g_dir_open (path, 0, &local_error);
	if (dir) {
		const gchar *name;

		while ((name = g_dir_read_name (dir)) != NULL && res == 0) {
			int len = strlen (name);

			if (len > 5 && g_ascii_strcasecmp (name + len - 5, ".html") == 0) {
				res = update_file (path, lang, name, options);
			}
		}

		g_dir_close (dir);
	} else {
		g_printerr ("Failed to read dir '%s': %s\n", path, local_error ? local_error->message : "Unknown error");
		g_clear_error (&local_error);
	}

	g_free (path);
	g_free (options);

	return res;
}

static gint
compare_strings_cb (gconstpointer aa,
		    gconstpointer bb)
{
	const gchar *str_a = *((const gchar **) aa);
	const gchar *str_b = *((const gchar **) bb);

	if (g_strcmp0 (str_a, "C") == 0)
		str_a = "en_US";

	if (g_strcmp0 (str_b, "C") == 0)
		str_b = "en_US";

	return g_strcmp0 (str_a, str_b);
}

int
main (int argc,
      const char *argv[])
{
	int res = 0;
	GDir *dir;
	GPtrArray *langs;
	const gchar *name;
	GError *local_error = NULL;

	if (argc != 2) {
		g_printerr ("Requires one argument, the root path with the help HTML files\n");
		return 1;
	}

	dir = g_dir_open (argv[1], 0, &local_error);
	if (!dir) {
		g_printerr ("Failed to open '%s': %s\n", argv[1], local_error ? local_error->message : "Unknown error");
		g_clear_error (&local_error);
		return 2;
	}

	langs = g_ptr_array_new_with_free_func (g_free);

	while ((name = g_dir_read_name (dir)) != NULL) {
		gchar *path = g_build_filename (argv[1], name, NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR) &&
		    g_strcmp0 (name, "C") != 0 &&
		    g_strcmp0 (name, "figures") != 0 &&
		    !g_str_has_prefix (name, "CMake"))
			g_ptr_array_add (langs, g_strdup (name));
		g_free (path);
	}

	if (langs->len == 0) {
		g_printerr ("Cannot find any directories in '%s'\n", argv[1]);
		res = 3;
	} else {
		guint ii;

		g_ptr_array_add (langs, g_strdup ("C"));

		g_ptr_array_sort (langs, compare_strings_cb);

		g_print ("Found %u help langs\n", langs->len);

		for (ii = 0; ii < langs->len && res == 0; ii++) {
			const gchar *lang = g_ptr_array_index (langs, ii);
			res = traverse_lang (argv[1], lang, langs);
			if (res == 0)
				g_print ("   Updated help files for lang '%s'\n", lang);
		}
	}

	g_ptr_array_free (langs, TRUE);
	g_dir_close (dir);

	return res;
}
