/*
    Copyright (C) 2000 Red Hat, Inc.
    Copyright (C) 2004 University of Southampton

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

/* By Elliot Lee <sopwith@redhat.com>, because some guy didn't bother
   to put a copyright/license on the previous rdate. See bugzilla.redhat.com, bug #8619.
   
   2004-11-27 - Improvements by Nicholas Humfrey <njh@ecs.soton.ac.uk>
   See ChangeLog for details.
*/


#include "config.h"
#include "rdate.h"


// difference between Unix time and net time
#define BASE1970		(2208988800L)
#define DEFAULT_SERVICE	("time")


static char *argv0;				// program name
static int use_tcp = 1;			// use TCP connect by default
static int log_mode = 0;		// send stuff to syslog instead of stderr
static int print_mode = 0;		// display the time
static int timeout = 10;		// timeout for each connection attempt
static char *service = DEFAULT_SERVICE;



// Returns string from address info pointer
static char*
ainfo2str(  struct addrinfo* ainfo )
{
	static char address[ NI_MAXHOST ];
	int result;
	
	// Empty the address
	memset(address, 0, sizeof(address));
	
	// Convert the address to string	
	result = getnameinfo(
				ainfo->ai_addr,
				ainfo->ai_addrlen,
				address, sizeof(address),
				NULL, 0,
				NI_NUMERICHOST | NI_NUMERICSERV);

	if (result) perror("ainfo2str()");
	
	return address;
}

/* syslog output requested by Michael Sanford <mtsanford@cryptobit.org>,
   based on ideas from his patch implementing it. */
static void
write_log(int is_error, char *format, ...)
{
	va_list args;
	int n;
	char buf[2048];
	va_start(args, format);
	n = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	
	if(n < 1)
		return; /* Error, which we can't report because this _is_ the error
				reporting mechanism */
	if(log_mode)
		syslog(is_error?LOG_WARNING:LOG_INFO, buf);
	if(is_error || print_mode)
		fprintf(is_error?stderr:stdout, "%s: %s\n", argv0, buf);
}


static
void timeout_callback(int unused)
{
	// This happens and returns immediately
}

static 
void set_timeout( int timeout )
{

	// Clear timeout ?
	if (timeout==0) {
		signal(SIGALRM, SIG_IGN);
		alarm(0);
		return;
	}


#ifdef HAVE_SIGACTION
{
	struct sigaction act;
	act.sa_handler = timeout_callback;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;

#ifdef SA_INTERRUPT
	// ensure system call is interrupted
	act.sa_flags |= SA_INTERRUPT; 
#endif

	if (sigaction (SIGALRM, &act, NULL) == -1)
    {
    	write_log(1, "sigaction failed: %s", strerror(errno));
    }	
}
#else

	signal(SIGALRM, timeout_callback);

#endif


	// Now set alarm
	alarm(timeout);
}




static int
rdate_tcp( int fd, struct addrinfo* ainfo, char* buf, int buf_size )
{
	int nr, n_toread;

	// Set timeout
	set_timeout( timeout );

	// Connect TCP session
	if (connect(fd, ainfo->ai_addr, ainfo->ai_addrlen))
	{
		if (errno == EINTR) {
			write_log(1, "timed out connecting to %s after %d seconds.", ainfo2str(ainfo), timeout);
		} else {
			write_log(1, "connect to address %s: %s", ainfo2str(ainfo), strerror(errno));
		}
		return -1;
	}

	// Read in the time
	for(n_toread = buf_size, nr = 1; nr > 0 && n_toread > 0; n_toread -= nr)
	{
		nr = read(fd, buf + buf_size - n_toread, n_toread);
	}

	if(n_toread)
	{
		if(nr)	write_log(1, "error in read from %s: %s", ainfo2str(ainfo), strerror(errno));
        else	write_log(1, "got EOF from time server");
        return -1;
	}
	
	set_timeout(0);

	// Success
	return 0;
}



static int
rdate_udp( int fd, struct addrinfo* ainfo, char* buf, int buf_size )
{
	fd_set readfds;
	struct timeval tv, *tv_ptr = NULL;
	int retval, nr;


    // Alarm doesn't seem to be enough for UDP
 	if (timeout) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		tv_ptr = &tv;
	}


	// Send empty packet to host
	if (sendto(fd, NULL, 0, 0, ainfo->ai_addr, ainfo->ai_addrlen)) {
		write_log(1, "failed to send UDP message to address %s: %s", ainfo2str( ainfo ), strerror(errno));
		return -1;
    }


	// Watch socket to see when it has input.
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	retval = select(FD_SETSIZE, &readfds, NULL, NULL, tv_ptr);

	// Check return value 
	if (retval == -1) {
		write_log(1, "select() failed: %s", strerror(errno));
		return -1;
	} else if (retval==0) {
		write_log(1, "timed out waiting for packet from %s after %d seconds.", ainfo2str( ainfo ), timeout);
		return -1;
	}

	// Recieve the reply
	nr = recv(fd, buf, buf_size, 0);
    if (nr != buf_size)
    {
		if(nr) {
			write_log(1, "error in read from address %s: %s", ainfo2str( ainfo ), strerror(errno));
		} else {
			write_log(1, "got EOF from time server");
		}

		return -1;
    }

	// Success
	return 0;
}



