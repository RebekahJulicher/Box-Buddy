# Box Buddy
 Single-code-file version of my Box Buddy project. Writeup available on my portfolio.

Required libraries:
Adafruit TCS34725
Adafruit ST7735
Adafruit MPU6050
Adafruit Sensor

See circuit diagram for details on physical implementation.

Sensors/components:

1.44" TFT LCD ST7735 screen x1
Basic 2-pin tactile button x1
TCS34725 color sensor x1
MPU6050 gyroscope x1
1k resistors x4
Arduino Nano x1


I used a 3"x3" folding cardboard gift box from Amazon for an enclosure. Hot glue is sufficient to attach the component perfboard to one side, the screen to the opposite side, the color sensor to the bottom, and the button to the top.

Some tips for this:
- Orient the Arduino Nano so that the port is near a side of the box that you don't mind cutting a small hole in to make a cable access point. Alternatively, power this with a battery. There might be enough room in the box.

- Arrange components in such a way that when you unfold the box partially, you get a vertical line of pieces and they are fully visible.