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
#include "uart.h"
#include "usb_serial.h"
#include "cab202_adc.h"

#define FREQ     (8000000.0)
#define PRESCALE (1024.0)
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)
#define LIFE_MAX 5

#define MAX_PLASMA 30
#define MIN_PLASMA_INTERVAL 0.2
#define MAX_TURRET_ANGLE 60

#define ASTEROID_SIZE 7
#define BOULDER_SIZE 5
#define FRAGMENT_SIZE 3

#define MAX_ASTEROID 3
#define MAX_BOULDER 6
#define MAX_FRAGMENT 12

//----------------------------------------------------------------
//----------------------------------------------------------------
//----------------------------------------------------------------

// constant variables
const double pot_left_per_angle = 120.0 / (double)ADC_MAX;
const int plasma_size = 2;
const int fighter_width = 14;
const int fighter_height = 3;
const int turret_width = 4;
const int turret_height = 1;
const int asteroid_score = 1;
const int boulder_score = 2;
const int fragment_score = 4;

// space ship properties
int fighter_x;
int fd = 0; // -1, 0, 1
const int fighter_y = 45;

double px[MAX_PLASMA];
double py[MAX_PLASMA];
double pdx[MAX_PLASMA];
double pdy[MAX_PLASMA];
double turret_x[4];
double turret_y[4];

int turret_angle = 90; // 30 ~ 150

int current_plasma = 0;
double last_fire = 0.0;

char * turret =
"tttt"
;

char * fighter =
"ffffffffffffff"
" ffffffffffff "
"  ffffffffff  "
;

char * plasma =
"pp"
"pp"
;

// falling object properties
double ax[MAX_ASTEROID];
double ay[MAX_ASTEROID];

double bx[MAX_BOULDER];
double by[MAX_BOULDER];
double bdx[MAX_BOULDER];
double bdy[MAX_BOULDER];

double fx[MAX_FRAGMENT];
double fy[MAX_FRAGMENT];
double fdx[MAX_FRAGMENT];
double fdy[MAX_FRAGMENT];

int current_asteroid = 0;
int current_boulder = 0;
int current_fragment = 0;

char * asteroid =
" aaaaa "
"aaaaaaa"
"aaaaaaa"
"aaaaaaa"
" aaaaa "
"  aaa  "
"   a   "
;

char * boulder =
" bbb "
"bbbbb"
"bbbbb"
" bbb "
"  b  "
;

char * fragment =
" f "
"fff"
" f "
;

// game status and rule properties
char studentID[] = "n10056513";
char* statusM =
"%s \r\n\
Time: %02d:%02d \r\n\
Life: %d \r\n\
Score: %d \r\n\
Asteroid: %d \r\n\
Bouler: %d \r\n\
Fragment: %d \r\n\
Plasma: %d \r\n\
Turret Aim: %d \r\n\
Speed: %f \r\n\n";
char* helpM =
"a-ship left\r\n\
d-ship right\r\n\
w-fire plasma\r\n\
s-send status\r\n\
r-reset\r\n\
p-pause\r\n\
q-quit\r\n\
t-set aim\r\n\
m-set speed\r\n\
l-set life\r\n\
g-set score\r\n\
?-this menu\r\n\
h-move ship to coord\r\n\
j-place asteroid at coord\r\n\
k-place boulder at coord\r\n\
i-place fragment at coord\r\n";

bool paused = true;
bool inputting = false;
bool game_over = false;
bool initial = true;
bool quit = false;
int score = 0;
int life = LIFE_MAX;
bool status_on = false;

double speed = 1.0;
const int shield = 39;

double start_time;
double adjust = 0.0;
double elapsed = 0.0;
double before_pause = 0.0;

bool turretMode = false;
bool speedMode = false;

// some properties for input numbers
bool input_ready = false;
char input_command;
char input_mode = 'd'; // type of input (normal input or need additional support for coordinate input)
bool input_turret_minus = false;

int inputArray1[10];
int inputArray2[5];
int input_counter1 = 0;
int input_counter2 = 0;

int input1;
int input2;

volatile int overflow_counter = 0;

//----------------------------------------------------------------
//----------------------------------------------------------------
//----------------------------------------------------------------

/**
 * an overflow business
 */
ISR(TIMER0_OVF_vect) {
    overflow_counter ++;
}

/**
 * return current time elapsed since login (not elaped game time!)
 */
double get_time(){
    return (overflow_counter * 256.0 + TCNT0 ) * PRESCALE  / FREQ;
}

/**
 * used to send message to computer through usb
 */
void usb_serial_send(char * message) {
    usb_serial_write((uint8_t *) message, strlen(message));
}

/**
 * set up one of the adc property
 */
void set_duty_cycle(int duty_cycle) {
    TC4H = duty_cycle >> 8;
    OCR4A = duty_cycle & 0xff;
}

/**
 * set up Teensy's hard ware input
 */
void setup_input(void){
    SET_BIT(DDRB, 2);
    SET_BIT(DDRB, 3);
    
    // turn off LED0, LED1, and all other outputs connected to port B
    for (int i = 0; i < 8; i++){
        CLEAR_BIT(PORTB, i);
    }
    
    // enable input from left & right button
    CLEAR_BIT(DDRF, 5);
    CLEAR_BIT(DDRF, 6);
    
    // enable input from switches
    CLEAR_BIT(DDRB, 0); // center
    CLEAR_BIT(DDRB, 1); // left
    CLEAR_BIT(DDRB, 7); // down
    CLEAR_BIT(DDRD, 0); // right
    CLEAR_BIT(DDRD, 1); // up
}

