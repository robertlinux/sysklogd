/*
    klogd.c - main program for Linux kernel log daemon.
    Copyright (c) 1995  Dr. G.W. Wettstein <greg@wind.rmcc.com>

    This file is part of the sysklogd package, a kernel and system log daemon.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * Steve Lord (lord@cray.com) 7th Nov 92
 *
 * Modified to check for kernel info by Dr. G.W. Wettstein 02/17/93.
 *
 * Fri Mar 12 16:53:56 CST 1993:  Dr. Wettstein
 * 	Modified LogLine to use a newline as the line separator in
 *	the kernel message buffer.
 *
 *	Added debugging code to dump the contents of the kernel message
 *	buffer at the start of the LogLine function.
 *
 * Thu Jul 29 11:40:32 CDT 1993:  Dr. Wettstein
 *	Added syscalls to turn off logging of kernel messages to the
 *	console when klogd becomes responsible for kernel messages.
 *
 *	klogd now catches SIGTERM and SIGKILL signals.  Receipt of these
 *	signals cases the clean_up function to be called which shuts down
 *	kernel logging and re-enables logging of messages to the console.
 *
 * Sat Dec 11 11:54:22 CST 1993:  Dr. Wettstein
 *	Added fixes to allow compilation with no complaints with -Wall.
 *
 *      When the daemon catches a fatal signal (SIGTERM, SIGKILL) a 
 *	message is output to the logfile advising that the daemon is
 *	going to terminate.
 *
 * Thu Jan  6 11:54:10 CST 1994:  Dr. Wettstein
 *	Major re-write/re-organization of the code.
 *
 *	Klogd now assigns kernel messages to priority levels when output
 *	to the syslog facility is requested.  The priority level is
 *	determined by decoding the prioritization sequence which is
 *	tagged onto the start of the kernel messages.
 *
 *	Added the following program options: -f arg -c arg -s -o -d
 *
 *		The -f switch can be used to specify that output should
 *		be written to the named file.
 *
 *		The -c switch is used to specify the level of kernel
 *		messages which are to be directed to the console.
 *
 *		The -s switch causes the program to use the syscall
 *		interface to the kernel message facility.  This can be
 *		used to override the presence of the /proc filesystem.
 *
 *		The -o switch causes the program to operate in 'one-shot'
 *		mode.  A single call will be made to read the complete
 *		kernel buffer.  The contents of the buffer will be
 *		output and the program will terminate.
 *
 *		The -d switch causes 'debug' mode to be activated.  This
 *		will cause the daemon to generate LOTS of output to stderr.
 *
 *	The buffer decomposition function (LogLine) was re-written to
 *	squash a bug which was causing only partial kernel messages to
 *	be written to the syslog facility.
 *
 *	The signal handling code was modified to properly differentiate
 *	between the STOP and TSTP signals.
 *
 *	Added pid saving when the daemon detaches into the background.  Thank
 *	you to Juha Virtanen (jiivee@hut.fi) for providing this patch.
 *
 * Mon Feb  6 07:31:29 CST 1995:  Dr. Wettstein
 *	Significant re-organization of the signal handling code.  The
 *	signal handlers now only set variables.  Not earth shaking by any
 *	means but aesthetically pleasing to the code purists in the group.
 *
 *	Patch to make things more compliant with the file system standards.
 *	Thanks to Chris Metcalf for prompting this helpful change.
 *
 *	The routines responsible for reading the kernel log sources now
 *	initialize the buffers before reading.  I think that this will
 *	solve problems with non-terminated kernel messages producing
 *	output of the form:  new old old old
 *
 *	This may also help influence the occassional reports of klogd
 *	failing under significant load.  I think that the jury may still
 *	be out on this one though.  My thanks to Joerg Ahrens for initially
 *	tipping me off to the source of this problem.  Also thanks to
 *	Michael O'Reilly for tipping me off to the best fix for this problem.
 *	And last but not least Mark Lord for prompting me to try this as
 *	a means of attacking the stability problem.
 *
 *	Specifying a - as the arguement to the -f switch will cause output
 *	to be directed to stdout rather than a filename of -.  Thanks to
 *	Randy Appleton for a patch which prompted me to do this.
 *
 * Wed Feb 22 15:37:37 CST 1995:  Dr. Wettstein
 *	Added version information to logging startup messages.
 *
 * Wed Jul 26 18:57:23 MET DST 1995: Martin Schulze
 *	Added an commandline argument "-n" to avoid forking. This obsoletes
 *	the compiler define NO_FORK. It's more useful to have this as an
 *	argument as there are many binary versions and one doesn't need to
 *	recompile the daemon.
 *
 * Thu Aug 10 19:01:08 MET DST 1995: Martin Schulze
 *	Added my pidfile.[ch] to it to perform a better handling with pidfiles.
 *	Now both, syslogd and klogd, can only be started once. They check the
 *	pidfile.
 *
 * Fri Nov 17 15:05:43 CST 1995:  Dr. Wettstein
 *	Added support for kernel address translation.  This required moving
 *	some definitions and includes to the new klogd.h file.  Some small
 *	code cleanups and modifications.
 *
 * Mon Nov 20 10:03:39 MET 1995
 *	Added -v option to print the version and exit.
 *
 * Thu Jan 18 11:19:46 CST 1996:  Dr. Wettstein
 *	Added suggested patches from beta-testers.  These address two
 *	two problems.  The first is segmentation faults which occur with
 *	the ELF libraries.  This was caused by passing a null pointer to
 *	the strcmp function.
 *
 *	Added a second patch to remove the pidfile as part of the
 *	termination cleanup sequence.  This minimizes the potential for
 *	conflicting pidfiles causing immediate termination at boot time.
 *	
 * Wed Aug 21 09:13:03 CDT 1996:  Dr. Wettstein
 *	Added ability to reload static symbols and kernel module symbols
 *      under control of SIGUSR1 and SIGUSR2 signals.
 *
 *	Added -p switch to select 'paranoid' behavior with respect to the
 *	loading of kernel module symbols.
 *
 *	Informative line now printed whenever a state change occurs due
 *	to signal reception by the daemon.
 *
 *	Added the -i and -I command line switches to signal the currently
 *	executing daemon.
 *
 * Tue Nov 19 10:15:36 PST 1996: Lee Olds
 *	Corrected vulnerability to buffer overruns by rewriting LogLine
 *	routine.  Obscenely long kernel messages will now be broken up
 *	into lines no longer than LOG_LINE_LENGTH.
 *
 *	The last version of LogLine was vulnerable to buffer overruns:
 *	- Kernel messages longer than LOG_LINE_LENGTH caused a buffer
 *	  overrun.
 *	- If a line was determined to be shorter than LOG_LINE_LENGTH,
 *	  the routine "ExpandKadds" could cause the line grow by
 *	  an unknown amount and overrun a buffer.
 *	I turned these routines into a little parsing state machine that
 *	should not have these problems.
 */


