#include <stdint.h>

extern uint8_t payload_data;
extern const uint64_t payload_size;

static const uint64_t PERIPHERAL_BASE = 0xfe000000;
static const uint64_t GPFSEL0 = PERIPHERAL_BASE + 0x200000;
static const uint64_t GPSET0 = PERIPHERAL_BASE + 0x20001c;
static const uint64_t GPCLR0 = PERIPHERAL_BASE + 0x200028;
static const uint64_t GPPUPPDN0 = PERIPHERAL_BASE + 0x2000e4;

static const unsigned int GPIO_MAX_PIN = 53;
static const unsigned int GPIO_FUNCTION_ALT5 = 2;

static const unsigned int Pull_None = 0;

static const uint64_t AUX_BASE = PERIPHERAL_BASE + 0x215000;
static const uint64_t AUX_ENABLES = AUX_BASE + 4;
static const uint64_t AUX_MU_IO_REG = AUX_BASE + 64;
static const uint64_t AUX_MU_IER_REG = AUX_BASE + 68;
static const uint64_t AUX_MU_IIR_REG = AUX_BASE + 72;
static const uint64_t AUX_MU_LCR_REG = AUX_BASE + 76;
static const uint64_t AUX_MU_MCR_REG = AUX_BASE + 80;
static const uint64_t AUX_MU_LSR_REG = AUX_BASE + 84;
static const uint64_t AUX_MU_CNTL_REG = AUX_BASE + 96;
static const uint64_t AUX_MU_BAUD_REG = AUX_BASE + 104;
static const uint64_t AUX_UART_CLOCK = 500000000;

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

static void mmio_write(uint64_t reg, unsigned int val) {
    *(volatile unsigned int *)reg = val;
}

static unsigned int mmio_read(uint64_t reg) {
    return *(volatile unsigned int *)reg;
}

static void gpio_call(unsigned int pin_number, unsigned int value, unsigned int base,
                      unsigned int field_size, unsigned int field_max) {
    unsigned int field_mask = (1 << field_size) - 1;

    if (pin_number > field_max)
        return;

    if (value > field_mask)
        return;

    unsigned int num_fields = 32 / field_size;
    unsigned int reg = base + ((pin_number / num_fields) * 4);
    unsigned int shift = (pin_number % num_fields) * field_size;

    unsigned int curval = mmio_read(reg);
    curval &= ~(field_mask << shift);
    curval |= value << shift;
    mmio_write(reg, curval);
}

static void gpio_pull(unsigned int pin_number, unsigned int value) {
    return gpio_call(pin_number, value, GPPUPPDN0, 2, GPIO_MAX_PIN);
}

static void gpio_function(unsigned int pin_number, unsigned int value) {
    return gpio_call(pin_number, value, GPFSEL0, 3, GPIO_MAX_PIN);
}

static void gpio_use_as_alt_5(unsigned int pin_number) {
    gpio_pull(pin_number, Pull_None);
    gpio_function(pin_number, GPIO_FUNCTION_ALT5);
}

static void uart_init() {
    mmio_write(AUX_ENABLES, 1); // enable UART1
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3); // 8 bits
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xc6); // disable interrupts
    mmio_write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    gpio_use_as_alt_5(14);
    gpio_use_as_alt_5(15);
    mmio_write(AUX_MU_CNTL_REG, 3); // enable RX/TX
}

static unsigned int uart_isWriteByteReady() {
    return mmio_read(AUX_MU_LSR_REG) & 0x20;
}

static void uart_writeByteBlockingActual(char ch) {
    while (!uart_isWriteByteReady()) {
    }

    mmio_write(AUX_MU_IO_REG, (unsigned int)ch);
}

static void uart_writeText(const char *buffer) {
    while (*buffer) {
       if (*buffer == '\n')
           uart_writeByteBlockingActual('\r');

       uart_writeByteBlockingActual(*buffer);
       buffer++;
    }
}

extern "C"
void start(uint64_t dtb, uint64_t image_base) {
    uart_init();
    uart_writeText("Hello world!\n");

    while (true) {
    }
}