/**
 * set up usb
 */
void setup_usb(void){
    usb_init();
    while ( !usb_configured() ) {
        // Block until USB is ready.
    }
}

/**
 * set up adc
 */
void setup_adc(void){
    adc_init();
    TC4H = OVERFLOW_TOP >> 8;
    OCR4C = OVERFLOW_TOP & 0xff;
    TCCR4A = BIT(COM4A1) | BIT(PWM4A);
    SET_BIT(DDRC, 7);
    TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
    TCCR4D = 0;
}

/**
 * enable timer
 */
void setup_timer(void){
    TCCR0A = 0;
    TCCR0B = 5;
    TIMSK0 = 1;
    sei();
}

/**
 * set up LCD
 */
void setup_lcd(void){
    lcd_init(LCD_DEFAULT_CONTRAST);
    lcd_clear();
    
}

/**
 * input, lcd, adc, timer, usb set up
 */
void setup(void){
    set_clock_speed(CPU_8MHz);
    setup_input();
    setup_lcd();
    setup_adc();
    setup_timer();
    setup_usb();
}

/**
 * Return minutes elapsed
 */
int get_minutes(){
    return elapsed / 60;
}

/**
 * Return seconds elapsed
 */
int get_seconds(){
    return (int)elapsed % 60;
}

/**
 * draw barrier
 */
void setupBarrier(void){
    draw_line(0, shield, LCD_X - 1, shield, FG_COLOUR);
}


//----------------------------------------------------------------
//----------------------------------------------------------------
//----------------------------------------------------------------


/**
 * draw pixels at specified place
 * Parameters:
 *      left, top -left and top coordinate of image to be drawn
 *      width, height -width and height of image
 *      bitmap[] -pixel image of object to be drawn
 *      space_is_transparent -true if you want to treat space as space
 */
void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent){
    
    for (int j=0; j<height; j++){
        for(int i=0; i<width; i++){
            if (bitmap[i + j * width] != ' '){
                draw_pixel(left + i, top + j, FG_COLOUR);
            } else if (space_is_transparent == false){
                draw_pixel(left + i, top + j, FG_COLOUR);
            } // end of if - else if
        } // end of for(i)
    } // end of for(j)
} // end of draw_pixels


/**
 * Return true if 2 objects specified by parameters collides each other
 * Parameters:
 *      x0, y0, w0, h0 -first image's x and y coordinate (top left) and width and height
 *      pixels0[] -first image's pixel image
 *      x1, y1, w1, h1 -second image's x and y coordinate (top left) and width and height
 *      pixels1[] -second image's pixel image
 */
bool pixel_collision(int x0, int y0, int w0, int h0, char pixels0[], int x1, int y1, int w1, int h1, char pixels1[]){
    
    for (int i=x0; i<x0+w0; i++){
        for(int j=y0; j<y0+h0; j++){
            if (i >= x1 &&
                i < x1 + w1 &&
                j >= y1 &&
                j < y1 + h1 &&
                pixels0[(i - x0) + (j - y0)*w0] != ' ' &&
                i >= x0 &&
                i < x0 + w0 &&
                j >= y0 &&
                j < y0 + h0 &&
                pixels1[(i - x1) + (j - y1)*w1] != ' '){
                return true;
            } // end of if
        } // end of for(i)
    } // end of for(j)
    
    return false;
}


void draw_formatted(int x, int y, char * buffer, int buffer_size, const char * format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    draw_string(x, y, buffer, FG_COLOUR);
}

//----------------------------------------------------------------
//----------------------------------------------------------------
//----------------------------------------------------------------

/**
 * draw fighter
 */
void draw_fighter(void){
    draw_pixels(fighter_x, fighter_y, fighter_width, fighter_height, fighter, true);
}

/**
 * draw turret accordin to turret angle (turret is visibly reflects its angle as closely as possible)
 */
void draw_turret(void){
    double base_x = fighter_x + 5.0;
    double base_y = fighter_y - 1.0;
    turret_x[0] = base_x;
    turret_y[0] = base_y;
    for (int a = 1; a < 4; a++){
        turret_x[a] = turret_x[a - 1] + cos(turret_angle * M_PI / 180.0);
        turret_y[a] = turret_y[a - 1] - sin(turret_angle * M_PI / 180.0);
    }
    for(int t = 0; t < 4; t++){
        draw_pixels(turret_x[t], turret_y[t], turret_width, turret_height, turret, true);
    }
}

/**
 * update (delete) plasma of index because the plasma at the index has gone
 * or hit to some objects
 */
void update_plasma_array(int index){
    for (int p = index; p < current_plasma; p++){
        px[p] = px[p + 1];
        py[p] = py[p + 1];
        pdx[p] = pdx[p + 1];
        pdy[p] = pdy[p + 1];
    }
    current_plasma--;
}

/**
 * read left potentiometer value, and set angle
 */
