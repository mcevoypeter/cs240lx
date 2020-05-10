#ifndef __MY_SW_UART_H__
#define __MY_SW_UART_H__

#define BAUD 115200
#define CYCLES_PER_BIT (700*10000*1000UL)/BAUD
#define MAX_PINS 8

typedef enum {
    parallel = 0,
    serial
} uart_type_t;

typedef struct {
    unsigned txs[MAX_PINS], rxs[MAX_PINS];
    unsigned baud;
    unsigned cycle_per_bit;
} my_sw_uart_t;


my_sw_uart_t my_sw_uart_init(const unsigned *txs, const unsigned tx_cnt, 
        const unsigned *rxs, const unsigned rx_cnt);

int my_sw_uart_serial_get8(my_sw_uart_t *uart);
void my_sw_uart_serial_put8(my_sw_uart_t *uart, unsigned char b);
int my_sw_uart_serial_get32(my_sw_uart_t *uart);
void my_sw_uart_serial_put32(my_sw_uart_t *uart, unsigned w);

int my_sw_uart_parallel_get8(my_sw_uart_t *uart);
void my_sw_uart_parallel_put8(my_sw_uart_t *uart, unsigned char b);
int my_sw_uart_parallel_get32(my_sw_uart_t *uart);
void my_sw_uart_parallel_put32(my_sw_uart_t *uart, unsigned w);

#endif
