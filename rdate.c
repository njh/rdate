/*
    Copyright (C) 2000 Red Hat, Inc.

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
   to put a copyright/license on the previous rdate. See bugzilla.redhat.com, bug #8619. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>

/* difference between Unix time and net time */
#define BASE1970	2208988800L
#define DEFAULT_PORT    37

static char *argv0;

/* Use TCP connect by default */
static int use_tcp = 1;
/* send stuff to syslog instead of stderr */
static int log_mode = 0;
static int print_mode = 0;
static jmp_buf coenv;

void abort_alarm(int dummy)
{
  siglongjmp(coenv, 1);
}

/* syslog output requested by Michael Sanford <mtsanford@cryptobit.org>,
   based on ideas from his patch implementing it. */
static void writeLog(int is_error, char *format, ...)
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

static int
rdate(const char *hostname, time_t *retval)
{
#ifndef INET6
  struct servent *sent;
  struct sockaddr_in saddr;
#else
  char host[NI_MAXHOST];
  struct addrinfo hints, *res, *res0;
  int err = -1;
#endif
  int fd;
  unsigned char time_buf[4];
  int nr, n_toread;

  assert(hostname);
  assert(retval);

#ifndef INET6
  saddr.sin_family = AF_INET;

  if(!inet_aton(hostname, &saddr.sin_addr))
    {
      struct hostent *hent;

      hent = gethostbyname(hostname);
      if(!hent)
	{
	  writeLog(1, "Unknown host %s: %s", hostname, hstrerror(h_errno));
	  return -1;
	}

      assert(hent->h_addrtype == AF_INET);
      memcpy(&saddr.sin_addr, hent->h_addr_list[0], hent->h_length);
    }

  if((sent = getservbyname("time", "tcp")))
    saddr.sin_port = sent->s_port;      
  else
    saddr.sin_port = htons(DEFAULT_PORT);

  if (use_tcp)
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  else
    fd = socket(AF_INET, SOCK_DGRAM, 0);

  if(fd < 0)
    {
      writeLog(1, "couldn't create socket: %s", strerror(errno));
      return -1;
    }

#else
  snprintf(host, sizeof(host), "%s", hostname);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = (use_tcp) ? SOCK_STREAM : SOCK_DGRAM;
  err = getaddrinfo(host, "time", &hints, &res0);
  if (err < 0) {
	  writeLog(1, "%s: %s", host, gai_strerror(err));
	  return -1;
  }
  err = -1;

  for (res = res0; res; res = res->ai_next) {
	  if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) >= 0) {
		  err = 0;
		  break;
	  }
  }
  freeaddrinfo(res0);
  if (err < 0) {
	  writeLog(1, "couldn't create socket: %s", strerror(errno));
	  return -1;
  }
#endif

  if (use_tcp) {
#ifndef INET6
    if(connect(fd, &saddr, sizeof(saddr)))
#else
    if (connect(fd, res->ai_addr, res->ai_addrlen))
#endif
      {
        writeLog(1, "couldn't connect to host %s: %s", hostname, strerror(errno));
        close(fd);
        return -1;
      }

    for(n_toread = sizeof(time_buf), nr = 1; nr > 0 && n_toread > 0; n_toread -= nr)
      nr = read(fd, time_buf + sizeof(time_buf) - n_toread, n_toread);
    if(n_toread)
      {
        if(nr)
	  writeLog(1, "error in read: %s", strerror(errno));
        else
	  writeLog(1, "got EOF from time server");
        close(fd);

        return -1;
      }
  } else {
#ifndef INET6
    if (sendto(fd, NULL, 0, 0, &saddr, sizeof(saddr))) {
#else
    if (sendto(fd, NULL, 0, 0, res->ai_addr, res->ai_addrlen)) {
#endif
      writeLog(1, "couldn't send UDP message to host %s: %s", hostname, strerror(errno));
      close(fd);
      return -1;
    }

#ifndef INET6
    if (recvfrom(fd, &time_buf, sizeof(time_buf), 0, &saddr, &nr) != 4)
#else
    if (recvfrom(fd, &time_buf, sizeof(time_buf), 0, res->ai_addr, &res->ai_addrlen) != 4)
#endif
    {
      if(nr)
	writeLog(1, "error in read: %s", strerror(errno));
      else
	writeLog(1, "got EOF from time server");
      close(fd);
      return -1;
    }
  }

  close(fd);
  
  /* See inetd's builtins.c for an explanation */
  *retval = (time_t)(ntohl(*(uint32_t *)time_buf) - 2208988800UL);

  return 0;
}

static void
usage(int iserr)
{
  fprintf(stderr, "Usage: %s [-s] [-p] [-u] [-l] [-t sec] <host> ...\n", argv0);
  exit(iserr?1:0);
}

int main(int argc, char *argv[])
{
  int i;
  int set_mode = 0;
  char **hosts = NULL;
  int nhosts = 0;
  int retval = 0;
  int success = 0;
  int timeout = 10;

  argv0 = strrchr(argv[0], '/');
  if(argv0)
    argv0++;
  else
    argv0 = argv[0];

  for(i = 1; i < argc; i++)
    {
      switch(argv[i][0])
	{
	case '-':
	  switch(argv[i][1])
	    {
	    case 's':
	      set_mode = 1;
	      break;
	    case 'p':
	      print_mode = 1;
	      break;
	    case 'u':
	      use_tcp = 0;
	      break;
	    case 'l':
	      log_mode = 1;
	      break;
	    case 't':
	      if(i + 1 >= argc)
		usage(0);
	      timeout = atoi(argv[++i]);
	      break;
	    case 'h':
	    case '?':
	      usage(0);
	      break;
	    default:
	      fprintf(stderr, "Unknown option %s\n", argv[i]);
	      break;
	    }
	  break;
	default:
	  hosts = realloc(hosts, sizeof(char *) * nhosts+1);
	  hosts[nhosts++] = argv[i];
	  break;
	}
    }

  if(!set_mode && !print_mode)
    print_mode = 1;
  if(log_mode)
    openlog(argv0, LOG_PID, LOG_CRON);

  signal(SIGALRM, abort_alarm);

  for(i = 0; i < nhosts; i++)
    {
      time_t timeval;

      alarm(timeout);

      if(sigsetjmp(coenv, 1) == 1) {
	printf("rdate: timeout for %s\n", hosts[i]);
	continue;
      }

      if(!rdate(hosts[i], &timeval))
	{
	  /* keep track of the succesful request */
	  success = 1;

	  writeLog(0, "[%s]\t%s", hosts[i], ctime(&timeval));

	  /* Do specified action(s) */
	  if(set_mode && stime(&timeval) < 0)
	    {
	      writeLog(1, "%s: could not set system time: %s", argv0, strerror(errno));
	      retval = 1;
	      break;
	    }
	  set_mode = 0;
	}
    }

  if(!nhosts)
    usage(1);
  else if (!retval && !success)
    retval = 1;

  return retval;
}