void set_turret_angle(void){
    turret_angle = (int)(-(adc_read(0) - ADC_MAX) * pot_left_per_angle + 30.0);
}


/**
 * fire plasma only if last fire happened 0.2 seconds before or more
 */
void fire_plasma(double time){
    
    if (current_plasma < MAX_PLASMA && time - last_fire > MIN_PLASMA_INTERVAL){
        px[current_plasma] = turret_x[3] + 1;
        py[current_plasma] = fighter_y - 4;
        pdx[current_plasma] = cos(turret_angle * M_PI / 180);
        pdy[current_plasma] = sin(turret_angle * M_PI / 180);
        current_plasma++;
        last_fire = time;
    }
}

void draw_plasma(void){
    for (int p = 0; p < current_plasma; p++){
        draw_pixels(px[p], py[p], plasma_size, plasma_size, plasma, true);
    }
}

/**
 * move plasma a bit
 * if next pixel location of plasma is out side the display, delete that plasma from the array
 */
void move_plasma(void){
    for (int p = 0; p < current_plasma; p++){
        if(px[p] + pdx[p] > LCD_X ||
           px[p] + pdx[p] < 0 ||
           py[p] - pdy[p] < 0){
            update_plasma_array(p);
        } else {
            px[p] += pdx[p];
            py[p] -= pdy[p];
        }
    }
}

/**
 * move fighter one pixel
 * if fighter is left or right side, ignore this command
 */
void move_fighter(){
    if (fighter_x + fd >  LCD_X - fighter_width){
        fd = 0;
    } else if (fighter_x + fd < 0){
        fd = 0;
    }
    fighter_x += fd;
}

/**
 * place fighter at the horizontally center of the display
 */
void setup_fighter(){
    fighter_x = (LCD_X - fighter_width) / 2;
}

/**
 * change fighter direction right or left
 */
void change_fighter_direction(char dir){
    if (dir == 'r'){
        if(fd == 0){
            fd = 1;
        } else if (fd == -1){
            fd = 0;
        }
    } else {
        if (fd == 0){
            fd = -1;
        } else if (fd == 1) {
            fd = 0;
        }
    }
}

//----------------------------------------------------------------
//----------------------------------------------------------------
//----------------------------------------------------------------

/**
 * remove asteroid with given index from array
 */
void update_asteroid_array(int index){
    for (int a = index; a < current_asteroid; a++){
        ax[a] = ax[a + 1];
        ay[a] = ay[a + 1];
    }
    current_asteroid--;
}

/**
 * remove boulder with given index from array
 */
void update_boulder_array(int index){
    for (int b = index; b < current_boulder; b++){
        bx[b] = bx[b + 1];
        by[b] = by[b + 1];
        bdx[b] = bdx[b + 1];
        bdy[b] = bdy[b + 1];
    }
    current_boulder--;
}

/**
 * remove fragment with given index from array
 */
void update_fragment_array(int index){
    for (int f = index; f < current_fragment; f++){
        fx[f] = fx[f + 1];
        fy[f] = fy[f + 1];
        fdx[f] = fdx[f + 1];
        fdy[f] = fdy[f + 1];
    }
    current_fragment--;
}


/**
 * check if asteroid overwraps atnother asteroid
 * used to spawn asteroid above the top of the LCD;
 */
bool check_asteroid_overwrap(int x, int y){
    for (int a = 0; a < current_asteroid; a++){
        if(pixel_collision(ax[a], ay[a], ASTEROID_SIZE, ASTEROID_SIZE, asteroid, x, y, ASTEROID_SIZE, ASTEROID_SIZE, asteroid)){
            return true;
        }
    }
    return false;
}

/**
 * flush led light at 2Hz
 *
 * Parameter:
 *      num -number of asteroids at the right side of LCD
 */
void flush_led(int num){
    
    if (num >= 2){
        for(int i = 0; i < 3; i++){
            SET_BIT(PORTB, 3);
            _delay_ms(500);
            CLEAR_BIT(PORTB, 3);
            _delay_ms(500);
        }
    } else {
        for(int i = 0; i < 3; i++){
            SET_BIT(PORTB, 2);
            _delay_ms(500);
            CLEAR_BIT(PORTB, 2);
            _delay_ms(500);
        }
    }
}

/**
 * spawn asteroid above the top of the screen
 * make sure asteroid does not overwraps each other
 */
void spawn_asteroid(void){
    int right = LCD_X - ASTEROID_SIZE;
    double y = -ASTEROID_SIZE - 1;
    int more = 0;
    ax[0] = rand() % (right + 1 );
    ay[0] = y;
    current_asteroid++;
    if(ax[0] > right / 2){
        more++;
    }
    
    while(current_asteroid < MAX_ASTEROID){
        double x = rand() % (right + 1);
        if (!check_asteroid_overwrap(x, y)){
            ax[current_asteroid] = x;
            ay[current_asteroid] = -ASTEROID_SIZE - 1;
            current_asteroid++;
            if (x > right / 2){
                more++;
            }
        }
    }
    flush_led(more);
}

/**
 * draw all asteroids at the postion.
 */
void draw_asteroid(void){
    for (int a = 0; a < current_asteroid; a++){
        draw_pixels(ax[a], ay[a], ASTEROID_SIZE, ASTEROID_SIZE, asteroid, true);
    }
}

