AC_PREREQ(2.50)


AC_INIT(rdate, 1.5)
AC_CONFIG_SRCDIR(src/rdate.c)
AM_CONFIG_HEADER(src/config.h)
AM_INIT_AUTOMAKE



dnl ############# Configure Arguments

AC_ARG_ENABLE(debug,
              [--enable-debug=[no/yes] turn on debugging],
              [ Debugging="Enabled" 
                AC_DEFINE(DEBUG, 1, [ Define if debugging is enabled. ]) ],
              [ Debugging="Disabled" ]
)



dnl ############# Compiler and tools Checks

AC_PROG_CC
AC_PROG_INSTALL


dnl ############## Library and Function Checks

dnl Checks for libraries.  We check for the library only if the function is
dnl not available without the library.
AC_CHECK_FUNC(gethostbyaddr, , [AC_CHECK_LIB(nsl, gethostbyaddr)])
AC_CHECK_FUNC(socket, , [AC_CHECK_LIB(socket, socket)])
AC_CHECK_FUNC(logwtmp, , [AC_CHECK_LIB(util, logwtmp)])


dnl Check for IPv6/protocol independent socket support
AC_CHECK_FUNCS( getaddrinfo \
		getnameinfo \
		adjtime \
		sigaction \
)

AC_TYPE_SIGNAL


dnl ############## Header Checks

AC_CHECK_HEADERS(time.h sys/time.h)
AC_CHECK_HEADERS(libutil.h util.h utmp.h unistd.h)


dnl ############## Type checks

AC_CHECK_TYPE(uint32_t, unsigned long)
AC_CHECK_TYPE(uint16_t, unsigned short)
AC_CHECK_TYPE(uint8_t, unsigned char)


dnl ############## Compiler and Linker Flags
CFLAGS="$CFLAGS -Wunused -Wall -pipe"
LDFLAGS="$LDFLAGS"

if test x$Debugging = xEnabled; then
	CFLAGS="$CFLAGS -g -Werror"
fi


dnl ############## Final Output

AC_OUTPUT( [ Makefile src/Makefile ] )



dnl ############## Display Message

echo "Now type 'make' and then 'make install'."
