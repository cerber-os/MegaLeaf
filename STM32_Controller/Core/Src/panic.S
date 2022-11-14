#include "../Inc/panic.h"


MACRO
$label 	do_exception	$exc_id

		push {fp};			// Save current frame pointer on stack

		add fp, sp, 4		// Push current SP - we need to use fp register
		push {fp}			//  as push{sp} is not allowed

		add fp, sp, 8		// Set fp  to the beginning of stack
							//		(i.e. sp + 8 bytes of pushed fp and sp registers)

		push {r0-r12}		// Push all regs except: r13 (sp), r14(lr), r15(pc)
		push {lr}			// Finally, push Link Register

		mrs r0, SP_MSP
		mrs r1, SP_PSP
		push {r0,r1}		// Push MSP/PSP stack pointers as well

		mov r0, sp			// Provide address of this stack frame as the 1-st arg
		mov r1, $exc_id
		bl handle_exception

		// Restore context
		pop {r0, r1}
		pop {lr}
		pop {r0-r12}
		pop {fp}
		pop {fp}

		// Resume normal execution
		bx lr

MEND

/*
 * Define entry-point handlers for all supported exceptions
 */
do_exception NMI_Handler NMIExc_ID
do_exception HardFault_Handler HardFaultExc_ID
do_exception MemManage_Handler MemManageExc_ID
do_exception BusFault_Handler BusFaultExc_ID
do_exception UsageFault_Handler UsageFaultExc_ID

