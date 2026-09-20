#include "stm32f4xx.h"
#include <stdio.h>

/* =====================================================
   Adaptive Physiological Monitoring
   Bare-Metal Register Programming using CMSIS
   ===================================================== */

/* Global variables */
volatile uint32_t adc_value = 0;
volatile uint32_t heart_rate = 0;

volatile uint32_t last_beat_time = 0;
volatile uint32_t current_time = 0;

volatile uint8_t sampling_mode = 1;

/*
   Sampling modes:
   0 = 10 Hz
   1 = 50 Hz
   2 = 250 Hz
   3 = 500 Hz
*/

volatile uint32_t sampling_rate = 50;

/* Sensor threshold - must be calibrated */
#define PULSE_THRESHOLD     2000


/* =====================================================
   FUNCTION PROTOTYPES
   ===================================================== */

void GPIO_Init(void);
void ADC_Init(void);
void USART2_Init(void);
void SysTick_Init(void);

uint32_t ADC_Read(void);

void USART2_SendChar(char c);
void USART2_SendString(char *str);

void Delay_ms(uint32_t ms);

void Calculate_Heart_Rate(void);
void Adaptive_Monitoring(void);
void Set_Sampling_Mode(uint8_t mode);

void Send_Monitoring_Data(void);


/* =====================================================
   MAIN FUNCTION
   ===================================================== */

int main(void)
{
    /* Initialize peripherals */
    GPIO_Init();
    ADC_Init();
    USART2_Init();
    SysTick_Init();

    USART2_SendString(
        "STM32 Adaptive Physiological Monitor\r\n"
    );

    USART2_SendString(
        "CMSIS Bare-Metal Mode\r\n\r\n"
    );


    while (1)
    {
        /* ---------------------------------------------
           1. Read physiological signal
           --------------------------------------------- */

        adc_value = ADC_Read();


        /* ---------------------------------------------
           2. Calculate heart rate
           --------------------------------------------- */

        Calculate_Heart_Rate();


        /* ---------------------------------------------
           3. Adaptive monitoring decision
           --------------------------------------------- */

        Adaptive_Monitoring();


        /* ---------------------------------------------
           4. Send data through UART
           --------------------------------------------- */

        Send_Monitoring_Data();


        /* ---------------------------------------------
           5. Adaptive delay
           --------------------------------------------- */

        if (sampling_mode == 0)
        {
            /* 10 Hz */
            Delay_ms(100);
        }

        else if (sampling_mode == 1)
        {
            /* 50 Hz */
            Delay_ms(20);
        }

        else if (sampling_mode == 2)
        {
            /* 250 Hz */
            Delay_ms(4);
        }

        else
        {
            /* 500 Hz */
            Delay_ms(2);
        }
    }
}


/* =====================================================
   GPIO INITIALIZATION
   PA0  -> ADC input
   PA2  -> USART2 TX
   PA3  -> USART2 RX
   ===================================================== */

void GPIO_Init(void)
{
    /* Enable GPIOA clock */

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;


    /* -------------------------------------------------
       PA0 = Analog input
       ------------------------------------------------- */

    GPIOA->MODER |= (3U << (0 * 2));


    /* -------------------------------------------------
       PA2 = USART2 TX
       PA3 = USART2 RX

       Alternate Function mode = 10
       ------------------------------------------------- */

    GPIOA->MODER &= ~(3U << (2 * 2));
    GPIOA->MODER &= ~(3U << (3 * 2));

    GPIOA->MODER |= (2U << (2 * 2));
    GPIOA->MODER |= (2U << (3 * 2));


    /* AF7 = USART2 */

    GPIOA->AFR[0] &= ~(0xFU << (2 * 4));
    GPIOA->AFR[0] &= ~(0xFU << (3 * 4));

    GPIOA->AFR[0] |= (7U << (2 * 4));
    GPIOA->AFR[0] |= (7U << (3 * 4));


    /* Push-pull output */

    GPIOA->OTYPER &= ~(1U << 2);
    GPIOA->OTYPER &= ~(1U << 3);


    /* High speed */

    GPIOA->OSPEEDR |= (3U << (2 * 2));
    GPIOA->OSPEEDR |= (3U << (3 * 2));
}


/* =====================================================
   ADC INITIALIZATION
   ADC1 Channel 0 -> PA0
   ===================================================== */

void ADC_Init(void)
{
    /* Enable ADC1 clock */

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;


    /* ADC common settings */

    ADC->CCR = 0;


    /* Disable ADC before configuration */

    ADC1->CR2 = 0;


    /* -------------------------------------------------
       ADC configuration
       ------------------------------------------------- */

    /*
       12-bit resolution
       Right alignment
       Software trigger
    */

    ADC1->CR1 = 0;

    ADC1->CR2 = 0;


    /* -------------------------------------------------
       Channel selection
       Channel 0 = PA0
       ------------------------------------------------- */

    ADC1->SQR1 = 0;

    ADC1->SQR3 = 0;


    /* Channel 0 as first conversion */

    ADC1->SQR3 |= 0;


    /* -------------------------------------------------
       Sampling time
       ------------------------------------------------- */

    ADC1->SMPR2 &= ~(7U << 0);

    /* Medium sampling time */

    ADC1->SMPR2 |= (4U << 0);


    /* Enable ADC */

    ADC1->CR2 |= ADC_CR2_ADON;
}


