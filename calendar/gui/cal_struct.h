struct actionitem {
	char	date[MAX_SZ];
	char	time[MAX_SZ];
}

struct event {
	struct actionitem start;
	struct actionitem end;

	char	description[MAX_SZ];
	char	subtype[MAX_SZ];
	GList	*properties;
}

