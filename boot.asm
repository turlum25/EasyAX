; Multiboot2 header for GRUB.
;
; Why not Multiboot1: MB1 has no way to ask for a graphics framebuffer,
; so GRUB falls back to legacy VGA text mode (0xB8000). Pure-UEFI real
; hardware has no legacy VGA text console at all (only a GOP framebuffer),
; so MB1 boots produced "WARNING: no console will be available to OS" and
; left us with no way to print anything. MB2's framebuffer tag lets us
; ask GRUB to hand us a linear framebuffer address instead, which works
; under UEFI, CSM/legacy BIOS, and in QEMU alike.
MAGIC    equ 0xE85250D6   ; multiboot2 magic
ARCH     equ 0            ; i386, protected mode
HDR_LEN  equ header_end - header_start
CHECKSUM equ -(MAGIC + ARCH + HDR_LEN)

section .multiboot
align 8
header_start:
    dd MAGIC
    dd ARCH
    dd HDR_LEN
    dd CHECKSUM

    ; --- framebuffer request tag ---
    ; width=height=depth=0 means "any mode you've got" - per the
    ; multiboot2 spec this asks GRUB to pick whatever linear framebuffer
    ; mode the hardware/firmware actually supports, rather than us
    ; demanding a specific resolution GRUB's video driver might not be
    ; able to set on this particular GPU. We read back whatever GRUB
    ; actually chose (addr/pitch/width/height/bpp) from the boot info
    ; tag list at runtime - never assume it matches any specific value.
    align 8
    dw 5        ; type = framebuffer
    dw 0        ; flags (0 = not optional)
    dd 20       ; size of this tag
    dd 0        ; width  (0 = any)
    dd 0        ; height (0 = any)
    dd 0        ; depth  (0 = any)

    ; --- end tag (required, terminates the tag list) ---
    align 8
    dw 0
    dw 0
    dd 8
header_end:

[bits 32]

section .text
global _start
extern kernel_start ; main.c entry point

_start:
    ; GRUB hands us: eax = multiboot2 magic (0x36d76289, NOT the old
    ; 0x2BADB002), ebx = ptr to multiboot2 info tag list. Both must be
    ; saved IMMEDIATELY - the GDT/segment-reload code below clobbers eax.
    mov [multiboot_magic], eax
    mov [multiboot_info_ptr], ebx

    ; Don't trust GRUB's leftover GDT/selectors - load our own known-good
    ; flat GDT so selector 0x08/0x10 used by idt_set_gate() are guaranteed
    ; to be a valid flat code/data segment.
    lgdt [gdt_descriptor]

    ; far jump to reload CS with our code selector (0x08) and flush
    ; the stale GRUB code segment out of the pipeline
    jmp 0x08:.reload_segments

.reload_segments:
    mov ax, 0x10        ; our data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; just setup a safe stack pointer in bss section
    mov esp, stack_top

    ; pass (magic, multiboot_info_ptr) to kernel_start - cdecl pushes
    ; right-to-left, so push info_ptr first, magic last (ends up first arg)
    push dword [multiboot_info_ptr]
    push dword [multiboot_magic]
    call kernel_start

_loop:
    hlt
    jmp _loop

section .data
align 8
gdt_start:
    dq 0x0000000000000000          ; null descriptor

gdt_code:
    dw 0xFFFF                      ; limit low
    dw 0x0000                      ; base low
    db 0x00                        ; base middle
    db 10011010b                   ; access: present, ring0, code, exec/read
    db 11001111b                   ; flags(4KB gran, 32-bit) + limit high
    db 0x00                        ; base high

gdt_data:
    dw 0xFFFF                      ; limit low
    dw 0x0000                      ; base low
    db 0x00                        ; base middle
    db 10010010b                   ; access: present, ring0, data, read/write
    db 11001111b                   ; flags(4KB gran, 32-bit) + limit high
    db 0x00                        ; base high

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1     ; GDT limit
    dd gdt_start                   ; GDT base address

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KB of safe stack space
stack_top:

multiboot_magic:    resd 1
multiboot_info_ptr: resd 1
