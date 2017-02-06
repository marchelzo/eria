#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#include "eria.h"
#include "term.h"

void
ui_draw(Eria *state);

void
ui_init(Eria *state);

Color
ui_nick_color(char const *nick);

#endif
