// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
#define NO_PTE		0xFFFFFFFF

extern volatile pte_t uvpt[];     // VA of "virtual page table"
extern volatile pde_t uvpd[];     // VA of current page directory

pte_t get_pte(void *va) {
	// Needs to check if the page table exists, using upvd
	return uvpd[PDX(va)] & PTE_P ? uvpt[PGNUM(va)] : NO_PTE;
}

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
// YY: This handler runs on the exception stack.
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	pte_t pte = get_pte(addr);
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	bool isWrite = utf->utf_err & FEC_WR;
	bool isCOW = pte & PTE_COW;
	if (!isWrite || !isCOW)
		panic("Addr: %08x, isWrite: %d, isCOW: %d\n", addr, isWrite, isCOW);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	void *remap_va = (void *)ROUNDDOWN((uint32_t)addr, PGSIZE);
	sys_page_alloc(0, (void *)PFTEMP, PTE_W|PTE_U|PTE_P);
	memcpy(PFTEMP, remap_va, PGSIZE);
	sys_page_map(0, PFTEMP, 0, remap_va, PTE_U|PTE_P|PTE_W);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
// YY: If the virtual page is not present in our space, silently does nothing.
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.

	// We're in user-mode, but we need to access the page tables.
	// Use the ones mapped at UVPT.
	void *va;
	pte_t pte;

	va = (void *) (pn * PGSIZE);
	pte = get_pte(va);
	if (pte == NO_PTE || (pte & PTE_P) == 0)
		return 0;

	if (pte & PTE_SHARE) {
		if ((r = sys_page_map(0, va, envid, va, PTE_P|PTE_U|PTE_W|PTE_SHARE)) < 0)
			panic("duppage: %e\n", r);
	} else if ((pte & PTE_W) || (pte & PTE_COW)) {
		if ((r = sys_page_map(0, va, envid, va, PTE_U|PTE_COW|PTE_P)) < 0)
			panic("duppage: cannot map COW into new env. %e\n", r);

		if ((r = sys_page_map(0, va, 0, va, PTE_U|PTE_COW|PTE_P)) < 0)
			panic("duppage: cannot remap COW back, %e\n");
	} else {
		if ((r = sys_page_map(0, va, envid, va, PTE_U|PTE_P)) < 0)
			panic("duppage: cannot map read-only into new env, %e\n", r);
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t child_eid;
	uint32_t va;

	// cprintf("fork called in [%x]\n", sys_getenvid());
	// sys_exofork() calls env_alloc() to allocate a new environment.
	// env_alloc() calls env_setup_vm() to set up the kernel portion of
	// the new env's address space (everything above UTOP).
	// sys_exofork() copies the register set, and returns 0 in the child env.
	set_pgfault_handler(pgfault);
	child_eid = sys_exofork(); // 0 in child environment

	if (child_eid > 0) {
		// Duplicate user portion
		for (va = 0; va < UXSTACKTOP - PGSIZE; va += PGSIZE) {
			duppage(child_eid, va / PGSIZE);
		}

		// New page is allocated for exception stack.
		sys_page_alloc(child_eid, (void *)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W);

		// Set page fault entrypoint for child
		sys_env_set_pgfault_upcall(child_eid, thisenv->env_pgfault_upcall);

		// set child status
		sys_env_set_status(child_eid, ENV_RUNNABLE);
	} else {
		// set thisenv
		for (int i = 0; i < 1024; ++i) {
			if (envs[i].env_id == sys_getenvid()) {
				thisenv = &envs[i];
				break;
			}
		}
	}

	return child_eid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
