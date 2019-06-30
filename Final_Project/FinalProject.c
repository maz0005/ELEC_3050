/*=========================================================*/
/* Marco A. Zuniga                                         */
/* ELEC 3050 - Final Project                               */
/* The following code will print a stopwatch on Keil's     */ 
/* Waveform application and handle a pulse width           */
/* modulated DC that will adjust itself when under a       */
/* a load                                                  */
/*This code is compatible with a STM32L100 microcontroller */
/*=========================================================*/

#include "STM32L1xx.h"  /* Microcontroller information*/
/*Global Variables*/

/**!!!!!Adjust PCB for different ouput periods
int ARR_Calculated = ((Fclk*Tcnt)/(PSC+1)) - 1
When you change timer10PSC, change it in timer10ARR as well.
*/

unsigned char count1; //.1 second increment count
unsigned char count2; //1 second increment count
unsigned char startstop; //1 start 0 stop. State of stopwatch


//Tout = (0x064 x 0x831) / 16000000 = .1 seconds for count 1
int timer4PSC = 99;
int timer4ARR = 15999;  //Stop watch every .1 seconds

int DR = 0;

int timer10PSC = 0;
int timer10ARR = (((16000000)*.001)/(0 + 1)) - 1;  //PWM Period


int timer11PSC = 9; 
int timer11ARR = 15999;   //ADC sampling

int voltages[] = {0, 383, 568, 753, 938, 1123, 1308, 1493, 1678, 1863, 2048};
int buttonPressed = 0; 

int errors[] = {0, 0, 0};
int errorIndex = 0;
int error1 = 0;
int error2 = 1; 
int error3 = 2;

//Will be shifted in formula
//1.0589
unsigned long a0 = 271;
//3.4787
unsigned long a1 = 890;
//-0.08145
unsigned long a2 = 21;

