#ifndef __FAST_GPIO_H__
#define __FAST_GPIO_H__

volatile unsigned *LEV0 = (unsigned *)0x20200034;
inline unsigned fast_gpio_read(unsigned pin) {
    return (*LEV0 >> pin) & 1;
}

// read n continguous pins starting at low_pin
inline unsigned fast_gpio_readn(unsigned low_pin, unsigned n) {
    return (*LEV0 >> low_pin) & ~(-1 << n);  
}


volatile unsigned *SET0 = (unsigned *)0x2020001c;
volatile unsigned *CLR0 = (unsigned *)0x20200028;
inline void fast_gpio_write(unsigned pin, unsigned val) {
    if (val)
        *SET0 = 1 << pin;
    else
        *CLR0 = 1 << pin;
}

// write n continguous pins starting at low_pin
inline void fast_gpio_writen(unsigned low_pin, unsigned n, unsigned val) {
    unsigned set_val = 0, clr_val = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned bit = (val >> i) & 1;
        if (bit)
            set_val |= 1 << (low_pin + i);
        else
            clr_val |= 1 << (low_pin + i);
    }
    *SET0 = set_val;
    *CLR0 = clr_val;
}

#endif
