#include <SdFat.h>
#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <ILI9341_t3.h>
#include <gfxfont.h>
#include <Adafruit_GFX.h>
#include <Metro.h>
#include <HT16K33.h>

//////////////////////////////////////
// Button Box 
// update interval for periodic operations

Metro _sleep_timer = Metro(4000);
bool _sleep = false;
bool _serialOutput = false;
String _error;
int _rotary_dial;
int _rotary_prev = -1;
String _current_category;
String _current_description;
int _last_button = -1;
int _button_prev = -1;
int _last_button_led = -1;
#define BUFFPIXEL 320
int _last_number = -1;
bool _number_latch = false;
bool _number_unlatch = true;
int _number_prev = -1;
String _number_string = "";
bool _joy_stick[4];
bool _joy_stick_prev[4];

//////////////////////////////////////
// HT16k33
#define I2C_HT16K33 0x70
HT16K33 ht;
HT16K33::KEYDATA _keydata;
int p_key_matrix[3][13];

//////////////////////////////////////
// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);
bool _update_lcd = true;

//////////////////////////////////////
// TFT
ILI9341_t3 tft = ILI9341_t3(10, 9, 8, 11, 13, 12);
bool _update_tft = true;

//////////////////////////////////////
// SDFat
SdFatSdioEX sd;
SdFile _file;

char BUTTON_BOX_DIR[] = "button_box";
#define MAX_NAME_LENGTH 30
char _name[MAX_NAME_LENGTH];
#define CATEGORIES 4
#define NUM_IMAGES 9
String _tmp_dir;
struct imageFile
{
    String directory;
    String filename;
    String ext;
};
imageFile _imageFiles[CATEGORIES][NUM_IMAGES];

////////////////////////////////////////////////////////////////////////////

// custom serial println
void serialPrint(String message)
{
    if (_serialOutput)
    {
        Serial.println(message);
    }
}

// fill a string with white space up to maxLength
String fillLcdWhiteSpace(String line, int maxLength = 20)
{
    if (int(line.length()) > maxLength)
    {
        line = line.substring(0, maxLength);
    }
    String whitespace = multiChar(' ', maxLength - line.length());
    return line + whitespace;
}

///
/// MAIN SETUP
///
void setup()
{
    //////////////////////////////////////
    // serial
    if (_serialOutput)
    {
        Serial.begin(9600);
    }

    //////////////////////////////////////
    // SD
    if (!sd.begin())
    {
        sd.initErrorHalt();
    }
    
    // build the array of images
    SdBaseFile s;
    if (s.open(BUTTON_BOX_DIR))
    {
        scan_sd(s, 0);
    }

    //////////////////////////////////////
    // ht16k33
	ht.begin(I2C_HT16K33);
    // button led arrays
    updateKeyMatrix();
    updateRotaryDial();
    updateButtonLedPairs();

    //////////////////////////////////////
    // LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();
    updateLCDScreen();

    //////////////////////////////////////
    // TFT
    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(ILI9341_BLACK);    
    displayImageOnTft();

    serialPrint("Setup Done");

}

///
/// MAIN LOOP
///
void loop()
{
    if (_sleep_timer.check())
    {
        //_sleep = true;
    }

    // update periodic variables
    keyPressedUpdate();

    if (_sleep)
    {
        ht.clearAll();
        lcd.clear();
        tft.fillScreen(ILI9341_BLACK);
    }
    else
    {
        if (_update_lcd)
        {
            updateLCDScreen();
            _update_lcd = false;
        }
        if (_update_tft)
        {
            displayImageOnTft();
            _update_tft = false;
        }
    }
}

// these items happen on a periodic interval
void keyPressedUpdate()
{
    if (ht.keyINTflag() != 0)
    {
        serialPrint("Pressed");
        serialPrint(ht.keyINTflag());

        // turn sleep off
        _sleep = false;

        // clear led states
        clearLedStates();
        
        updateKeyMatrix();
        updateRotaryDial();
        updateButtonLedPairs();
        updateNumberPad();
        updateJoyStick();
        
        // write all leds
        ht.sendLed();
    }
}

