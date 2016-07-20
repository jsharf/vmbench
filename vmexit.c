#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vmm/vmm.h>

struct virtual_machine local_vm, *vm = &local_vm;
struct vmm_gpcore_init gpci;

unsigned long long *p512, *p1, *p2m, *stack;

void **my_retvals;
int nr_threads = 4;
int debug = 0;
static int count;

static void vmcall(void)
{
  __asm__ __volatile__ ("2: jmp 2b\n");
	__asm__ __volatile__("1: mov $0x30,  %rdi\n\tvmcall\n\t");
	while (count < 1000) {
		__asm__ __volatile__("1: xor %rdi, %rdi\n\tvmcall\n\tjmp 1b\n\t");
		count++;
	}
	__asm__ __volatile__("1: mov $0x31,  %rdi\n\tvmcall\n\t");
}


void *page(void *addr)
{
	void *v;
	unsigned long flags = MAP_POPULATE | MAP_ANONYMOUS;
	if (addr)
		flags |= MAP_FIXED;

	v = (void *)mmap(addr, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
	fprintf(stderr, "Page: request %p, get %p\n", addr, v);
	return v;
}

int main(int argc, char **argv)
{
	int vmmflags = 0; // Disabled probably forever. VMM_VMCALL_PRINTF;
	uint64_t entry = 0x1200000, kerneladdress = 0x1200000;
	int ret;
	uintptr_t size;
	void * xp;
	void *a_page;
	struct vm_trapframe *vm_tf;

	fprintf(stderr, "%p %p %p %p\n", (void *)PGSIZE, (void *)PGSHIFT, (void *)PML1_SHIFT,
		(void *)PML1_PTE_REACH);



	//Place mmap(Gan)
	a_page = mmap((void *)0xfee00000, PGSIZE, PROT_READ | PROT_WRITE,
		              MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
	fprintf(stderr, "a_page mmap pointer %p\n", a_page);

	if (a_page == (void *) -1) {
		perror("Could not mmap APIC");
		exit(1);
	}
	if (((uint64_t)a_page & 0xfff) != 0) {
		perror("APIC page mapping is not page aligned");
		exit(1);
	}

	memset(a_page, 0, 4096);
	((uint32_t *)a_page)[0x30/4] = 0x01060015;
	//((uint32_t *)a_page)[0x30/4] = 0xDEADBEEF;

	xp = page((void *)kerneladdress);
	memmove(xp, (void *)vmcall+4, 4096);
	xp += 4096;
	size = ROUNDUP((uintptr_t)xp - kerneladdress, PML2_PTE_REACH);
	//fprintf(stderr, "Read in %u bytes\n", size);

	gpci.posted_irq_desc = page(NULL);
	gpci.vapic_addr = page(NULL);

	// set up apic values? do we need to?
	// qemu does this.
	//((uint8_t *)a)[4] = 1;

	gpci.apic_addr = page((void*)0xfee00000);

	vm->nr_gpcs = 1;
	vm->gpcis = &gpci;
	ret = vmm_init(vm, vmmflags);
	assert(!ret);

	p512 = page(0x1000000);
	p1 = page(p512+512);
	p2m = page(p1+512);
	stack = page(p2m+512);
	/* Allocate 3 pages for page table pages: a page of 512 GiB
	 * PTEs with only one entry filled to point to a page of 1 GiB
	 * PTEs; a page of 1 GiB PTEs with only one entry filled to
	 * point to a page of 2 MiB PTEs; and a page of 2 MiB PTEs,
	 * only a subset of which will be filled. */

	/* Set up a 1:1 ("identity") page mapping from guest virtual
	 * to guest physical using the (host virtual)
	 * `kerneladdress`. This mapping is used for only a short
	 * time, until the guest sets up its own page tables. Be aware
	 * that the values stored in the table are physical addresses.
	 * This is subtle and mistakes are easily disguised due to the
	 * identity mapping, so take care when manipulating these
	 * mappings. */

	p512[PML4(kerneladdress)] = (uint64_t)p1 | PTE_KERN_RW;
	p1[PML3(kerneladdress)] = (uint64_t)p2m | PTE_KERN_RW;
	for (uintptr_t i = 0; i < size; i += PML2_PTE_REACH) {
		p2m[PML2(kerneladdress + i)] =
		    (uint64_t)(kerneladdress + i) | PTE_KERN_RW | PTE_PS;
	}



	vm_tf = gth_to_vmtf(vm->gths[0]);
	vm_tf->tf_cr3 = (uint64_t) p512;
	vm_tf->tf_rip = entry;
	vm_tf->tf_rsp = (uint64_t) (stack+512);
	vm_tf->tf_rsi = 0;
	start_guest_thread(vm->gths[0]);

	uthread_sleep_forever();
	return 0;
}