void PinSetup() {
/*16 MHz clock*/	
RCC->CR |= RCC_CR_HSION; // Turn on 16MHz HSI oscillator
while ((RCC->CR & RCC_CR_HSIRDY) == 0); // Wait until HSI ready
RCC->CFGR |= RCC_CFGR_SW_HSI; // Select HSI as system clock

/* Enable GPIOA clock (bit 0) */
RCC->AHBENR |= 0x01; 
/*PA1 as an input for the interrupt. Output from AND gate.*/
/*PA6 as alternate function.*/
GPIOA->MODER &= ~(0x0000300C); //Clear the bit first
GPIOA->MODER |= (0x00002000); //~(0010000000001100)  //Enable PA6 AF
/*Enable GPIOB clock (bit 1) */
RCC->AHBENR |= 0x02; 
/*PB3-PB0 Inputs read from rows. Make sure PB7-4 are cleared*/
GPIOB->MODER &= ~(0x0000FFFF); 
/*PB7-PB4 Outputs from columns*/
GPIOB->MODER |= (0x00005500); 
GPIOB->ODR &= ~(0x00F0);  //Make sure all columns are grounded.
/*Set up pull up resistors*/
GPIOB->PUPDR &= ~(0x000000FF); //Clear bits 7-0
GPIOB->PUPDR |= (0x00000055); //0101_0101; PB3-PB0
/* Enable GPIOC clock (bit 2) */
RCC->AHBENR |= 0x04; 
GPIOC->MODER &= ~(0x000000FF); //Clear PC3-PC0 mode bits
/*PC7-PC0 as outputs*/
GPIOC->MODER |= (0x00005555);

/*----------------------Timer Interrupt Configurations(BSD Stopwatch)------------------*/
RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Enable the clock to time TIM4
TIM4->PSC = timer4PSC;  //prescale value
TIM4->ARR = timer4ARR;  //auto-reload value
TIM4->DIER |= TIM_DIER_UIE; //Enable interrupts for TIM10
NVIC_EnableIRQ(TIM4_IRQn); //Enable interrupts from TIM10 at the NVIC.
NVIC_ClearPendingIRQ(TIM4_IRQn);
NVIC_SetPriority(TIM4_IRQn, 2);

/*------------------------External Interrupt Configuration---------------*/
/*System Configuration Module*/
SYSCFG->EXTICR[0] &= 0xFF0F;  //Choose PA1 through multiplexer
/*Configure PA1 as falling triggered*/
EXTI->FTSR |= 0x0002;
/*Enable EXTI1*/
EXTI->IMR |= 0x0002;
/*Clear EXTI1 pending status*/
EXTI->PR |= 0x0002;
/*Nested Vectored Interrupt Controller Enable*/
NVIC_SetPriority(EXTI1_IRQn, 1);
NVIC_EnableIRQ(EXTI1_IRQn);   //EXTI1_IRQn = 7
NVIC_ClearPendingIRQ(EXTI1_IRQn); //Clearing pending bit at NVIC
/*-------------------------Timer 11 Configurations -----------------------*/
GPIOA->AFR[0] &= ~(0x0F000000); //Clear AFRL6
GPIOA->AFR[0] |= 0x03000000; //PA6 = AF3
RCC->APB2ENR |= RCC_APB2ENR_TIM10EN; //Enable the clock to TIM10.Bit 0
TIM10->PSC = timer10PSC;  //prescale value
TIM10->ARR = timer10ARR;  //auto-reload value
NVIC_EnableIRQ(TIM10_IRQn); //Enable interrupts from TIM10 at the NVIC.
NVIC_ClearPendingIRQ(TIM10_IRQn);
TIM10->CR1 |= (TIM_CR1_CEN); //Timer System Control Register. Enable CEN
TIM10->SR &= ~(0xFF); //Timer Status Register. Clear CC1IF
TIM10->CCMR1 &= ~(0x73); //Clear bits 6-4/1-0 in Capture/Compare Mode Register
TIM10->CCMR1 |= (0x60); //PWM mode 1(active to inactive)/Output
TIM10->CCER &= ~(0x02); //Clear bit 1 at Capture/Compare Enable Register. OC1 active high
TIM10->CCER |= (TIM_CCER_CC1E);//bit 0 '1' = OC1 drives output pin
TIM10->CCR1 = (timer10ARR + 1) * 0; //Initialize duty cycle as 0%
NVIC_ClearPendingIRQ(TIM10_IRQn);
/*-------------------------Lab 11 Modifications--------------------------*/
NVIC_SetPriority(TIM11_IRQn, 0);
RCC->APB2ENR |= RCC_APB2ENR_TIM11EN; //Enable the clock to TIM11 bit 5
TIM11->PSC = timer11PSC;
TIM11->ARR = timer11ARR;
NVIC_EnableIRQ(TIM11_IRQn);
NVIC_ClearPendingIRQ(TIM11_IRQn);
TIM11->DIER |= (TIM_DIER_UIE); //Make sure UIE is enabled
TIM11->CR1 |= (TIM_CR1_CEN);

/*-------Enable Counter and set to 0--------*/
GPIOA->MODER |= (0x00000003); //Set PA0 as Analog
RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; //ADC1 clock enable 
/*Set as scan mode*/
ADC1->CR1 &= ~(ADC_CR1_SCAN);
ADC1->CR2 &= ~(ADC_CR2_CONT);

ADC1->CR2 |= ADC_CR2_ADON;  //Power on the ADC
while ((ADC1->SR & ADC_CSR_ADONS1) == 0); //Wait for ADC signal to start convert

/*Input channel*/
ADC1->SQR1 &= ~ADC_SQR1_L; //set L=0 Single Conversion
ADC1->SQR5 &= ~(ADC_SQR5_SQ1); //Clear SQ1 bits for ADC1_IN0. First channel in sequence

//ADC1->SMPR3 &= ~(0x00000007);
//ADC1->SMPR3 |= (0x00000003);   //Tsampling to 24 clock cycles

__enable_irq();  //Enable Interrupts Globally
}

/*-----------------------------------------------------------*/
/* Delay from writing to columns and reading from rows*/
/* Delay to handle debouncing before clearing pending bits and exiting*/
/*-----------------------------------------------------------*/
void smallDelay() {
int i;
for (i = 0; i < 2000; i++){
	i++;
	i--;
}
}