/**
 * draw all boulders at the position
 */
void draw_boulder(void){
    for (int b = 0; b < current_boulder; b++){
        draw_pixels(bx[b], by[b], BOULDER_SIZE, BOULDER_SIZE, boulder, true);
    }
}

/**
 * draw all the fragments at the position
 */
void draw_fragment(void){
    for (int f = 0; f < current_fragment; f++){
        draw_pixels(fx[f], fy[f], FRAGMENT_SIZE, FRAGMENT_SIZE, fragment, true);
    }
}


/**
 * move asteroid a bit (downwards only)
 * first asteroids after game starts will be moved after 2 seconds elapsed
 * this 2 seconds is handled by spawn asteroid that flush LED at 2Hz which is equivalent to 2 seconds
 * decrease life it asteroid hits shield
 */
void move_asteroid(void){
    for (int a = 0; a < current_asteroid; a++){
        if (ay[a] >= shield - ASTEROID_SIZE + 1){
            update_asteroid_array(a);
            life -=  1;
        } else {
            ay[a] += speed;
        }
    }
}

/**
 * move boulder a bit towards direction
 * bounce off if boulder hits side edges
 * decrease life if boulder hits shield
 */
void move_boulder(void){
    for (int b = 0; b < current_boulder; b++){
        if(bx[b] + speed * bdx[b] > LCD_X - BOULDER_SIZE + 1||
           bx[b] + speed * bdx[b] < 0){
            bdx[b] = - speed * bdx[b];
        } else if (by[b] + speed * bdy[b] >= shield - BOULDER_SIZE + 1){
            update_boulder_array(b);
            life -= 1;
        } else {
            bx[b] += speed * bdx[b];
            by[b] += speed * bdy[b];
        }
    }
}

/**
 * move fragment a bit towards direction
 * bounce off if fragment hits side edges
 * decrease life if fragment hits shield
 */
void move_fragment(void){
    for (int f = 0; f < current_fragment; f++){
        if (fx[f] + speed * fdx[f] > LCD_X - FRAGMENT_SIZE + 1||
            fx[f] + speed * fdx[f] < 0){
            fdx[f] = - speed * fdx[f];
        } else if (fy[f] + speed * fdy[f] >= shield - FRAGMENT_SIZE + 1){
            update_fragment_array(f);
            life -= 1;
        } else {
            fx[f] += speed * fdx[f];
            fy[f] += speed * fdy[f];
        }
    }
}


/**
 * defines behaviour when plasma hit asteroid
 * that is, generating 2 boulders that goes different direction (right and left)
 * Parameter:
 *      index - hit asteroid index in the array
 */
void hit_asteroid(int index){
    int x1 = rand() % (31) + 60;
    int x2 = rand() % (31) + 90;
    
    if (ax[index] > LCD_X / 2){
        bx[current_boulder] = ax[index];
    } else {
        bx[current_boulder] = ax[index] + 5;
    }
    by[current_boulder] = ay[index];
    bdx[current_boulder] = cos(x1 * M_PI / 180);
    bdy[current_boulder] = sin(x1 * M_PI / 180);
    current_boulder++;
    
    if (ax[index] > LCD_X / 2){
        bx[current_boulder] = ax[index] - 5;
    } else {
        bx[current_boulder] = ax[index];
    }
    by[current_boulder] = ay[index];
    bdx[current_boulder] = cos(x2 * M_PI / 180);
    bdy[current_boulder] = sin(x2 * M_PI / 180);
    current_boulder++;
    
    update_asteroid_array(index);
    
}

/**
 * defines behaviour when plasma hit boulder
 * that is, generating 2 fragments that goes different direction (right and left)
 * Parameter:
 *      index - hit boulder index in the array
 */
void hit_boulder(int index){
    int x1 = rand() % (90 + 1 - 60) + 60;
    int x2 = rand() % (120 + 1 - 90) + 90;
    
    if(bx[index] > LCD_X / 2){
        fx[current_fragment] = bx[index];
    } else {
        fx[current_fragment] = bx[index] + 5;
    }
    
    fy[current_fragment] = by[index];
    fdx[current_fragment] = cos(x1 * M_PI / 180) + fdx[current_fragment];
    fdy[current_fragment] = sin(x1 * M_PI / 180) + fdx[current_fragment] ;
    current_fragment++;
    
    if(bx[index] > LCD_X / 2){
        fx[current_fragment] = bx[index] - 5;
    } else {
        fx[current_fragment] = bx[index];
    }
    fx[current_fragment] = bx[index];
    fy[current_fragment] = by[index];
    fdx[current_fragment] = cos(x2 * M_PI / 180) + fdx[current_fragment];
    fdy[current_fragment] = sin(x2 * M_PI / 180) + fdx[current_fragment];
    current_fragment++;
    
    update_boulder_array(index);
    
}

/**
 * defines behaviour when plasma hit fragment
 * just delete that fragment from array
 */
void hit_fragment(int index){
    update_fragment_array(index);
}

/**
 * used to place an asteroid at the given location by terminal input
 * if input is invalid (out side the display), it is ignored
 * Parameters:
 *      x - x location of new asteroid
 *      y - y location of new asteroid
 */
