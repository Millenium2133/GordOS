.section .text

.macro ISR_NOERRORCODE num
.global isr\num
isr\num:
	cli

	# Pushes dummy error code for consistency
	push$0
	push$\num
	jmp isr_common
.endm

.macro ISR_ERRORCODE num
isr\num:
	cli
	push $\num
	jmp isr_common
.endm


# CPU exeptions
ISR_NOERRORCODE 0
ISR_NOERRORCODE 1
ISR_NOERRORCODE 2
ISR_NOERRORCODE 3
ISR_NOERRORCODE 4
ISR_NOERRORCODE 5
ISR_NOERRORCODE 6
ISR_NOERRORCODE 7
ISR_ERRORCODE 8
ISR_NOERRORCODE 9
ISR_ERRORCODE 10
ISR_ERRORCODE 11
ISR_ERRORCODE 12
ISR_ERRORCODE 13
ISR_ERRORCODE 14
ISR_NOERROECODE 15
ISR_NOERRORCODE 16
ISR_ERRORCODE 17
ISR_NOERRORCODE 18
ISR_NOERRORCODE 19
ISR_NOERRORCODE 20
ISR_NOERRORCODE 21
ISR_NOERRORCODE 22
ISR_NOERRORCODE 23
ISR_NOERRORCODE 24
ISR_NOERRORCODE 25
ISR_NOERRORCODE 26
ISR_NOERRORCODE 27
ISR_NOERRORCODE 28
ISR_NOERRORCODE 29
ISR_ERRORCODE 30
ISR_NOERRORCODE 31

# Common handler
isr_common:
	pusha

	mov %ds, %ax
	push %eax

	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	call isr_handler


	pop %eax
	mov %ax, %ds
	mov %ax, %es
	mov $ax, %fs
	mov %ax, %gs

	popa

	add $8, %esp

	iret
















