/**
********************************************************************                        
====================================================================
* Written by: Tamsanqa Thwala
*                  		               
* Version: 11/05/2023                                         
* Modified:                                                       
====================================================================            
Target:        STM32F0                                           
====================================================================
Description                                                                    
********************************************************************

Include files
====================================================================
*/

#include <stdint.h>
#include "stm32f0xx.h"
#include <lcd_stm32f0.c>

/**
====================================================================
Global constants
====================================================================
*/

#define TRUE 1
#define FALSE 0

//redefine uint8_t to flag_t
typedef uint8_t flag_t;  

/**
====================================================================
Global variables
====================================================================
*/

flag_t startFlag = FALSE;
flag_t lapFlag = FALSE;
flag_t stopFlag = FALSE;
flag_t resetFlag = TRUE;
flag_t updateLapFlag = FALSE;

uint8_t minutes = 0;
uint8_t seconds = 0;
uint8_t hundredths = 0;

char mins[2] = {0x30,0x30};
char secs[2] = {0x30,0x30};
char hunds[2] = {0x30,0x30};

int counter = 0;
char time[8];


// flags for checking if stopwatch reached a point in time
uint8_t pastTenSecs = FALSE;
uint8_t pastTenMinutes = FALSE;


/**
====================================================================
Function declarations
====================================================================
*/
void initGPIO(void);
void initTIM14(void);
void checkPB(void);
void display(void);
void TIM14_IRQHandler(void);
void convert2BCDASCII(const uint8_t min, const uint8_t sec, const uint8_t hund, char* resultPtr);
void BDCHelper(uint8_t n, char * result);

/**
====================================================================
 Main function
====================================================================
*/
int main (void)
{
    initGPIO(); // initialise GPIO pins
    init_LCD(); // initialise lcd
    initTIM14();


    for(;;){
        checkPB();
        display(); // turn leds based on flags
        for( volatile int i = 0; i < 10000; i++);
        updateLapFlag = FALSE; // set updateFlag to false
        
    }

} // End of main

/**
==================================================================== 
Function definitions
====================================================================
*/

void initGPIO(void){
    // initailise  GPIO B
    RCC->AHBENR |= 1<<18;
    //configure pin 0-7, 10, and 11 to output mode
    GPIOB->MODER|= 0x00505555; 
    GPIOB-> ODR = 0x00000000; // turn the 8 leds off 

    //initialise GPIO A
    RCC->AHBENR |= 1<<17; 
    //configure pins 0 - 3 to input mode   
    GPIOA->MODER &= ~GPIO_MODER_MODER0;
    GPIOA->MODER &= ~GPIO_MODER_MODER1;
    GPIOA->MODER &= ~GPIO_MODER_MODER2;
    GPIOA->MODER &= ~ GPIO_MODER_MODER3; 

    /** enable pull-up resitors for port A pin 0 - 3
    * by setting relevant bits in register to 1
    */
    GPIOA -> PUPDR |= GPIO_PUPDR_PUPDR0_0;
    GPIOA -> PUPDR |= GPIO_PUPDR_PUPDR1_0;
    GPIOA -> PUPDR |= GPIO_PUPDR_PUPDR2_0;
    GPIOA -> PUPDR |= GPIO_PUPDR_PUPDR3_0;

 }


void initTIM14(void){
    //initialise timer 14
    RCC -> APB1ENR |= RCC_APB1ENR_TIM14EN; // Enable clock to timer 14 on bit 8
    TIM14 -> PSC = 4; // set prescaler multipler to 4
    TIM14 -> ARR = 14000;
    TIM14 -> DIER |= TIM_DIER_UIE; // enable interrupts for timer 14 in bit 0/ update interrupt enable

    
    NVIC_EnableIRQ(19); // enable interrupt to run
}