void place_asteroid(int x, int y){
    if (x >= 0 && x < LCD_X - ASTEROID_SIZE + 1 &&
        y >= 0 && y < 39 - ASTEROID_SIZE + 1 &&
        current_asteroid < MAX_ASTEROID){
        ax[current_asteroid] = x;
        ay[current_asteroid] = y;
        current_asteroid++;
    }
}

/**
 * used to place an boulder at the given location by terminal input
 * if input is invalid (out side the display), it is ignored
 * Parameters:
 *      x - x location of new boulder
 *      y - y location of new boulder
 */
void place_boulder(int x, int y){
    if (x >= 0 && x < LCD_X - BOULDER_SIZE + 1 &&
        y >= 0 && y < 39 - BOULDER_SIZE + 1 &&
        current_boulder < MAX_BOULDER){
        bx[current_boulder] = x;
        by[current_boulder] = y;
        bdx[current_boulder] = 0;
        bdy[current_boulder] = 1;
        current_boulder++;
    }
}

/**
 * used to place an fragment at the given location by terminal input
 * if input is invalid (out side the display), it is ignored
 * Parameters:
 *      x - x location of new fragment
 *      y - y location of new fragment
 */
void place_fragment(int x, int y){
    if (x >= 0 && x < LCD_X - FRAGMENT_SIZE + 1&&
        y >= 0 && y < 39 - FRAGMENT_SIZE + 1 &&
        current_fragment < MAX_FRAGMENT){
        fx[current_fragment] = x;
        fy[current_fragment] = y;
        fdx[current_fragment] = 0;
        fdy[current_fragment] = 1;
        current_fragment++;
    }
}

/**
 * check if asteroid is hit
 */
void collide_asteroid(void){
    for(int a = 0; a < current_asteroid; a++){
        for(int p = 0; p < current_plasma; p++){
            if (py[p] >= 0){
                if(pixel_collision(ax[a], ay[a], (int)ASTEROID_SIZE, (int)ASTEROID_SIZE, asteroid, px[p], py[p], plasma_size, plasma_size, plasma)){
                    update_plasma_array(p);
                    hit_asteroid(a);
                    score += asteroid_score;
                    break;
                }
            }
        }
    }
}

/**
 * check if boulder is hit
 */
void collide_boulder(void){
    for(int b = 0; b < current_boulder; b++){
        for(int p = 0; p < current_plasma; p++){
            if (py[p] >= 0){
                if(pixel_collision(bx[b], by[b], (int)BOULDER_SIZE, (int)BOULDER_SIZE, boulder, px[p], py[p], plasma_size, plasma_size, plasma)){
                    update_plasma_array(p);
                    hit_boulder(b);
                    score += boulder_score;
                    break;
                }
            }
        }
    }
}

/**
 * check if fragment is hit
 */
void collide_fragment(void){
    for(int f = 0; f < current_fragment; f++){
        for(int p = 0; p < current_plasma; p++){
            if (py[p] >= 0){
                if (pixel_collision(fx[f], fy[f], (int)FRAGMENT_SIZE, (int)FRAGMENT_SIZE, fragment, px[p], py[p], plasma_size, plasma_size, plasma)){
                    update_plasma_array(p);
                    hit_fragment(f);
                    score += fragment_score;
                    break;
                }
            }
        }
    }
}

int calculate_angle_to_display(){
    return -turret_angle + 90;
}

/**
 * send status (& game over) or help menu to computer
 */
void send_status(char mode){
    char buffer[200];
    char *message;
    if (mode == 'o'){
        message = "GAME OVER";
        snprintf(buffer, sizeof(buffer), statusM,
                 message, get_minutes(), get_seconds(),
                 life, score,
                 current_asteroid, current_boulder, current_fragment,
                 current_plasma, calculate_angle_to_display(), speed);
    } else if (mode == 's'){
        message = "STATUS";
        snprintf( buffer, sizeof(buffer), statusM,
                 message, get_minutes(), get_seconds(),
                 life, score,
                 current_asteroid, current_boulder, current_fragment,
                 current_plasma, calculate_angle_to_display(), speed);
    } else if (mode == 'h'){
        message = "CONTROLS";
        snprintf(buffer, sizeof(buffer), helpM);
    } else {
        message = "Game started";
        snprintf(buffer, sizeof(buffer), statusM,
                 message, get_minutes(), get_seconds(),
                 life, score,
                 current_asteroid, current_boulder, current_fragment,
                 current_plasma, calculate_angle_to_display(), speed);
    }
    usb_serial_send( buffer );
}

/**
 * set game speed by potentiometer
 */
void set_speed(void){
    double right_adc = adc_read(1);
    speed = right_adc * 2 / ADC_MAX;
}

/**
 * set game speed by input
 * 0 ~ 20
 */
void overwrite_speed(int new){
    if (!(new < 0 || new > 20)){
        speed = (double)new / 10;
    }
}



/**
 * enable pause mode, that is, paused = true and store paused start time
 */
void pause_on(void){
    paused = true;
    before_pause = get_time();
}

/**
 * disable pause time, and adjust time not to include this pause time in the game time
 */
