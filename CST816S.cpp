/*
   MIT License

  Copyright (c) 2021 Felix Biego

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "Arduino.h"
#include <Wire.h>
#include <FunctionalInterrupt.h>

#include "CST816S.h"

/*!
    @brief  Constructor for CST816S
  @param	sda
      i2c data pin
  @param	scl
      i2c clock pin
  @param	rst
      touch reset pin
  @param	irq
      touch interrupt pin
*/
CST816S::CST816S(int sda, int scl, int rst, int irq)
{
  _sda = sda;
  _scl = scl;
  _rst = rst;
  _irq = irq;
}

/*!
    @brief  read touch data
*/
void CST816S::read_touch()
{
  byte data_raw[8];
  i2c_read(CST816S_ADDRESS, 0x01, data_raw, 6);

  data.gestureID = data_raw[0];
  data.points = data_raw[1];
  data.event = data_raw[2] >> 6;
  data.x = ((data_raw[2] & 0xF) << 8) + data_raw[3];
  data.y = ((data_raw[4] & 0xF) << 8) + data_raw[5];
}

/*!
    @brief  handle interrupts
*/
void CST816S::handleISR(void)
{
  _event_available = true;

  // Call user callback if it has been set
  if (userISR != nullptr) {
      userISR();
  }
}


/*!
    @brief  enable double click
*/
void CST816S::enable_double_click(void)
{
  byte enableDoubleTap = 0x01; // Set EnDClick (bit0) to enable double-tap
  i2c_write(CST816S_ADDRESS, 0xEC, &enableDoubleTap, 1);
}

/*!
  @brief Configure which gestures the CST816S will scan for.

  The MotionMask register (0xEC) lets you enable or disable gesture detection
  at the source.  Each bit turns on one gesture:

    • bit0 = EnDClick   → double-tap  
    • bit1 = EnClick    → single tap  
    • bit2 = EnLong     → long-press  
    • bit3 = EnSW       → swipe (all directions)  
    • bits4–7 = reserved  

  @param mask  A bitmask made by OR’ing together any of these flags.  
               e.g. `(1<<0) | (1<<2)` to only detect double-taps and long-presses.
*/
void CST816S::set_motion_mask(uint8_t mask)
{
  i2c_write(CST816S_ADDRESS, 0xEC, &mask, 1);
}

/*!
  @brief Control which events will actually trigger the IRQ line.

  The IrqCtl register (0xFA) lets you decide which enabled motion events
  raise the interrupt, and which do not:

    • bit4 = EnMotion   → interrupt on any enabled motion bit  
    • bit3 = OnceWLP    → one-shot wake on long-press  
    • bit2 = EnChange   → interrupt on X/Y coordinate change  
    • bit1 = EnTouch    → interrupt on touch down/up  
    • bit0 = reserved  

  For example, to only get an interrupt when a double-tap occurs (assuming
  you’ve already enabled EnDClick in the MotionMask), you would:

    ```cpp
    touch.set_motion_mask(1<<0);        // only EnDClick
    touch.set_irq_control(1<<4);        // only wake on motion
    ```

  @param mask  A bitmask of IrqCtl flags. Combine bits as needed.
*/
void CST816S::set_irq_control(uint8_t mask)
{
  i2c_write(CST816S_ADDRESS, 0xFA, &mask, 1);
}


/*!
    @brief  Only enable double-tap interrupt (disable all other IRQ sources)
*/
void CST816S::enable_double_click_interrupt_only(void)
{
  // 1) Only enable double-tap in MotionMask (0xEC: EnDClick = bit0)
  set_motion_mask(0x01);   // EnDClick

  // 2) Configure IrqCtl (0xFA) to only raise IRQ on motion events (EnMotion = bit4)
  set_irq_control(0x10);   // EnMotion
}


/*!
    @brief  Disable auto sleep mode
*/
void CST816S::disable_auto_sleep(void)
{
  byte disableAutoSleep = 0xFE; // Non-zero value disables auto sleep
  i2c_write(CST816S_ADDRESS, 0xFE, &disableAutoSleep, 1);
}

/*!
    @brief  Enable auto sleep mode
*/
void CST816S::enable_auto_sleep(void)
{
  byte enableAutoSleep = 0x00; // 0 value enables auto sleep
  i2c_write(CST816S_ADDRESS, 0xFE, &enableAutoSleep, 1);
}

