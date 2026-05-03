#include "i2s.h"



#ifdef _USE_HW_I2S
#include <SD.h>
#include "driver/cli/cli.h"
#include "common/mixer.h"
#include <nrfx_i2s.h>
#include <nrf_gpio.h>


#define lock()   xSemaphoreTake(mutex_lock, portMAX_DELAY);
#define unLock() xSemaphoreGive(mutex_lock);
static SemaphoreHandle_t mutex_lock;


#define I2S_MAX_CH      1

#define I2S_SAMPLERATE_HZ       16000
#define I2S_BUF_MS              (4)
#define I2S_BUF_CNT             16
#define I2S_BUF_FRAME_LEN       ((48000 * 2 * I2S_BUF_MS) / 1000)  // 48Khz, Stereo, 4ms


#define I2S_SCK_PIN             _PINNUM(1, 9)
#define I2S_LRCK_PIN            _PINNUM(1, 8)
#define I2S_MCK_PIN             NRFX_I2S_PIN_NOT_USED
#define I2S_SDOUT_PIN           _PINNUM(0, 19)
#define I2S_SDIN_PIN            NRFX_I2S_PIN_NOT_USED
#define I2S_IRQ_PRIORITY        7




typedef struct
{
  int16_t volume;
} i2s_cfg_t;



static void cliI2s(cli_args_t *args);
static bool i2sInitHw(void);
static void i2s_data_handler(nrfx_i2s_buffers_t const *p_released, uint32_t status);

static bool      is_init         = false;
static bool      is_started      = false;
static uint32_t  i2s_sample_rate = I2S_SAMPLERATE_HZ;
static uint32_t  i2s_frame_len   = 0;
static int16_t   i2s_volume      = 100;
static bool      is_beep         = false;
static bool      is_stop         = false;


static mixer_t           mixer;
static i2s_cfg_t         i2s_cfg;
static nrfx_i2s_config_t i2s_hw_cfg;

__attribute__((aligned(64))) 
static int16_t i2s_frame_buf[I2S_BUF_FRAME_LEN * 2];

__attribute__((aligned(64))) 
static int16_t i2s_frame_buf_zero[I2S_BUF_FRAME_LEN] = {0, };



bool i2sInit(void)
{
  bool ret = true;

  mutex_lock = xSemaphoreCreateMutex();

  i2s_frame_len = (i2s_sample_rate * 2 * I2S_BUF_MS) / 1000;

  mixerInit(&mixer);

  ret = i2sInitHw();
  is_init = ret;

  cliAdd("i2s", cliI2s);

  return ret;
}

bool i2sInitHw(void)
{
  i2s_hw_cfg = {
    .sck_pin      = I2S_SCK_PIN,
    .lrck_pin     = I2S_LRCK_PIN,
    .mck_pin      = I2S_MCK_PIN,
    .sdout_pin    = I2S_SDOUT_PIN,
    .sdin_pin     = I2S_SDIN_PIN,
    .irq_priority = I2S_IRQ_PRIORITY,
    .mode         = NRF_I2S_MODE_MASTER,
    .format       = NRF_I2S_FORMAT_I2S,
    .alignment    = NRF_I2S_ALIGN_LEFT,
    .sample_width = NRF_I2S_SWIDTH_16BIT,
    .channels     = NRF_I2S_CHANNELS_STEREO,
    .mck_setup    = NRF_I2S_MCK_32MDIV31,
    .ratio        = NRF_I2S_RATIO_64X,
  };
  
  nrfx_err_t err = nrfx_i2s_init(&i2s_hw_cfg, i2s_data_handler);
  if (err != NRFX_SUCCESS)
  {
    return false;
  }


  return true;
}

void i2s_data_handler(nrfx_i2s_buffers_t const *p_released, uint32_t status)
{
  static uint8_t index = 0;


  if (status & NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED)
  {
    // 다음으로 전송할 버퍼 설정
    nrfx_i2s_buffers_t next_buffers;

    if (mixerAvailable(&mixer) >= i2s_frame_len)
    {
      mixerRead(&mixer, &i2s_frame_buf[index * I2S_BUF_FRAME_LEN], i2s_frame_len);

      if (is_stop)
        next_buffers.p_tx_buffer = (uint32_t *)&i2s_frame_buf_zero[0];
      else
        next_buffers.p_tx_buffer = (uint32_t *)&i2s_frame_buf[index * I2S_BUF_FRAME_LEN];
    }
    else
    {
      is_stop = false;
      next_buffers.p_tx_buffer = (uint32_t *)&i2s_frame_buf_zero[0];
    }
    index ^= 1;

    // RX를 사용하지 않으므로 NULL 설정
    next_buffers.p_rx_buffer = NULL;

    // 버퍼 교체
    nrfx_i2s_next_buffers_set(&next_buffers);
  }
}

