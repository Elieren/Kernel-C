#ifndef KERNEL_EXCEPTION_H
#define KERNEL_EXCEPTION_H

#include <stdint.h>

#define EXIT_SIGSEGV (-11) /* Segmentation Violation — выход за пределы памяти */
#define EXIT_SIGBUS (-7)   /* Bus Error — неверное обращение к шине/порту       */

void handle_page_fault(uint64_t cr2, uint64_t error_code,
                       uint64_t rip, uint64_t cs);
void handle_gpf(uint64_t error_code, uint64_t rip, uint64_t cs);

#endif