/* Includes. */
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <linux/time.h>
#include <stdarg.h>
#include <paths.h>
#include <stdlib.h>
#include "klogd.h"
#include "ksyms.h"
#include "pidfile.h"
#include "version.h"

#define __LIBRARY__
#include <linux/unistd.h>
#ifndef __alpha__
# define __NR_ksyslog __NR_syslog
_syscall3(int,ksyslog,int, type, char *, buf, int, len);
#endif

#define LOG_BUFFER_SIZE 4096
#define LOG_LINE_LENGTH 1024

#if defined(FSSTND)
static char	*PidFile = _PATH_VARRUN "klogd.pid";
#else
static char	*PidFile = "/etc/klogd.pid";
#endif

static int	kmsg,
		change_state = 0,
		terminate = 0,
		caught_TSTP = 0,
		reload_symbols = 0,
		console_log_level = 6;

static int	use_syscall = 0,
		one_shot = 0,
		NoFork = 0;	/* don't fork - don't run in daemon mode */

static char	*symfile = (char *) 0,
		log_buffer[LOG_BUFFER_SIZE];

static FILE *output_file = (FILE *) 0;

static enum LOGSRC {none, proc, kernel} logsrc;

int debugging = 0;


/* Function prototypes. */
extern int ksyslog(int type, char *buf, int len);
static void CloseLogSrc(void);
extern void restart(int sig);
extern void stop_logging(int sig);
extern void stop_daemon(int sig);
extern void reload_daemon(int sig);
static void Terminate(void);
static void SignalDaemon(int);
static void ReloadSymbols(void);
static void ChangeLogging(void);
static enum LOGSRC GetKernelLogSrc(void);
static void LogLine(char *ptr, int len);
static void LogKernelLine(void);
static void LogProcLine(void);
extern int main(int argc, char *argv[]);


static void CloseLogSrc()

