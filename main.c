/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs 

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs  - 1.45
        Device            :  PIC16F18855
        Driver Version    :  2.00
    The generated drivers are tested against the following:
        Compiler          :  XC8 1.35
        MPLAB             :  MPLAB X 3.40
*/

/*
    (c) 2016 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/
/*
 2017-06-29 ozh this is working:  read fader 0 & change the level of all 8 fader LEDs
 2017-07-06 ozh get fader input & set LED by the fader value.  This is working (with some delays).
 *              NOTE: this is a great "test program" for the Fader hardware
 2017-07-09 ozh updated to 32MHz clock. ADClock FOSC/64.  2x burst average mode.  good reliable responsive faders
 2018-06-05 ozh this version works as a slave to flash all LEDs from Master.  The flash changes tempo with fader0 on the Master!
 2018-06-08 ozh Read ADC (fader) mode is working correctly 
 *              ( i.e. the PTL30_fader_Slave_test code driven by Matrix Switch Teensy code 5.2.3 )
 * 
 todo: formalize the parsing of the PointerByte
 Pointer Byte:
D7:D4 ? Mode Bits               D3:D0 - Address  Comments
0 0 0 0 ? Configuration Mode                     Unused at the moment
0 0 0 1 ? Write LED mode                         Always use Address 0 with 2 data bytes
0 0 1 0 ? Read ADC (fader) mode 0000 to 0111     Only 0-7 address for this module

 */
#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/i2c1.h"

/* BEGIN
 copied from PLT30_fader_led_test1 
 */

volatile uint8_t byteFaderValue[FADERCOUNT];
volatile uint8_t prevbyteFaderValue[FADERCOUNT];  
    
typedef uint16_t adc_result_t;

typedef enum
{
    FADER0 = 0x0,
    FADER1 = 0x1,
    FADER2 = 0x2,
    FADER3 = 0x3,
    FADER4 = 0x4,
    POT =  0x4,
    FADER5 = 0x5,
    FADER6 = 0x6,
    FADER7 = 0x7,      
} adcc_channel_t;

void delay(int);   
 // master clock frequency adjust.  1x = 1MHz   8x = 8MHz  16=16MHz
const int cMstrClkAdjust = 32;
void delay(int delaytime) {     
    long counter = 0;
    long adjustedDelaytime=delaytime*cMstrClkAdjust;
    if (0<delaytime)
        for (counter = 0; counter<adjustedDelaytime; counter++);   
    //__delay_ms(delaytime);     // this should be the proper way to do this
 } 

void blinkyLoop (int maxLoops){
    int loopCount=maxLoops;
    int delayTime;
    
    ODCONB &= 0xE0;  // not open drain
    ODCONC &= 0x1F;  // not open drain    
    while(loopCount>0){
        delayTime=loopCount*400;    

        // brightest
        LATB |= 0b00011111; // bottom 5 bits of PORTB
        LATC |= 0b11100000; // top 3 bits of PORTC all led's on activate all

        delay(delayTime);
        // deactivate all led's
        LATB &= 0b11100000; // deactivate all led's 
        LATC &= 0b00011111; // deactivate all led's 

        delay(delayTime);
        
        loopCount--;
    }
    LATB |= 0b00011111; // bottom 5 bits of PORTB
    LATC |= 0b11100000; // top 3 bits of PORTC all led's on activate all
    delay(100);     // must be shorter than master
    return;
}
/* END
 copied from PLT30_fader_led_test1 
 */
void UpdateLEDsFromValue(uint8_t inFaderNum,uint8_t inValue);
/*
                         Main application
 */
const int cMaxFaderCnt=8;    // number of faders in the submodule

