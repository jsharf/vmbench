#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vmm/vmm.h>

struct virtual_machine vm;

static volatile int count;
int mc;

static void vmcall(void *a)
{
	while (count < 1000000) {
		__asm__ __volatile__("vmcall\n\t");
		count++;
	}
	__asm__ __volatile__("hlt\n\t");
}


unsigned long long s[512];

int main(int argc, char **argv)
{

	if (vthread_attr_init(&vm, 0) < 0)
		err(1, "vthread_attr_init: %r: ");
	vthread_create(&vm, 0, vmcall, NULL, &s[511]);

	while(count < 100000) {
		mc++;
	}

	printf("Guest count %d, master count %d\n", count, mc);
	return 0;
}