bool i2sCfgLoad(void)
{
  bool ret = true;

  i2s_cfg.volume = i2s_volume;

  i2sSetVolume(i2s_cfg.volume);
  return ret;
}

bool i2sCfgSave(void)
{
  bool ret = true;

  i2s_cfg.volume = i2s_volume;

  return ret;
}

bool i2sSetSampleRate(uint32_t freq)
{
  typedef struct
  {
    uint32_t        target_freq;
    nrf_i2s_mck_t   mck_setup;
    nrf_i2s_ratio_t ratio;
  } i2s_freq_config_t;

  static const i2s_freq_config_t m_freq_table[] = {
    {8000,  NRF_I2S_MCK_32MDIV63, NRF_I2S_RATIO_64X}, // 실제: 8,064 Hz
    {16000, NRF_I2S_MCK_32MDIV31, NRF_I2S_RATIO_64X}, // 실제: 16,129 Hz
    {32000, NRF_I2S_MCK_32MDIV31, NRF_I2S_RATIO_32X}, // 실제: 32,258 Hz
    {44100, NRF_I2S_MCK_32MDIV23, NRF_I2S_RATIO_32X}, // 실제: 43,478 Hz
    {48000, NRF_I2S_MCK_32MDIV21, NRF_I2S_RATIO_32X}  // 실제: 47,619 Hz
  };
  bool ret = false;

  nrf_i2s_mck_t   selected_mck   = NRF_I2S_MCK_32MDIV31; 
  nrf_i2s_ratio_t selected_ratio = NRF_I2S_RATIO_64X;
  bool            match_found    = false;
  uint16_t        freq_list_max  = sizeof(m_freq_table) / sizeof(m_freq_table[0]);


  for (size_t i = 0; i < freq_list_max; i++)
  {
    if (m_freq_table[i].target_freq == freq)
    {
      selected_mck   = m_freq_table[i].mck_setup;
      selected_ratio = m_freq_table[i].ratio;
      match_found    = true;
      break;
    }
  }

  if (match_found)
  {    
    bool is_restart = false;

    if (is_started)
    {
      i2sStop();
      is_restart = true;
    }

    lock();
    i2s_sample_rate = freq;
    i2s_frame_len   = (i2s_sample_rate * 2 * I2S_BUF_MS) / 1000;

    i2s_hw_cfg.mck_setup = selected_mck;
    i2s_hw_cfg.ratio     = selected_ratio;

    nrfx_i2s_uninit();    
    nrfx_err_t err = nrfx_i2s_init(&i2s_hw_cfg, i2s_data_handler);
    if (err == NRFX_SUCCESS)
    {
      ret = true;
    }    
    unLock();    

    if (is_restart)
      i2sStart();
  }

  return ret;
}

uint32_t i2sGetSampleRate(void)
{
  return i2s_sample_rate;
}

bool i2sStart(void)
{
  bool ret = false;
  int i2s_ret;
  nrfx_err_t err;


  if (is_started)
    return true;

  lock();
  nrfx_i2s_buffers_t initial_buffers = {
    .p_rx_buffer = NULL,
    .p_tx_buffer = (uint32_t *)i2s_frame_buf_zero,
  };

  err = nrfx_i2s_start(&initial_buffers, i2s_frame_len / 2, 0);
  if (err == NRFX_SUCCESS)
  {
    is_started = true;
  }
  else 
  {
    is_started = false;
  }

  unLock();  
  ret = is_started;

  return ret;
}

bool i2sStop(void)
{
  if (!is_started)
    return true;

  lock();
  is_started = false;
  nrfx_i2s_stop();
  unLock();
  delay(10);
  
  return true;
}

int8_t i2sGetEmptyChannel(void)
{
  return mixerGetEmptyChannel(&mixer);
}

uint32_t i2sGetFrameSize(void)
{
  return i2s_frame_len;
}

uint32_t i2sAvailableForWrite(uint8_t ch)
{
  return mixerAvailableForWrite(&mixer, ch);
}

bool i2sWrite(uint8_t ch, int16_t *p_data, uint32_t length)
{
  return mixerWrite(&mixer, ch, p_data, length);
}

