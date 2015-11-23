CC = gcc
CSCOPE = cscope
CFLAGS += -Wall -Werror

BLASTEE-OBJS := blastee.o \

BLASTER-OBJS := blaster.o \

ifeq ($(DEBUG), y)
 CFLAGS += -g -DDEBUG
endif

.PHONY: all
all: blastee blaster

blastee: blastee.o
	$(CC) $(CFLAGS) $(BLASTEE-OBJS) -o $@

blaster: blaster.o
	$(CC) $(CFLAGS) $(BLASTER-OBJS) -o $@

%.o: %.c *.h
	$(CC) $(CFLAGS) -o $@ -c $<

cscope:
	$(CSCOPE) -bqR

.PHONY: clean
clean:
	rm -rf *.o blastee blaster
