#include <camel.h>

int main (int argc, char **argv)
{
	CamelURL *url;
	CamelException *ex;

	if (argc != 2) {
		fprintf (stderr, "Usage: test-url URL\n");
		exit (1);
	}

	ex = camel_exception_new ();
	url = camel_url_new (argv[1], ex);
	if (!url) {
		fprintf (stderr, "Could not parse URL:\n%s",
			 camel_exception_get_description (ex));
		exit (1);
	}

	printf ("URL     : %s\n\n", camel_url_to_string (url, TRUE));
	printf ("Protocol: %s\n", url->protocol);
	if (url->user)
		printf ("User    : %s\n", url->user);
	if (url->authmech)
		printf ("Authmech: %s\n", url->authmech);
	if (url->passwd)
		printf ("Password: %s\n", url->passwd);
	if (url->host)
		printf ("Host    : %s\n", url->host);
	if (url->port)
		printf ("Port    : %d\n", url->port);
	if (url->path)
		printf ("Path    : %s\n", url->path);

	return 0;
}
