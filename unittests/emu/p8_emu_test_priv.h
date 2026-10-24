/* Auto-generated extern declarations for unit tests */

#ifndef UNITTEST_TEST_IMPL_H
#define UNITTEST_TEST_IMPL_H

extern int color_index(uint8_t c);
extern int p8_init_lcd(void);
extern int p8_main_loop(void);
extern void p8_reset_cart_and_screen(void);
extern jmp_buf jmpbuf_restart;
extern bool restart;
extern bool cart_running;
extern bool lua_running;
extern bool quit_requested;
extern bool in_main_loop;
extern bool skip_main_loop_if_no_callbacks;
extern FILE *cartdata;
extern bool cartdata_needs_flush;
extern int m_initialized;
extern int vfrontreq;
extern void p8_sleep(unsigned ms);
extern int p8_init_lcd(void);
extern void screen_transform_pixel(uint8_t mode, int ox, int oy, int *sx, int *sy);
extern uint8_t high_color_resolve(uint8_t hc_mode, uint8_t pix_index, int sx, int sy);
extern int dialog_showing_clear_n;
extern void p8_post_flip(void);
extern int p8_main_loop();
extern void p8_common_reset_cart();
extern void p8_reset_cart_and_screen(void);

#endif /* UNITTEST_TEST_IMPL_H */
