#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#include "eria.h"
#include "term.h"

#define C_QUIT      ((Color) { 240, 60,  60  })
#define C_JOIN      ((Color) { 60,  240, 60  })
#define C_MISC      ((Color) { 233, 45,  250 })
#define C_JOIN_TEXT ((Color) { 170, 227, 120 })
#define C_QUIT_TEXT ((Color) { 224, 141, 145 })

void
ui_draw(Eria *state);

void
ui_init(Eria *state);

Color
ui_nick_color(char const *nick);

void
ui_cleanup(void);

#endif
