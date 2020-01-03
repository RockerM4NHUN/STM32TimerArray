// Code from
// https://docs.platformio.org/en/latest/tutorials/ststm32/stm32cube_debugging_unit_testing.html#tutorial-stm32cube-debugging-unit-testing

#ifndef UNITTEST_TRANSPORT_H
#define UNITTEST_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

void unittest_uart_begin();
void unittest_uart_putchar(char c);
void unittest_uart_flush();
void unittest_uart_end();

#ifdef __cplusplus
}
#endif

#endif // UNITTEST_TRANSPORT_H