/* =====================================================
   ADC READ FUNCTION
   ===================================================== */

uint32_t ADC_Read(void)
{
    uint32_t value;


    /* Start conversion */

    ADC1->CR2 |= ADC_CR2_SWSTART;


    /* Wait for conversion complete */

    while (!(ADC1->SR & ADC_SR_EOC))
    {
    }


    /* Read ADC result */

    value = ADC1->DR;


    return value;
}


/* =====================================================
   USART2 INITIALIZATION
   115200 BAUD
   ===================================================== */

void USART2_Init(void)
{
    /* Enable USART2 clock */

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;


    /*
       Assuming APB1 peripheral clock = 16 MHz

       Baud rate = 115200

       Approximate BRR value:
       16,000,000 / 115200 = 138.88
    */

    USART2->BRR = 0x008B;


    /* Enable transmitter and receiver */

    USART2->CR1 = 0;

    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_RE;


    /* Enable USART */

    USART2->CR1 |= USART_CR1_UE;
}


/* =====================================================
   SEND ONE CHARACTER
   ===================================================== */

void USART2_SendChar(char c)
{
    /* Wait until transmit data register is empty */

    while (!(USART2->SR & USART_SR_TXE))
    {
    }


    /* Send character */

    USART2->DR = c;
}


/* =====================================================
   SEND STRING
   ===================================================== */

void USART2_SendString(char *str)
{
    while (*str)
    {
        USART2_SendChar(*str);

        str++;
    }
}


/* =====================================================
   SYSTICK INITIALIZATION
   ===================================================== */

void SysTick_Init(void)
{
    /*
       Assuming system clock = 16 MHz

       SysTick interrupt every 1 ms
    */

    SysTick->LOAD = 16000 - 1;

    SysTick->VAL = 0;

    SysTick->CTRL =
        SysTick_CTRL_CLKSOURCE_Msk |
        SysTick_CTRL_ENABLE_Msk;
}


/* =====================================================
   DELAY FUNCTION
   ===================================================== */

void Delay_ms(uint32_t ms)
{
    uint32_t start;

    start = current_time;

    while ((current_time - start) < ms)
    {
    }
}


/* =====================================================
   SYSTICK INTERRUPT
   ===================================================== */

void SysTick_Handler(void)
{
    current_time++;
}


/* =====================================================
   HEART RATE CALCULATION
   ===================================================== */

void Calculate_Heart_Rate(void)
{
    static uint8_t pulse_detected = 0;

    /*
       Detect rising edge of pulse signal
    */

    if ((adc_value > PULSE_THRESHOLD) &&
        (pulse_detected == 0))
    {
        pulse_detected = 1;


        /*
           Calculate time between two beats
        */

        if (last_beat_time != 0)
        {
            uint32_t beat_interval;

            beat_interval =
                current_time - last_beat_time;


            /*
               Accept realistic heartbeat intervals
            */

            if ((beat_interval > 300) &&
                (beat_interval < 2000))
            {
                heart_rate =
                    60000 / beat_interval;
            }
        }


        last_beat_time = current_time;
    }


    /*
       Wait for signal to go below threshold
    */

    if (adc_value < PULSE_THRESHOLD)
    {
        pulse_detected = 0;
    }
}


/* =====================================================
   ADAPTIVE MONITORING DECISION
   ===================================================== */

void Adaptive_Monitoring(void)
{
    /*
       Normal physiological condition
       -> Standard monitoring
    */

    if (heart_rate >= 50 &&
        heart_rate <= 100)
    {
        Set_Sampling_Mode(1);
    }


    /*
       Low heart rate
       -> Increase monitoring
    */

    else if (heart_rate > 0 &&
             heart_rate < 50)
    {
        Set_Sampling_Mode(2);
    }


    /*
       Elevated heart rate
       -> Increase monitoring
    */

    else if (heart_rate > 100 &&
             heart_rate <= 130)
    {
        Set_Sampling_Mode(2);
    }


    /*
       Very high heart rate
       -> Diagnostic mode
    */

    else if (heart_rate > 130)
    {
        Set_Sampling_Mode(3);
    }


    /*
       No reliable heart rate
       -> Standard monitoring
    */

    else
    {
        Set_Sampling_Mode(1);
    }
}


/* =====================================================
   SET SAMPLING MODE
   ===================================================== */

void Set_Sampling_Mode(uint8_t mode)
{
    sampling_mode = mode;


    if (mode == 0)
    {
        sampling_rate = 10;
    }

    else if (mode == 1)
    {
        sampling_rate = 50;
    }

    else if (mode == 2)
    {
        sampling_rate = 250;
    }

    else
    {
        sampling_rate = 500;
    }
}


/* =====================================================
   SEND MONITORING DATA
   ===================================================== */

void Send_Monitoring_Data(void)
{
    char buffer[100];

    sprintf(
        buffer,
        "DATA,HR=%lu,ADC=%lu,MODE=%d,RATE=%luHz\r\n",
        heart_rate,
        adc_value,
        sampling_mode,
        sampling_rate
    );

    USART2_SendString(buffer);
}
