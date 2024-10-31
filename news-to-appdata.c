/*
 * SPDX-FileCopyrightText: (C) 2021 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <locale.h>
#include <stdio.h>
#include <errno.h>
#include <glib.h>

static gint
extract_ver_number (const gchar **pstr)
{
	const gchar *str = *pstr;
	gint num = 0;

	if (*str == 0)
		return num;

	while (*str && *str != '.') {
		num = num * 10 + (*str) - '0';
		str++;
	}

	if (*str == '.')
		str++;

	*pstr = str;

	return num;
}

static gint
cmp_version_str (const gchar *ver1,
		 const gchar *ver2)
{
	gint num1 = 0, num2 = 0;

	while (*ver1 && *ver2 && num1 == num2) {
		num1 = extract_ver_number (&ver1);
		num2 = extract_ver_number (&ver2);
	}

	return num1 - num2;
}

typedef struct _ESection {
	gchar *header;
	GSList *items; /* gchar * */
} ESection;

static void
e_section_free (gpointer ptr)
{
	ESection *section = ptr;

	if (section) {
		g_free (section->header);
		g_slist_free_full (section->items, g_free);
		g_free (section);
	}
}

typedef struct _EVersion {
	gint order;
	gchar *project_name;
	gchar *version;
	gchar *date;
	GSList *sections; /* ESection * */
} EVersion;

static void
e_version_free (gpointer ptr)
{
	EVersion *version = ptr;

	if (version) {
		g_free (version->project_name);
		g_free (version->version);
		g_free (version->date);
		g_slist_free_full (version->sections, e_section_free);
		g_free (version);
	}
}

