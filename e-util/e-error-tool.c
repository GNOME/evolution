
#include <errno.h>

#include <stdio.h>

#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <glib.h>

static void
dump_cstring(FILE *out, const char *tmp)
{
	const char *p;
	int c;

	fprintf(out, "char *s = N_(\"");
	p = tmp;
	while ( (c = *p++) ) {
		switch (c) {
		case '\n':
			fprintf(out, "\\n\"\n\t\"");
			break;
		case '\r':
			fprintf(out, "\\r");
			break;
		case '\t':
			fprintf(out, "\\t");
			break;
		case '"':
		case '\\':
			fprintf(out, "\\%c", c);
			break;
		default:
			fputc(c, out);
		}
	}

	fprintf(out, "\");\n");
}

static void
extract_strings(FILE *out, const char *path)
{
	xmlDocPtr doc;
	xmlNodePtr root, error, scan;
	char *tmp, *domain;

	doc = xmlParseFile(path);
	if (doc == NULL) {
		g_warning("Error file '%s' not found", path);
		return;
	}

	root = xmlDocGetRootElement(doc);
	if (root == NULL
	    || strcmp(root->name, "error-list") != 0
	    || (domain = xmlGetProp(root, "domain")) == NULL) {
		g_warning("Error file '%s' invalid format", path);
		xmlFreeDoc(doc);
		return;
	}

	for (error = root->children;error;error = error->next) {
		char *id;

		if (strcmp(error->name, "error") != 0)
			continue;

		id = xmlGetProp(error, "id");
		if (id == NULL) {
			g_warning("Error format invalid, missing error id");
			_exit(1);
		}

		for (scan = error->children;scan;scan=scan->next) {
			if (!strcmp(scan->name, "primary")
			    || !strcmp(scan->name, "secondary")
			    || !strcmp(scan->name, "title")) {
				fprintf(out, "/* %s:%s %s */\n", domain, id, scan->name);
				tmp = xmlNodeGetContent(scan);
				if (tmp) {
					dump_cstring(out, tmp);
					xmlFree(tmp);
				}
			} else if (!strcmp(scan->name, "button")) {
				tmp = xmlGetProp(scan, "label");
				if (tmp) {
					dump_cstring(out, tmp);
					xmlFree(tmp);
				}
			}
		}
		xmlFree(id);
	}

	xmlFree(domain);

	xmlFreeDoc(doc);
}

int main(int argc, char **argv)
{
	int i;
	FILE *out;

	for (i=1;i<argc;i++) {
		char *name;

		name = g_strdup_printf("%s.h", argv[i]);
		out = fopen(name, "w");
		if (out == NULL) {
			fprintf(stderr, "Error creating %s: %s\n", name, strerror(errno));
			return 1;
		}
		extract_strings(out, argv[i]);
		if (fclose(out) != 0) {
			fprintf(stderr, "Error writing to %s: %s\n", name, strerror(errno));
			return 1;
		}
		g_free(name);
	}

	return 0;
}
