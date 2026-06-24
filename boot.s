# Declare constants for the Multiboot header.
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Declare a multiboot header that marks the program as a kernel.
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Set up a 16 KiB stack.
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# The linker specifies _start as the entry point to the kernel.
.section .text
.global _start
.type _start, @function
_start:
    # Set up stack pointer register (esp)
    mov $stack_top, %esp

    # Call the main kernel function.
    call kernel_main

    # If the kernel returns, put the CPU into an infinite loop.
    cli
1:  hlt
    jmp 1b

# Set the size of the _start symbol for debugging/linking.
.size _start, . - _start
