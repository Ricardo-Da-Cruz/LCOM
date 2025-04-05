// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>
#include <lcom/lab5.h>
#include <stdint.h>
#include <stdio.h>
#include "gpu.h"
#include "time.h"
#include "keyboard.h"
#include "i8042.h"

// Any header files included below this line should have been created by you

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need it]
  lcf_trace_calls("/home/lcom/labs/lab5/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab5/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

int(video_test_init)(uint16_t mode, uint8_t delay) {
  
  uint8_t irq_set_timer = 0;
  int ipc_status;
  message msg;
  int r;
  delay *= 60;

  if (timer_set_frequency(0, 60)){
    printf("timer_set_frequency failed");
    return 1;
  }

  if (timer_subscribe_int(&irq_set_timer)){
    printf("timer_subscribe_int failed");
    return 1;
  }

  if(set_graphics_mode(mode)){
    printf("set_graphics_mode failed");
    return 1;
  }

  while (delay > 0){
    if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
      printf("driver_receive failed with: %d", r);
      continue;
    }

    if (is_ipc_notify(ipc_status)) { /* received notification */
      switch (_ENDPOINT_P(msg.m_source)) {
          case HARDWARE: /* hardware interrupt notification */				
              if (msg.m_notify.interrupts & irq_set_timer) { /* subscribed interrupt */
                  delay--;
                  printf("delay: %d\n", delay);
              }
              break;
          default:
              break; /* no other notifications expected: do nothing */	
      }
    }
  }
  
  if(exit_graphics_mode()){
    printf("exit_graphics_mode failed");
    return 1;
  }

  if (timer_unsubscribe_int()){
    printf("timer_unsubscribe_int failed");
    return 1;
  }

  return 0;
}

int(video_test_rectangle)(uint16_t mode, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
  uint8_t irq_set_keyboard = 1;
  int ipc_status;
  message msg;
  int r;
  
  if(keyboard_subscribe_int(&irq_set_keyboard)){
    printf("keyboard_subscribe_int failed\n");
    return 1;
  }

  if(set_graphics_mode(mode)){
    printf("set_graphics_mode failed\n");
    return 1;
  }

  if(vg_draw_rectangle(x, y, width, height, color)){
    printf("vg_draw_rectangle failed\n");
    return 1;
  }

  printf("waiting for ESC key\n");

  scancode = 0;

  while (scancode != ESC_MAKE_CODE){
    if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
      printf("driver_receive failed with: %d1n", r);
      continue;
    }

    if (is_ipc_notify(ipc_status)) { /* received notification */
      switch (_ENDPOINT_P(msg.m_source)) {
          case HARDWARE: /* hardware interrupt notification */				
              if (msg.m_notify.interrupts & irq_set_keyboard) { /* subscribed interrupt */
                  kbc_ih();

                  if(!verify_status())
                    scancode = 0;

                  printf("scancode: %d\n", scancode);
              }
              break;
          default:
              break; /* no other notifications expected: do nothing */	
      }
    }
  }

  printf("unsubscribing from keyboard\n");

  if(keyboard_unsubscribe_int()){
    printf("keyboard_unsubscribe_int failed\n");
    return 1;
  }

  printf("exiting graphics mode\n");

  if(exit_graphics_mode()){
    printf("exit_graphics_mode failed\n");
    return 1;
  }

  return 0;
}

int(video_test_pattern)(uint16_t mode, uint8_t no_rectangles, uint32_t first, uint8_t step) {
  uint8_t irq_set_keyboard = 1;
  int ipc_status;
  message msg;
  int r;
  
  if(keyboard_subscribe_int(&irq_set_keyboard)){
    printf("keyboard_subscribe_int failed\n");
    return 1;
  }

  if(set_graphics_mode(mode)){
    printf("set_graphics_mode failed\n");
    return 1;
  }

  uint32_t first_red = (first >> (vmi.RedFieldPosition)) & ((1 << vmi.RedMaskSize) - 1);
  uint32_t first_green = (first >> (vmi.GreenFieldPosition)) & ((1 << vmi.GreenMaskSize) - 1);
  uint32_t first_blue = (first >> (vmi.BlueFieldPosition)) & ((1 << vmi.BlueMaskSize) - 1);

  int len = vmi.YResolution / no_rectangles;
  
  for(int col = 0; col < no_rectangles; col++){
    for(int row = 0; row < no_rectangles; row++){
      uint32_t color = 0;

      if (mode == 0x105){
        color = (first + (row * no_rectangles + col) * step) % (1 << vmi.BitsPerPixel);
        printf("color: %08x\n", color);
      }else{
        color += ((first_red + col * step) % (1 << vmi.RedMaskSize)) << vmi.RedFieldPosition;
        printf("color: %08x\n", color);
        color += ((first_green + row * step) % (1 << vmi.GreenMaskSize)) << vmi.GreenFieldPosition;
        printf("color: %08x\n", color);
        color += (first_blue + (row + col) * step) % (1 << vmi.BlueMaskSize) << vmi.BlueFieldPosition;
        printf("color: %08x\n", color);
      }

      if(vg_draw_rectangle(col * len, row * len, len, len, color)){
        printf("vg_draw_rectangle failed\n");
        return 1;
      }

    }
  }

  printf("waiting for ESC key\n");

  scancode = 0;

  while (scancode != ESC_MAKE_CODE){
    if ( (r = driver_receive(ANY, &msg, &ipc_status)) != 0) { 
      printf("driver_receive failed with: %d1n", r);
      continue;
    }

    if (is_ipc_notify(ipc_status)) { /* received notification */
      switch (_ENDPOINT_P(msg.m_source)) {
          case HARDWARE: /* hardware interrupt notification */				
              if (msg.m_notify.interrupts & irq_set_keyboard) { /* subscribed interrupt */
                  kbc_ih();

                  if(!verify_status())
                    scancode = 0;

                  printf("scancode: %d\n", scancode);
              }
              break;
          default:
              break; /* no other notifications expected: do nothing */	
      }
    }
  }

  printf("unsubscribing from keyboard\n");

  if(keyboard_unsubscribe_int()){
    printf("keyboard_unsubscribe_int failed\n");
    return 1;
  }

  printf("exiting graphics mode\n");

  if(exit_graphics_mode()){
    printf("exit_graphics_mode failed\n");
    return 1;
  }

  return 0;
}

int(video_test_xpm)(xpm_map_t xpm, uint16_t x, uint16_t y) {
  /* To be completed */
  printf("%s(%8p, %u, %u): under construction\n", __func__, xpm, x, y);

  return 1;
}

int(video_test_move)(xpm_map_t xpm, uint16_t xi, uint16_t yi, uint16_t xf, uint16_t yf,
                     int16_t speed, uint8_t fr_rate) {
  /* To be completed */
  printf("%s(%8p, %u, %u, %u, %u, %d, %u): under construction\n",
         __func__, xpm, xi, yi, xf, yf, speed, fr_rate);

  return 1;
}

int(video_test_controller)() {
  /* To be completed */
  printf("%s(): under construction\n", __func__);

  return 1;
}