uint32_t i2sWriteTimeout(uint8_t ch, int16_t *p_data, uint32_t length, uint32_t timeout)
{
  uint32_t pre_time;
  uint32_t buf_i;
  uint32_t remain;
  uint32_t wr_len;

  buf_i = 0;
  pre_time = millis();
  while(buf_i < length)
  {
    remain = length - buf_i;
    wr_len = min(mixerAvailableForWrite(&mixer, ch), remain);

    mixerWrite(&mixer, ch, &p_data[buf_i], wr_len);
    buf_i += wr_len;

    if (millis()-pre_time >= timeout)
    {
      break;
    }
    delay(1);
  }

  return buf_i;
}

// https://m.blog.naver.com/PostView.nhn?blogId=hojoon108&logNo=80145019745&proxyReferer=https:%2F%2Fwww.google.com%2F
//
float i2sGetNoteHz(int8_t octave, int8_t note)
{
  float hz;
  float f_note;

  if (octave < 1) octave = 1;
  if (octave > 8) octave = 8;

  if (note <  1) note = 1;
  if (note > 12) note = 12;

  f_note = (float)(note-10)/12.0f;

  hz = pow(2, (octave-1)) * 55 * pow(2, f_note);

  return hz;
}

// https://gamedev.stackexchange.com/questions/4779/is-there-a-faster-sine-function
//
float i2sSin(float x)
{
  const float B = 4 / M_PI;
  const float C = -4 / (M_PI * M_PI);

  return -(B * x + C * x * ((x < 0) ? -x : x));
}

bool i2sStopBeep(void)
{
  is_beep = true;
  is_stop = true;
  delay(10);  
  return true;
}

bool i2sPlayBeep(uint32_t freq_hz, uint16_t volume, uint32_t time_ms)
{
  uint32_t pre_time;
  int32_t sample_rate = i2s_sample_rate;
  int32_t num_samples = i2s_frame_len;
  float sample_point;
  int16_t sample[num_samples];
  int16_t sample_index = 0;
  float div_freq;
  int8_t mix_ch;
  int32_t volume_out;


  volume = constrain(volume, 0, 100);
  volume_out = (INT16_MAX/40) * volume / 100;

  mix_ch =  i2sGetEmptyChannel();

  div_freq = (float)sample_rate/(float)freq_hz;

  is_beep = true;
  pre_time = millis();
  while(millis()-pre_time <= time_ms)
  {
    if (i2sAvailableForWrite(mix_ch) >= num_samples)
    {
      for (int i=0; i<num_samples; i+=2)
      {
        sample_point = i2sSin(2.0f * (float)M_PI * (float)(sample_index) / ((float)div_freq));
        sample[i + 0] = (int16_t)(sample_point * volume_out);
        sample[i + 1] = (int16_t)(sample_point * volume_out);
        sample_index = (sample_index + 1) % (int)(div_freq);
      }
      i2sWrite(mix_ch, (int16_t *)sample, num_samples);

      if (!is_beep)
        break;      
    }
    delay(2);

    if (!is_beep)
      break;
  }  

  return true;
}

int16_t i2sGetVolume(void)
{
  return i2s_volume;
}

bool i2sSetVolume(int16_t volume)
{
  volume = constrain(volume, 0, 100);
  i2s_volume = volume;

  mixerSetVolume(&mixer, i2s_volume);
  return true;
}

typedef struct wavfile_header_s
{
  char    ChunkID[4];     /*  4   */
  int32_t ChunkSize;      /*  4   */
  char    Format[4];      /*  4   */

  char    Subchunk1ID[4]; /*  4   */
  int32_t Subchunk1Size;  /*  4   */
  int16_t AudioFormat;    /*  2   */
  int16_t NumChannels;    /*  2   */
  int32_t SampleRate;     /*  4   */
  int32_t ByteRate;       /*  4   */
  int16_t BlockAlign;     /*  2   */
  int16_t BitsPerSample;  /*  2   */

  char    Subchunk2ID[4];
  int32_t Subchunk2Size;
} wavfile_header_t;