static int
rdate(const char *hostname, time_t *retval)
{
	struct addrinfo hints, *res, *res0;
	unsigned char time_buf[4];
	int result = -1;
	int fd = -1;
	int err = -1;
	*retval = 0;
	
	// Check paramaters	
	assert(hostname);
	assert(retval);


	// Hints about the type of socket we want
	memset(&hints, 0, sizeof(hints));
    //hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = (use_tcp) ? SOCK_STREAM : SOCK_DGRAM;
	
	
	// Look up addresses
	err = getaddrinfo(hostname, service, &hints, &res0);
	if (err || res0 == NULL) {
		write_log(1, "failed to lookup '%s': %s", hostname, gai_strerror(err));
		return -1;
	}
	
	// Try each returned address to create a socket with
	for (res = res0; res; res = res->ai_next) {
		if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) >= 0) {

			// Get the time from the remote host
			if (use_tcp) {
				result = rdate_tcp( fd, res, time_buf, sizeof(time_buf) );
			} else {
				result = rdate_udp( fd, res, time_buf, sizeof(time_buf) );
			}

			// Close the socket
			close(fd);

			// Stop trying if we succeeded
			if (!result) break;
		}
	}
	
	// Clean up allocated memory
	freeaddrinfo(res0);


	// Successfully got time ?
	if (result==0) {
		// See inetd's builtins.c for an explanation
		*retval = (time_t)(ntohl(*(uint32_t *)time_buf) - 2208988800UL);
	}

	return result;
}


static void
usage(int iserr)
{
	fprintf(stderr, "Usage: %s [-s] [-p] [-u] [-l] [-t sec] [-a] <host> ...\n", argv0);
	exit(iserr?1:0);
}

int main(int argc, char *argv[])
{
	int set_mode = 0;
	int adjust_time = 0;
	int retval = 0;
	int success = 0;
	int c;


	// Store the name of the program
	argv0 = strrchr(argv[0], '/');
	if (argv0)	argv0++;
	else		argv0 = argv[0];


	// Parse parameters 
	while ((c = getopt(argc, argv, "aspuln:t:h?")) != -1) {
	
		switch(c) {
			case 'a': adjust_time = 1; break;
			case 's': set_mode = 1;	break;
			case 'p': print_mode = 1; break;
			case 'u': use_tcp = 0; break;
			case 'l': log_mode = 1; break;
			case 't': timeout = atoi(optarg); break;
			case 'n': service = optarg; break;
			case 'h':
			case '?':
				usage(0);
			break;
			default:
				fprintf(stderr, "Unknown option %c\n", c);
				usage(1);
			break;	
		}
		
	}
	
	// Remove the already parsed parameters
	argc -= optind;
	argv += optind;

	// No hosts on command line ?
	if (argc<1) usage(1);


	if (!set_mode && !print_mode)	print_mode = 1;
	if (log_mode)					openlog(argv0, LOG_PID, LOG_CRON);



	// Query each of the servers on the command line
	for(; argc-- ; argv++)
    {
		time_t timeval=0;
		char timestr[26];

		if(!rdate(*argv, &timeval)) {
			// keep track of the succesful request
			success = 1;

			// Convert the time to a string
			ctime_r( &timeval, timestr );
			timestr[ strlen(timestr)-1 ] = 0;

			write_log(0, "[%s]\t%s", *argv, timestr);


			// Set local time to remote host's ?
			if (set_mode)
			{
				struct timeval tv;
				
				if (!adjust_time) {
					logwtmp("|", "date", "");
					tv.tv_sec = timeval;
					tv.tv_usec = 0;
					if (settimeofday(&tv, NULL)) {
						write_log(1, "could not set system time: %s", strerror(errno));
						retval = 1;
						break;
					}
					logwtmp("{", "date", "");
				} else {
					struct timeval tv_now;

					if (gettimeofday(&tv_now, NULL) == -1) {
						write_log(1, "could not get system time: %s", strerror(errno));
						retval = 1;
						break;
					}
					tv.tv_sec = timeval - tv_now.tv_sec;
					tv.tv_usec = 0;

					write_log(0, "adjusting local clock by %d seconds.", tv.tv_sec);
					if (adjtime(&tv, NULL)) {
						write_log(1, "could not adjust system time: %s", strerror(errno));
						retval = 1;
						break;
					}
				}
				
				// Only set time to first successful host
				set_mode = 0;
			}
		}
    }



	// Close the log
	if (log_mode) closelog();


	// Successful ?
	if (!retval && !success)
		retval = 1;

	return retval;
}

