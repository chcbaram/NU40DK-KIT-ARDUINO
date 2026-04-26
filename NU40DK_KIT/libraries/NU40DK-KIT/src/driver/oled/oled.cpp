#include "oled.h"



#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       32

#define MAX_OLED_LINES      4
#define MAX_CHARS_PER_LINE  21 // 128px / 6px = Max 21 characters

static String oledLines[MAX_OLED_LINES] = {"", "", "", ""};





OLed::~OLed()
{
    
}

boolean OLed::begin(void)
{
  boolean ret;


  Wire.begin();    
  ret = Adafruit_SSD1306::begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);

  textcolor = WHITE;
  textbgcolor = BLACK;

  clearDisplay(); 
  

  is_init = true;
  return ret;
}

void OLed::printf(int x, int y,  const char *fmt, ...)
{
  int32_t ret = 0;
  va_list arg;
  va_start (arg, fmt);
  int32_t len;
  char print_buffer[256];
  int Str_Size;
  int Size_Char;
  int i, x_Pre = x;
  PHAN_FONT_OBJ FontBuf;


  len = vsnprintf(print_buffer, 255, fmt, arg);
  va_end (arg);

  for( i=0; i<len; i+=Size_Char )
  {
    PHan_FontLoad( &print_buffer[i], &FontBuf );
      
      
    disHanFont( x, y, &FontBuf );
                      
    Size_Char = FontBuf.Size_Char;
    if (Size_Char >= 2)
    {           
        x += 2*8;
    }       
    else
    {
        x += 1*8;
    }
    
    if( 128 < x ) 
    {
        x  = x_Pre;
        y += 16;
    }
    
    if( FontBuf.Code_Type == PHAN_END_CODE ) break;
  }         
}

void OLed::disHanFont(int x, int y, PHAN_FONT_OBJ *FontPtr)
{
  uint16_t    i, j, Loop;
  uint16_t  FontSize = FontPtr->Size_Char;
  uint16_t index_x;

  if (FontSize > 2)
  {
    FontSize = 2;
  }

  for ( i = 0 ; i < 16 ; i++ )        // 16 Lines per Font/Char
  {
    index_x = 0;
    for ( j = 0 ; j < FontSize ; j++ )      // 16 x 16 (2 Bytes)
    {
      for( Loop=0; Loop<8; Loop++ )
      {
        if( FontPtr->FontBuffer[i*FontSize +j] & (0x80>>Loop))
        {
          drawPixel(x + index_x++, y + i, textcolor);
        } 
        else
        {
          drawPixel(x + index_x++, y + i, textbgcolor);
        }
      }
    }
  }
}

static void pushOledLine(String line)
{
  for (int i = 0; i < MAX_OLED_LINES - 1; i++)
  {
    oledLines[i] = oledLines[i + 1];
  }
  oledLines[MAX_OLED_LINES - 1] = line;
}

void OLed::logPrint(String msg, bool is_serial_log)
{
  if (is_serial_log)
    Serial.println(msg);

  int len = msg.length();
  if (len == 0)
  {
    pushOledLine("");
  }
  else
  {
    for (int i = 0; i < len; i += MAX_CHARS_PER_LINE)
    {
      pushOledLine(msg.substring(i, i + MAX_CHARS_PER_LINE));
    }
  }

  clearDisplay();
  setCursor(0, 0);
  for (int i = 0; i < MAX_OLED_LINES; i++)
  {
    println(oledLines[i]);
  }
  display();
}

void OLed::logPrintf(const char *fmt, ...)
{
char buf[128]; // 메시지를 담을 충분한 크기의 버퍼 (필요에 따라 조절)
  
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  String msg = String(buf);

  // --- 이후 로직은 logPrint와 동일하게 구성 ---
  
  // 1. Serial 로그 출력 (기본적으로 출력하도록 설정)
  // Serial.println(msg);

  int len = msg.length();
  if (len == 0)
  {
    pushOledLine("");
  }
  else
  {
    // 2. 글자 수에 따라 줄바꿈 처리하여 oledLines 배열에 푸시
    for (int i = 0; i < len; i += MAX_CHARS_PER_LINE)
    {
      pushOledLine(msg.substring(i, min(i + MAX_CHARS_PER_LINE, len)));
    }
  }

  // 3. OLED 화면 갱신
  clearDisplay();
  setCursor(0, 0);
  for (int i = 0; i < MAX_OLED_LINES; i++)
  {
    println(oledLines[i]);
  }
  display();
}