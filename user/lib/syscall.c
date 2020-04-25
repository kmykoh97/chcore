#include <lib/print.h>
#include <lib/syscall.h>
#include <lib/type.h>

u64 syscall(u64 sys_no, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4,
	    u64 arg5, u64 arg6, u64 arg7, u64 arg8)
{
	u64 ret = 0;
	/*
	 * Lab3: Your code here
	 * Use inline assembly to store arguments into x0 to x7, store syscall number to x8,
	 * And finally use svc to execute the system call. After syscall returned, don't forget
	 * to move return value from x0 to the ret variable of this function
	 */
	asm ("mov x9, %[value]" :: [value]"r"(sys_no));
	asm ("mov x0, %[value]" :: [value]"r"(arg0));
	asm ("mov x1, %[value]" :: [value]"r"(arg1));
	asm ("mov x2, %[value]" :: [value]"r"(arg2));
	asm ("mov x3, %[value]" :: [value]"r"(arg3));
	asm ("mov x4, %[value]" :: [value]"r"(arg4));
	asm ("mov x5, %[value]" :: [value]"r"(arg5));
	asm ("mov x6, %[value]" :: [value]"r"(arg6));
	asm ("mov x7, %[value]" :: [value]"r"(arg7));
	asm ("mov x8, x9");
	asm ("svc #0");
	asm ("mov %[des], x0" : [des]"=r" (ret));

	return ret;
}

/*
 * Lab3: your code here:
 * Finish the following system calls using helper function syscall
 */
void usys_putc(char ch)
{
	syscall(SYS_putc, (u64)ch, 0, 0, 0, 0, 0, 0, 0, 0);
}

void usys_exit(int ret)
{
	syscall(SYS_exit, (u64)ret, 0, 0, 0, 0, 0, 0, 0, 0);
}

int usys_create_pmo(u64 size, u64 type)
{
	return (int)syscall(SYS_create_pmo, size, type, 0, 0, 0, 0, 0, 0, 0);
}

int usys_map_pmo(u64 process_cap, u64 pmo_cap, u64 addr, u64 rights)
{
	return (int)syscall(SYS_map_pmo, process_cap, pmo_cap, addr, rights, 0, 0, 0, 0, 0);
}

u64 usys_handle_brk(u64 addr)
{
	return (u64)syscall(SYS_handle_brk, addr, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* Here finishes all syscalls need by lab3 */