// clear the led states without update
void clearLedStates()
{
    for (int i = 0; i < 128; i++)
    {
        ht.clearLed(i);
    }
}

// update the key matrix with ht16k33 ram data
void updateKeyMatrix()
{
    ht.readKeyRaw(_keydata);

    // read each bit value of the keydata and store it in the matix
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 13; j++)
        {
            p_key_matrix[i][j] = bitRead(_keydata[i], j);
        }
    }
}

// set the rotary dial global
void updateRotaryDial()
{
    for (int i = 0; i < 4; i++)
    {
        int keystate = p_key_matrix[1][i];
        if (keystate)
        {
            _rotary_dial = i;
            ht.setLed(22 - i);
        }
    }

    if (_rotary_dial != _rotary_prev)
    {
        _rotary_prev = _rotary_dial;
        _last_button = -1;
        _last_button_led = -1;
        _current_description = "";
        _update_lcd = true;
        _update_tft = true;
    }

    _current_category = _imageFiles[_rotary_dial][0].directory;
}

// update the buttons
void updateButtonLedPairs()
{
    // switches
    if (p_key_matrix[1][11]) ht.setLed(35);
    if (p_key_matrix[1][10]) ht.setLed(25);
    if (p_key_matrix[1][9]) ht.setLed(24);
    if (p_key_matrix[1][8]) ht.setLed(23);
    if (p_key_matrix[1][7]) ht.setLed(39);
    if (p_key_matrix[1][6]) ht.setLed(38);
    if (p_key_matrix[1][5]) ht.setLed(37);
    if (p_key_matrix[1][4]) ht.setLed(36);

    // buttons
    if (p_key_matrix[0][4]) 
    {
        ht.setLed(40);
        _last_button_led = 40;
        _last_button = 2;
    }
    if (p_key_matrix[0][5])
    {
        ht.setLed(41);
        _last_button_led = 41;
        _last_button = 1;
    }
    if (p_key_matrix[0][6])
    {
        ht.setLed(51);
        _last_button_led = 51;
        _last_button = 0;
    }
    if (p_key_matrix[0][7])
    {
        ht.setLed(52);
        _last_button_led = 52;
        _last_button = 5;
    }
    if (p_key_matrix[0][8])
    {
        ht.setLed(53);
        _last_button_led = 53;
        _last_button = 4;
    }
    if (p_key_matrix[0][9])
    {
        ht.setLed(54);
        _last_button_led = 54;
        _last_button = 3;
    }
    if (p_key_matrix[0][10])
    {
        ht.setLed(55);
        _last_button_led = 55;
        _last_button = 8;
    }
    if (p_key_matrix[0][11])
    {
        ht.setLed(56);
        _last_button_led = 56;
        _last_button = 7;
    }
    if (p_key_matrix[0][12])
    {
        ht.setLed(57);
        _last_button_led = 57;
        _last_button = 6;
    }

    if (_last_button_led >= 0)
    {
        ht.setLed(_last_button_led);
    }

    if (_button_prev != _last_button)
    {
        _button_prev = _last_button;
        _update_lcd = true;
        _update_tft = true;

        if (_last_button >= 0)
        {
            _current_description = _imageFiles[_rotary_dial][_last_button].filename;
        }
    }
}

