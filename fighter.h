#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <lcd.h>
#include "lcd_model.h"
#include "cab202_adc.h"

#include "general.h"

void draw_fighter(void);
void draw_turret(void);
void update_plasma_array(int index);
void set_turret_angle(void);
void fire_plasma(double time);
void move_plasma(void);
int get_current_plasma();
int get_px(int index);
int get_py(int index);
int get_plasma_size();
char *get_plasma();
int get_turret_angle();
void set_current_plasma(int num);
void overwrite_turret_angle(int angle);
int get_fighter_x();
void set_fighter_x(int x);
int get_fighter_size(char wh);
void change_fighter_direction(char dir);
void move_fighter();
void setup_fighter();
void set_last_fire(double last);

