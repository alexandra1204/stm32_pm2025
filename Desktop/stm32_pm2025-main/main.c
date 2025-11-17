#include <stdint.h>
#include <stddef.h>
#include <stm32f10x.h>

static void delay(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; i++) {
        __NOP();
    }
}

#define OLED_PORT GPIOA
#define OLED_CS_PIN 4U
#define OLED_DC_PIN 1U
#define OLED_RES_PIN 0U

static void gpio_set_pin(GPIO_TypeDef *port, uint32_t pin) {
    port->BSRR = (1U << pin);
}

static void gpio_reset_pin(GPIO_TypeDef *port, uint32_t pin) {
    port->BSRR = (1U << (pin + 16U));
}

void SPI1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE5 | GPIO_CRL_CNF5 |
                    GPIO_CRL_MODE7 | GPIO_CRL_CNF7 |
                    GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
                    GPIO_CRL_MODE4 | GPIO_CRL_CNF4 |
                    GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
                    GPIO_CRL_MODE0 | GPIO_CRL_CNF0);

    GPIOA->CRL |= (GPIO_CRL_MODE5_1 | GPIO_CRL_MODE5_0 | GPIO_CRL_CNF5_1); // PA5 - SCK AF PP
    GPIOA->CRL |= (GPIO_CRL_MODE7_1 | GPIO_CRL_MODE7_0 | GPIO_CRL_CNF7_1); // PA7 - MOSI AF PP
    GPIOA->CRL |= GPIO_CRL_CNF6_0;                                         // PA6 - MISO floating input

    GPIOA->CRL |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE4_0); // PA4 - CS output push-pull
    GPIOA->CRL |= (GPIO_CRL_MODE1_1 | GPIO_CRL_MODE1_0); // PA1 - DC output push-pull
    GPIOA->CRL |= (GPIO_CRL_MODE0_1 | GPIO_CRL_MODE0_0); // PA0 - RES output push-pull

    gpio_set_pin(OLED_PORT, OLED_CS_PIN);
    gpio_set_pin(OLED_PORT, OLED_DC_PIN);
    gpio_set_pin(OLED_PORT, OLED_RES_PIN);

    SPI1->CR1 = 0;
    SPI1->CR1 |= SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR1 |= SPI_CR1_CPOL | SPI_CR1_CPHA;
    SPI1->CR1 |= SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_BR_2; // fPCLK/256
    SPI1->CR1 |= SPI_CR1_SPE;
}

void SPI1_Write(uint8_t data) {
    while ((SPI1->SR & SPI_SR_TXE) == 0U) {
    }
    *(volatile uint8_t *)&SPI1->DR = data;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U) {
    }
    (void)SPI1->DR;
    while ((SPI1->SR & SPI_SR_BSY) != 0U) {
    }
}

uint8_t SPI1_Read(void) {
    while ((SPI1->SR & SPI_SR_TXE) == 0U) {
    }
    *(volatile uint8_t *)&SPI1->DR = 0xFFU;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U) {
    }
    uint8_t data = (uint8_t)SPI1->DR;
    while ((SPI1->SR & SPI_SR_BSY) != 0U) {
    }
    return data;
}

static void display_select(void) {
    gpio_reset_pin(OLED_PORT, OLED_CS_PIN);
}

static void display_deselect(void) {
    gpio_set_pin(OLED_PORT, OLED_CS_PIN);
}

static void display_set_data_mode(void) {
    gpio_set_pin(OLED_PORT, OLED_DC_PIN);
}

static void display_set_command_mode(void) {
    gpio_reset_pin(OLED_PORT, OLED_DC_PIN);
}

static void display_cmd(uint8_t cmd) {
    display_select();
    display_set_command_mode();
    SPI1_Write(cmd);
    display_deselect();
}

static void display_send_data_start(void) {
    display_select();
    display_set_data_mode();
}

static void display_send_data_end(void) {
    display_deselect();
}

static void display_reset(void) {
    gpio_set_pin(OLED_PORT, OLED_CS_PIN);
    gpio_set_pin(OLED_PORT, OLED_DC_PIN);
    gpio_reset_pin(OLED_PORT, OLED_RES_PIN);
    delay(50000U);
    gpio_set_pin(OLED_PORT, OLED_RES_PIN);
    delay(50000U);
}

static void display_init(void) {
    SPI1_Init();
    display_reset();

    static const uint8_t init_sequence[] = {
        0xAE, // Display OFF
        0xD5, 0x80, // Set display clock divide ratio
        0xA8, 0x3F, // Multiplex ratio
        0xD3, 0x00, // Display offset
        0x40,       // Display start line
        0x8D, 0x14, // Charge pump enable
        0x20, 0x00, // Horizontal addressing mode
        0xA1,       // Segment remap
        0xC8,       // COM output scan direction
        0xDA, 0x12, // COM pins hardware configuration
        0x81, 0xCF, // Contrast control
        0xD9, 0xF1, // Pre-charge period
        0xDB, 0x40, // VCOMH deselect level
        0xA4,       // Entire display on (resume)
        0xA6        // Normal display (not inverted)
    };

    for (size_t i = 0; i < sizeof(init_sequence); ++i) {
        display_cmd(init_sequence[i]);
    }

    display_cmd(0xAF); // Display ON
}

static void display_set_full_window(void) {
    display_cmd(0x21); // Set column address
    display_cmd(0x00);
    display_cmd(0x7F);
    display_cmd(0x22); // Set page address
    display_cmd(0x00);
    display_cmd(0x07);
}

static void display_draw_chessboard(void) {
    display_set_full_window();

    display_send_data_start();
    for (uint8_t page = 0; page < 8U; ++page) {
        for (uint8_t column = 0; column < 128U; ++column) {
            uint8_t block = (column >> 3) & 0x01U;
            uint8_t pattern = ((page & 0x01U) ^ block) ? 0xFFU : 0x00U;
            SPI1_Write(pattern);
        }
    }
    display_send_data_end();
}

int __attribute__((noreturn)) main(void) {
    display_init();
    display_draw_chessboard();

    while (1) {
        __NOP();
    }
}