void pause_off(void){
    if (initial){
        start_time = get_time();
        srand(start_time);
        int fdir = rand() % (10 + 1 - 1) + 10;
        if (fdir % 2 == 0){
            change_fighter_direction('r');
        } else {
            change_fighter_direction('l');
        }
        send_status('i');
        initial = false;
    } else {
        adjust += get_time() - before_pause;
    }
    paused = false;
    status_on = false;
}


/**
 * enable special pause time for inputting number
 * so that the time during this input is not included in the game time
 */
void input_pause_on(void){
    inputting = true;
    if (!paused){
        before_pause = get_time();
    }
}

/**
 * special pause time for inputting number is disabled
 * and time during this pause time is considered
 */
void input_pause_off(void){
    inputting = false;
    if (!paused){
        adjust += get_time() - before_pause;
    }
}

/**
 * special LCD picture for introduction
 */
void intro_set(void){
    clear_screen();
    draw_string(0, 8, studentID, FG_COLOUR);
    draw_string(0, 16, "SPACE FIGHTER", FG_COLOUR);
    setupBarrier();
    draw_fighter();
    draw_turret();
    show_screen();
}

void set_fighter_x(int x){
    if (!(x < 0 || x > LCD_X - fighter_width)){
        fighter_x = x;
    }
}

/**
 * introduction animation
 * pressing 'r' or left buton skips this.
 * Needs to skip at some point, this animation won't terminate without skipping
 */
void introduction(void){
    set_duty_cycle(ADC_MAX);
    set_fighter_x(0);
    change_fighter_direction('r');
    while (1){
        intro_set();
        move_fighter();
        if (fighter_x >= LCD_X - fighter_width){
            change_fighter_direction('l');
        } else if (fighter_x <= 0){
            change_fighter_direction('r');
        }
        if (BIT_IS_SET(PINF, 6) ||
            (char)usb_serial_getchar() == 'r'){ // skip
            break;
        }
    }
    _delay_ms(1000);
}

/**
 * terminate gaming loop and display student ID only in inverse mode
 */
void do_quit(void){
    clear_screen();
    draw_string(10, 20, studentID, FG_COLOUR);
    _delay_ms(100);
    LCD_CMD( lcd_set_display_mode, lcd_display_inverse);
    show_screen();
    quit = true;
}



void overwrite_turret_angle(int angle){
    if (angle <= 60 && angle >= -60){
        turret_angle = 180 - (angle + 90);
    }
}

/**
 * turn off/ on some boolean variables such as game_over to run game
 * set values of some bariable such as current number of plasma on screen to 0
 * so that when you reset game, previously played game score and and other stuff
 * will not remain on the screen
 */
void back_to_default(void){
    current_plasma = 0;
    current_asteroid = 0;
    current_boulder = 0;
    current_fragment = 0;
    overwrite_turret_angle(0);
    life = LIFE_MAX;
    elapsed = 0.0;
    adjust = 0.0;
    before_pause = 0.0;
    start_time = 0.0;
    last_fire = 0.0;
    score = 0;
    speed = 1;
    turretMode = false;
    speedMode = false;
    quit = false;
    inputting = false;
    paused = true;
    initial = true;
    input_turret_minus = false;
    game_over = false;
}

/**
 * Sets score, life, number of plasma, asteroid, boulder and fragment,
 * place spaceship at the center of screen width
 */
void setup_game_property(void){
    setup_fighter();
    set_fighter_x((LCD_X - fighter_width) / 2);
    back_to_default();
    LCD_CMD( lcd_set_display_mode, lcd_display_normal);
}

/**
 * Displays status on Teensy LCD
 */
void show_status(void){
    char buf_time[10];
    char buf_life[10];
    char buf_score[10];
    if (paused){
        clear_screen();
        draw_formatted(0, 0, buf_time, sizeof(buf_time), "%02d:%02d", get_minutes(), get_seconds());
        draw_formatted(0, 10, buf_life, sizeof(buf_life), "Life: %d", life);
        draw_formatted(0, 20, buf_score, sizeof(buf_score), "Score: %d", score);
        show_screen();
    }
    if (BIT_IS_SET(PINB, 7)){
        status_on = false;
    }
    if ((char)usb_serial_getchar() == 'p' || BIT_IS_SET(PINB, 0)){
        pause_off();
        status_on = false;
    }
}

/**
 * draw shield, space ship, plasma, asteroid, boulder, fragment, and turret
 * While not in puased, this function is responsible for moving properties
 */
void draw_all(void){
    clear_screen();
    if (status_on){
        show_status();
    } else {
        setupBarrier();
        if (!paused && !inputting){
            move_fighter();
            move_plasma();
            move_asteroid();
            move_boulder();
            move_fragment();
            collide_asteroid();
            collide_boulder();
            collide_fragment();
        }
        
        draw_plasma();
        draw_asteroid();
        draw_boulder();
        draw_fragment();
        draw_fighter();
        draw_turret();
    }
    show_screen();

}

/**
 * Introduction stuff
 * Prepare game screen to ready to be played
 */
void before_game_start(void){
    clear_screen();
    introduction();
    clear_screen();
    setup_game_property();
    setupBarrier();
    draw_fighter();
    draw_turret();
    draw_all();
    show_screen();
}

/**
 * Force quit input mode because of array size
 * This force quitting is safe as anyway relevant function will ignore such big input
 * e.g. if you try to place asteroid at (1000, 1000), the function place_asteroid()
 * will ignore this command.
 */
