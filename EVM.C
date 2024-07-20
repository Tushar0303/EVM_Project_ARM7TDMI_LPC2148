#include <lpc214x.h>          
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

// Function prototypes
void delay(unsigned int count);
void Terminal_INIT(void);
void LCD_CMD(char command);
void LCD_INIT(void);
void LCD_STRING(char* msg);
void UART_SendString(char* str);
void UART0_IRQHandler(void) __irq;

// Global variables for vote counting and display
unsigned int can1 = 0, can2 = 0, can3 = 0, can4 = 0;
char txt1[] = "Candidate 1 :";
char txt2[] = "Candidate 2 :";
char txt3[] = "Candidate 3 :";
char txt4[] = "Candidate 4 :";
char num[2]; // Buffer for number conversion
char *info = "Candidate 1: A \n\r\r\nCandidate 2: B \n\r\r\nCandidate 3: C \n\r\r\nCandidate 4: D \n\r\r\n";
char Wintxt[] = "Winner ";
char NoWintxt[] = "No Winner ";

// UART Receive Buffer
#define BUFFER_SIZE 64
char rxBuffer[BUFFER_SIZE];
volatile unsigned int rxWriteIndex = 0;
volatile unsigned int rxReadIndex = 0;

// Function definitions
void delay(unsigned int count) {
    unsigned int j, k;
    for (j = 0; j <= count; j++)
        for (k = 0; k <= 600; k++);
}

void Terminal_INIT(void) {
    PINSEL0 |= 0x00000005; // Set UART0 pins
    U0LCR = 0x83;          // 8 bits, no parity, 1 stop bit, DLAB = 1
    U0DLL = 97;            // Set baud rate (assuming 14.7456 MHz clock)
    U0LCR = 0x03;          // Clear DLAB to access the divisor latch

    U0FCR = 0x07;          // Enable FIFO, reset RX and TX FIFO
    U0IER = 0x01;          // Enable RBR interrupt

    // Enable UART0 interrupt in VIC
    VICIntSelect &= ~(1 << 6); // UART0 interrupt is assigned to IRQ (not FIQ)
    VICVectCntl0 = 0x20 | 6;   // Enable slot 0 and assign UART0 interrupt to it
    VICVectAddr0 = (unsigned long)UART0_IRQHandler; // Set ISR address
    VICIntEnable = 1 << 6;     // Enable UART0 interrupt
}

void UART0_IRQHandler(void) __irq {
    uint8_t IIRValue, LSRValue;
    uint8_t data;
    unsigned int nextIndex;

    IIRValue = U0IIR; // Read Interrupt ID Register

    if ((IIRValue & 0x4) == 0x4) { // RDA interrupt
        while (U0LSR & 0x01) { // While data is available in the receiver buffer
            data = U0RBR; // Read received data

            nextIndex = (rxWriteIndex + 1) % BUFFER_SIZE;
            if (nextIndex != rxReadIndex) {
                rxBuffer[rxWriteIndex] = data;
                rxWriteIndex = nextIndex;
            }
        }
    }

    // Check for overrun error
    LSRValue = U0LSR;
    if (LSRValue & 0x02) { // Overrun error
        volatile uint8_t dummy = U0RBR; // Clear the error by reading U0RBR
        (void)dummy; // Prevent unused variable warning
    }

    VICVectAddr = 0x00000000; // Acknowledge interrupt
}

void UART_SendString(char* str) {
    int i = 0;
    while (str[i] != '\0') {
        while (!(U0LSR & 0x20)); // Wait until THR is empty
        U0THR = str[i]; // Send the character
        i++;
    }
}

void LCD_CMD(char command) {
    IO0PIN = ((IO0PIN & 0xFFFF00FF) | ((command & 0xF0) << 8)); 
    IO0SET = 0x00000040; // RS = 0 for command
    IO0CLR = 0x00000030; // Enable = 0
    delay(5);
    IO0CLR = 0x00000040; // Disable
    delay(5);
    IO0PIN = ((IO0PIN & 0xFFFF00FF) | ((command & 0x0F) << 12));
    IO0SET = 0x00000040; // RS = 0 for command
    IO0CLR = 0x00000030; // Enable = 0
    delay(5);
    IO0CLR = 0x00000040; // Disable
    delay(5);    
}

