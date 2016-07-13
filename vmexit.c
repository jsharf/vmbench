#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vmm/vmm.h>

struct virtual_machine local_vm, *vm = &local_vm;

struct vmm_gpcore_init gpci;

#define MiB 0x100000ull
#define GiB (1ull << 30)
#define GKERNBASE (16*MiB)
#define KERNSIZE (128*MiB+GKERNBASE)

unsigned long long *p512, *p1, *p2m;

void **my_retvals;
int nr_threads = 4;
int debug = 0;
int resumeprompt = 0;
int main(int argc, char **argv)
{
	int vmmflags = 0; // Disabled probably forever. VMM_VMCALL_PRINTF;
	uint64_t entry = 0x1200000;
	int ret;
	int i;
	struct vm_trapframe *vm_tf;

	vm->low4k = malloc(PGSIZE);
	memset(vm->low4k, 0xff, PGSIZE);

	ret = vmm_init(vm, vmmflags);
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
	uint64_t highkernbase = 0xffffffff80000000;
	p512[PML4(kernbase)] = (unsigned long long)p1 | 7;
	p1[PML3(kernbase)] = /*0x87; */(unsigned long long)p2m | 7;
	p512[PML4(highkernbase)] = (unsigned long long)p1 | 7;
	p1[PML3(highkernbase)] = /*0x87; */(unsigned long long)p2m | 7;
#define _2MiB (0x200000)

	for (i = 0; i < 512; i++) {
		p2m[PML2(kernbase + i * _2MiB)] = 0x87 | i * _2MiB;
	}

	kernbase >>= (0+12);
	kernbase <<= (0 + 12);

	vm_tf = gth_to_vmtf(vm->gths[0]);
	vm_tf->tf_cr3 = (uint64_t) p512;
	vm_tf->tf_rip = entry;
	vm_tf->tf_rsp = 0;
	start_guest_thread(vm->gths[0]);

	uthread_sleep_forever();
	return 0;
}
