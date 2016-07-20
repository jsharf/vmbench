#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vmm/vmm.h>

static struct virtual_machine vm;
struct vmm_gpcore_init gpci;

#define MiB 0x100000ull
#define GiB (1ull << 30)
#define GKERNBASE (16 * MiB)
#define KERNSIZE (128 * MiB + GKERNBASE)

void hexdump(FILE *f, void *v, int length);
static unsigned long long *p512, *p1, *p2m;
static volatile int count;
static void vmcall(void)
{

	__asm__ __volatile__("1: mov $0x30,  %rdi\n\tvmcall\n\t");
	while (count < 1000) {
		__asm__ __volatile__("1: xor %rdi, %rdi\n\tvmcall\n\tjmp 1b\n\t");
		count++;
	}
	__asm__ __volatile__("1: mov $0x31,  %rdi\n\tvmcall\n\t");
}

void *page(void *addr)
{
	unsigned long flags = MAP_POPULATE | MAP_ANONYMOUS;
	if (addr)
		flags |= MAP_FIXED;

	return (void *)mmap(addr, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
}
int main(int argc, char **argv)
{
	int vmmflags = VMM_VMCALL_PRINTF;
	void *cr3 = (void *)0x1000000;
	void *code;
	int ret;
	int i;
	struct vm_trapframe *vm_tf;
	fprintf(stderr, "Start\n");
	code = page((void *)0x1200000);
	memmove(code, vmcall, 4096);
	hexdump(stderr, code, 128);
	fprintf(stderr, "vmcs stuf\n");
	gpci.posted_irq_desc = page(NULL);
	gpci.vapic_addr = page(NULL);
	gpci.apic_addr = page((void *)0xfee00000);

	vm.nr_gpcs = 1;
	vm.gpcis = &gpci;
	ret = vmm_init(&vm, vmmflags);
	if (ret) {
		fprintf(stderr, "vmm_init: ret %d, errno %d, %r\n", ret, errno);
		exit(1);
	}

	p512 = page((void *)cr3);
	p1 = page((void *)(cr3+0x1000));
	p2m = page((void *)(cr3+0x2000));
	uint64_t kernbase = 0;
	p512[PML4(kernbase)] = (unsigned long long)p1 | 7;
	p1[PML3(kernbase)] = /*0x87; */ (unsigned long long)p2m | 7;
#define _2MiB (0x200000)

	for (i = 0; i < 512; i++) {
		p2m[PML2(kernbase + i * _2MiB)] = 0x87 | i * _2MiB;
	}

	vm_tf = gth_to_vmtf(vm.gths[0]);
	fprintf(stderr, "cr3 is %p\n", p512);
	vm_tf->tf_cr3 = (uint64_t)cr3;
	vm_tf->tf_rsp = 0;
	vm_tf->tf_rip = (uint64_t)code;
	start_guest_thread(vm.gths[0]);

	uthread_sleep_forever();
	return 0;
}
