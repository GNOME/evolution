/*
 * Alarm handling for the GNOME Calendar.
 *
 * (C) 1998 the Free Software Foundation
 *
 * Author: Miguel de Icaza (miguel@kernel.org)
 */
#include <config.h>
#include <time.h>
#include <gnome.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include "alarm.h"

/* The pipes used to notify about an alarm */
int alarm_pipes [2];

/* The list of pending alarms */
static GList *alarms;

static void *head_alarm;

typedef struct {
	time_t        activation_time;
	AlarmFunction fn;
	void          *closure;
} AlarmRecord;

/*
 * SIGALRM handler.  Notifies the callback about the alarm
 */
static void
alarm_activate ()
{
	char c = 0;

	write (alarm_pipes [1], &c, 1);
}

static void
alarm_ready (void *closure, int fd, GdkInputCondition cond)
{
	AlarmRecord *ar = head_alarm;
	char c;

	if (read (alarm_pipes [0], &c, 1) != 1)
		return;

	if (ar == NULL){
		g_warning ("Empty events.  This should not happen\n");
		return;
	}
	(*ar->fn)(ar->activation_time, ar->closure);
	alarms = g_list_remove (alarms, head_alarm);
	if (alarms)
		head_alarm = alarms->data;
	else
		head_alarm = NULL;
	g_free (ar);
}

static int
alarm_compare_by_time (gpointer a, gpointer b)
{
	AlarmRecord *ara = a;
	AlarmRecord *arb = b;
	time_t diff;
	
	diff = ara->activation_time - arb->activation_time;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

void
alarm_add (time_t alarm_time, AlarmFunction fn, void *closure)
{
	time_t now = time (NULL);
	AlarmRecord *ar;

	/* If it already expired, do not add it */
	if (alarm_time < now)
		return;
	
	ar = g_new0 (AlarmRecord, 1);
	ar->activation_time = alarm_time;
	ar->fn = fn;
	ar->closure = closure;

	alarms = g_list_insert_sorted (alarms, ar, alarm_compare_by_time);

	/* If first alarm is not the previous first alarm, reschedule SIGALRM */
	if (head_alarm != alarms->data){
		struct itimerval itimer;
		int v;
		
		/* Set the timer to disable upon activation */
		itimer.it_interval.tv_sec = 0;
		itimer.it_interval.tv_usec = 0;
		itimer.it_value.tv_sec = alarm_time - now;
		itimer.it_value.tv_usec = 0;
		v = setitimer (ITIMER_REAL, &itimer, NULL);
		head_alarm = alarms->data;
	}
}

void
alarm_kill (void *closure_key)
{
	GList *p;

	for (p = alarms; p; p = p->next){
		AlarmRecord *ar = p->data;
		
		if (ar->closure == closure_key){
			alarms = g_list_remove (alarms, p->data);
			if (alarms)
				head_alarm = alarms->data;
			else
				head_alarm = NULL;
			return;
		}
	}
}

void
alarm_init (void)
{
	struct sigaction sa;
	int flags;
	
	pipe (alarm_pipes);
	
	/* set non blocking mode */
	fcntl (alarm_pipes [0], F_GETFL, &flags);
	fcntl (alarm_pipes [0], F_SETFL, flags | O_NONBLOCK);
	gdk_input_add (alarm_pipes [0], GDK_INPUT_READ, alarm_ready, 0);

	/* Setup the signal handler */
	sa.sa_handler = alarm_activate;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags   = SA_RESTART;
	sigaction (SIGALRM, &sa, NULL);
}

