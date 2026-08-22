#ifndef P8_AUDIO_VERIATED_H
#define P8_AUDIO_VERIATED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the verilated audio subsystem */
void p8audio_verilated_init(void);

/* MMIO write - called from CPU thread */
void p8audio_verilated_mmio_write(uint8_t byte_addr, uint16_t data);

/* MMIO read - called from CPU thread */
uint16_t p8audio_verilated_mmio_read(uint8_t byte_offset);

/* Close the verilated audio subsystem */
void p8audio_verilated_close(void);

void audio_callback_verilated(int16_t *buf, int samples);

#ifdef __cplusplus
}
#endif

#endif /* P8_AUDIO_VERIATED_H */
