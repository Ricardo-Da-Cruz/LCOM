// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <stdint.h>
#include <stdio.h>
#include <lcom/lab4.h>

#include "mouse.h"
#include "timer.h"
#include "i8042.h"

// Any header files included below this line should have been created by you

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need/ it]
  lcf_trace_calls("/home/lcom/labs/lab4/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab4/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}


int (mouse_test_packet)(uint32_t cnt) {
    int ipc_status;
    message msg;
    int r;

    uint8_t mouse_irq_set = 2;

    int p = 0;
    struct packet packet;
    
    if (write_mouse_cmd(MOUSE_ENABLE_DATA_REPORTING))return 1;
    if (mouse_subscribe_int(&mouse_irq_set))return 1;

    while(cnt > 0) { /* You may want to use a different condition */
        /* Get a request message. */
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }

        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & mouse_irq_set) { /* subscribed interrupt */
                        mouse_ih();

                        if (mouse_status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR))continue;
                        if ((mouse_status & KBC_AUX) == 0)continue;
                        if (p == 0 && (mouse_scancode & MOUSE_FIRST_BYTE_BIT) == 0)continue;
                        if (p == 2)cnt--;

                        construct_packet(&packet, p, mouse_scancode);

                        p = (p+1)%3;
                    }
                    break;
            }
        }
    }

    if (mouse_unsubscribe_int())return 1;
    if (write_mouse_cmd(MOUSE_DISABLE_DATA_REPORTING))return 1;

    return 0;
}

int (mouse_test_async)(uint8_t idle_time) {
    int ipc_status;
    message msg;
    int r;

    uint8_t mouse_irq_set = 2;
    uint8_t timer_irq_set;

    int p = 0;
    int counter = 0;
    int freq = sys_hz();
    struct packet packet;


    if (write_mouse_cmd(MOUSE_ENABLE_DATA_REPORTING))return 1;
    if (timer_subscribe_int(&timer_irq_set))return 1;
    if (mouse_subscribe_int(&mouse_irq_set))return 1;

    while(counter < idle_time * freq) { /* You may want to use a different condition */
        /* Get a request message. */
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }

        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & mouse_irq_set) { /* subscribed interrupt */
                        mouse_ih();

                        if (mouse_status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR))continue;
                        if ((mouse_status & KBC_AUX) == 0)continue;
                        if (p == 0 && (mouse_scancode & MOUSE_FIRST_BYTE_BIT) == 0)continue;

                        construct_packet(&packet, p, mouse_scancode);

                        p = (p+1)%3;
                        counter = 0;
                    }
                    if (msg.m_notify.interrupts & timer_irq_set) { /* subscribed interrupt */
                        timer_int_handler();
                        counter++;
                    }
                    break;
            }
        }
    }

    if (mouse_unsubscribe_int())return 1;
    if (timer_unsubscribe_int())return 1;
    if (write_mouse_cmd(MOUSE_DISABLE_DATA_REPORTING))return 1;

    return 0;
}

int (mouse_test_gesture)(uint8_t x_len, uint8_t tolerance) {
    int ipc_status;
    message msg;
    int r;
    
    uint8_t mouse_irq_set = 2;

    int p = 0;
    struct packet packet;
    int state = 0;
    
    if (write_mouse_cmd(MOUSE_ENABLE_DATA_REPORTING))return 1;
    if (mouse_subscribe_int(&mouse_irq_set))return 1;

    int x_pos = 0;
    int y_pos = 0;
    int x_line_start = 0;
    int y_line_start = 0;
    int x_line_end = 0;
    int y_line_end = 0;

    while(state != 4) { /* You may want to use a different condition */
        /* Get a request message. */
        if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
            printf("driver_receive failed with: %d", r);
            continue;
        }

        if (is_ipc_notify(ipc_status)) { /* received notification */
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE: /* hardware interrupt notification */
                    if (msg.m_notify.interrupts & mouse_irq_set) { /* subscribed interrupt */
                        mouse_ih();

                        if (mouse_status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR))continue;
                        if ((mouse_status & KBC_AUX) == 0)continue;
                        if (p == 0 && (mouse_scancode & MOUSE_FIRST_BYTE_BIT) == 0)continue;

                        construct_packet(&packet, p, mouse_scancode);

                        if (p == 2){
                            x_pos += packet.delta_x;
                            y_pos += packet.delta_y;
                            switch(state){
                                case 0:
                                    if (packet.lb == 1){
                                        state = 1;
                                        x_line_start = x_pos;
                                        y_line_start = y_pos;
                                    }
                                    break;
                                case 1: 
                                    if (packet.lb == 0){
                                        int line_x = x_pos - x_line_start;
                                        int line_y = y_pos - y_line_start;
                                        // the line should go up and to the right
                                        // line_x should be positive and line_y should be positive
                                        // slope should be more than 1 meaning that abs(line_x) < abs(line_y)
                                        if ((line_x <= x_len + tolerance && line_x >= x_len - tolerance) && line_x <= line_y){
                                            state = 2;
                                            x_line_end = x_pos;
                                            y_line_end = y_pos;
                                        }else{
                                            state = 0;   
                                        }
                                    }
                                    break;
                                case 2:
                                    if (packet.rb == 1){
                                        // the mouse should start in the same position as the end of the line
                                        if ((x_pos + tolerance >= x_line_end && x_pos - tolerance <= x_line_end) && (y_pos + tolerance >= y_line_end && y_pos - tolerance <= y_line_end)){
                                            state = 3;
                                            x_line_start = x_pos;
                                            y_line_start = y_pos;
                                        }else{
                                            state = 0;
                                        }
                                    }
                                    break;
                                case 3:
                                    if (packet.rb == 0){
                                        int line_x = x_pos - x_line_start;
                                        int line_y = y_pos - y_line_start;
                                        // the line should go down and to the right
                                        // line_x should be positive and line_y should be negative
                                        // slope should be more than 1 meaning that abs(line_x) < abs(line_y)
                                        if ((line_x <= x_len + tolerance && line_x >= x_len - tolerance) && line_x <= -line_y){
                                            state = 4;
                                        }else{
                                            state = 0;
                                        }
                                    }
                                    break;
                            }
                        }
                        p = (p+1)%3;
                    }


                        
                break;
                        
            }
        }
    }

    if (mouse_unsubscribe_int())return 1;
    if (write_mouse_cmd(MOUSE_DISABLE_DATA_REPORTING))return 1;

    return 0;
}