{
	/* Turn on logging of messages to console. */
  	ksyslog(7, NULL, 0);
  
        /* Shutdown the log sources. */
	switch ( logsrc )
	{
	    case kernel:
		ksyslog(0, 0, 0);
		Syslog(LOG_INFO, "Kernel logging (ksyslog) stopped.");
		break;
            case proc:
		close(kmsg);
		Syslog(LOG_INFO, "Kernel logging (proc) stopped.");
		break;
	    case none:
		break;
	}

	if ( output_file != (FILE *) 0 )
		fflush(output_file);
	return;
}


void restart(sig)
	
	int sig;

{
	signal(SIGCONT, restart);
	change_state = 1;
	caught_TSTP = 0;
	return;
}


void stop_logging(sig)

	int sig;
	
{
	signal(SIGTSTP, stop_logging);
	change_state = 1;
	caught_TSTP = 1;
	return;
}


void stop_daemon(sig)

	int sig;

{
	change_state = 1;
	terminate = 1;
	return;
}


void reload_daemon(sig)

     int sig;

{
	change_state = 1;
	reload_symbols = 1;


	if ( sig == SIGUSR2 )
	{
		++reload_symbols;
		signal(SIGUSR2, reload_daemon);
	}
	else
		signal(SIGUSR1, reload_daemon);
		
	return;
}


static void Terminate()

{
	CloseLogSrc();
	Syslog(LOG_INFO, "Kernel log daemon terminating.");
	sleep(1);
	if ( output_file != (FILE *) 0 )
		fclose(output_file);
	closelog();
	(void) remove_pid(PidFile);
	exit(1);
}

static void SignalDaemon(sig)

     int sig;

{
	auto int pid = check_pid(PidFile);

	kill(pid, sig);
	return;
}


static void ReloadSymbols()

{
	if ( reload_symbols > 1 )
		InitKsyms(symfile);
	InitMsyms();
	reload_symbols = change_state = 0;
	return;
}


static void ChangeLogging(void)

{
	/* Terminate kernel logging. */
	if ( terminate == 1 )
		Terminate();

	/* Indicate that something is happening. */
	Syslog(LOG_INFO, "klogd %s-%s, ---------- state change ----------\n", \
	       VERSION, PATCHLEVEL);

	/* Reload symbols. */
	if ( reload_symbols > 0 )
	{
		ReloadSymbols();
		return;
	}

	/* Stop kernel logging. */
	if ( caught_TSTP == 1 )
	{
		CloseLogSrc();
		logsrc = none;
		change_state = 0;
		return;
	}
		
	/*
	 * The rest of this function is responsible for restarting
	 * kernel logging after it was stopped.
	 *
	 * In the following section we make a decision based on the
	 * kernel log state as to what is causing us to restart.  Somewhat
	 * groady but it keeps us from creating another static variable.
	 */
	if ( logsrc != none )
	{
		Syslog(LOG_INFO, "Kernel logging re-started after SIGSTOP.");
		change_state = 0;
		return;
	}

	/* Restart logging. */
	logsrc = GetKernelLogSrc();
	change_state = 0;
	return;
}


static enum LOGSRC GetKernelLogSrc(void)

{
	auto struct stat sb;


	/* Set level of kernel console messaging.. */
	if ( (ksyslog(8, NULL, console_log_level) < 0) && \
	     (errno == EINVAL) )
	{
		/*
		 * An invalid arguement error probably indicates that
		 * a pre-0.14 kernel is being run.  At this point we
		 * issue an error message and simply shut-off console
		 * logging completely.
		 */
		Syslog(LOG_WARNING, "Cannot set console log level - disabling "
			      "console output.");
		ksyslog(6, NULL, 0);
	}
	

	/*
	 * First do a stat to determine whether or not the proc based
	 * file system is available to get kernel messages from.
	 */
	if ( use_syscall ||
	    ((stat(_PATH_KLOG, &sb) < 0) && (errno == ENOENT)) )
	{
	  	/* Initialize kernel logging. */
	  	ksyslog(1, NULL, 0);
		Syslog(LOG_INFO, "klogd %s-%s, log source = ksyslog "
		       "started.", VERSION, PATCHLEVEL);
		return(kernel);
	}
	
	if ( (kmsg = open(_PATH_KLOG, O_RDONLY)) < 0 )
	{
		fprintf(stderr, "klogd: Cannot open proc file system, " \
			"%d - %s.\n", errno, strerror(errno));
		ksyslog(7, NULL, 0);
		exit(1);
	}

