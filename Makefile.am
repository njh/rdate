AUTOMAKE_OPTIONS = foreign

dist_man_MANS = rdate.1
SUBDIRS = src
EXTRA_DIST = autogen.sh rdate.spec

# Copy README.md to README when building distribution
dist-hook:
	[ -f README.md ] && cat README.md > README || true
