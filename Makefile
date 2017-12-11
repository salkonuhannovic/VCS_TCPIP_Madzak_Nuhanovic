## @file Makefile
## VCS - WS 2017
## TCP/IP Projekt
## 
##
## @author Claudia Madzak <ic16b028@technikum-wien.at>
## @author Salko Nuhanovic <ic17b064@technikum-wien.at>
##
## @date 2017/12/10
## 
##
##
##-------------------------------------------------------------------------------------------------
TARGETS=simple_message_client simple_message_server

CC=gcc52
CFLAGS=-DDEBUG -Wall -pedantic -Werror -Wextra -Wstrict-prototypes -fno-common -g -O3 -std=gnu11
LDLIBS=-lsimple_message_client_commandline_handling 

MAKE=make
RM=rm -f
CP=cp
CD=cd
MV=mv
GREP=grep
DOXYGEN=doxygen

EXCLUDE_PATTERN=footrulewidth

##
## ----------------------------------------------------------------- rules --
##

%.o: %.c
	$(CC) $(CFLAGS) -c $<

##
## --------------------------------------------------------------- targets --
##

.PHONY: all
all: $(TARGETS)

client:
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

server:
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: distclean
distclean:
	$(RM) *.o *~

.PHONY: clean
clean:
	$(RM) *.o *~ $(TARGETS)
	$(RM) -r doc
	$(RM) *.html *.png

.PHONY: doc
doc: html pdf

.PHONY: html
html:
	$(DOXYGEN) doxygen.dcf

.PHONY: pdf
pdf: html
	$(CD) doc/pdf && \
	$(MV) refman.tex refman_save.tex && \
	$(GREP) -v $(EXCLUDE_PATTERN) refman_save.tex > refman.tex && \
	$(RM) refman_save.tex && \
	make && \
	$(MV) refman.pdf refman.save && \
	$(RM) -f *.pdf *.html *.tex *.aux *.sty *.log *.eps *.out *.ind *.idx \
	      *.ilg *.toc *.tps Makefile && \
	$(MV) refman.save refman.pdf