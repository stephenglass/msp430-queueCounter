/*
Stephen Glass
lineCounter Project - main.c
https://stephen.glass
*/

/* Ultrasonic Sensor Calculations:

VCC (Sensor): 5V
ADC10: 2^(10) = 1024

2.5V Reference:
Distance = ADC_Result * ((2.5/1024) * (1 in./0.0098V))
Distance = ADC_Result * [0.00244140625 * 102.0408163265]
Distance = ADC_Result * 0.2491230
*/

#include <msp430.h>

/* Constant Definitions */
#define DELAY_PARAM_1       1000000 // __delay_cycles only takes constants
#define DELAY_PARAM_2       2000000
#define DELAY_PARAM_3       3000000
#define DELAY_PARAM_4       4000000
#define DELAY_PARAM_5       5000000
#define DELAY_PARAM_6       6000000
#define DELAY_PARAM_7       7000000
#define DELAY_PARAM_8       8000000
#define DELAY_PARAM_9       9000000
#define DELAY_PARAM_10      10000000

#define MAX_SEND_LENGTH     3  // 3 characters or 3 digits to send over WiFi
// MAX_SEND_LENGTH determines maximum amount of digits for sensor reading and maximum TCP request message length
// For this case, 3 is chosen because sensor data is only from 0-255 and TCP message length will not exceed 999 (3 digits/chars)
#define BASE_URL            "GET /api/test.php?data="
#define BASE_URL_END        " HTTP/1.0\r\nHost: 198.54.119.76\r\n\r\n" // Note the space in the beginning - this is necessary formatting
#define TCP_REQUEST         "AT+CIPSTART=\"TCP\",\"198.54.119.76\",80"
#define WIFI_NETWORK_SSID	"Rowan_IOT" // Rowan_IOT network
#define WIFI_NETWORK_PASS	"" // No password for this network
#define BASE_URL_LEN        23 // Length of BASE_URL (\r & \n = 1 character)
#define BASE_URL_END_LEN    34 // Length of BASE_URL_END

#define UPLOAD_RATE         20 // Rate at which the sensor data is uploaded [0.5s]
// UPLOAD_RATE * 0.5 = Update interval (s)
// E.g: 20 * 0.5 = Update every 10s

#define SAMPLE_RATE         10 // Rate at which the sensor data is sampled for conversion [0.5s]
// SAMPLE_RATE * 0.5 = Update interval (s)

//#define ENABLE_RX_IE        1  // Enable if you want to be able to receive information from ESP8266 on MSP430
// This seems to cause more problems than its worth, so this will remain uncommented.
// However, the ability to uncomment and fix these problems are available

//#define ENABLE_CONFIG_WIFI	1 // Enable if you want to program the ESP8266 to connect to WiFi network
// Network configuration on ESP8266 only needs to run once. The ESP8266 keeps memory and will automatically connect to programmed networks
// Only enable if connecting to a new network or updating network settings
// Disable after use

/* Function Prototypes */
void initUART(void);
void initLED(void);
void initADC(void);
void initUploadTimer(void);
void putc(const unsigned c);
void TX(const char *s);
void crlf(void);
void WifiConfigureNetwork(void);
void WiFiSendMessage(unsigned int data);
void WiFiSendLength(unsigned int data);
void WiFiSendTCP();
void appendArray(char* s, char* c);
void appendChar(char* s, char c);
char singleDigitToChar(unsigned int digit);
int calcLen(const char *s);
int readUART(char *search, char len, int delayParam);
int findString(char *c_to_search, char *text, int pos_search, int pos_text, int len_search, int len_text);

/* Variable Initializations */
// Theoretical limit for search length is 256 (char, 2^(8))
char findText[16]; // Max search length of 16 characters
char findIndex = 0;
char startSearching = 0;
char foundResult = 0;
char findLen = 0;

