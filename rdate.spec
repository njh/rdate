Summary: Tool for getting the date/time from a remote machine.
Name: rdate
Version: 1.4
Release: 1
Copyright: GPL
Group: Applications/System
Source: ftp://people.redhat.com/sopwith/rdate-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root

%description
The rdate utility retrieves the date and time from another machine on
your network, using the protocol described in RFC 868. If you run
rdate as root, it will set your machine's local time to the time of
the machine that you queried.

%prep
%setup -q

%build
make CFLAGS="$RPM_OPT_FLAGS"

%install
rm -rf ${RPM_BUILD_ROOT}

mkdir -p ${RPM_BUILD_ROOT}%{_bindir}
%makeinstall

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%{_bindir}/rdate
%{_mandir}/man1/rdate.1*

%changelog
* Mon Mar 22 2004 Elliot Lee <sopwith@redhat.com> 1.4-1
- Timeout (-t) patch from Johan Nilsson <joh-nils@dsv.su.se>

* Wed Jan 29 2003 Phil Knirsch <pknirsch@redhat.com> 1.3-2
- Bump release and rebuild.

* Wed Nov 06 2002 Elliot Lee <sopwith@redhat.com> 1.3-1
- New release

* Fri Jun 21 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Wed Jun 19 2002 Phil Knirsch <pknirsch@redhat.com> 1.2-4
- Don't forcibly strip binaries

* Thu May 23 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Thu Mar 07 2002 Elliot Lee <sopwith@redhat.com>
- Make a 1.2 release, update to it. In the future, please commit changes
to CVS and make a new release, instead of adding patches to the rpm
package.

* Mon Feb 25 2002 Elliot Lee <sopwith@redhat.com>
- Bump & rebuild for 7.3

* Wed Dec  5 2001 Tim Powers <timp@redhat.com>
- bump release number and rebuild on alpha.

* Thu Dec  7 2000 Crutcher Dunnavant <crutcher@redhat.com>
- Fixed Bugzilla bug #41119. More of a RFE, but still important.

* Thu Dec  7 2000 Crutcher Dunnavant <crutcher@redhat.com>
- rebuild for new tree

* Thu Aug 17 2000 Jeff Johnson <jbj@redhat.com>
- summaries from specspo.

* Wed Aug 09 2000 Philipp Knirsch <pknirsch@redhat.com>
- Bugfix for missing /etc/services entry for time protocol (#15797)

* Mon Jul 31 2000 Crutcher Dunnavant <crutcher@redhat.com>
- tracked successful rdate attempts, so that failure returns 1

* Wed Jul 12 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Sun Jun 18 2000 Jeff Johnson <jbj@redhat.com>
- FHS packaging.

* Fri Feb 04 2000 Elliot Lee <sopwith@redhat.com>
- Rewrite the stinking thing due to license worries (bug #8619)

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 8)

* Sun Aug 16 1998 Jeff Johnson <jbj@redhat.com>
- build root

* Tue May 05 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr

* Mon Oct 20 1997 Otto Hammersmith <otto@redhat.com>
- fixed the url to the source

* Mon Jul 21 1997 Erik Troan <ewt@redhat.com>
- built against glibc