void force_input_off(){
    if (input_counter1 == 9 || input_counter2 == 5){
        inputting = false;
        adjust += get_time() - before_pause;
        input_counter1 = 0;
    }
}

/**
 * back to introduction game to restart game
 */
void do_reset(void){
    before_game_start();
}




/**
 * Function of game over animation
 */
void do_gameover(void){
    send_status('o');
    game_over = true;
    double now = get_time();
    SET_BIT(PORTB, 3);
    SET_BIT(PORTB, 2);
    int i = ADC_MAX;
    while (get_time() - now < 2.0){
        clear_screen();
        set_duty_cycle(i);
        draw_string(0, 20, "G A M E O V E R", FG_COLOUR);
        show_screen();
        i -= 20;
    }
    CLEAR_BIT(PORTB, 2);
    CLEAR_BIT(PORTB, 3);
    set_duty_cycle(0);
}

/**
 * Until left button or right button is pressed,
 * Teensy will ask to press either of these
 */
void after_game_over(void){
    for (int l = 50; l < 1024; l+=20){
        clear_screen();
        draw_string(0, 10, "Quit:left btn", FG_COLOUR);
        draw_string(0, 20, "Reset:right btn", FG_COLOUR);
        show_screen();
        set_duty_cycle(l);
        _delay_ms(30);
    }

    while(1){
        if(BIT_IS_SET(PINF, 6)){
            do_quit();
            break;
        } else if (BIT_IS_SET(PINF, 5)){
            game_over = false;
            do_reset();
            break;
        }
    }
}

/**
 * return true if char is number
 */
bool isNumber(char ch){
    return (ch == '0' || ch == '1' || ch == '2' || ch == '3' || ch == '4' ||
            ch == '5' || ch == '6' || ch == '7' || ch == '8' || ch == '9');
}

/**
 * return true if input char is valid in this game
 */
bool isInputValid(char ch){
    return (ch == 'a' || ch == 'd' || ch == 'w' || ch == 's' || ch == 'r'
            || ch == 'p' || ch == 'q' || ch == '?' || ch == 't' || ch == 'm'
            || ch == 'l' || ch == 'g' || ch == 'h' || ch == 'j' || ch == 'k'
            || ch == 'i');
}

/**
 * convert char (number) to integer
 */
int get_int(char ch){
    return ch - '0';
}

/**
 * convert stored input sequence to one integer
 */
int convert_input1_to_int(){
    int converted1 = 0;
    int l = 0;
    
    for(int i = input_counter1; i >= 0; i--){
        converted1 += inputArray1[l] * pow(10, i - 1);
        l++;
    }
    return converted1;
}

/**
 * convert stored input sequence to one integer
 */
int convert_input2_to_int(){
    int converted2 = 0;
    int l = 0;
    for(int i = input_counter2; i >= 0; i--){
        converted2 += inputArray2[l] * pow(10, i - 1);
        l++;
    }
    return converted2;
}

/**
 * this is called when input is finished, that is input_ready = true
 * and make changes based on command and its input
 */
void input_finihsed(){
    if (input_ready){
        if (input_command == 't'){
            if (!turretMode){
                overwrite_turret_angle(input1);
                turretMode = true;
            } else {
                turretMode = false;
            }
        } else if (input_command == 'm'){
            if (!speedMode){
                overwrite_speed(input1);
                speedMode = true;
            } else {
                speedMode = false;
            }
        } else if (input_command == 'l'){
            life = input1;
        } else if (input_command == 'g'){
            score = input1;
        } else if (input_command == 'h'){
            set_fighter_x(input1);
        } else if (input_command == 'j'){
            place_asteroid(input1, input2);
        } else if (input_command == 'k'){
            place_boulder(input1, input2);
        } else if (input_command == 'i'){
            place_fragment(input1, input2);
        }
        input_ready = false;
        input_pause_off();
    }
}

/**
 * process number input
 * this function must be called while inputting is true
 */
void further_input(char ch){
    
    if (input_command == 't' && ch == '-'){
        input_turret_minus = true;
        return;
    }
    if (input_mode == 'n' && isNumber(ch)){
        inputArray1[input_counter1] = get_int(ch);
        input_counter1++;
    } else if (input_mode == 'n' && ch == 0x0D){
        if (input_command == 't' && input_turret_minus){
            input1 = -convert_input1_to_int();
            input_turret_minus = false;
        } else {
            input1 = convert_input1_to_int();
        }
        input_counter1 = 0;
        input_ready = true;
    } else if (input_mode == 'c' && isNumber(ch)){
        inputArray1[input_counter1] = get_int(ch);
        input_counter1++;
    } else if (input_mode == 'c' && ch == 0x0D){
        input1 = convert_input1_to_int();
        input_mode = 'o';
    } else if (input_mode == 'o' && isNumber(ch)){
        inputArray2[input_counter2] = get_int(ch);
        input_counter2++;
    } else if (input_mode == 'o' && ch == 0x0D){
        input2 = convert_input2_to_int();
        input_counter1 = 0;
        input_counter2 = 0;
        input_ready = true;
    }
}

/**
 * perform keyboard input operation that needs to pause the game
 */