	Syslog(LOG_INFO, "klogd %s-%s, log source = %s started.", \
	       VERSION, PATCHLEVEL, _PATH_KLOG);
	return(proc);
}


extern void Syslog(int priority, char *fmt, ...)

{
	va_list ap;

	if ( debugging )
	{
		fputs("Logging line:\n", stderr);
		fprintf(stderr, "\tLine: %s\n", fmt);
		fprintf(stderr, "\tPriority: %d\n", priority);
	}

	/* Handle output to a file. */
	if ( output_file != (FILE *) 0 )
	{
		va_start(ap, fmt);
		vfprintf(output_file, fmt, ap);
		va_end(ap);
		fputc('\n', output_file);
		fflush(output_file);
		fsync(fileno(output_file));
		return;
	}
	
	/* Output using syslog. */
	if ( *fmt == '<' )
	{
		switch ( *(fmt+1) )
		{
		    case '0':
			priority = LOG_EMERG;
			break;
		    case '1':
			priority = LOG_ALERT;
			break;
		    case '2':
			priority = LOG_CRIT;
			break;
		    case '3':
			priority = LOG_ERR;
			break;
		    case '4':
			priority = LOG_WARNING;
			break;
		    case '5':
			priority = LOG_NOTICE;
			break;
		    case '6':
			priority = LOG_INFO;
			break;
		    case '7':
		    default:
			priority = LOG_DEBUG;
		}
		fmt += 3;
	}
	
	va_start(ap, fmt);
	vsyslog(priority, fmt, ap);
	va_end(ap);

	return;
}


/*
 *     Copy characters from ptr to line until a char in the delim
 *     string is encountered or until min( space, len ) chars have
 *     been copied.
 *
 *     Returns the actual number of chars copied.
 */
static int copyin( char *line,      int space,
                   const char *ptr, int len,
                   const char *delim )
{
    auto int i;
    auto int count;

    count = len < space ? len : space;

    for(i=0; i<count && !strchr(delim, *ptr); i++ ) { *line++ = *ptr++; }

    return( i );
}

/*
 * Messages are separated by "\n".  Messages longer than
 * LOG_LINE_LENGTH are broken up.
 *
 * Kernel symbols show up in the input buffer as : "[<aaaaaa>]",
 * where "aaaaaa" is the address.  These are replaced with
 * "[symbolname+offset/size]" in the output line - symbolname,
 * offset, and size come from the kernel symbol table.
 *
 * If a kernel symbol happens to fall at the end of a message close
 * in length to LOG_LINE_LENGTH, the symbol will not be expanded.
 * (This should never happen, since the kernel should never generate
 * messages that long.
 */