static gboolean
extract_versions (GSList **pversions,
		  gint order,
		  const gchar *read_version,
		  const gchar *filename)
{
	gchar *content = NULL, **lines;
	gboolean res = TRUE;
	gint ii, n_read = 0;
	GError *error = NULL;

	if (!g_file_get_contents (filename, &content, NULL, &error)) {
		g_printerr ("news-to-appdata: Failed to read '%s': %s\n", filename, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return FALSE;
	}

	lines = g_strsplit (content, "\n", -1);

	for (ii = 0; lines[ii] && res; ii++) {
		/* Version separator, the previous line contains the version information header */
		if (ii > 0 && g_str_has_prefix (lines[ii], "-----")) {
			gchar **info;

			info = g_strsplit (lines[ii - 1], " ", -1);

			if (g_strv_length (info) == 3) {
				EVersion *version;
				ESection *current_section = NULL;
				GString *paragraph = NULL;

				if (cmp_version_str (read_version, info[1]) > 0) {
					g_strfreev (info);
					break;
				}

				version = g_new0 (EVersion, 1);
				version->order = order;
				version->project_name = info[0];
				version->version = info[1];
				version->date = info[2];

				*pversions = g_slist_prepend (*pversions, version);
				n_read++;

				g_clear_pointer (&info, g_free);

				for (ii++; lines[ii] && res; ii++) {
					gchar *line = lines[ii];

					/* Empty line ends the section */
					if (!*line) {
						current_section = NULL;

						if (paragraph) {
							ESection *section;

							section = g_new0 (ESection, 1);
							section->header = g_string_free (paragraph, FALSE);

							version->sections = g_slist_prepend (version->sections, section);

							paragraph = NULL;
						}
					/* Starts a free paragraph section */
					} else if (*line == '*' || (line[0] == ' ' && line[1] == '*')) {
						if (paragraph) {
							g_printerr ("news-to-appdata: Unexpected start of a free paragraph section when reading one at line %d of '%s'\n", ii, filename);
							res = FALSE;
						} else {
							paragraph = g_string_new (g_strstrip (line + (*line == '*' ? 1 : 2)));
						}
					/* Continues the free paragraph section */
					} else if (*line == ' ') {
						if (paragraph) {
							g_string_append_c (paragraph, ' ');
							g_string_append (paragraph, g_strstrip (line));
						} else {
							g_printerr ("news-to-appdata: Unexpected free paragraph section continuation at line %d of '%s'\n", ii, filename);
							res = FALSE;
						}
					/* Section item */
					} else if (*line == '\t') {
						if (current_section) {
							current_section->items = g_slist_prepend (current_section->items, g_strdup (line + 1));
						} else {
							g_printerr ("news-to-appdata: Unexpected section item at line %d of '%s'\n", ii, filename);
							res = FALSE;
						}
					/* Maybe the next version information header, stop reading here */
					} else if (lines[ii + 1] && g_str_has_prefix (lines[ii + 1], "-----")) {
						break;
					/* Anything else is a new section header */
					} else {
						current_section = g_new0 (ESection, 1);
						current_section->header = g_strdup (line);
						version->sections = g_slist_prepend (version->sections, current_section);
					}
				}

				if (paragraph)
					g_string_free (paragraph, TRUE);

				ii--;

				if (res) {
					GSList *slink;

					version->sections = g_slist_reverse (version->sections);

					for (slink = version->sections; slink; slink = g_slist_next (slink)) {
						ESection *section = slink->data;

						section->items = g_slist_reverse (section->items);
					}
				}
			} else {
				g_printerr ("news-to-appdata: Version info line should contain 3 parts, but it has %d; at line %d of '%s'\n",
					g_strv_length (info), ii - 1, filename);
				res = FALSE;
			}

			g_strfreev (info);
		}
	}

	g_strfreev (lines);
	g_free (content);

	if (res && !n_read) {
		g_printerr ("news-to-appdata: No version information for '%s' found in '%s'\n", read_version, filename);
#ifndef BUILD_RUN
		res = FALSE;
#endif
	}

	return res;
}

static gint
sort_versions_cb (gconstpointer ptr1,
		  gconstpointer ptr2)
{
	EVersion *ver1 = (EVersion *) ptr1, *ver2 = (EVersion *) ptr2;
	gint res;

	res = cmp_version_str (ver1->version, ver2->version);

	if (!res) {
		res = ver1->order - ver2->order;
	} else {
		/* Sort in reverse order, highest version first */
		res *= -1;
	}

	return res;
}

gint
main (gint argc,
      const gchar *argv[])
{
	FILE *output;
	GSList *vlink, *versions = NULL; /* EVersion */
	const gchar *release_type, *last_version = NULL, *output_filename;
	gboolean multiple_projects;
	gint ii;

#ifdef BUILD_RUN
	const gchar *margv[] = {
		"news-to-appdata",
		BUILD_OUTPUT,
		BUILD_TYPE,
		BUILD_VERSION,
		BUILD_NEWS_FILE
	};

	argc = G_N_ELEMENTS (margv);
	argv = margv;
#endif

	setlocale (LC_ALL, "");

	if (argc == 1 || (argc == 2 && g_strcmp0 (argv[1], "--help") == 0)) {
		g_print ("news-to-appdata: Converts NEWS entries into appdata <release/> content\n");
		g_print ("Usage: news-to-appdata <output> <type> <version> <NEWS> [<version> <NEWS> ...]\n");
		g_print ("Arguments:\n");
		g_print ("   <output>  - path to output file, empty string for stdout\n");
		g_print ("   <type>    - build type, like \"stable\" or \"development\"\n");
		g_print ("   <version> - version down to filter the NEWS file for, like 3.40\n");
		g_print ("   <NEWS>    - path to the NEWS file to extract the information from\n");

		return argc == 1 ? -1 : 0;
	}

	if (argc < 5) {
		g_printerr ("news-to-appdata: Expects at least four arguments\n");
		return -3;
	}

	if ((argc - 3) % 2 != 0) {
		g_printerr ("news-to-appdata: Expects pairs of the arguments (<version> <NEWS>)\n");
		return -4;
	}

	output_filename = argv[1];
	release_type = argv[2];
	multiple_projects = argc - 3 > 2;

	for (ii = 3; ii < argc; ii += 2) {
		const gchar *version, *filename;

		version = argv[ii];
		filename = argv[ii + 1];

		if (!extract_versions (&versions, ii, version, filename)) {
			g_slist_free_full (versions, e_version_free);
			return -5;
		}
	}

	versions = g_slist_sort (versions, sort_versions_cb);

	if (*output_filename) {
		output = fopen (output_filename, "w+b");

		if (!output) {
			g_printerr ("news-to-appdata: Failed to open '%s' for writing: %s\n", output_filename, g_strerror (errno));
			g_slist_free_full (versions, e_version_free);
			return -6;
		}
	} else
		output = stdout;

	for (vlink = versions; vlink; vlink = g_slist_next (vlink)) {
		EVersion *version = vlink->data;
		GSList *slink;
		gchar *tmp;

		if (g_strcmp0 (version->version, last_version) != 0) {
			if (last_version) {
				fprintf (output, "      </description>\n");
				fprintf (output, "    </release>\n");
			}

			/* Uses the release data of the first noticed version */
			tmp = g_markup_printf_escaped ("    <release version=\"%s\" date=\"%s\" type=\"%s\">\n", version->version, version->date, release_type);
			fprintf (output, "%s", tmp);
			fprintf (output, "      <description>\n");

			last_version = version->version;
			g_free (tmp);
		}

		for (slink = version->sections; slink; slink = g_slist_next (slink)) {
			ESection *section = slink->data;

			if (multiple_projects && section->items)
				tmp = g_markup_printf_escaped ("        <p>%s %s</p>\n", version->project_name, section->header);
			else
				tmp = g_markup_printf_escaped ("        <p>%s</p>\n", section->header);

			fprintf (output, "%s", tmp);
			g_free (tmp);

			if (section->items) {
				GSList *ilink;

				fprintf (output, "        <ul>\n");

				for (ilink = section->items; ilink; ilink = g_slist_next (ilink)) {
					const gchar *item = ilink->data;

					tmp = g_markup_printf_escaped ("          <li>%s</li>\n", item);
					fprintf (output, "%s", tmp);
					g_free (tmp);
				}

				fprintf (output, "        </ul>\n");
			}
		}
	}

	if (last_version) {
		fprintf (output, "      </description>\n");
		fprintf (output, "    </release>\n");
	}

	if (output != stdout)
		fclose (output);

	g_slist_free_full (versions, e_version_free);

	return 0;
}
