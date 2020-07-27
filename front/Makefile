#
#	Makefile
#
CC	= gcc
#CFLAGS	= -g -Wall -D_REENTRANT -D_GNU_SOURCE -DFRONT_T
CFLAGS	= -O3 -Wall -D_REENTRANT -D_GNU_SOURCE -DFRONT_T
INC	= -I../common -I.

TARGET = front.o pol_lookup.o command_proc.o pol_tbl.o svr_tbl.o log.o mac_resolution.o ping.o \
	front_properties.o  route.o front_init.o\

OBJ	= sasat_f

.SUFFIXES: .c .o

.PHONY: all
all: depend $(OBJ)

$(OBJ): $(TARGET)
	$(CC) -lrt -lpthread -o $(OBJ) $^

.c.o:
	$(CC) $(INC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET) depend.inc

.PHONY: install
install: all
	cd ../cmd; make
	./inst_tr.sh

.PHONY: depend
depend: $(TARGET:.o=.c)
	-@ rm -f depend.inc
	-@ for i in $^; do \
	cpp $(INC) -MM $$i | sed "s/\ [_a-zA-Z0-9][_a-zA-Z0-9]*\.c//g" >> depend.inc;\
	done

-include depend.inc