void do_operation_need_pause(char ch){

    if (ch == 't' || ch == 'm' || ch == 'l' || ch == 'g' ||
               ch == 'h'){
        input_mode = 'n';
        input_command = ch;
    }else if (ch == 'j' || ch == 'k' || ch == 'i'){
        input_mode = 'c';
        input_command = ch;
    }
}

/**
 * some key is pressed, then check the pressed key is valid in this game
 */
void keyboard_pressed(char ch){
//    char ch = (char)usb_serial_getchar();
//    //    if (ch == 'w'){SET_BIT(PORTB, 3);}
//    if (!inputting && isInputValid(ch)){
//        if (ch == 'm' && speedMode){
//            speedMode = false;
//        } else if (ch == 't' && turretMode){
//            turretMode = false;
//        } else if (ch == 'a'){
//            change_fighter_direction('l');
//        } else if (ch == 'd'){
//            change_fighter_direction('r');
//        } else if (ch == 'w'){
//            fire_plasma(get_time());
//        } else if (ch == 's'){
//            send_status('s');
//        } else if (ch == 'r'){
//            do_reset();
//        } else if (ch == 'p'){
//            if (paused) {pause_off();}
//            else {pause_on();}
//        } else if (ch == 'q'){
//            quit = true;
//            return;
//        } else if (ch == '?'){
//            send_status('h');
//            pause_on();
//        } else {
//            input_pause_on();
//            do_operation_need_pause(ch);
//        }
//    } else if (inputting){
//        further_input(ch);
//    }
    if (ch == 'm' && speedMode){
        speedMode = false;
    } else if (ch == 't' && turretMode){
        turretMode = false;
    } else if (ch == 'a'){
        change_fighter_direction('l');
    } else if (ch == 'd'){
        change_fighter_direction('r');
    } else if (ch == 'w'){
        fire_plasma(get_time());
    } else if (ch == 's'){
        send_status('s');
    } else if (ch == 'r'){
        do_reset();
    } else if (ch == 'p'){
        if (paused) {pause_off();}
        else {pause_on();}
    } else if (ch == 'q'){
        quit = true;
    } else if (ch == '?'){
        send_status('h');
        pause_on();
    } else {
        input_pause_on();
        do_operation_need_pause(ch);
    }
}

/**
 * Get input on Teensy board's hardware such as buttons, switches and potentiometer
 */
void do_teensy_operation(){
    // left swtich
    if (BIT_IS_SET(PINB, 1)){
        change_fighter_direction('l');
        // right switch
    } else if (BIT_IS_SET(PIND, 0)){
        change_fighter_direction('r');
        // up switch
    } else if (BIT_IS_SET(PIND, 1) && !paused){
        fire_plasma(get_time());
        // down swtich
    } else if (BIT_IS_SET(PINB, 7)){
        if (paused){
            status_on = true;
        }
        send_status('s');
        // center switch
    } else if (BIT_IS_SET(PINB, 0)){
        if (!paused){pause_on();}
        else {pause_off();}
        // right button
    } else if (BIT_IS_SET(PINF, 5)){
        quit = true;
        return;
        // left button
    } else if(BIT_IS_SET(PINF, 6)){
        do_reset();
    }
}

/**
 * If no falling objects are on LCD, then spawn new asteroid
 */
void no_objects_on_screen(){
    if (current_asteroid == 0 && current_boulder == 0 && current_fragment == 0 && !paused){
        spawn_asteroid();
    }
}

/**
 * game loop
 */
void loop(void){
    
    if (life <= 0){
        do_gameover();
        after_game_over();
        return;
    }
    char ch = (char)usb_serial_getchar();
    if (!inputting){
        if (isInputValid(ch)){
            keyboard_pressed(ch);
        }
        if (!quit){
            if (!paused){
                elapsed = get_time() - start_time - adjust;
                if (!turretMode){
                    set_turret_angle();
                }
                if (!speedMode){
                    set_speed();
                }
            }
            do_teensy_operation();
            no_objects_on_screen();
            draw_all();
        }
    } else {
        further_input(ch);
    }
    input_finihsed();
    force_input_off();
//    if (status_on){
//        show_status();
//        if (BIT_IS_SET(PINB, 7)){
//            status_on = false;
//        }
//        if ((char)usb_serial_getchar() == 'p' || BIT_IS_SET(PINB, 0)){
//            pause_off();
//            status_on = false;
//        }
//        return;
//    }
//
//    if (life <= 0){
//        do_gameover();
//        after_game_over();
//        return;
//    }
//    if (!paused && !inputting){
//        elapsed = get_time() - start_time - adjust;
//    }
//    keyboard_pressed();
//    if (!inputting && !quit){
//        do_teensy_operation();
//        no_objects_on_screen();
//        if (!status_on){
//            draw_all();
//        }
//    }
//    input_finihsed();
//    force_input_off();
//    if (!turretMode && !inputting && !quit && !paused){
//        set_turret_angle();
//    }
//    if (!speedMode && !inputting && !quit && !paused){
//        set_speed();
//    }
}


/**
 * Main function
 */
int main(void) {
    setup();
    before_game_start();
    for ( ;; ) {
        loop();
        _delay_ms(100);
        if (quit){
            break;
        }
    }
    do_quit();
    
    return 0;
}




