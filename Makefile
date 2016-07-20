# Simple little programs to test Akaros vm performance.
# Rules: keep this simple. Make sure the gcc is in your path and nobody gets hurt.

### Build flags for all targets
#
CFLAGS          = -O2 -std=gnu99 -fno-stack-protector -fgnu89-inline -fPIC -static -fno-omit-frame-pointer -g -Iinclude -Wall
LDFLAGS          =
LDLIBS         = -L$(AKAROS)/install/x86_64-ucb-akaros/sysroot/usr/lib -lpthread -lbenchutil -lm -liplib -lndblib -lvmm -lbenchutil
DEST	= $(AKAROS)/kern/kfs/bin

### Build tools
# 
CC=x86_64-ucb-akaros-gcc
AR=x86_64-ucb-akaros-ar
ALL=vmexit
all: $(ALL)
	scp $(ALL) skynet:

install: all
	echo "Installing $(ALL) in $(DEST)"
	cp $(ALL) $(DEST)

# compilers are fast. Just rebuild it each time.
vmexit: vmexit.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o vmexit vmexit.c  $(LDLIBS)
clean:
	rm -f $(ALL) *.o

# this is intended to be idempotent, i.e. run it all you want.
gitconfig:
	curl -Lo .git/hooks/commit-msg http://review.gerrithub.io/tools/hooks/commit-msg
	chmod u+x .git/hooks/commit-msg
	git config remote.origin.push HEAD:refs/for/master