char timerWaitSample = 0;
char timerWaitActivation = 0;
unsigned int distanceReading = 0;

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD;               // Stop WDT
    if (CALBC1_1MHZ == 0xFF)                // If calibration constant erased
    {
        while(1);                           // do not load, trap CPU!!
    }

    initUART();
    initLED();

    #if defined(ENABLE_RX_IE)
        IE2 &= ~UCA0RXIE;                   // Disable UART RX interrupt before we send data
    #endif
    // The line above is extremely important! If interrupts are not disabled, the UART RX interrupt will
    // constantly be busy and will prevent any UART TX
    // Interrupts only enabled when necessary

    #if defined(ENABLE_CONFIG_WIFI)
        WifiConfigureNetwork(); 			// Configure the ESP8266 WiFi settings
    #endif

    /* Send sample TCP request to verify connection and reset data on web server */
    __delay_cycles(10000000);               // Wait for the ESP8266 to settle and fetch WiFi connection
    TX("AT+CIPSTART=\"TCP\",\"198.54.119.76\",80\r\n");
    __delay_cycles(1000000);
    TX("AT+CIPSEND=60\r\n");
    __delay_cycles(1000000);
    TX("GET /api/test.php?data=ESP HTTP/1.0\r\nHost: 198.54.119.76\r\n\r\n"); // Send "ESP" to signal database of a RESET
    /* ------------------------------------------------------------------------- */


    initADC();                              // Initialize the ADC registers
    initUploadTimer();                      // Enable timer to upload at 2Hz frequency

    __bis_SR_register(LPM0_bits + GIE);     // Enter low power mode with interrupts enabled
    // The UploadTimer will interrupt and start ADC sampling and WiFi uploading
}

// Initialize TimerB for sending WiFi timer
void initUploadTimer(void)
{
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL_2 + MC_1 + ID_3;        // SMCLK/8, UPMODE, Compare mode
    TA0CCR0 = 62500;                        // Timer period (125kHz/2Hz), slowest before overflow
}

void initADC(void)
{
    ADC10CTL0 = ADC10SHT_1 + SREF_1 + REFON + REF2_5V + ADC10ON + ADC10IE;
    // ADC Sample-and-Hold time: 8 x ADC10CLKs
    // Select reference: VR+ = VREF+ and VR- = VSS
    // Reference generator: ON
    // Reference generator voltage: 2.5V
    // ADC10 Enable: ON
    // ADC10 Interrupt: ENABLED

    // Data sheet: A4 allows VREF+
    ADC10CTL1 = INCH_4; // Input channel A4
    ADC10AE0 |= BIT4; // ADC Analog enable A4
}

// ADC10 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC10_VECTOR))) ADC10_ISR (void)
#else
#error Compiler not supported!
#endif
{
    distanceReading = ADC10MEM * 0.2491230; // Convert ADC Result to inches, calculation info in header
}

// Configure the ESP8266 for the current WiFi network
// This function needs to only run once. The ESP8266 will always connect to the network after being programmed.
void WifiConfigureNetwork(void)
{
	TX("AT+RST");				// Reset the board
	crlf();						// CR LF
	__delay_cycles(20000000);	// Delay
	TX("AT+CWMODE_DEF=1");	    // Set mode to client (save to flash)
	// Modem-sleep mode is the default sleep mode for CWMODE = 1
	// ESP8266 will automatically sleep and go into "low-power" (15mA average) when idle
	crlf();						// CR LF
	__delay_cycles(20000000);	// Delay
	TX("AT+CWJAP_DEF=\"");      // Assign network settings (save to flash)
	TX(WIFI_NETWORK_SSID);
	TX("\",\"");
	TX(WIFI_NETWORK_PASS);	    // Connect to WiFi network
	TX("\"");
	crlf();
	__delay_cycles(50000000); 	// Delay, wait for ESP8266 to fetch IP and connect
}


// Transmits the TCP request
void WiFiSendTCP()
{
    // TCP Request is a constant string as defined
    TX(TCP_REQUEST); // Requests a TCP connection to the web server
    crlf();
}

// Converts a single digit to its corresponding character
char singleDigitToChar(unsigned int digit)
{
    /*char returnDigit = '0';
    if(digit == 0) returnDigit = '0';
    else if(digit == 1) returnDigit = '1';
    else if(digit == 2) returnDigit = '2';
    else if(digit == 3) returnDigit = '3';
    else if(digit == 4) returnDigit = '4';
    else if(digit == 5) returnDigit = '5';
    else if(digit == 6) returnDigit = '6';
    else if(digit == 7) returnDigit = '7';
    else if(digit == 8) returnDigit = '8';
    else if(digit == 9) returnDigit = '9';
    else returnDigit = '0';
    return returnDigit;*/
    char returnDigit = 48 + digit; // ASCII 48 = '0' - ASCII 57 = '9'
    return returnDigit;
}

