#include <lcom/lcf.h>

#include <lcom/lab3.h>

#include <stdbool.h>
#include <stdint.h>
#include "keyboard.h"
#include "i8042.h"

extern int counter;

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need it]
  lcf_trace_calls("/home/lcom/labs/lab3/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab3/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

int(kbd_test_scan)() {
    int ipc_status;
    message msg;
    int r;
    int size = 1;

    uint8_t irq_set = 0;

    if (keyboard_subscribe_int(&irq_set))
        return 1;

    scancode = 0;

    while(scancode != ESC_MAKE_CODE) { /* You may want to use a different condition */
        /* Get a request message. */
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }

        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & irq_set) { /* subscribed interrupt */
                        kbc_ih();

                        if(status & KBC_PARITY_ERR || status & KBC_TIMEOUT_ERR)
                            continue;

                        if (scancode == MULTIBYTE_CODE){
                            size++;
                            continue;
                        }

                        uint8_t bytes[size];

                        for (int i = 0; i < size; i++){
                            bytes[i] = MULTIBYTE_CODE;
                        }

                        bytes[size-1] = scancode;

                        kbd_print_scancode(scancode & BREAK_CODE_BIT ? false: true, size, bytes);

                        size = 1;
                    }
                    break;
            }
        }
    }

    if(keyboard_unsubscribe_int())
        return 1;

    return 0;
}

int(kbd_test_poll)() {
    int size = 1;
    scancode = 0;

    if (enable_kbc_int())
        return 1;

    while(scancode != ESC_MAKE_CODE) {
        kbc_poll();

        if(status & KBC_PARITY_ERR || status & KBC_TIMEOUT_ERR) continue;

        if (scancode == MULTIBYTE_CODE){
            size++;
            continue;
        }

        uint8_t bytes[size];

        for (int i = 0; i < size; i++){
            bytes[i] = MULTIBYTE_CODE;
        }
        bytes[size-1] = scancode;

        kbd_print_scancode(scancode & BREAK_CODE_BIT ? false: true, size, &scancode);

        size = 1;
    }

    if (disable_kbc_int())
        return 1;

    return 0;
}

int(kbd_test_timed_scan)(uint8_t n) {
    int ipc_status;
    message msg;
    int r;
    int size = 1;
    counter = 0;
    scancode = 0;

    uint8_t irq_set_timer = 0;
    uint8_t irq_set_keyboard = 1;

    if (timer_subscribe_int(&irq_set_timer)) return 1;
    if (keyboard_subscribe_int(&irq_set_keyboard)) return 1;
    if (timer_set_frequency(0, 60)) return 1;

    while(scancode != ESC_MAKE_CODE && counter < n*60) { /* You may want to use a different condition */
        /* Get a request message. */
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }

        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & irq_set_keyboard) { /* subscribed interrupt */
                        kbc_ih();
                        counter = 0;

                        if(status & KBC_PARITY_ERR || status & KBC_TIMEOUT_ERR)
                            continue;

                        if (scancode == MULTIBYTE_CODE){
                            size++;
                            continue;
                        }

                        uint8_t bytes[size];

                        for (int i = 0; i < size; i++){
                            bytes[i] = MULTIBYTE_CODE;
                        }

                        bytes[size-1] = scancode;

                        kbd_print_scancode(scancode & BREAK_CODE_BIT ? false: true, size, bytes);

                        size = 1;
                    }
                    if (msg.m_notify.interrupts & irq_set_timer) { /* subscribed interrupt */
                        timer_int_handler();
                    }
                    break;
            }
        }
    }

    if (timer_unsubscribe_int()) return 1;
    if (keyboard_unsubscribe_int()) return 1;

    return 0;
}
