CC = gcc
CFLAGS = -O2 -g -Wall -I.
CFLAGS += -fsanitize=thread
LDFLAGS = -fsanitize=thread

all: lbring

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

OBJS := lbring.o tests.o

deps := $(OBJS:%.o=.%.o.d)

lbring: $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	@mkdir -p .$(DUT_DIR)
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<

clean:
	rm -f $(OBJS) $(deps) lfring
	rm -rf *.dSYM

-include $(deps)