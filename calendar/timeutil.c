#include <glib.h>
#include <time.h>

#define digit_at(x,y) (x [y] - '0')
	
time_t
time_from_isodate (char *str)
{
	struct tm my_tm;

	my_tm.tm_year = digit_at (str, 0) * 1000 + digit_at (str, 1) * 100 +
		digit_at (str, 2) * 10 + digit_at (str, 3);

	my_tm.tm_mon  = digit_at (str, 4) * 10 + digit_at (str, 5);
	my_tm.tm_mday = digit_at (str, 6) * 10 + digit_at (str, 7);
	my_tm.tm_hour = digit_at (str, 9) * 10 + digit_at (str, 10);
	my_tm.tm_min  = digit_at (str, 11) * 10 + digit_at (str, 12);
	my_tm.tm_sec  = digit_at (str, 13) * 10 + digit_at (str, 14);
	my_tm.tm_isdst = -1;
	
	return mktime (&my_tm);
}

time_t
time_from_start_duration (time_t start, char *duration)
{
	printf ("Not yet implemented\n");
	return 0;
}
