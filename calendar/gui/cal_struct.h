#define MAX_SZ 30

enum RepeatType {
	Single,
	Days,
	Months,
	WeekDays,
	MonthDays 
};


struct actionitem {
	char	date[MAX_SZ];
	int	time;  /* Minutes past midnight */
};

struct event {
	struct actionitem start;
	struct actionitem end;

	enum RepeatType repeat;
	int	repeatcount;
	char	description[MAX_SZ];
	char	subtype[MAX_SZ];
	GList	*properties;
};

