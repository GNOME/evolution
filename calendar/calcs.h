/* 
 * function prototypes
 */
int days_of_february(const int year);
int is_leap_year(const int year);
int valid_date(const int day, const int month, const int year);
void get_system_date(int *day, int *month, int *year);
void prev_date(int *day, int *month, int *year);
void next_date(int *day, int *month, int *year);
int month_atoi(const char *string);
int day_atoi(const char *string);
const char *day_suffix(int day);
const char *short3_day_name(int day);
const char *short_day_name(int day);
const char *day_name(int day);
const char *short_month_name(int month);
const char *month_name(int month);
unsigned long int date2num(const int day, const int month, const int year);
int weekday_of_date(const int day, const int month, const int year);


/*
 *  Important preprocessor symbols for the internal ranges.
 */
#define  DAY_LAST    365             /* Last day in a NON leap year */
#define  DAY_MIN     1               /* Minimum day of week/month/year */
#define  DAY_MAX     7               /* Maximum day/amount of days of week */
#define  WEEK_MAX    52              /* Maximum week number of year */
#define  MONTH_LAST  31              /* Highest day number in a month */
#define  MONTH_MIN   1               /* Minimum month of year */
#define  MONTH_MAX   12              /* Maximum month of year */
#define  YEAR_MIN    1               /* Minimum year able to compute */
#define  YEAR_MAX    9999            /* Maximum year able to compute */
#define  EASTER_MIN  30              /* Minimum year for computing Easter Sunday (29+1) */
#define  EASTER_MAX  YEAR_MAX        /* Maximum year for computing Easter Sunday */
#define  MONTH_COLS  6               /* Maximum number of columns of a month */
#define  VEC_BLOCK   42              /* Maximum number of elements per month (7*6) */
#define  VEC_ELEMS   504             /* Maximum number of elements per year (42*12) */
#define  CENTURY       1900          /* Operating system standard starting century, DON'T change ! */

/*
*  The Gregorian Reformation date record.
*/
typedef
	struct  greg_type
	{
		int  year;        /* Year of Gregorian Reformation */
		int  month;       /* Month of Gregorian Reformation */
		int  first_day;   /* First missing day of Reformation period */
		int  last_day;    /* Last missing day of Reformation period */
	}
	Greg_struct;

