#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vmm/vmm.h>

static struct virtual_machine vm;

#define MiB 0x100000ull
#define GiB (1ull << 30)
#define GKERNBASE (16*MiB)
#define KERNSIZE (128*MiB+GKERNBASE)

static unsigned long long *p512, *p1, *p2m;
static volatile int count;

static void vmcall(void)
{
	while (count < 1000){
		while ((count & 1) == 0)
			;
		count++;
	}
}

int main(int argc, char **argv)
{
	int vmmflags = 0; // Disabled probably forever. VMM_VMCALL_PRINTF;
	int ret;
	int i;
	struct vm_trapframe *vm_tf;
 
	vm.low4k = malloc(PGSIZE);
	memset(vm.low4k, 0xff, PGSIZE);

	ret = vmm_init(&vm, vmmflags);
	assert(!ret);

	ret = posix_memalign((void **)&p512, 4096, 3*4096);
	fprintf(stderr, "memalign is %p\n", p512);
	if (ret) {
		perror("ptp alloc");
		exit(1);
	}
	p1 = &p512[512];
	p2m = &p512[1024];
	uint64_t kernbase = 0; //0xffffffff80000000;
	p512[PML4(kernbase)] = (unsigned long long)p1 | 7;
	p1[PML3(kernbase)] = /*0x87; */(unsigned long long)p2m | 7;
#define _2MiB (0x200000)

	for (i = 0; i < 512; i++) {
		p2m[PML2(kernbase + i * _2MiB)] = 0x87 | i * _2MiB;
	}

	vm_tf = gth_to_vmtf(vm.gths[0]);
	vm_tf->tf_cr3 = (uint64_t) p512;
	vm_tf->tf_rsp = 0;
	vm_tf->tf_rip = (uint64_t) vmcall;
	start_guest_thread(vm.gths[0]);

	while (count < 1000){
		while ((count & 1) == 1)
			;
		count++;
	}
//	uthread_sleep_forever();
	return 0;
}