// The ESP8266 requires that the message request length be known to the processor before the request is sent. This tells the ESP8266 when the stop searching for data to transmit
// Function calculates the length of the message to be sent and transmits to ESP8266
void WiFiSendLength(unsigned int data) // Input the ADC distance reading
{
    TX("AT+CIPSEND="); // The start of the command, the next parameter is the length
    unsigned int messageLen = BASE_URL_LEN + BASE_URL_END_LEN; // These constants are a given length

    // Find the amount of characters for the data, where the data is the distance from the sensor
    if(data < 10) messageLen += 1; // 0-9, 1 character
    else if(data >= 10 && data < 100) messageLen += 2; // 10-99, 2 characters
    else if(data >= 100) messageLen += 3; // 100-999, 3 characters
    else messageLen += 1; // In case fail check, just set += 1

    // Convert the integer of message length to separated chars. Store in the reverseBuffer array.
    unsigned int i;
    unsigned int reverseIndex = MAX_SEND_LENGTH - 1;
    char reverseBuffer[MAX_SEND_LENGTH - 1];
    for(i = 0; i < MAX_SEND_LENGTH; i++) reverseBuffer[i] = 0; // Initialize all indices at zero
    while(messageLen > 0) // Loop while remaining digits
    {
        unsigned int operatedDigit = messageLen % 10;
        reverseBuffer[reverseIndex] = singleDigitToChar(operatedDigit); // Convert the single digit to its corresponding char
        reverseIndex--;
        messageLen /= 10;
    }

    // The algorithm above outputs in reverse order. Transmit in the correct order to ESP8266.
    for(i = 0; i < MAX_SEND_LENGTH; i++)
    {
        if(reverseBuffer[i] != 0) putc(reverseBuffer[i]); // If initialized indice, transmit individual characters
    }

    crlf();
}

void WiFiSendMessage(unsigned int data) // Input the ADC distance reading
{
    TX(BASE_URL); // Send the beginning part of the request
    // Split the data integer into separate characters
    unsigned int i;
    unsigned int reverseIndex = MAX_SEND_LENGTH - 1;
    char reverseBuffer[MAX_SEND_LENGTH - 1];
    for(i = 0; i < MAX_SEND_LENGTH; i++) reverseBuffer[i] = 0; // Initialize all indices at zero
    while(data > 0)
    {
        unsigned int operatedDigit = data % 10;
        reverseBuffer[reverseIndex] = singleDigitToChar(operatedDigit);
        reverseIndex--;
        data /= 10;
    }

    for(i = 0; i < MAX_SEND_LENGTH; i++)
    {
        if(reverseBuffer[i] != 0) putc(reverseBuffer[i]); // If initialized indice, transmit each individual character
    }

    TX(BASE_URL_END); // Send the ending part of the request
    // No crlf needed because the BASE_URL_END already ends with \r\n\r\n
}

// Timer A0 interrupt service routine (UploadTimer)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void)
{
    if(timerWaitSample >= SAMPLE_RATE) // 10/2 = 5 seconds
    {
        ADC10CTL0 |= ENC + ADC10SC;         // Sampling and conversion start
        timerWaitSample = 0;
    }
    else timerWaitSample++;

    if(timerWaitActivation >= UPLOAD_RATE) // 20 ticks on 2Hz timer, activate every 10 seconds
    {
        timerWaitActivation = 0; // Reset activation tick (1)

        #if defined(ENABLE_RX_IE)
            IE2 &= ~UCA0RXIE; // Disable RX interrupts before sending data
        #endif

        unsigned int ADC_Result = distanceReading; // Grab distance reading in case it changes while servicing

        P1OUT ^= BIT0; // Toggle output LED
        WiFiSendTCP();
        __delay_cycles(500000); // 0.5s delay

        WiFiSendLength(ADC_Result);
        P1OUT ^= BIT0; // Toggle output LED
        __delay_cycles(500000); // 0.5s delay

        WiFiSendMessage(ADC_Result);
        P1OUT |= BIT0; // Enable output LED

        timerWaitActivation = 0; // Reset activation tick (2)

        #if defined(ENABLE_RX_IE)
            IE2 |= UCA0RXIE; // Enable RX interrupts after done sending data
        #endif
    }
    else timerWaitActivation++; // Increment activation tick
}

// Initialize LED on P1.0 (indicator LED)
void initLED(void)
{
    P1DIR |= BIT0; // Set P1.0 to the output direction
    P1OUT |= BIT0; // Enable the LED
}

