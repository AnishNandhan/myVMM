#include <stdio.h>
#include <uart.h>

/*
    Needs to emulate 16550 UART:
        1. Use 115200 baud rate
        2. Handle Rx and Tx:
            a. UART data framing -> Start bit, stop bit and parity bit
            b. Timing control -> tx and rx handlers need to run every 1/115200 second
               Can use timer in C ?
               itimerval.it_value.uv_sec = 1 / 115200 

               timer raises SIGALARM. two threads for rx and tx.

        (Optional: Implement FIFO mode)
*/

struct uart_16550_buffers {
    char tx;
    char rx;
};

struct uart_16550 {
    struct uart_16550_buffers buf;
};

void uart_handler(struct mmio_access *mmio) {
    if (mmio->is_write) {
        char s[mmio->len + 1];
        memcpy(s, mmio->data, mmio->len);
        s[mmio->len] = '\0';
        printf("%s", s);
    }
}