void UpdateLEDs(uint8_t inMSByteLED,uint8_t inLSByteLED);
// void MYI2C_Read2LEDBytes(uint8_t slaveDeviceAddress, uint8_t MSBWriteByte,uint8_t LSBWriteByte);
void main(void)
{
    uint8_t I2C_ADDRESS_FADELED0 =  0x68
            ;  // assume only 1 device for now 
    adc_result_t rawFaderValue[8];  // cMaxFaderCnt
    int wkInt;
    uint8_t wkFaderValue=0; 
    uint8_t iFaderNum=0;
    
    iLEDBytesChangedCount=0;
    // initialize the device
    SYSTEM_Initialize();

    blinkyLoop(5); 

    // When using interrupts, you need to set the Global and Peripheral Interrupt Enable bits
    // Use the following macros to:

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();

    // Disable the Peripheral Interrupts
    //INTERRUPT_PeripheralInterruptDisable();
    

     
    // get value from faders
    for (iFaderNum=0;iFaderNum<cMaxFaderCnt;iFaderNum++){
        rawFaderValue[iFaderNum]=ADCC_GetSingleConversion(iFaderNum);  // not POT
        //prevbyteFaderValue[iFaderNum]=byteFaderValue[iFaderNum];     // nothing to save the first time
        byteFaderValue[iFaderNum]=(rawFaderValue[iFaderNum]>>2)&0xFF;  // convert 10 bit to 8 bit    
    }

    //Clear_WDT(); // clear watchdog timer, until i figure  out how to shut it off 

    while (1) {

            // get value from faders
            for (iFaderNum=0;iFaderNum<cMaxFaderCnt;iFaderNum++){
                // was getting spurious readings.  Fixed by adjusting ADClock (32MHz, FOSC/128, Burst Avg Mode - 3x)
                rawFaderValue[iFaderNum]=ADCC_GetSingleConversion(iFaderNum);  // 
                prevbyteFaderValue[iFaderNum]=byteFaderValue[iFaderNum];
                byteFaderValue[iFaderNum]=(rawFaderValue[iFaderNum])>>2; // convert 10 bit to 8 bit  
                if(prevbyteFaderValue[iFaderNum]!=byteFaderValue[iFaderNum]) { // only update if is changes
                    UpdateLEDsFromValue(iFaderNum,byteFaderValue[iFaderNum]);
                    //printf()
                }
            }
            
            // note: this is kind of crazy because we're updating the LED from 
            // a) slider value AND 
            // b) I2C message
            // now check iLEDBytesChangedCount for 2 new bytes received
            
            if (1 < iLEDBytesChangedCount) 
            {
                iLEDBytesChangedCount=0;
                UpdateLEDs(MSByteLED,LSByteLED);
            }

            //Clear_WDT(); // clear watchdog timer, until i figure  out how to shut it off
    } 
 }