static void LogLine(char *ptr, int len)
{
    enum parse_state_enum {
        PARSING_TEXT,
        PARSING_SYMSTART,      /* at < */
        PARSING_SYMBOL,        
        PARSING_SYMEND         /* at ] */
    };

    static char line_buff[LOG_LINE_LENGTH];

    static char *line                        =line_buff;
    static enum parse_state_enum parse_state = PARSING_TEXT;
    static int space                         = sizeof(line_buff)-1;

    static char *sym_start;            /* points at the '<' of a symbol */

    auto   int delta = 0;              /* number of chars copied        */

    while( len >= 0 )
    {
        if( space == 0 )    /* line buffer is full */
        {
            /*
            ** Line too long.  Start a new line.
            */
            *line = 0;   /* force null terminator */

	    if ( debugging )
	    {
		fputs("Line buffer full:\n", stderr);
		fprintf(stderr, "\tLine: %s\n", line);
	    }

            Syslog( LOG_INFO, line_buff );
            line  = line_buff;
            space = sizeof(line_buff)-1;
            parse_state = PARSING_TEXT;
        }

        switch( parse_state )
        {
        case PARSING_TEXT:
               delta = copyin( line, space, ptr, len, "\n[" );
               line  += delta;
               ptr   += delta;
               space -= delta;
               len   -= delta;
               if( space == 0 || len == 0 )
               {
		  break;  /* full line_buff or end of input buffer */
               }
               if( *ptr == '\n' )  /* newline */
               {
                  *line++ = *ptr++;  /* copy it in */
                  space -= 1;
                  len   -= 1;

                  *line = 0;  /* force null terminator */
	          Syslog( LOG_INFO, line_buff );
                  line  = line_buff;
                  space = sizeof(line_buff)-1;
                  break;
               }
               if( *ptr == '[' )   /* possible kernel symbol */
               {
                  *line++ = *ptr++;
                  space -= 1;
                  len   -= 1;
                  parse_state = PARSING_SYMSTART;      /* at < */
                  break;
               }
               break;
        
        case PARSING_SYMSTART:
               if( *ptr != '<' )
               {
                  parse_state = PARSING_TEXT;        /* not a symbol */
                  break;
               }

               /*
               ** Save this character for now.  If this turns out to
               ** be a valid symbol, this char will be replaced later.
               ** If not, we'll just leave it there.
               */

               sym_start = line; /* this will point at the '<' */

               *line++ = *ptr++;
               space -= 1;
               len   -= 1;
               parse_state = PARSING_SYMBOL;     /* symbol... */
               break;

        case PARSING_SYMBOL:
               delta = copyin( line, space, ptr, len, ">\n[" );
               line  += delta;
               ptr   += delta;
               space -= delta;
               len   -= delta;
               if( space == 0 || len == 0 )
               {
                  break;  /* full line_buff or end of input buffer */
               }
               if( *ptr != '>' )
               {
                  parse_state = PARSING_TEXT;
                  break;
               }

               *line++ = *ptr++;  /* copy the '>' */
               space -= 1;
               len   -= 1;

               parse_state = PARSING_SYMEND;

               break;

        case PARSING_SYMEND:
               if( *ptr != ']' )
               {
                  parse_state = PARSING_TEXT;        /* not a symbol */
                  break;
               }

               /*
               ** It's really a symbol!  Replace address with the
               ** symbol text.
               */
           {
	       auto int sym_space;

	       auto int value;
	       auto struct symbol sym;
	       auto char *symbol;

               *(line-1) = 0;    /* null terminate the address string */
               value  = strtol(sym_start+1, (char **) 0, 16);
               *(line-1) = '>';  /* put back delim */

               symbol = LookupSymbol(value, &sym);
               if ( symbol == (char *) 0 )
               {
                  parse_state = PARSING_TEXT;
                  break;
               }

               /*
               ** verify there is room in the line buffer
               */
               sym_space = space + ( line - sym_start );
               if( sym_space < strlen(symbol) + 30 ) /*(30 should be overkill)*/
               {
                  parse_state = PARSING_TEXT;  /* not enough space */
                  break;
               }

               delta = sprintf( sym_start, "%s+%d/%d]",
                                symbol, sym.offset, sym.size );

               space = sym_space + delta;
               line  = sym_start + delta;
           }
               ptr++;
               len--;
               parse_state = PARSING_TEXT;
               break;

        default: /* Can't get here! */
               parse_state = PARSING_TEXT;

        }
    }

    return;
}


static void LogKernelLine(void)

{
	auto int rdcnt;

	/*
	 * Zero-fill the log buffer.  This should cure a multitude of
	 * problems with klogd logging the tail end of the message buffer
	 * which will contain old messages.  Then read the kernel log
	 * messages into this fresh buffer.
	 */
	memset(log_buffer, '\0', sizeof(log_buffer));
	if ( (rdcnt = ksyslog(2, log_buffer, sizeof(log_buffer))) < 0 )
	{
		if ( errno == EINTR )
			return;
		fprintf(stderr, "klogd: Error return from sys_sycall: " \
			"%d - %s\n", errno, strerror(errno));
	}
	
	LogLine(log_buffer, rdcnt);
	return;
}


static void LogProcLine(void)

{
	auto int rdcnt;

	/*
	 * Zero-fill the log buffer.  This should cure a multitude of
	 * problems with klogd logging the tail end of the message buffer
	 * which will contain old messages.  Then read the kernel messages
	 * from the message pseudo-file into this fresh buffer.
	 */
	memset(log_buffer, '\0', sizeof(log_buffer));
	if ( (rdcnt = read(kmsg, log_buffer, sizeof(log_buffer))) < 0 )
	{
		if ( errno == EINTR )
			return;
		Syslog(LOG_ERR, "Cannot read proc file system: %d - %s.", \
		       errno, strerror(errno));
	}
	
	LogLine(log_buffer, rdcnt);

	return;
}


int main(argc, argv)

	int argc;

	char *argv[];

