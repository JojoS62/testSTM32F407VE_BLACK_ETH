/* 
 * Copyright (c) 2019 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "threadIO.h"
#include "Adafruit_ADS1015.h"
#include "MCP23017.h"
#include "TextLCD.h"


#define STACKSIZE   (4 * 1024)
#define THREADNAME  "ThreadIO"

ThreadIO::ThreadIO(uint32_t cycleTime_ms) :
    _thread(osPriorityNormal, STACKSIZE, nullptr, THREADNAME)
{
    _cycleTime = cycleTime_ms;
}

/*
    start() : starts the thread
*/
void ThreadIO::start()
{
    _running = true;
    _thread.start( callback(this, &ThreadIO::myThreadFn) );
}


/*
    start() : starts the thread
*/
void ThreadIO::myThreadFn()
{
    // thread local objects
    // take care of thread stacksize !
    // DigitalOut led1(LED1);
    I2C i2c(PC_9,PA_8);
    Adafruit_ADS1115 ads(&i2c);


#if 0
    I2C i2c_lcd(PB_9,PB_8); // PC_9,PA_8 SDA, SCL 
    //TextLCD_I2C lcd(&i2c_lcd, 0x4C, TextLCD::LCD20x4); // I2C bus, PCF8574 Slaveaddress, LCD Type Adresse: 8bit vs 7 bit!!! Arduino I2c scanner gibt 7bit an 


    MCP23017 MCP23017(&i2c, 0x40);

    for (int p = 0; p < 16; p++)
    {
        MCP23017.pinMode(p, DIR_OUTPUT);
        MCP23017.digitalWrite(p, 1);
    }
#endif     

    int i = 0;
    bool testpin = false;

    while(_running) {
        uint64_t nextTime = get_ms_count() + _cycleTime;

        //MCP23017.digitalWrite(3, testpin ? 0 : 1);

        testpin = !testpin;


        // led1 = !led1;
        float reading = ads.readADC_SingleEnded_V(0); // read channel 0
        float reading1 = ads.readADC_SingleEnded_V(1); // read channel 0
        float reading2 = ads.readADC_SingleEnded_V(2); // read channel 0
        float reading3 = ads.readADC_SingleEnded_V(3); // read channel 0
        
        //printf("reading: %7.3f %7.3f %7.3f %7.3f\r\n", reading, reading1, reading2, reading3); // print reading

#if 0
        lcd.cls();
        lcd.locate(0, 0);
        char converted[10];
        sprintf(converted, "%G", reading);
        lcd.printf(converted); 

        lcd.locate(0, 1);
        char converted1[10];
        sprintf(converted1, "%G", reading1);
        lcd.printf(converted1); 

        lcd.locate(0, 2);
        char converted2[10];
        sprintf(converted2, "%G", reading2);
        lcd.printf(converted2); 

        lcd.locate(0, 3);
        char converted3[10];
        sprintf(converted3, "%G", reading3);
        lcd.printf(converted3); 

        printf("hello from thread %d\n", i++);
#endif

        ThisThread::sleep_until(nextTime);
    }
}
