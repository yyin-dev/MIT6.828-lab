/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	uint32_t s_start = (uint32_t) s;
	uint32_t s_end = (uint32_t)s + len;
	uint32_t start_va = ROUNDDOWN(s_start, PGSIZE);
	uint32_t end_va = ROUNDUP(s_end, PGSIZE);

	uint32_t va;
	for (va = start_va; va < end_va; va += PGSIZE) {
		// Use curenv->env_pgdir instead of kern_pgdir
		pte_t* pte = pgdir_walk(curenv->env_pgdir, (void *)va, 0);
		if (pte == NULL || (*pte & PTE_U) == 0) {
			cprintf("kern/syscall.c:sys_cputs: Memory error, destory env\n");
			env_destroy(curenv);
		}
	}

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	cprintf("kern/syscall.c:syscall: syscallno = %d\n", syscallno);

	switch (syscallno) {
	case SYS_cputs:
		// cprintf("kern/syscall.c:syscall: %.*s\n\n", (char *)a1, a2);
		sys_cputs((char *)a1, a2);
		return 0;
	case SYS_cgetc:
		;
		int c = sys_cgetc();
		return c;
	case SYS_getenvid:
		;
		int envid = sys_getenvid();
		return envid;
	case SYS_env_destroy:
		sys_env_destroy(a1);
		return 0;
	case NSYSCALLS:
	default:
		return -E_INVAL;
	}
}

