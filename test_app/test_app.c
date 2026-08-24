// test_app.c - minimal freestanding test ELF for runelf
// entry point is `main` itself here - no argc/argv, no libc, nothing.
// It just writes a known value to a known memory address, then
// returns - proving the loader placed and jumped to real code.
int _start(void)
{
    volatile unsigned int marker;
    marker = 0xDEADC0DE;
    // falls off the end -> compiler emits `ret` -> control returns
    // cleanly to command_runelf in the kernel
}