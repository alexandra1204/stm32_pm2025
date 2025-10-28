#include <stdint.h>
#include <stm32f10x.h>

void delay(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; i++) {
        __NOP();
    }
}

static uint32_t blink_delay_from_exponent(uint32_t baseDelay, int8_t exponent) {
    if (exponent >= 0) {
        uint32_t divisor = 1U << exponent;
        uint32_t result = baseDelay / divisor;
        return (result == 0U) ? 1U : result;
    } else {
        uint32_t multiplier = 1U << (-exponent);
        return baseDelay * multiplier;
    }
}

int __attribute((noreturn)) main(void) {
    // Enable clocks for AFIO, GPIOC (LED) and GPIOA (buttons)
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // Configure PC13 as push-pull output
    GPIOC->CRH &= ~GPIO_CRH_CNF13;
    GPIOC->CRH |= GPIO_CRH_MODE13_0;
    GPIOC->BSRR = GPIO_BSRR_BS13; // LED off initially (depends on board wiring)

    // Configure PA0 and PA1 as inputs with pull-up (buttons to ground)
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOA->CRL |= GPIO_CRL_CNF0_1;
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOA->CRL |= GPIO_CRL_CNF1_1;
    GPIOA->ODR |= GPIO_ODR_ODR0 | GPIO_ODR_ODR1;

    const uint32_t baseDelay = 500000U;
    const int8_t maxExponent = 6;  // 2^6 = 64 Hz
    const int8_t minExponent = -6; // 2^-6 = 1/64 Hz
    int8_t freqExponent = 0;

    uint32_t blinkDelay = blink_delay_from_exponent(baseDelay, freqExponent);
    uint8_t prevButtonA = 1U;
    uint8_t prevButtonB = 1U;

    while (1) {
        GPIOC->ODR ^= GPIO_ODR_ODR13;
        delay(blinkDelay);

        uint32_t idr = GPIOA->IDR;
        uint8_t buttonA = (idr & GPIO_IDR_IDR0) ? 1U : 0U;
        uint8_t buttonB = (idr & GPIO_IDR_IDR1) ? 1U : 0U;

        if (!buttonA && prevButtonA) {
            delay(20000U);
            if (!(GPIOA->IDR & GPIO_IDR_IDR0) && freqExponent < maxExponent) {
                freqExponent++;
                blinkDelay = blink_delay_from_exponent(baseDelay, freqExponent);
            }
        }

        if (!buttonB && prevButtonB) {
            delay(20000U);
            if (!(GPIOA->IDR & GPIO_IDR_IDR1) && freqExponent > minExponent) {
                freqExponent--;
                blinkDelay = blink_delay_from_exponent(baseDelay, freqExponent);
            }
        }

        prevButtonA = buttonA;
        prevButtonB = buttonB;
    }
}
