// engler, cs240lx initial driver code.
//
// everything is put in here so it's easy to find.  when it works,
// seperate it out.
//
// KEY: document why you are doing what you are doing.
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
//  **** put page numbers for any device-specific things you do ***
// 
// also: a sentence or two will go a long way in a year when you want 
// to re-use the code.
#include "rpi.h"
#include "i2c.h"
#include "lsm6ds33.h"
#include <limits.h>

/**********************************************************************
 * some helpers
 */

enum { VAL_WHO_AM_I      = 0x69, };

// read register <reg> from i2c device <addr>
uint8_t imu_rd(uint8_t addr, uint8_t reg) {
    i2c_write(addr, &reg, 1);
        
    uint8_t v;
    i2c_read(addr,  &v, 1);
    return v;
}

// write register <reg> with value <v> 
void imu_wr(uint8_t addr, uint8_t reg, uint8_t v) {
    uint8_t data[2];
    data[0] = reg;
    data[1] = v;
    i2c_write(addr, data, 2);
    // printk("writeReg: %x=%x\n", reg, v);
}

// <base_reg> = lowest reg in sequence. --- hw will auto-increment 
// if you set IF_INC during initialization.
int imu_rd_n(uint8_t addr, uint8_t base_reg, uint8_t *v, uint32_t n) {
        i2c_write(addr, (void*) &base_reg, 1);
        return i2c_read(addr, v, n);
}

/**********************************************************************
 * simple accel setup and use
 */

// returns the raw value from the sensor.
static short mg_raw(uint8_t hi, uint8_t lo) {
    return (hi << 8) | lo;
}

// returns milligauss, integer
static int mg_scaled(int v, int mg_scale) {
    if (mg_scale > 16)
        panic("accel: mg_scale of %d is greater than 16\n", mg_scale);
    // `v` can be at most `SHORT_MAX` and at least `SHORT_MIN`
    // scale `v` so that it fits in the range [-2000*mg_scale, 2000*mg_scale]
    return (1000 * mg_scale * v) / SHORT_MAX;
}

static void test_mg(int expected, uint8_t h, uint8_t l, unsigned g) {
    int s_i = mg_scaled(mg_raw(h,l),g);
    printk("expect = %d, got %d\n", expected, s_i);
    assert(s_i == expected);
}

static imu_xyz_t xyz_mk(int x, int y, int z) {
    return (imu_xyz_t){.x = x, .y = y, .z = z};
}

// takes in raw data and scales it.
imu_xyz_t accel_scale(accel_t *h, imu_xyz_t xyz) {
    int g = h->g;
    int x = mg_scaled(xyz.x, h->g);
    int y = mg_scaled(xyz.y, h->g);
    int z = mg_scaled(xyz.z, h->g);
    return xyz_mk(x,y,z);
}

int accel_has_data(accel_t *h) {
    return (imu_rd(h->addr, STATUS_REG) >> XLDA) & 1;
}

// block until there is data and then return it (raw)
//
// p26 interprets the data.
// if high bit is set, then accel is negative.
//
// read them all at once for consistent
// readings using autoincrement.
// these are blocking.  perhaps make non-block?
// returns raw, unscaled values.
imu_xyz_t accel_rd(accel_t *h) {
    // not sure if we have to drain the queue if there are more readings?

    unsigned mg_scale = h->g;
    uint8_t addr = h->addr;

    // wait for new data to be available (appnote p.23)
    while (!accel_has_data(h));
    
    // TODO: use `imu_rdn` instead
    short x = mg_raw(imu_rd(addr, OUTX_H_XL), imu_rd(addr, OUTX_L_XL));
    short y = mg_raw(imu_rd(addr, OUTY_H_XL), imu_rd(addr, OUTY_L_XL));
    short z = mg_raw(imu_rd(addr, OUTZ_H_XL), imu_rd(addr, OUTZ_L_XL));

    return xyz_mk(x, y, z);
}

// first do the cookbook from the data sheet.
// make sure:
//  - you use BDU and auto-increment.
//  - you get reasonable results!
//
// note: the initial readings are garbage!  skip these (see the data sheet)
accel_t accel_init(uint8_t addr, lsm6ds33_g_t g, lsm6ds33_hz_t hz) {
    dev_barrier();

    // some sanity checking
    switch(hz) {
    // these only work for "low-power" i think (p10)
    case lsm6ds33_1660hz:
    case lsm6ds33_3330hz:
    case lsm6ds33_6660hz:
        panic("accel: hz setting of %x does not appear to work\n", hz);
    default:
        break;
    }
    if(!legal_G(g))
        panic("invalid G value: %x\n", g);
    if(hz > lsm6ds33_6660hz)
        panic("invalid hz: %x\n", hz);

    // see header: pull out the scale and the bit pattern.
    unsigned g_scale = g >> 16;
    unsigned g_bits = g&0xff;
    assert(legal_g_bits(g_bits));
   
    accel_t accel = { .addr = addr, .hz = hz, .g = g_scale };

    // start-up sequence (appnote p.23)

    // configure the accelerometer
    imu_wr(accel.addr, CTRL9_XL, 0x38);     // enable acc X, Y, Z axes
    imu_wr(accel.addr, CTRL1_XL, (hz << 4) | (g_scale << 2));     // acc = 416Hz (high-performance mode)
    imu_wr(accel.addr, INT1_CTRL, 0x1);     // acc data ready interrupt on INT1
    imu_wr(accel.addr, CTRL3_C, 1 << BDU);  // enable block update 

    // TODO: discard samples 
    
    // wait for device to boot
    delay_ms(20);

    dev_barrier();

    return accel;
}


void do_accel_test(void) {
    // initialize accel.
    accel_t h = accel_init(lsm6ds33_default_addr, lsm6ds33_2g, lsm6ds33_416hz);

    int x,y,z;

    // p 26 of application note.
    test_mg(0, 0x00, 0x00, 2);
    test_mg(350, 0x16, 0x69, 2);
    test_mg(1000, 0x40, 0x09, 2);
    test_mg(-350, 0xe9, 0x97, 2);
    test_mg(-1000, 0xbf, 0xf7, 2);

    for(int i = 0; i < 10; i++) {
        imu_xyz_t v = accel_rd(&h);
        printk("accel raw values: x=%d,y=%d,z=%d\n", v.x,v.y,v.z);

        v = accel_scale(&h, v);
        printk("accel scaled values in mg: x=%d,y=%d,z=%d\n", v.x,v.y,v.z);

        delay_ms(500);
    }
}

/**********************************************************************
 * trivial driver.
 */
void notmain(void) {
    uart_init();

    delay_ms(100);   // allow time for device to boot up.
    i2c_init();
    delay_ms(100);   // allow time to settle after init.


    uint8_t dev_addr = lsm6ds33_default_addr;
    uint8_t v = imu_rd(dev_addr, WHO_AM_I);
    if(v != VAL_WHO_AM_I)
        panic("Initial probe failed: expected %x, got %x\n", VAL_WHO_AM_I, v);
    else
        printk("SUCCESS: lsm acknowledged our ping!!\n");

    do_accel_test();
    clean_reboot();
}
