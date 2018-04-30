//Author: Fangyi Fan Project AutoSort

#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>


#define METAL_THRESHOLD 160
#define PAPER_THRESHOLD 51
#define AIR_THRESHOLD 20

#define PLASTIC_THRESHOLD 740
#define MEASURING_THRESHOLD 840

volatile unsigned int adc_PHOTO;
volatile unsigned int adc_RADAR;
volatile unsigned int arr[40] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0};

volatile unsigned int index = 0;
volatile unsigned int prev_adc = 890;
volatile unsigned int flag = 1; //cases in the process of measuring

volatile unsigned int decrement_timer = 0; // this timer will be non-zero when non-plastics are detected by photoresistor
volatile unsigned int decrement_timer2 = 0; // this timer will be non-zero when paper is detected by radar

volatile unsigned int adc_0 = 890; //10 value before
volatile unsigned int adc_1 = 890;
volatile unsigned int adc_2 = 890;
volatile unsigned int adc_3 = 890;
volatile unsigned int adc_4 = 890;

unsigned int r_duty_cycle_2 = 2900; //timer counts for straight, right motor
unsigned int r_duty_cycle_1 = 1400; //timer counts for right
unsigned int r_duty_cycle_3 = 3900; //timer counts for left
unsigned int r_off_cycle_2 = 37100;
unsigned int r_off_cycle_1 = 38600;
unsigned int r_off_cycle_3 = 36100;

unsigned int l_duty_cycle_2 = 2500; //timer counts for straight, left motor
unsigned int l_duty_cycle_1 = 1400; //timer counts for right
unsigned int l_duty_cycle_3 = 3500; //timer counts for left
unsigned int l_off_cycle_2 = 37500;
unsigned int l_off_cycle_1 = 38600;
unsigned int l_off_cycle_3 = 36500;

char HiorLo_R = 0;
char HiorLo_L = 0;

volatile unsigned int duty_period_r;
volatile unsigned int off_duty_period_r;
volatile unsigned int duty_period_l;
volatile unsigned int off_duty_period_l;

ISR (TIMER1_COMPA_vect) {
  if (HiorLo_R == 0) {
    OCR1A += duty_period_r;
    HiorLo_R = 1;
  }
  else {
    OCR1A += off_duty_period_r;
    HiorLo_R = 0;
  }
}

ISR (TIMER1_COMPB_vect) {
  if (HiorLo_L == 0) {
    OCR1B += duty_period_l;
    HiorLo_L = 1;
  }
  else {
    OCR1B += off_duty_period_l;
    HiorLo_L = 0;
  }
}

int min_of_arr(volatile unsigned int a[], int num_elements)
{
  volatile int tgt = a[0];
  int i;
  for (i=0; i<num_elements - 1; i++)
  {
    if (tgt > a[i+1]) {
      tgt = a[i+1];
    }
  }
  return tgt;
}

void clear_arr(volatile unsigned int a[], int num_elements)
{
  int i;
  for (i=0; i<num_elements; i++)
  {
    arr[i] = 890;
  }
}

int main() {
  uart_init();

  DDRB |= 0x06; //set pin9 and pin 10 to output
  TCCR1B |= 0x02; //Initialize timer with 8 prescalar
  sei();

  ADCSRA |= 0x07; // ADC prescaler to 1/128 clocks -> 9.6 kHz sampling frequency
  ADMUX |= (1<<REFS0);
  ADMUX &= ~(1<<REFS1); // AREF = 5V
  ADCSRA |= 0x80; // enable ADC

  while (1){
    // printf("%d", flag);
    ADMUX |= (1<<MUX1); //use pin A2
    ADCSRA |= 0x40; // Start an ADC measurement
    while(ADCSRA & 0x40);
    adc_PHOTO = ADC;
    //850 780

    // if non-plastic detected, then if condition will be true
    if (decrement_timer > 0){
      --decrement_timer;
      adc_PHOTO = 0;
    }

    if (adc_PHOTO > MEASURING_THRESHOLD) {
      if (flag == 0) {
        flag = 1; //start measuring
      }
      else if ((flag == 1) && (prev_adc < MEASURING_THRESHOLD)) {
        printf ("it is plastic!!!!!!!!!!!\n");

        flag = 0;
        // index = 0;
        // clear_arr(arr, 40);

        TCCR1A |= 0x50; //enable output compare on OCR1A and OCR1B to toggle
        duty_period_r = r_off_cycle_2;
        duty_period_l = l_off_cycle_2;
        off_duty_period_r = r_duty_cycle_2;
        off_duty_period_l = l_duty_cycle_2;
        OCR1A = TCNT1 + 16; //pull PB1 pin high quickly
        OCR1A += duty_period_r;
        OCR1B = TCNT1 + 16; //pull PB2 pin high quickly
        OCR1B += duty_period_l;
        TIMSK1 |= 0x06; //enable interrupt for OCR1A and OCR1B
      }
    }
    else if (flag == 1) {
      if (adc_PHOTO < MEASURING_THRESHOLD) {
        printf("%d\n", ADC);

        if (adc_PHOTO < PLASTIC_THRESHOLD) {
          flag = 0;
          printf ("it is other\n");
          decrement_timer = 10000;

          // begin an ADC conversion from the radar sensor.
          ADMUX &= 0xF0; // use ADC pin A0
          ADCSRA |= 0x40; // Start an ADC measurement
          while(ADCSRA & 0x40);
          adc_RADAR = ADC;
          printf("RADAR VALUE: %d\n", adc_RADAR);

          if (decrement_timer2 > 0){
            --decrement_timer2;
            adc_RADAR = 0;
          }

          // assume that the material is a paper unless the ADC conversion crosses the metal threshold
          if (adc_RADAR > METAL_THRESHOLD){
            printf ("it is a metal\n");
            TCCR1A |= 0x50; //enable output compare on OCR1A and OCR1B to toggle
            duty_period_r = r_off_cycle_1;
            duty_period_l = l_off_cycle_1;
            off_duty_period_r = r_duty_cycle_1;
            off_duty_period_l = l_duty_cycle_1;
            OCR1A = TCNT1 + 16; //pull PB1 pin high quickly
            OCR1A += duty_period_r;
            OCR1B = TCNT1 + 16; //pull PB2 pin high quickly
            OCR1B += duty_period_l;
            TIMSK1 |= 0x06; //enable interrupt for OCR1A and OCR1B
          }
          else {
            // printf ("it is paper\n");
            // decrement_timer2 = 10000;
            // TCCR1A |= 0x50; //enable output compare on OCR1A and OCR1B to toggle
            // duty_period_r = r_off_cycle_3;
            // duty_period_l = l_off_cycle_3;
            // off_duty_period_r = r_duty_cycle_3;
            // off_duty_period_l = l_duty_cycle_3;
            // OCR1A = TCNT1 + 16; //pull PB1 pin high quickly
            // OCR1A += duty_period_r;
            // OCR1B = TCNT1 + 16; //pull PB2 pin high quickly
            // OCR1B += duty_period_l;
            // TIMSK1 |= 0x06; //enable interrupt for OCR1A and OCR1B
          }
          // int c;
          // int d;
          // for ( c = 1 ; c <= 32700; c++ )
          //   for ( d = 1 ; d <= 32700; d++ )
          //     {}

        }
        // else {
        //   arr[index] = ADC;
        //   printf("%d is %d \n", index, ADC);
        //   index++;
        // }
      }
    }
    prev_adc = adc_PHOTO;
  }
  return 0;
}