// update the number pad
void updateNumberPad()
{
    if (p_key_matrix[2][9])
    {
        _last_number = 1;
        _number_latch = true;
    }
    else if (p_key_matrix[2][5])
    {
        _last_number = 2;
        _number_latch = true;
    }
    else if (p_key_matrix[2][1])
    {
        _last_number = 3;
        _number_latch = true;
    }
    else if (p_key_matrix[2][10])
    {
        _last_number = 4;
        _number_latch = true;
    }
    else if (p_key_matrix[2][6])
    {
        _last_number = 5;
        _number_latch = true;
    }
    else if (p_key_matrix[2][2])
    {
        _last_number = 6;
        _number_latch = true;
    }
    else if (p_key_matrix[2][11])
    {
        _last_number = 7;
        _number_latch = true;
    }
    else if (p_key_matrix[2][7])
    {
        _last_number = 8;
        _number_latch = true;
    }
    else if (p_key_matrix[2][3])
    {
        _last_number = 9;
        _number_latch = true;
    }
    else if (p_key_matrix[2][8])
    {
        _last_number = 0;
        _number_latch = true;
    }
    else if (p_key_matrix[2][12])
    {
        _number_string = "";
        _update_lcd = true;
    }
    else
    {
        _number_latch = false;
        _number_unlatch = true;
    }

    if (_number_latch && _number_unlatch)
    {
        if (_number_string.length() >= 18)
        {
            _number_string = "";
        }
        _number_latch = false;
        _number_unlatch = false;
        _number_string += _last_number;
        _update_lcd = true;
    }
}

// update joystick status
void updateJoyStick()
{
    if (p_key_matrix[0][0])
    {
        _joy_stick[0] = true;
    }
    else
    {
        _joy_stick[0] = false;
    }
    if (p_key_matrix[0][1])
    {
        _joy_stick[1] = true;
    }
    else
    {
        _joy_stick[1] = false;
    }
    if (p_key_matrix[0][2])
    {
        _joy_stick[2] = true;
    }
    else
    {
        _joy_stick[2] = false;
    }
    if (p_key_matrix[0][3])
    {
        _joy_stick[3] = true;
    }
    else
    {
        _joy_stick[3] = false;
    }

    for (int i = 0; i < 4; i++)
    {
        if (_joy_stick[i] != _joy_stick_prev[i])
        {
            _update_tft = true;
            _joy_stick_prev[i] = _joy_stick[i];
        }
    }
}

// update the image on the tft display
void displayImageOnTft()
{
    if (_last_button == -1)
    {
        tft.fillScreen(ILI9341_BLACK);
        if (_rotary_dial == 0)
        {
            tft.setTextColor(ILI9341_BLUE);
        }
        else if(_rotary_dial == 1)
        {
            tft.setTextColor(ILI9341_GREEN);
        }
        else if (_rotary_dial == 2)
        {
            tft.setTextColor(ILI9341_YELLOW);
        }
        else if (_rotary_dial == 3)
        {
            tft.setTextColor(ILI9341_RED);
        }
        tft.setTextSize(32);
        tft.setCursor(80, 8);
        tft.print(_rotary_dial + 1);
    }
    else
    {
        imageFile file_struct = _imageFiles[_rotary_dial][_last_button];
        String filename = "button_box/" + file_struct.directory + "/" + file_struct.filename + "." + file_struct.ext;
        bmpDraw(filename, 0, 0);
    }

    // draw joy stick bars
    if (_joy_stick[3])// up
    {
        tft.fillRect(0, 0, 320, 10, ILI9341_RED);
    }
    if (_joy_stick[0])// down
    {
        tft.fillRect(0, 230, 320, 10, ILI9341_BLUE);
    }
    if (_joy_stick[1])// right
    {
        tft.fillRect(310, 10, 10, 220, ILI9341_GREEN);
    }
    if (_joy_stick[2])// left
    {
        tft.fillRect(0, 10, 10, 220, ILI9341_YELLOW);
    }
}

//update the lcd display's 4 lines
void updateLCDScreen()
{
    lcd.setCursor(0, 0);
    lcd.print(fillLcdWhiteSpace("> " + _current_category.toUpperCase()));

    lcd.setCursor(0, 1);
    lcd.print(fillLcdWhiteSpace("> " + _current_description));

    lcd.setCursor(0, 2);

    lcd.setCursor(0, 3);
    lcd.print(fillLcdWhiteSpace("# " + _number_string));

}

