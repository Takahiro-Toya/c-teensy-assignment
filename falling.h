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

void update_asteroid_array(int index);
void update_boulder_array(int index);
void update_fragment_array(int index);
bool check_asteroid_overwrap(int x, int y);
void flush_led(int num);
void spawn_asteroid(void);
int move_asteroid(double elapsed, int shiled, double speed);
int move_boulder(int shield, double speed);
int move_fragment(int shield, double speed);
void hit_asteroid(int index);
void hit_boulder(int index);
void hit_fragment(int index);
int get_current_asteroid();
int get_current_boulder();
int get_current_fragment();
int get_asteroid_size();
int get_boulder_size();
int get_fragment_size();
char *get_asteroid();
char *get_boulder();
char *get_fragment();
int get_axy(char xy, int index);
int get_bxy(char xy, int index);
int get_fxy(char xy, int index);
void set_current_object(char abf, int num);
int get_object_score(char abf);
void place_asteroid(int x, int y);
void place_boulder(int x, int y);
void place_fragment(int x, int y);
void draw_asteroid(void);
void draw_boulder(void);
void draw_fragment(void);

