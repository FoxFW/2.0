/*
    SK6805 FlipperZero driver
    Copyright (C) 2022-2023 Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifdef __cplusplus
extern "C" {
#endif

#ifndef SK6805_H_
#define SK6805_H_

#include <furi.h>

void SK6805_init(void);

uint8_t SK6805_get_led_count(void);

void SK6805_set_led_color(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);

void SK6805_update(void);

#endif

#ifdef __cplusplus
}
#endif
