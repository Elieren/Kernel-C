#ifndef REBOOT_H
#define REBOOT_H

#include <stdint.h>

/**
 * reboot_system — перезагрузка системы.
 *
 * Работает:
 *  - На реальном железе через Keyboard Controller
 *  - В QEMU
 */
void reboot_system(void);

#endif // REBOOT_H