void cliI2s(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {

    cliPrintf("i2s init      : %d\n", is_init);
    cliPrintf("i2s rate      : %d Khz\n", i2s_sample_rate/1000);
    cliPrintf("i2s buf ms    : %d ms\n", I2S_BUF_MS);
    cliPrintf("i2s frame len : %d \n", i2s_frame_len);
    cliPrintf("i2s volume    : %d \n", i2sGetVolume());
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "start") == true)
  {
    bool i2s_ret;

    i2s_ret = i2sStart();
    cliPrintf("i2sStart() -> %d\n", i2s_ret);
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "stop") == true)
  {
    bool i2s_ret;

    i2s_ret = i2sStop();
    cliPrintf("i2sStop() -> %d\n", i2s_ret);
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "beep") == true)
  {
    uint32_t freq;
    uint32_t time_ms;

    freq = args->getData(1);
    time_ms = args->getData(2);
    
    i2sPlayBeep(freq, 100, time_ms);

    ret = true;
  }

  if (args->argc >= 1 && args->isStr(0, "volume") == true)
  {
    uint8_t volume;


    if (args->argc == 1)
    {
      cliPrintf("Volume : %d%%\n", i2sGetVolume());
    }
    else
    {
      volume = args->getData(1);
      i2sSetVolume(volume);
      i2sCfgSave();
      cliPrintf("i2s volume : %d \n", i2sGetVolume());
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "melody"))
  {
    uint16_t melody[] = {NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
    int note_durations[] = { 4, 8, 8, 4, 4, 4, 4, 4 };
    int16_t note_min = 10000;
    int16_t note_max = 0;

    for (int i=0; i<8; i++) 
    {
      if (melody[i] == 0)
        continue;

      if (melody[i] < note_min)
        note_min = melody[i];
      if (melody[i] > note_max)
        note_max = melody[i];
    }    

    for (int i=0; i<8; i++) 
    {
      int note_duration = 1000 / note_durations[i];

      i2sPlayBeep(melody[i], 100, note_duration);
      delay(note_duration * 0.3);    
    }

    #if HW_I2S_LCD > 0
    lcdClear(black);
    #endif    
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "wav") == true)
  {
    char *file_name;
    File fp;
    wavfile_header_t header;
    uint32_t r_len;
    int32_t  volume = 100;
    int8_t ch;

    file_name = args->getStr(1);

    cliPrintf("FileName      : %s\n", file_name);


    fp = SD.open(file_name, FILE_READ);
    if (!fp)
    {
      cliPrintf("fopen fail : %s\n", file_name);
      return;
    }
    if (fp.read((uint8_t *)&header, sizeof(wavfile_header_t)) != sizeof(wavfile_header_t))
    {
      cliPrintf("Header read fail\n");
      fp.close();
      return;
    }

    cliPrintf("ChunkSize     : %d\n", header.ChunkSize);
    cliPrintf("Format        : %c%c%c%c\n", header.Format[0], header.Format[1], header.Format[2], header.Format[3]);
    cliPrintf("Subchunk1Size : %d\n", header.Subchunk1Size);
    cliPrintf("AudioFormat   : %d\n", header.AudioFormat);
    cliPrintf("NumChannels   : %d\n", header.NumChannels);
    cliPrintf("SampleRate    : %d\n", header.SampleRate);
    cliPrintf("ByteRate      : %d\n", header.ByteRate);
    cliPrintf("BlockAlign    : %d\n", header.BlockAlign);
    cliPrintf("BitsPerSample : %d\n", header.BitsPerSample);
    cliPrintf("Subchunk2Size : %d\n", header.Subchunk2Size);


    i2sSetSampleRate(header.SampleRate);

    r_len = i2sGetFrameSize()/2;

    int16_t buf_frame[i2sGetFrameSize()];

    fp.seek(sizeof(wavfile_header_t) + 1024);

    ch = i2sGetEmptyChannel();

    while(cliKeepLoop())
    {
      int len;


      if (i2sAvailableForWrite(ch) >= i2s_frame_len)
      {        
        uint32_t bytes_to_read = r_len * 2 * header.NumChannels;

        len = fp.read((uint8_t *)buf_frame, bytes_to_read);        

        if (len != bytes_to_read)
        {
          break;
        }

        int16_t buf_data[2];

        for (int i=0; i<r_len; i++)
        {
          if (header.NumChannels == 2)
          {
            buf_data[0] = buf_frame[i*2 + 0] * volume / 100;;
            buf_data[1] = buf_frame[i*2 + 1] * volume / 100;;
          }
          else
          {
            buf_data[0] = buf_frame[i] * volume / 100;;
            buf_data[1] = buf_frame[i] * volume / 100;;
          }

          i2sWrite(ch, (int16_t *)buf_data, 2);
        }
      }
      delay(1);      
    }
    fp.close();

    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("i2s info\n");
    cliPrintf("i2s start\n");
    cliPrintf("i2s stop\n");    
    cliPrintf("i2s melody\n");
    cliPrintf("i2s volume 0~100\n");
    cliPrintf("i2s beep freq time_ms\n");
    cliPrintf("i2s wav filename\n");
  }
}

#endif