void LCD_INIT(void) {
    IO0DIR = 0x0000FFF0; // Set pins for LCD
    delay(20);
    LCD_CMD(0x02); // Initialize LCD in 4-bit mode
    LCD_CMD(0x28); // 2 lines, 5x7 matrix
    LCD_CMD(0x0C); // Display ON, Cursor OFF
    LCD_CMD(0x06); // Increment cursor
    LCD_CMD(0x01); // Clear display
    LCD_CMD(0x80); // Set cursor to the beginning
}

void LCD_STRING(char* msg) {
    uint8_t i = 0;
    while (msg[i] != 0) {
        IO0PIN = ((IO0PIN & 0xFFFF00FF) | ((msg[i] & 0xF0) << 8));
        IO0SET = 0x00000050; // RS = 1 for data
        IO0CLR = 0x00000020; // Enable
        delay(2);
        IO0CLR = 0x00000040; // Disable
        delay(5);
        IO0PIN = ((IO0PIN & 0xFFFF00FF) | ((msg[i] & 0x0F) << 12));
        IO0SET = 0x00000050; 
        IO0CLR = 0x00000020; 
        delay(2);
        IO0CLR = 0x00000040; 
        delay(5);
        i++;
    }
}

int main() {
    unsigned int i = 0, n = 8; // Loop counter and total votes

    Terminal_INIT(); // Initialize UART
    LCD_INIT(); // Initialize LCD

    // Send initial info to terminal
    UART_SendString(info);

    // Set up GPIO for voting
    PINSEL2 = 0xFFFF0000; // Configure pins for input
    IO1DIR |= 0x00100000; // Set pin as output for LED indication
    IO1DIR &= 0xFFF0FFFF; // Set pins as input for buttons

    while (i < n) {
        // Display vote counts for each candidate
        LCD_CMD(0x80);
        sprintf(num, "%d", can1);
        LCD_STRING(txt1);
        LCD_CMD(0xC0);
        LCD_STRING(num);
        delay(5000);

        LCD_CMD(0x80);
        sprintf(num, "%d", can2);
        LCD_STRING(txt2);
        LCD_CMD(0xC0);
        LCD_STRING(num);
        delay(5000);

        LCD_CMD(0x80);
        sprintf(num, "%d", can3);
        LCD_STRING(txt3);
        LCD_CMD(0xC0);
        LCD_STRING(num);
        delay(5000);

        LCD_CMD(0x80);
        sprintf(num, "%d", can4);
        LCD_STRING(txt4);
        LCD_CMD(0xC0);
        LCD_STRING(num);
        delay(5000);

        // Check for votes for each candidate
        if (IO1PIN & 0x00010000) { can1++; IO1SET = 0x00100000; delay(200); IO1CLR = 0x00100000; }
        if (IO1PIN & 0x00020000) { can2++; IO1SET = 0x00100000; delay(200); IO1CLR = 0x00100000; }
        if (IO1PIN & 0x00040000) { can3++; IO1SET = 0x00100000; delay(200); IO1CLR = 0x00100000; }
        if (IO1PIN & 0x00080000) { can4++; IO1SET = 0x00100000; delay(200); IO1CLR = 0x00100000; }

        i++; // Increment vote count
    }

    // Display total votes
    sprintf(num, "%d", i);
    LCD_CMD(0x80);
    LCD_STRING("Total Votes: ");
    LCD_CMD(0xC0);
    LCD_STRING(num);

    // Determine the winner
    if ((can1 > can2) && (can1 > can3) && (can1 > can4)) {
        UART_SendString(Wintxt);
        UART_SendString(txt1);
    }
		else if ((can2 > can1) && (can2 > can3) && (can2 > can4)) {
        UART_SendString(Wintxt);
        UART_SendString(txt2);
    } 
		else if ((can3 > can1) && (can3 > can2) && (can3 > can4)) {
        UART_SendString(Wintxt);
        UART_SendString(txt3);
    }
		else if ((can4 > can1) && (can4 > can2) && (can4 > can3)) {
        UART_SendString(Wintxt);
        UART_SendString(txt4);
    } 
		else {
        UART_SendString(NoWintxt);
    }

    return 0; // End of main function
}
