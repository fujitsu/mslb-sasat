#
#	Makefile
#
CC     = gcc
#CFLAGS = -g -Wall -D_REENTRANT -D_GNU_SOURCE -DBACKEND_T
CFLAGS  = -O3 -Wall -D_REENTRANT -D_GNU_SOURCE -DBACKEND_T
INC    = -I ../common -I.

TARGET = backend.o client_tbl.o command_proc.o mac_resolution.o log.o ping.o route.o \
	back_init.o back_properties.o

OBJ    = sasat_b

.SUFFIXES: .c .o

.PHONY: all
all: depend $(OBJ)

$(OBJ): $(TARGET)
	$(CC) -lrt -lpthread -o $(OBJ) $^

.c.o:
	$(CC) $(INC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f  $(OBJ) $(TARGET) depend.inc

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
