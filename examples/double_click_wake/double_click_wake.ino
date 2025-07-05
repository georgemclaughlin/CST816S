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

#include <CST816S.h>

// SDA, SCL, RST, IRQ
CST816S touch(21, 22, 5, 4);

// a flag set in the ISR, read & cleared in loop()
volatile bool doubleTapFlag = false;

void setup() {
  Serial.begin(115200);
  delay(100);

  touch.begin(RISING);
  // Configure the chip to only generate IRQ on double-tap:
  touch.enable_double_click_interrupt_only();
  touch.attachUserInterrupt([]() {
    // ISR context: only called on a DOUBLE_CLICK
    doubleTapFlag = true;
  });
}

void loop() {
  // only act when our flag has been raised by the ISR
  if (doubleTapFlag) {
    doubleTapFlag = false;                // clear it immediately
    // now you can do whatever you need:
    Serial.println("👆 Double-tap detected!");
    // e.g. update display, send a message, etc.
  }

  // …do any other low-priority tasks here…
  delay(10);
}
