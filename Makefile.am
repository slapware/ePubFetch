## $Id: Makefile.am,v 1.14 2007/07/08 20:06:58 sebdiaz Exp $

CLEANFILES = *~

EXTRA_DIST = styles.h

noinst_PROGRAMS = test.cgi dns.cgi upload.cgi cookie.cgi epubFetch eList capture cardgame.cgi regex

INCLUDES = -I$(top_srcdir) -I$(top_builddir)

test_cgi_SOURCES = test.cpp
test_cgi_LDADD = ../cgicc/libcgicc.la @DEMO_LIBS@

regex_SOURCES = regex.cpp
regex_LDADD = -lboost_regex @DEMO_LIBS@

dns_cgi_SOURCES = dns.cpp
dns_cgi_LDADD = ../cgicc/libcgicc.la @DEMO_LIBS@ @LIB_NET@

upload_cgi_SOURCES = upload.cpp
upload_cgi_LDADD = ../cgicc/libcgicc.la @DEMO_LIBS@

cookie_cgi_SOURCES = cookie.cpp
cookie_cgi_LDADD = ../cgicc/libcgicc.la @DEMO_LIBS@

epubFetch_SOURCES = epubfetch.cpp
epubFetch_LDADD = -lcgicc -lmysqlcppconn -lboost_regex @DEMO_LIBS@
## epubFetch_LDADD = ../cgicc/libcgicc.la -lmysqlcppconn -lboost_regex @DEMO_LIBS@
## epubFetch_CXXFLAGS = -Wall -W -O2 

eList_SOURCES = eList.cpp Markup.cpp
eList_LDADD = -lcgicc -lmysqlcppconn @DEMO_LIBS@
eList_CXXFLAGS = -Wall -W -O2

capture_SOURCES = capture.cpp
capture_LDADD = -lcgicc
capture_CXXFLAGS = -Wall -W -O2


cardgame_cgi_SOURCES = cardgame.cpp

cardgame_cgi_LDADD = ../cgicc/libcgicc.la @DEMO_LIBS@