// get a string of the coords for the pressed buttons
String getPressedCoords()
{
    String changed = "P: ";
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 13; j++)
        {
            int value = p_key_matrix[i][j];
            if (value == 1)
            {
                changed = changed + i + ":" + j + " ";
            }
        }
    }
    return fillLcdWhiteSpace(changed);
}

// print to the lcd the button information
void printPressedMatrix()
{
    lcd.setCursor(0, 0);
    String msg = getPressedCoords();
    lcd.print(fillLcdWhiteSpace(msg));

    for (int i = 0; i < 3; i++)
    {
        msg = "> ";
        lcd.setCursor(0, i + 1);
        // concat each value for the row
        for (int j = 0; j < 13; j++)
        {
            msg = msg + p_key_matrix[i][j];
        }
        lcd.print(fillLcdWhiteSpace(msg));
    }
}

// scan the sd card and build an array of image files
void scan_sd(SdBaseFile s, int d)
{
    int f = 0;

    while (d < CATEGORIES)
    {
        if (!_file.openNext(&s, O_READ)) break;
        bool z = _file.isDir();
        _file.getName(_name, MAX_NAME_LENGTH);
        _file.close();
        if (z)
        {
            _tmp_dir = String(_name);
        }
        else if(f < NUM_IMAGES)
        {
            String fileExt[2];
            splitFileExt(fileExt, String(_name));
            _imageFiles[d][f].directory = _tmp_dir;
            _imageFiles[d][f].filename = fileExt[0];
            _imageFiles[d][f].ext = fileExt[1];
            f++;
        }
        if (z)
        {
            SdBaseFile n;
            uint16_t index = s.curPosition() / 32 - 1;
            n.open(&s, index, O_READ);
            scan_sd(n, d++);
        }
    }
}

// build a string out of n characters, such as 0000, tttt, etc.
String multiChar(char c, int mult)
{
    String string = "";
    for (int i = 0; i < mult; i++)
    {
        string += c;
    }
    return string;
}

// split a filename into name and extension array
void splitFileExt(String fileExt[2], String filename)
{
    for (int i = filename.length() - 1; i >= 0; i--)
    {
        if (filename[i] == '.')
        {
            fileExt[0] = filename.substring(0, i);
            fileExt[1] = filename.substring(i + 1);
        }
    }
}