// Read incoming UART transmission for a certain string
// Example: readUART("ok", 2, 1); // Search for "ok" in UART RX
int readUART(char *search, char len, int delayParam)
{
    memcpy(findText, search, len); // Set the global findText variable equal to the string entered
    findLen = len;
    foundResult = 0;
    findIndex = 0;
    startSearching = 1;

    #if defined(ENABLE_RX_IE)
        IE2 |= UCA0RXIE; // Enable RX interrupts because we want to receive data
    #endif

    // We can only search for a certain amount of time before we just give up

    if(delayParam == 1) __delay_cycles(DELAY_PARAM_1);
    else if(delayParam == 2) __delay_cycles(DELAY_PARAM_2);
    else if(delayParam == 3) __delay_cycles(DELAY_PARAM_3);
    else if(delayParam == 4) __delay_cycles(DELAY_PARAM_4);
    else if(delayParam == 5) __delay_cycles(DELAY_PARAM_5);
    else if(delayParam == 6) __delay_cycles(DELAY_PARAM_6);
    else if(delayParam == 7) __delay_cycles(DELAY_PARAM_7);
    else if(delayParam == 8) __delay_cycles(DELAY_PARAM_8);
    else if(delayParam == 9) __delay_cycles(DELAY_PARAM_9);
    else __delay_cycles(DELAY_PARAM_10);
    // Delay then check the result to see if it found anything in the amount of time
    startSearching = 0;
    #if defined(ENABLE_RX_IE)
        IE2 &= ~UCA0RXIE; // Disable RX interrupts because we are finished
    #endif

    if(foundResult == 1) return 1; // Return 1 if it found what it was looking for
    return 0; // Return 0 on default
}

// Find the length of a string
int calcLen(const char *s)
{
    int resultLen = 0;
    while(*s)
    {
        resultLen++;
        *s++;
    }
    return resultLen;
}

// Output char to UART
void putc(const unsigned c)
{
    while (!(IFG2&UCA0TXIFG)); // USCI_A0 TX buffer ready?
    UCA0TXBUF = c;
}

// Output string to UART
void TX(const char *s)
{
    while(*s) putc(*s++);
}

// CR LF
void crlf(void)
{
    TX("\r\n");
}

void initUART(void)
{
    DCOCTL = 0;                               // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                    // Set DCO
    DCOCTL = CALDCO_1MHZ;
    P1DIR = 0xFF;                             // All P1.x outputs
    P1OUT = 0;                                // All P1.x reset
    P1SEL = BIT1 + BIT2 + BIT4;               // P1.1 = RXD, P1.2=TXD
    P1SEL2 = BIT1 + BIT2;                     // P1.4 = SMCLK, others GPIO
    P2DIR = 0xFF;                             // All P2.x outputs
    P2OUT = 0;                                // All P2.x reset
    P3DIR = 0xFF;                             // All P3.x outputs
    P3OUT = 0;                                // All P3.x reset
    UCA0CTL1 |= UCSSEL_2;                     // SMCLK
    UCA0BR0 = 8;                              // 1MHz 115200
    UCA0BR1 = 0;                              // 1MHz 115200
    UCA0MCTL = UCBRS2 + UCBRS0;               // Modulation UCBRSx = 5
    UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
    #if defined(ENABLE_RX_IE)
        IE2 |= UCA0RXIE;                      // Enable USCI_A0 RX interrupt
    #endif
}

// Echo back RXed character, confirm TX buffer is ready first
#if defined(ENABLE_RX_IE)
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCIAB0RX_VECTOR))) USCI0RX_ISR (void)
#else
#error Compiler not supported!
#endif
{
    // Code reads in characters as they come in and trys to compare it to a predefined search string
    if(startSearching == 1) // Flag set off by readUART function to start searching
    {
        if(UCA0RXBUF == findText[findIndex]) // Found letter, continue search
        {
            if(findIndex >= findLen - 1) // Matched until the end of the string
            {
                startSearching = 0;
                foundResult = 1;
                findIndex = 0;
            }
            else findIndex++; // If we didn't find it all but something keep incrementing
        }
        else findIndex = 0; // Reset if it didn't find the next character
    }
}
#endif

// Appends a character array (string) to an existing character array (string)
void appendArray(char* s, char* c)
{
    unsigned int lenBase = calcLen(s);
    unsigned int lenInsert = calcLen(c);
    unsigned int addingIndex = 0;
    volatile unsigned int i;
    for(i = lenBase; i < lenBase + lenInsert; i++)
    {
        s[i] = c[addingIndex];
        addingIndex++;
    }
}

// Appends a character to an existing character array (string)
void appendChar(char* s, char c)
{
    unsigned int lenBase = calcLen(s);
    s[lenBase] = c;
}