{
	auto int	ch,
			use_output = 0;

	auto char	*log_level = (char *) 0,
			*output = (char *) 0;

	/* Parse the command-line. */
	while ((ch = getopt(argc, argv, "c:df:iIk:nopsv")) != EOF)
		switch((char)ch)
		{
		    case 'c':		/* Set console message level. */
			log_level = optarg;
			break;
		    case 'd':		/* Activity debug mode. */
			debugging = 1;
			break;
		    case 'f':		/* Define an output file. */
			output = optarg;
			use_output++;
			break;
		    case 'i':		/* Reload module symbols. */
			SignalDaemon(SIGUSR1);
			return(0);
		    case 'I':
			SignalDaemon(SIGUSR2);
			return(0);
		    case 'k':		/* Kernel symbol file. */
			symfile = optarg;
			break;
		    case 'n':		/* don't fork */
			NoFork++;
			break;
		    case 'o':		/* One-shot mode. */
			one_shot = 1;
			break;
		    case 'p':
			SetParanoiaLevel(1);	/* Load symbols on oops. */
			break;	
		    case 's':		/* Use syscall interface. */
			use_syscall = 1;
			break;
		    case 'v':
			printf("klogd %s-%s\n", VERSION, PATCHLEVEL);
			exit (1);
		}


	/* Set console logging level. */
	if ( log_level != (char *) 0 )
	{
		if ( (strlen(log_level) > 1) || \
		     (strchr("1234567", *log_level) == (char *) 0) )
		{
			fprintf(stderr, "klogd: Invalid console logging "
				"level <%s> specified.\n", log_level);
			return(1);
		}
		console_log_level = *log_level - '0';
	}		


	/*
	 * The following code allows klogd to auto-background itself.
	 * What happens is that the program forks and the parent quits.
	 * The child closes all its open file descriptors, and issues a
	 * call to setsid to establish itself as an independent session
	 * immune from control signals.
	 *
	 * fork() is only called if it should run in daemon mode, fork is
	 * not disabled with the command line argument and there's no
	 * such process running.
	 */
	if ( (!one_shot) && (!NoFork) )
	{
		if (!check_pid(PidFile))
		{
			if ( fork() == 0 )
			{
				auto int fl;
				int num_fds = getdtablesize();
		
				/* This is the child closing its file descriptors. */
				for (fl= 0; fl <= num_fds; ++fl)
				{
					if ( fileno(stdout) == fl && use_output )
						if ( strcmp(output, "-") == 0 )
							continue;
					close(fl);
				}
 
				setsid();
			}
			else
				exit(0);
		}
		else
		{
			fputs("klogd: Already running.\n", stderr);
			exit(1);
		}
	}


	/* tuck my process id away */
	if (!check_pid(PidFile))
	{
		if (!write_pid(PidFile))
			Terminate();
	}
	else
	{
		fputs("klogd: Already running.\n", stderr);
		Terminate();
	}
	

	/* Signal setups. */
	for (ch= 1; ch < NSIG; ++ch)
		signal(ch, SIG_IGN);
	signal(SIGINT, stop_daemon);
	signal(SIGKILL, stop_daemon);
	signal(SIGTERM, stop_daemon);
	signal(SIGHUP, stop_daemon);
	signal(SIGTSTP, stop_logging);
	signal(SIGCONT, restart);
	signal(SIGUSR1, reload_daemon);
	signal(SIGUSR2, reload_daemon);


	/* Open outputs. */
	if ( use_output )
	{
		if ( strcmp(output, "-") == 0 )
			output_file = stdout;
		else if ( (output_file = fopen(output, "w")) == (FILE *) 0 )
		{
			fprintf(stderr, "klogd: Cannot open output file " \
				"%s - %s\n", output, strerror(errno));
			return(1);
		}
	}
	else
		openlog("kernel", 0, LOG_KERN);


	/* Handle one-shot logging. */
	if ( one_shot )
	{
		InitKsyms(symfile);
		InitMsyms();
		if ( (logsrc = GetKernelLogSrc()) == kernel )
			LogKernelLine();
		else
			LogProcLine();
		Terminate();
	}

	/* Determine where kernel logging information is to come from. */
#if defined(KLOGD_DELAY)
	sleep(KLOGD_DELAY);
#endif
	logsrc = GetKernelLogSrc();
	InitKsyms(symfile);
	InitMsyms();

        /* The main loop. */
	while (1)
	{
		if ( change_state )
			ChangeLogging();
		switch ( logsrc )
		{
			case kernel:
	  			LogKernelLine();
				break;
			case proc:
				LogProcLine();
				break;
		        case none:
				pause();
				break;
		}
	}
}