//===========================================================
// Try Draw using writeRect
void bmpDraw(String filename, uint8_t x, uint16_t y)
{

    File     bmpFile;
    int      bmpWidth, bmpHeight;   // W+H in pixels
    uint8_t  bmpDepth;              // Bit depth (currently must be 24)
    uint32_t bmpImageoffset;        // Start of image data in file
    uint32_t rowSize;               // Not always = bmpWidth; may have padding
    uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
    uint16_t buffidx = sizeof(sdbuffer); // Current position in sdbuffer
    boolean  goodBmp = false;       // Set to true on valid header parse
    boolean  flip = true;        // BMP is stored bottom-to-top
    int      w, h, row, col;
    uint8_t  r, g, b;
    uint32_t pos = 0, startTime = millis();

    uint16_t awColors[320];  // hold colors for one row at a time...

    if ((x >= tft.width()) || (y >= tft.height())) return;

    serialPrint(F("Loading image '"));
    serialPrint(filename);
    serialPrint('\'');

    // Open requested file on SD card
    bmpFile = sd.open(filename);

    if (!bmpFile.isFile())
    {
        serialPrint(F("File not found"));
        return;
    }

    elapsedMicros usec;
    uint32_t us;
    uint32_t total_seek = 0;
    uint32_t total_read = 0;
    uint32_t total_parse = 0;
    uint32_t total_draw = 0;

    // Parse BMP header
    if (read16(bmpFile) == 0x4D42)
    { // BMP signature
        serialPrint(F("File size: "));
        serialPrint(read32(bmpFile));
        (void)read32(bmpFile); // Read & ignore creator bytes
        bmpImageoffset = read32(bmpFile); // Start of image data
        serialPrint(F("Image Offset: "));
        serialPrint(bmpImageoffset);
        // Read DIB header
        serialPrint(F("Header size: "));
        serialPrint(read32(bmpFile));
        bmpWidth = read32(bmpFile);
        bmpHeight = read32(bmpFile);
        if (read16(bmpFile) == 1)
        { // # planes -- must be '1'
            bmpDepth = read16(bmpFile); // bits per pixel
            serialPrint(F("Bit Depth: ")); serialPrint(bmpDepth);
            if ((bmpDepth == 24) && (read32(bmpFile) == 0))
            { // 0 = uncompressed

                goodBmp = true; // Supported BMP format -- proceed!
                serialPrint(F("Image size: "));
                serialPrint(bmpWidth);
                serialPrint('x');
                serialPrint(bmpHeight);

                // BMP rows are padded (if needed) to 4-byte boundary
                rowSize = (bmpWidth * 3 + 3) & ~3;

                // If bmpHeight is negative, image is in top-down order.
                // This is not canon but has been observed in the wild.
                if (bmpHeight < 0)
                {
                    bmpHeight = -bmpHeight;
                    flip = false;
                }

                // Crop area to be loaded
                w = bmpWidth;
                h = bmpHeight;
                if ((x + w - 1) >= tft.width())  w = tft.width() - x;
                if ((y + h - 1) >= tft.height()) h = tft.height() - y;

                usec = 0;
                for (row = 0; row<h; row++)
                { // For each scanline...

                  // Seek to start of scan line.  It might seem labor-
                  // intensive to be doing this on every line, but this
                  // method covers a lot of gritty details like cropping
                  // and scanline padding.  Also, the seek only takes
                  // place if the file position actually needs to change
                  // (avoids a lot of cluster math in SD library).
                    if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
                        pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
                    else     // Bitmap is stored top-to-bottom
                        pos = bmpImageoffset + row * rowSize;
                    if (bmpFile.position() != pos)
                    { // Need seek?
                        bmpFile.seek(pos);
                        buffidx = sizeof(sdbuffer); // Force buffer reload
                    }
                    us = usec;
                    usec -= us;
                    total_seek += us;

                    for (col = 0; col<w; col++)
                    { // For each pixel...
                      // Time to read more pixel data?
                        if (buffidx >= sizeof(sdbuffer))
                        { // Indeed
                            us = usec;
                            usec -= us;
                            total_parse += us;
                            bmpFile.read(sdbuffer, sizeof(sdbuffer));
                            buffidx = 0; // Set index to beginning
                            us = usec;
                            usec -= us;
                            total_read += us;
                        }

                        // Convert pixel from BMP to TFT format, push to display
                        b = sdbuffer[buffidx++];
                        g = sdbuffer[buffidx++];
                        r = sdbuffer[buffidx++];
                        awColors[col] = tft.color565(r, g, b);
                    } // end pixel
                    us = usec;
                    usec -= us;
                    total_parse += us;
                    tft.writeRect(0, row, w, 1, awColors);
                    us = usec;
                    usec -= us;
                    total_draw += us;
                } // end scanline
                serialPrint(F("Loaded in "));
                serialPrint(millis() - startTime);
                serialPrint(" ms");
                serialPrint("Seek: ");
                serialPrint(total_seek);
                serialPrint("Read: ");
                serialPrint(total_read);
                serialPrint("Parse: ");
                serialPrint(total_parse);
                serialPrint("Draw: ");
                serialPrint(total_draw);
            } // end goodBmp
        }
    }

    bmpFile.close();
    if (!goodBmp) serialPrint(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f)
{
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(File &f)
{
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}