/*!
    @brief  Set the auto sleep time
    @param  seconds Time in seconds (1-255) before entering standby mode after inactivity
*/
void CST816S::set_auto_sleep_time(int seconds)
{
  if (seconds < 1)
  {
    seconds = 1; // Enforce minimum value of 1 second
  }
  else if (seconds > 255)
  {
    seconds = 255; // Enforce maximum value of 255 seconds
  }

  byte sleepTime = static_cast<byte>(seconds); // Convert int to byte
  i2c_write(CST816S_ADDRESS, 0xF9, &sleepTime, 1);
}

/*!
    @brief  initialize the touch screen
  @param	interrupt
      type of interrupt FALLING, RISING..
*/
void CST816S::begin(int interrupt)
{
  Wire.begin(_sda, _scl);

  pinMode(_irq, INPUT_PULLUP);
  pinMode(_rst, OUTPUT);

  digitalWrite(_rst, HIGH);
  delay(50);
  digitalWrite(_rst, LOW);
  delay(5);
  digitalWrite(_rst, HIGH);
  delay(50);

  i2c_read(CST816S_ADDRESS, 0x15, &data.version, 1);
  delay(5);
  i2c_read(CST816S_ADDRESS, 0xA7, data.versionInfo, 3);

  attachInterrupt(_irq, std::bind(&CST816S::handleISR, this), interrupt);
}

/*!
    @brief  Attaches a user-defined callback function to be triggered on an interrupt event from the CST816S touch controller.
    @param  callback  A function to be called when an interrupt event occurs, must have no parameters and return void.
*/
void CST816S::attachUserInterrupt(std::function<void(void)> callback)
{
  userISR = callback;
}

/*!
    @brief  check for a touch event
*/
bool CST816S::available()
{
  if (_event_available)
  {
    read_touch();
    _event_available = false;
    return true;
  }
  return false;
}

/*!
    @brief  put the touch screen in standby mode
*/
void CST816S::sleep()
{
  digitalWrite(_rst, LOW);
  delay(5);
  digitalWrite(_rst, HIGH);
  delay(50);
  byte standby_value = 0x03;
  i2c_write(CST816S_ADDRESS, 0xA5, &standby_value, 1);
}

/*!
    @brief  get the gesture event name
*/
String CST816S::gesture()
{
  switch (data.gestureID)
  {
  case NONE:
    return "NONE";
    break;
  case SWIPE_DOWN:
    return "SWIPE DOWN";
    break;
  case SWIPE_UP:
    return "SWIPE UP";
    break;
  case SWIPE_LEFT:
    return "SWIPE LEFT";
    break;
  case SWIPE_RIGHT:
    return "SWIPE RIGHT";
    break;
  case SINGLE_CLICK:
    return "SINGLE CLICK";
    break;
  case DOUBLE_CLICK:
    return "DOUBLE CLICK";
    break;
  case LONG_PRESS:
    return "LONG PRESS";
    break;
  default:
    return "UNKNOWN";
    break;
  }
}

/*!
    @brief  read data from i2c
  @param	addr
      i2c device address
  @param	reg_addr
      device register address
  @param	reg_data
      array to copy the read data
  @param	length
      length of data
*/
uint8_t CST816S::i2c_read(uint16_t addr, uint8_t reg_addr, uint8_t *reg_data, size_t length)
{
  Wire.beginTransmission(addr);
  Wire.write(reg_addr);
  if (Wire.endTransmission(true))
    return -1;
  Wire.requestFrom(addr, length, true);
  for (int i = 0; i < length; i++)
  {
    *reg_data++ = Wire.read();
  }
  return 0;
}

/*!
    @brief  write data to i2c
  @brief  read data from i2c
  @param	addr
      i2c device address
  @param	reg_addr
      device register address
  @param	reg_data
      data to be sent
  @param	length
      length of data
*/
uint8_t CST816S::i2c_write(uint8_t addr, uint8_t reg_addr, const uint8_t *reg_data, size_t length)
{
  Wire.beginTransmission(addr);
  Wire.write(reg_addr);
  for (int i = 0; i < length; i++)
  {
    Wire.write(*reg_data++);
  }
  if (Wire.endTransmission(true))
    return -1;
  return 0;
}
