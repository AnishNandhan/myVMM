#if !defined(UART_H)
#define UART_H

#include <std.h>

#define UART_16550_BASE MMIO_BASE + 0x000000
#define UART_16550_SIZE 0x8

// 16550 UART Register Map
#define RBR_16550 0x0   // Receiver Buffer Register
#define THR_16550 0x0   // Transmitter Holding Register
#define IER_16550 0x1   // Interrupt Enable Register
#define IIR_16550 0x2   // Interrupt Identification Register
#define FCR_16550 0x2   // FIFO Control Register
#define LCR_16550 0x3   // Line Control Register
#define MCR_16550 0x4   // MODEM Control Register
#define LSR_16550 0x5   // Line Status Register
#define MSR_16550 0x6   // MODEM Status Register
#define SCR_16550 0x7   // Scratch Register
#define DLL_16550 0x0   // Divisor Latch (LS)
#define DLM_16550 0x1   // Divisor Latch (LM)

void uart_handler(struct mmio_access *mmio);

#endif // UART_H