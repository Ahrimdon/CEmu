#ifndef H_USB
#define H_USB

#ifdef __cplusplus
extern "C" {
#endif

#include "port.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* Standard USB state */
typedef struct usb_state {
    uint8_t dummy;
} usb_state_t;

/* Global GPT state */
extern usb_state_t usb;

/* Available Functions */
eZ80portrange_t init_usb(void);
void usb_reset(void);

/* Save/Restore */
bool usb_restore(FILE *image);
bool usb_save(FILE *image);

#ifdef __cplusplus
}
#endif

#endif