void UpdateLEDsFromValue(uint8_t inFaderNum,uint8_t inValue)
{
    bool    wkOnFlag = 0;
    bool    wkBrightFlag = 0;
    
    // parse value
    if (inValue > 80)  wkOnFlag=1;
    if (inValue > 170) wkBrightFlag=1;
             
    switch(inFaderNum)
    {
        case 0 : {
            if(wkOnFlag) {  LATB   |= 0b00000001;
                if(wkBrightFlag) {ODCONB &= 0b11111110;}else{ODCONB |= 0b00000001;};
            }else{          LATB   &= 0b11111110; ODCONB &= 0b11111110; }
            break;
        }
        case 1 : {
            if(wkOnFlag) {  LATB   |= 0b00000010;
                if(wkBrightFlag) {ODCONB &= 0b11111101;}else{ODCONB |= 0b00000010;};
            }else{          LATB   &= 0b11111101; ODCONB &= 0b11111101; }
            break;
        }
        case 2 : {
            if(wkOnFlag) {  LATB   |= 0b00000100;
                if(wkBrightFlag) {ODCONB &= 0b11111011;}else{ODCONB |= 0b00000100;}
            }else{          LATB   &= 0b11111011; ODCONB &= 0b11111011; }
            break;
        }
        case 3 : {
            if(wkOnFlag) {  LATB   |= 0b00001000;
                if(wkBrightFlag) {ODCONB &= 0b11110111;}else{ODCONB |= 0b00001000;}
            }else{          LATB   &= 0b11110111; ODCONB &= 0b11110111; }
            break;
        }
        case 4 : {
            if(wkOnFlag) {  LATB   |= 0b00010000;
                if(wkBrightFlag) {ODCONB &= 0b11101111;}else{ODCONB |= 0b00010000;}
            }else{          LATB   &= 0b11101111; ODCONB &= 0b11101111; }
            break;
        }
        case 5 : {
            if(wkOnFlag) {  LATC   |= 0b00100000;
                if(wkBrightFlag) {ODCONC &= 0b11011111;}else{ODCONC |= 0b00100000;}
            }else{          LATC   &= 0b11011111; ODCONC &= 0b11011111; }
            break;
        }
        case 6 : {
            if(wkOnFlag) {  LATC   |= 0b01000000;
                if(wkBrightFlag) {ODCONC &= 0b10111111;}else{ODCONC |= 0b01000000;}
            }else{          LATC   &= 0b10111111; ODCONC &= 0b10111111; }
            break;
        }
        case 7 : {
            if(wkOnFlag) {  LATC   |= 0b10000000;
                if(wkBrightFlag) {ODCONC &= 0b01111111;}else{ODCONC |= 0b10000000;}
            }else{          LATC   &= 0b01111111; ODCONC &= 0b01111111; }
        }
    }
}
void UpdateLEDs(uint8_t inMSByteLED,uint8_t inLSByteLED){
    uint8_t wkByte;
    /* 
     LSB 0b00000011  Fader0 LED RB0
     LSB 0b00001100  Fader1 LED RB1
     LSB 0b00110000  Fader2 LED RB2
     LSB 0b11000000  Fader3 LED RB3
     */
    // note: this test harness is only changing 10 bits (only the bottom 2 of MSB)
    // parse the bits to make map to the PORTB/C & ODCONB/C
    
    // Now parse LSB
    wkByte=inLSByteLED;
    wkByte &= 0b00000010;   //LED 0
    if(0<wkByte)  // off/on bit
    {
        LATB |= 0b00000001;   // 
    }else{
        LATB &= 0b11111110;
    }
    wkByte=inLSByteLED;
    wkByte &= 0b00000001;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONB &= 0b11111110;
    }else{

        ODCONB |= 0b00000001; 
    }
   
    wkByte=inLSByteLED;
    wkByte &= 0b00001000;  // LED 1
    if(0<wkByte)  // off/on bit
    {
        LATB |= 0b00000010;   // 
    }else{
        LATB &= 0b11111101;
    }
    wkByte=inLSByteLED;
    wkByte &= 0b00000100;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONB &= 0b11111101;
    }else{

        ODCONB |= 0b00000010;   // 
    }
 
    wkByte=inLSByteLED;
    wkByte &= 0b00100000;  // LED 2
    if(0<wkByte)  // off/on bit
    {
        LATB |= 0b00000100;   // 
    }else{
        LATB &= 0b11111011;
    }
    wkByte=inLSByteLED;
    wkByte &= 0b00010000;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONB &= 0b11111011;
    }else{

        ODCONB |= 0b00000100;   // 
    }  
    
    wkByte=inLSByteLED;
    wkByte &= 0b10000000;  // LED 3
    if(0<wkByte)  // off/on bit
    {
        LATB |= 0b00001000;   // 
    }else{
        LATB &= 0b11110111;
    }
    wkByte=inLSByteLED;
    wkByte &= 0b01000000;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONB &= 0b11110111;
    }else{

        ODCONB |= 0b00001000;   // 
    } 
    // Now parse MSB
    wkByte=inMSByteLED;
    wkByte &= 0b00000010;  // LED 4
    if(0<wkByte)  // off/on bit
    {
        LATB |= 0b00010000;   // 
    }else{
        LATB &= 0b11101111;
    }
    wkByte=inMSByteLED;
    wkByte &= 0b00000001;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONB &= 0b11101111;
    }else{

        ODCONB |= 0b00010000;   // 
    } 
    
    wkByte=inMSByteLED;
    wkByte &= 0b00001000;  // LED 5
    if(0<wkByte)  // off/on bit
    {
        LATC |= 0b00100000;   // 
    }else{
        LATC &= 0b11011111;
    }
    wkByte=inMSByteLED;
    wkByte &= 0b00000100;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONC &= 0b11011111;
    }else{

        ODCONC |= 0b00100000;   // 
    } 
        
    wkByte=inMSByteLED;
    wkByte &= 0b00100000;  // LED 6
    if(0<wkByte)  // off/on bit
    {
        LATC |= 0b01000000;   // 
    }else{
        LATC &= 0b10111111;
    }
    wkByte=inMSByteLED;
    wkByte &= 0b00010000;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONC &= 0b10111111;
    }else{

        ODCONC |= 0b01000000;   // 
    } 
    
    wkByte=inMSByteLED;
    wkByte &= 0b10000000;  // LED 7
    if(0<wkByte)  // off/on bit
    {
        LATC |= 0b10000000;   // 
    }else{
        LATC &= 0b01111111;
    }
    wkByte=inMSByteLED;
    wkByte &= 0b01000000;
    if(0<wkByte)  // dim/bright bit
    {
        ODCONC &= 0b01111111;
    }else{

        ODCONC |= 0b10000000;   // 
    } 
}
/*  this is Master code
void MYI2C_Read2LEDBytes(uint8_t slaveDeviceAddress,uint8_t MSBWriteByte,uint8_t LSBWriteByte)
{
   //<code>
        #define SLAVE_I2C_GENERIC_RETRY_MAX     100
        
        // initialize the module
        //I2C1_Initialize();  done in system_initialize())
           

        // write to Fader/LED Device
        
        uint16_t        dataAddress;
        uint8_t         sourceData[16] = {  0xA0, 0xA1, 0xA2, 0xA3, 
                                            0xA4, 0xA5, 0xA6, 0xA7, 
                                            0xA8, 0xA9, 0xAA, 0xAB, 
                                            0xAC, 0xAD, 0xAE, 0xAF }; 
        uint8_t         *pData;
        uint16_t        nCount;

        uint8_t         writeBuffer[3];
        uint8_t         *pD;
        uint16_t        counter, timeOut;
        uint8_t         pointerByte = 0b00100000;    // 7 bit address + 0 for Write
        
        I2C1_MESSAGE_STATUS status = I2C1_MESSAGE_PENDING;

        dataAddress = 0x00;             // starting Fader Submodule address 
        pD = sourceData;                // initialize the source of the data
        nCount = 1;                     // number of byte pairs to write

        // signal I2C traffic
        // TODO convert RC1 back to Analog In for the slave module
        PORTC |= 0x20; // RC1 bit of PORTC led activate
        
        for (counter = 0; counter < nCount; counter++)
        {
            // build the write buffer first
            // starting address of the EEPROM memory
            writeBuffer[0] = pointerByte;                        // pointer byte
            // data to be written
            writeBuffer[1] = (MSBWriteByte);            // high address
            writeBuffer[2] = (LSBWriteByte);            // low address

            // Now it is possible that the slave device will be slow.
            // As a work around on these slaves, the application can
            // retry sending the transaction
            timeOut = 0;
            while(status != I2C1_MESSAGE_FAIL)
            {
                // write one byte to EEPROM (3 is the number of bytes to write)
                I2C1_MasterWrite(  writeBuffer,
                                        3,
                                        slaveDeviceAddress,
                                        &status);

                // wait for the message to be sent or status has changed.
                while(status == I2C1_MESSAGE_PENDING);

                if (status == I2C1_MESSAGE_COMPLETE)
                    break;

                // if status is  I2C1_MESSAGE_ADDRESS_NO_ACK,
                //               or I2C1_DATA_NO_ACK,
                // The device may be busy and needs more time for the last
                // write so we can retry writing the data, this is why we
                // use a while loop here

                // check for max retry and skip this byte
                if (timeOut == SLAVE_I2C_GENERIC_RETRY_MAX)
                    break;
                else
                    timeOut++;
            }

            if (status == I2C1_MESSAGE_FAIL)
            {
                break;
            }
            dataAddress++;
        }
        PORTC &= 0xDF; // deactivate all led's 
        delay(1000);

    //</code>    
}
*/      
/**
 End of File
*/