void EXTI1_IRQHandler() {
	smallDelay();
	smallDelay();
	
/*Note: This is for a typical 16-KEY MATRIX KEYPAD*/
/*Remember to delay between writing a 0 to a column and reading from the rows.*/
unsigned char currentColumn = 1;  //Keep track on which column we're setting to 0.
unsigned char currentRow = 0; //Keep incrementing to get appropriate value from array "rows"

int keys[] = {1, 4, 7, 15, 2, 5, 8, 0, 3, 6, 9, 14, 10, 11, 12, 13};  //Row "1-16" 5-8 indicates on second row

float percentages[] = {0, .35, .40, .45, .50, .55, .60, .65, .70, .75, .80}; //Get duty cycle percentage 

				
while(currentColumn < 5) {
	//-----------------------------Find out what column you're on--------------------------------
	if(currentColumn == 1) {
		GPIOB->ODR &= ~(0x00F0); //Clear column bits 
		GPIOB->ODR |= (0x0070); //Column 1 0111 From top to bottom 1-4-7-F
		smallDelay();
	}
	else if(currentColumn == 2) {
		GPIOB->ODR &= ~(0x00F0); //Clear column bits 
		GPIOB->ODR |= (0x00B0); //Column 2 1011 From top to bottom 2-5-8-0
		smallDelay();
	}
	else if(currentColumn == 3) {
		GPIOB->ODR &= ~(0x00F0); //Clear column bits 
		GPIOB->ODR |= (0x00D0); //Column 3 1101 From top to bottom 3-6-9-E
		smallDelay();
	}
	else {
		GPIOB->ODR &= ~(0x00F0); //Clear column bits 
		GPIOB->ODR |= (0x00E0); //Column 4 1110 From top to bottom A-B-C-D
		smallDelay();
	}

	//------------End of grounding a specific column. Now break when finding row with 0.--------------------//
	
	if ((GPIOB->IDR & ~(0xFFFE)) == 0){  //~(1110) => 0001
		break;
	}
	currentRow++;
    if(((GPIOB->IDR & ~(0xFFFD)) >> 1) == 0){ //~(1101) => 0010
		break;
	}
    currentRow++;
	if(((GPIOB->IDR & ~(0xFFFB)) >> 2) == 0) { //~(1011) => 0100
		break;
	}
    currentRow++;
	if(((GPIOB->IDR & ~(0xFFF7)) >> 3) == 0) {    //~(0111) => 1000
    	break;	
	}
    currentRow++;
	currentColumn++; //In case current column is not found
	}//-----------------End of while loop------------------------------
	
	/*------------------Found Key Pressed. In rows[currentRows]----------*/


            /* Now Modify the duty cycle or stopwatch*/
if (keys[currentRow] == 0x00) {
	buttonPressed = keys[currentRow];
	TIM10->CCR1 = 0x0000;
	TIM10->CNT = 0;
	TIM10->CR1 &= ~(TIM_CR1_CEN); //Timer System Control Register. Disable CEN/Counter
}

else if (keys[currentRow] <= 10){  //Key A or below was pressed
	buttonPressed = keys[currentRow];
	TIM10->CNT = 0;
	TIM10->CCR1 = (timer10ARR + 1)*(percentages[keys[currentRow]]);
	TIM10->CR1 |= (TIM_CR1_CEN); //Timer System Control Register. Enable CEN
}

else if (keys[currentRow] == 15){   //Start/Stop
	if (startstop == 0) {  //Enable counter. start
		TIM4->CR1 |= 0x01; //Enable the counter
		startstop = 1;
		}
	else {
		TIM4->CR1 &= ~(0x01); //Disable counter. stop
		startstop = 0;
	}
}

else if (keys[currentRow] == 14){   //Reset
	if (startstop == 0) {  //can only reset in stop state.
		count1 = 0;
		count2 = 0;
		TIM4->CNT = 0;
		GPIOC->ODR &= ~(0x00FF);
	}	
}


/*Handle Debouncing*/
for(int j = 0; j < 250; j++) smallDelay();

GPIOB->ODR &= ~(0x00F0);  //Set all columns back to 0
EXTI->PR |= 0x0002; //Clear EXTI1 pending status. We clear by writing a 1
NVIC_ClearPendingIRQ(EXTI1_IRQn);
}

/*
*Handle the stopwatch
*/
void TIM4_IRQHandler() {
if (count1 == 0x09) { //Reset to 0
	count1 = 0;
	
	if (count2 == 0x09) { //should only increment when first counter goes from 9 to 0
		count2 = 0;	
	}
	
	else {
		count2++;
	}
}

else {
	count1++;
}

GPIOC->ODR &= ~(0x00FF);  //Clear output
GPIOC->ODR |= ((count2 << 4) | count1); //Display new stopwatch value
TIM4->SR &= ~(TIM_SR_UIF);      //Clear the UIF(Update Interrupt Flag). Write 0
NVIC_ClearPendingIRQ(TIM4_IRQn);   //clear pending bit at the NVIC
}


void TIM11_IRQHandler() {
	
ADC1->CR2 |= ADC_CR2_SWSTART;  //Start conversion
while ((ADC1->SR & ADC_SR_EOC) == 0); //Wait for end of conversion (EOC=1)
DR = ADC1->DR;

if (DR < voltages[buttonPressed]) {
		TIM10->CCR1 = TIM10->CCR1 + 1;
}

else if (DR > voltages[buttonPressed]) {
			TIM10->CCR1 = TIM10->CCR1 - 1;
}

TIM11->SR &= ~(TIM_SR_UIF);
NVIC_ClearPendingIRQ(TIM11_IRQn);	

}

	
void main () {

 PinSetup(); 
 while (1); //Keep program running.

}
