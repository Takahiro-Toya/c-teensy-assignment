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
#include "general.h"
#include "fighter.h"
#include "falling.h"

#define FREQ     (8000000.0)
#define PRESCALE (1024.0)
#define BIT(x) (1 << (x))
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)
#define LIFE_MAX 5

char studentID[] = "n10056513";
char* statusM =
"%s \r\n\
Time: %d:%d \r\n\
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
h-move ship to coordinate\r\n\
j-place asteroid at coordinate\r\n\
k-place boulder at coordinate\r\n\
i-place fragment at coordinate\r\n";
char buffer[333];
bool paused = true;
bool inputting = false;
bool game_over = false;
bool initial = true;
bool quit = false;
int score = 0;
int life = LIFE_MAX;

double speed = 1;
const int shield = 39;

double start_time;
double adjust = 0.0;
double elapsed = 0.0;
double before_pause = 0.0;

bool turretMode = false;
bool speedMode = false;

bool input_ready = false;
char inputModeChar;
char inputMode = 'd';

int inputArray1[10];
int inputArray2[5];
int input_counter1 = 0;
int input_counter2 = 0;

volatile int overflow_counter = 0;

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

/**
 * check if asteroid is hit
 */
void collide_asteroid(void){
    for(int a = 0; a < get_current_asteroid(); a++){
        for(int p = 0; p < get_current_plasma(); p++){
            if (get_py(p) >= 0){
                if(pixel_collision(get_axy('x', a), get_axy('y', a), get_asteroid_size(), get_asteroid_size(), get_asteroid(), get_px(p), get_py(p), get_plasma_size(), get_plasma_size(), get_plasma())){
                    update_plasma_array(p);
                    hit_asteroid(a);
                    score += get_object_score('a');
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
    for(int b = 0; b < get_current_boulder(); b++){
        for(int p = 0; p < get_current_plasma(); p++){
            if (get_py(p) >= 0){
                if(pixel_collision(get_bxy('x', b), get_bxy('y', b), (int)get_boulder_size(), (int)get_boulder_size(), get_boulder(), get_px(p), get_py(p), get_plasma_size(), get_plasma_size(), get_plasma())){
                    update_plasma_array(p);
                    hit_boulder(b);
                    score += get_object_score('b');
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
    for(int f = 0; f < get_current_fragment(); f++){
        for(int p = 0; p < get_current_plasma(); p++){
            if (get_py(p) >= 0){
                if (pixel_collision(get_fxy('x', f), get_fxy('y', f), (int)get_fragment_size(), (int)get_fragment_size(), get_fragment(), get_px(p), get_py(p), get_plasma_size(), get_plasma_size(), get_plasma())){
                    update_plasma_array(p);
                    hit_fragment(f);
                    score += get_object_score('f');
                    break;
                }
            }
        }
    }
}


/**
 * send status (& game over) or help menu to computer
 */
void send_status(char mode){
    char *message;
    if (mode == 'o'){
        message = "GAME OVER";
    } else if (mode == 's'){
        message = "STATUS";
        snprintf( buffer, sizeof(buffer), statusM,
                 message, get_minutes(), get_seconds(),
                 life, score,
                 get_current_asteroid(), get_current_boulder(), get_current_fragment(),
                 get_current_plasma(), get_turret_angle(), speed);
    } else {
        message = "CONTROLS";
        snprintf(buffer, sizeof(buffer), helpM);
    }
    
    usb_serial_send( buffer );
}

/**
 * set game speed by potentiometer
 */
void set_speed(void){
    double right_adc = adc_read(1);
    speed = right_adc * 2 / 1023;
}

/**
 * set game speed by input
 */
void overwrite_speed(int new){
    speed = new;
}



/**
 * enable pause mode, that is, paused = true and store paused start time
 */
void pause_on(void){
    paused = true;
    before_pause = get_time();
}

/**
 * draw shield, space ship, plasma, asteroid, boulder, fragment, and turret
 * While not in puased, this function is responsible for moving properties
 */
void draw_all(void){
    clear_screen();
    setupBarrier();
    if (!paused && !inputting){
        move_fighter();
        move_plasma();
        if (elapsed > 2.0){
            life += move_asteroid(elapsed, shield, speed);
        }
        life += move_boulder(shield, speed);
        life += move_fragment(shield, speed);
        collide_asteroid();
        collide_boulder();
        collide_fragment();
    } else {
        draw_asteroid();
        draw_boulder();
        draw_fragment();
    }
    draw_fighter();
    draw_turret();
    show_screen();
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

/**
 * introduction animation
 * pressing 'r' or left buton skips this.
 * Needs to skip at some point, this animation won't terminate without skipping
 */
void introduction(void){
    set_duty_cycle(1023);
    set_fighter_x(0);
    change_fighter_direction('r');
    while (1){
        intro_set();
        move_fighter();
        if (BIT_IS_SET(PINF, 6) ||
            (char)usb_serial_getchar() == 'r'){ // skip
            break;
        }
    }
    _delay_ms(1000);
    set_duty_cycle(0);
}

/**
 * terminate gaming loop and display student ID only in inverse mode
 */
void do_quit(void){
    clear_screen();
    LCD_CMD( lcd_set_display_mode, lcd_display_inverse);
    draw_string(10, 20, studentID, FG_COLOUR);
    show_screen();
    quit = true;
}



/**
 * Sets score, life, number of plasma, asteroid, boulder and fragment,
 * place spaceship at the center of screen width
 */
void setup_game_property(void){
    game_over = false;
    setup_fighter();
    set_fighter_x((LCD_X - get_fighter_size('w')) / 2);
    set_current_plasma(0);
    set_current_object('a', 0);
    set_current_object('b', 0);
    set_current_object('f', 0);
    overwrite_turret_angle(90);
    life = LIFE_MAX;
    score = 0;
    paused = true;
    initial = true;
    elapsed = 0.0;
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
 * back to introduction game to restart game
 */
void do_reset(void){
    before_game_start();
}

char buf_time[5];
char buf_life[10];
char buf_score[10];

/**
 * Displays status on Teensy LCD
 */
void show_status(void){
    clear_screen();
    draw_formatted(0, 0, buf_time, sizeof(buf_time), "%d:%d", get_minutes(), get_seconds());
    draw_formatted(0, 10, buf_life, sizeof(buf_life), "Life: %d", life);
    draw_formatted(0, 20, buf_score, sizeof(buf_score), "Score: %d", score);
    show_screen();
}


/**
 * Function of game over animation
 */
void do_gameover(void){
    send_status('o');
    game_over = true;
    draw_string(0, 20, "G A M E O V E R", FG_COLOUR);
    show_screen();
    SET_BIT(PORTB, 3);
    SET_BIT(PORTB, 2);
    _delay_ms(2000);
    for (int i = 1023; i >= 0; i-=10){
        set_duty_cycle(i);
        _delay_ms(10);
    }
    set_duty_cycle(0);
    CLEAR_BIT(PORTB, 2);
    CLEAR_BIT(PORTB, 3);
}

/**
 * Until left button or right button is pressed,
 * Teensy will ask to press either of these
 */
void after_game_over(void){
    clear_screen();
    draw_string(0, 10, "Quit:left btn", FG_COLOUR);
    draw_string(0, 20, "Reset:right btn", FG_COLOUR);
    show_screen();
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
 * Force quit input mode because of array size
 * This force quitting is safe as anyway relevant function will ignore such big input
 * e.g. if you try to place asteroid at (1000, 1000), the function place_asteroid()
 * will ignore this command.
 */
void force_input_off(){
    if (input_counter1 == 9 || input_counter2 == 5){
        inputting = false;
        input_counter1 = 0;
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



int input1;
int input2;

/**
 * reflects changes to Teensy LCD
 * if input is 't' or 'm' (setting turret & speed), these inputs enable special mode
 * Once user input 't' or 'm' and changes turret aim and speed,
 * potentiometer is disabled. Users need to input 't' or 'm' again to enable pot.
 */
void reflect_input(){
    if (input_ready){
        if (inputModeChar == 't'){
            if (!turretMode){
                overwrite_turret_angle(input1);
                turretMode = true;
            } else {
                turretMode = false;
            }
        } else if (inputModeChar == 'm'){
            if (!speedMode){
                overwrite_speed(input1 / 10);
                speedMode = true;
            } else {
                speedMode = false;
            }
        } else if (inputModeChar == 'l'){
            life = input1;
        } else if (inputModeChar == 'g'){
            score = input1;
        } else if (inputModeChar == 'h'){
            set_fighter_x(input1);
        } else if (inputModeChar == 'j'){
            place_asteroid(input1, input2);
        } else if (inputModeChar == 'k'){
            place_boulder(input1, input2);
        } else if (inputModeChar == 'i'){
            place_fragment(input1, input2);
        }
        input_ready = false;
        inputting = false;
        CLEAR_BIT(PORTB, 3);
    }
}


/**
 * return true if input char is valid in this game
 */
bool isInputValid(char ch){
    return (ch == 'a' || ch == 'd' || ch == 'w' || ch == 's' || ch == 'r'
    || ch == 'p' || ch == 'q' || ch == '?' || ch == 't' || ch == 'm' || ch == 'l' ||
            ch == 'g' || ch == 'h' || ch == 'j' || ch == 'k' || ch == 'i');
}

/**
 * perform keyboard input operation that needs to pause the game
 */
void do_operation_need_pause(char ch){
    if (ch == '?'){
        send_status('h');
    } else if (ch == 't' || ch == 'm' || ch == 'l' || ch == 'g' ||
               ch == 'h'){
        inputMode = 'n';
        inputModeChar = ch;
    }else if (ch == 'j' || ch == 'k' || ch == 'i'){
        inputMode = 'c';
        inputModeChar = ch;
    }
}

/**
 * Classfies key board input to enable further input
 * This is essential process as some input char needs number input after
 * command has been input.
 */
void classify_input(char ch){
    if (isInputValid(ch)){
        if (ch == 'a'){
            change_fighter_direction('l');
        } else if (ch == 'd'){
            change_fighter_direction('r');
        } else if (ch == 'w' && !paused){
            fire_plasma(elapsed);
        } else if (ch == 's'){
            send_status('s');
        } else if (ch == 'r'){
            do_reset();
        } else if (ch == 'p'){
            if (paused) {paused = false;}
            else {pause_on();}
        } else if (ch == 'q'){
            do_quit();
        } else {
            inputting = true;
            SET_BIT(PORTB, 3);
            do_operation_need_pause(ch);
        }
        
    }
}


/**
 * Receive input through USB, and process operation
 * If input is being enabled, then add new input (number) to input array.
 * If input is being enabled, and non-number is passed, then disable input process
 * If input is being enabled, and enter is pressed, then terminate input process to
 * make changes on Teensy LCD
 * If input is not being enabled, then get new command input, this will enable input
 * process
 */
void process_keyboard_input(){
    int16_t char_code = usb_serial_getchar();
    char ch = (char)char_code;
    
    if (inputting && inputMode == 'n' && isNumber(ch)){
        inputArray1[input_counter1] = get_int(ch);
        input_counter1++;
    } else if (inputting && inputMode == 'n' && ch == 0x0D){
        
        input1 = convert_input1_to_int();
        input_counter1 = 0;
        input_ready = true;
        
    } else if (inputting && inputMode == 'c' && isNumber(ch)){
        inputArray1[input_counter1] = get_int(ch);
        input_counter1++;
    } else if (inputting && inputMode == 'c' && ch == 0x0D){
        input1 = convert_input1_to_int();
        inputMode = 'o';
    } else if (inputting && inputMode == 'o' && isNumber(ch)){
        inputArray2[input_counter2] = get_int(ch);
        input_counter2++;
    } else if (inputting && inputMode == 'o' && ch == 0x0D){
        input2 = convert_input2_to_int();
        input_counter1 = 0;
        input_counter2 = 0;
        input_ready = true;
        
    } else if (!inputting){
        classify_input(ch);
    } else {
        inputting = false;
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
        send_status('s');
        // center switch
    } else if (BIT_IS_SET(PINB, 0) && !paused){
        pause_on();
        // right button
    } else if (BIT_IS_SET(PINF, 5)){
        do_quit();
        // left button
    } else if(BIT_IS_SET(PINF, 6)){
        do_reset();
    }
}

/**
 * If no falling objects are on LCD, then spawn new asteroid
 */
void no_objects_on_screen(){
    if (get_current_asteroid() == 0 && get_current_boulder() == 0 && get_current_fragment() == 0){
        spawn_asteroid();
    }
}


/**
 * While in paused mode, some input is still allowed to process
 * Pausing time should not be included in the game time, so once unpaused,
 * this function manupulate elapsed time
 */
void do_operation_while_pause(){
    if(BIT_IS_SET(PINB, 0) ||
       (char)usb_serial_getchar() == 'p'){ // switch pressed -> turn off pause
        if (initial){
            start_time = get_time();
            srand(start_time);
            initial = false;
        } else {
            adjust += get_time() - before_pause;
        }
        paused = false;
    }
    process_keyboard_input();
    reflect_input();
    do_teensy_operation();
    draw_all();
    force_input_off();
}

/**
 * game loop
 */
void loop(void){
    
    if (inputting){
        process_keyboard_input();
        reflect_input();
        draw_all();
        force_input_off();
        return;
    }

    if (!game_over){
        if (!paused){
            elapsed = get_time() - start_time - adjust;
            if (life <= 0){
                do_gameover();
                return;
            }
            if (!turretMode){
                set_turret_angle();
            }
            if (!speedMode){
                set_speed();
            }
            process_keyboard_input();
            reflect_input();
            do_teensy_operation();
            no_objects_on_screen();
            draw_all();
            force_input_off();
        } else {
            do_operation_while_pause();
        }
    } else{
        after_game_over();
    }
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
    
    return 0;
}

