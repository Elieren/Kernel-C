#ifndef KERNEL_EXCEPTION_H
#define KERNEL_EXCEPTION_H

#include <stdint.h>

#define EXIT_SIGSEGV (-11) /* Segmentation Violation — выход за пределы памяти */
#define EXIT_SIGBUS (-7)   /* Bus Error — неверное обращение к шине/порту       */

void handle_page_fault(uint64_t fault_addr, uint64_t fault_flags,
                       uint64_t pc, uint64_t privilege);
void handle_gpf(uint64_t fault_flags, uint64_t pc, uint64_t privilege);

#endif