void TIM14_IRQHandler(void){
    
    // implement interrupt handler
    hundredths += 1;
    if(hundredths == 100){
        seconds += 1;
        hundredths = 0; 
    }
     // set flag to true if seconds greater than ten
    pastTenSecs = seconds >= 10 ? TRUE: FALSE;
    // set flag to true if minutes greater than ten
    pastTenMinutes = minutes >= 10 ? TRUE: FALSE;

    if(seconds == 60){
        minutes += 1;
        seconds = 0;
    }

    TIM14 -> SR &= ~TIM_SR_UIF; // clear interrupt status register
}
void checkPB(void){
    //check if push buttons are pressed
    int16_t SW0 = ((GPIOA -> IDR & GPIO_IDR_0) == 0); // start
    int16_t SW1 = ((GPIOA-> IDR & GPIO_IDR_1) == 0); // lap
    int16_t SW2 = ((GPIOA-> IDR & GPIO_IDR_2)== 0); // stop
    int16_t SW3 = ((GPIOA-> IDR & GPIO_IDR_3) == 0); // reset
     
    if(SW0){
        // SW0 is pressed
        startFlag = TRUE; 
        lapFlag = FALSE;
        stopFlag = FALSE;
        resetFlag = FALSE;
    }
    if(SW1){
        // SW1 is pressed
        startFlag = TRUE;
        lapFlag = TRUE;
        stopFlag = FALSE;
        resetFlag = FALSE;
        updateLapFlag = TRUE;
        
    }

    if(SW2){
        // SW2 is pressed
        startFlag = TRUE; 
        lapFlag = FALSE;
        stopFlag = TRUE;
        resetFlag = FALSE;    

    }

    if(SW3){
        // SW3 is pressed
        startFlag = FALSE;
        lapFlag = FALSE;
        stopFlag = FALSE;
        resetFlag = TRUE;
    }

}

void convert2BCDASCII(const uint8_t min, const uint8_t sec, const uint8_t hund, char* resultPtr){

    BDCHelper(min, mins); // convert minutes to BCD equivalent
    counter = 0; // reset counter

    BDCHelper(sec, secs); // convert seconds to BCD equivalent
    counter = 0; // reset counter

    BDCHelper(hund, hunds); // convert hundredths to BCD equivalent
    counter = 0; // reset counter

    if(pastTenMinutes){
        // store values as they are
        resultPtr[0] = mins[0]; 
        resultPtr[1] = mins[1];
    }
    else{
        // swap values so that zero comes first
        resultPtr[1] = mins[0]; 
        resultPtr[0] = mins[1];
    }

    resultPtr[2] = ':'; // add colon to time format

    if(pastTenSecs){
        resultPtr[3] = secs[0];
        resultPtr[4] = secs[1];
     }
     else{
        // swap values so that zero comes first
        resultPtr[3] = 0x30;
        resultPtr[4] = secs[0];
     }

    resultPtr[5] = '.'; // add decimal point to time format
    resultPtr[6] = hunds[0]; resultPtr[7] = hunds[1];

}

void display(void){

    lcd_command(CLEAR); // clear lcd
    if (resetFlag){
        // sw3 pressed
        lcd_command(CLEAR); // clear lcd
        lcd_putstring("Stopwatch"); // write string to lcd
        lcd_command(LINE_TWO); // move cursor to next line
        lcd_putstring("Press SW0..."); // write string to lcd
        TIM14 -> CR1 &= ~TIM_CR1_CEN; // stop timer
        minutes = 0; seconds = 0; hundredths = 0; // reset time values to zero

        // turn on led pb3
        GPIOB -> ODR = 0x00000008;

    }
    if (startFlag && !(lapFlag || stopFlag)){

        // sw0 pressed, start timer
        TIM14 -> CR1 |= TIM_CR1_CEN; // start timer
        convert2BCDASCII(minutes,seconds,hundredths,time);

        lcd_putstring("Time"); // write string to lcd
        lcd_command(LINE_TWO); // move cursor to next line
        
        lcd_putstring(time);

        // turn on led pb0
        GPIOB -> ODR = 0x00000001;
    }
    if(startFlag && lapFlag){
        
        //sw1 pressed, take lap time
        
        if(updateLapFlag){
            convert2BCDASCII(minutes,seconds,hundredths,time);

        }
        lcd_putstring("Time"); // write string to lcd
        lcd_command(LINE_TWO); // move cursor to next line
        lcd_putstring(time); // display time on screen


        // turn on led pb1
        GPIOB -> ODR = 0x00000002;

    } 
    if(startFlag && stopFlag){
        // sw2 pressed, stop timer
        TIM14 -> CR1 &= ~TIM_CR1_CEN; // stop timer

        lcd_putstring("Time"); // write string to lcd
        lcd_command(LINE_TWO); // move cursor to next line

        lcd_putstring(time); // write time to screen
     


        // turn on led pb2
        GPIOB -> ODR = 0x00000004;
    }

}

void BDCHelper(uint8_t n, char * result){
    /**
     * convert value to BCD and then ascii helper
     */

    // bcd values array for 0 - 9
    uint8_t bdcValues[10] = {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39};

    if (n >= 10){
        BDCHelper(n/10, result); // call BCDHelper again
    }

    uint8_t digit = n%10;
    result[counter] = bdcValues[digit]; // add value to result array/pointer
    counter++; // increment counter
}

/**
********************************************************************
 End of program
********************************************************************
*/