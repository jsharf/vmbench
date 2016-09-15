#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vmm/vmm.h>

struct virtual_machine vm = {.halt_exit = true,};

static volatile int count;
int mc;

static void vmcall(void *a)
{
	__asm__ __volatile__("hlt\n\t");
	while (count < 10/*00000*/) {
		__asm__ __volatile__("vmcall\n\t");
		count++;
	}
	__asm__ __volatile__("hlt\n\t");
}


int main(int argc, char **argv)
{

	if (vthread_attr_init(&vm, 0) < 0)
		err(1, "vthread_attr_init: %r: ");
	vthread_create(&vm, 0, vmcall, NULL);

	uthread_sleep_forever();
	return 0;
}
