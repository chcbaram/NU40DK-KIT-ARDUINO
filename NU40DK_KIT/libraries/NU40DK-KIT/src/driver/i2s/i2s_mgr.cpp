#include "i2s_mgr.h"
#include "i2s.h"

#include <SD.h>
#include "driver/cli/cli.h"

// I2S 명령어 타입 정의
typedef enum
{
  I2S_CMD_PLAY_FILE,
  I2S_CMD_BEEP,
  I2S_CMD_STOP
} i2s_cmd_type_t;

// 큐에 전달할 메시지 구조체
typedef struct
{
  i2s_cmd_type_t cmd;
  char           file_name[64]; // 재생할 파일 경로 (길이 제한)

  struct
  {
    uint32_t freq_hz;
    uint32_t duration_ms;
  } beep;
} i2s_msg_t;

// WAV 파일 헤더 (기존 유지)
typedef struct
{
  char     ChunkID[4];
  uint32_t ChunkSize;
  char     Format[4];
  char     Subchunk1ID[4];
  uint32_t Subchunk1Size;
  uint16_t AudioFormat;
  uint16_t NumChannels;
  uint32_t SampleRate;
  uint32_t ByteRate;
  uint16_t BlockAlign;
  uint16_t BitsPerSample;
  char     Subchunk2ID[4];
  uint32_t Subchunk2Size;
} wavfile_header_t;



I2S_MGR::I2S_MGR() : is_init(false), m_volume(100), m_cmd_queue(NULL), m_task_handle(NULL)
{
}

I2S_MGR::~I2S_MGR()
{
    
}

bool I2S_MGR::begin(void)
{
  bool ret = false;  

  if (is_init) 
    return true;

  if (i2sInit())
  {
    ret = i2sStart();    
  }
  
  m_cmd_queue = xQueueCreate(4, sizeof(i2s_msg_t));
  if (m_cmd_queue == NULL) 
    return false;

  BaseType_t task_ret = xTaskCreate(
  I2S_MGR::i2sTask,         // 태스크 함수
  "i2s_task",               // 태스크 이름
  4096,                     // 스택 크기 (필요에 따라 조절)
  this,                     // 파라미터 (클래스 인스턴스 포인터)
  configMAX_PRIORITIES - 1, // 오디오 끊김 방지를 위한 높은 우선순위
  &m_task_handle            // 태스크 핸들
  );

  if (task_ret != pdPASS)
  {
    vQueueDelete(m_cmd_queue);
    m_cmd_queue = NULL;
    return false;
  }

  is_init = true;

  return ret;
}

bool I2S_MGR::isBusy(void) const
{
  return m_is_busy;
}

void I2S_MGR::setVolume(uint8_t vol)
{
  if (vol > 100) vol = 100;
  m_volume = vol;
}

uint8_t I2S_MGR::getVolume(void) const
{
  return m_volume;
}

bool I2S_MGR::setSampleRate(uint32_t sample_rate)
{
  return i2sSetSampleRate(sample_rate);
}

bool I2S_MGR::playFile(const char *file_name)
{
  if (!is_init || m_cmd_queue == NULL) return false;

  i2s_msg_t msg;
  msg.cmd = I2S_CMD_PLAY_FILE;
  strncpy(msg.file_name, file_name, sizeof(msg.file_name) - 1);
  msg.file_name[sizeof(msg.file_name) - 1] = '\0';

  // 큐가 가득 차면 즉시 false 리턴
  if (xQueueSend(m_cmd_queue, &msg, 0) != pdPASS)
  {
    return false;
  }
  return true;
}

bool I2S_MGR::beep(uint32_t freq_hz, uint32_t duration_ms)
{
  if (!is_init || m_cmd_queue == NULL) return false;

  i2s_msg_t msg;
  msg.cmd              = I2S_CMD_BEEP;
  msg.beep.freq_hz     = freq_hz;
  msg.beep.duration_ms = duration_ms;

  if (xQueueSend(m_cmd_queue, &msg, 0) != pdPASS)
  {
    return false;
  }
  return true;
}

void I2S_MGR::stop(void)
{
  if (!is_init || m_cmd_queue == NULL) return;

  i2s_msg_t msg;
  msg.cmd = I2S_CMD_STOP;

  xQueueSendToFront(m_cmd_queue, &msg, 0);
  
  i2sStopBeep();

  uint32_t start_time = millis();
  while (m_is_busy)
  {    
    if (millis() - start_time >= 100)
    {
      break;
    }    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void I2S_MGR::i2sTask(void *pvParameters)
{
  I2S_MGR  *p_mgr = (I2S_MGR *)pvParameters;
  i2s_msg_t msg;

  while (1)
  {    
    if (xQueueReceive(p_mgr->m_cmd_queue, &msg, portMAX_DELAY) == pdPASS)
    {
      p_mgr->m_is_busy = true;

      switch (msg.cmd)
      {
        case I2S_CMD_PLAY_FILE:
          p_mgr->processPlayFile(msg.file_name);
          break;

        case I2S_CMD_BEEP:
          p_mgr->processBeep(msg.beep.freq_hz, msg.beep.duration_ms);
          break;

        case I2S_CMD_STOP:
          break;
      }

      p_mgr->m_is_busy = false;
    }
  }
}

void I2S_MGR::processPlayFile(const char *file_name)
{
  File fp = SD.open(file_name, FILE_READ);
  if (!fp) return;

  wavfile_header_t header;
  if (fp.read((uint8_t *)&header, sizeof(wavfile_header_t)) != sizeof(wavfile_header_t))
  {
    fp.close();
    return;
  }

  if (memcmp(header.Format, "WAVE", 4) != 0)
  {
    fp.close();
    return;
  }

  if (!setSampleRate(header.SampleRate))
  {
    fp.close();
    return;
  }

  uint32_t r_len = i2sGetFrameSize() / 2;
  int16_t  buf_frame[i2sGetFrameSize()];

  fp.seek(sizeof(wavfile_header_t) + 1024);

  int8_t ch = i2sGetEmptyChannel();
  if (ch < 0)
  {
    fp.close();
    return;
  }


  i2s_msg_t next_msg;

  while (fp.available())
  {
    if (xQueuePeek(m_cmd_queue, &next_msg, 0) == pdPASS)
    {
      break;
    }

    if (i2sAvailableForWrite(ch) >= i2sGetFrameSize())
    {
      uint32_t bytes_to_read = r_len * 2 * header.NumChannels;
      int      len           = fp.read((uint8_t *)buf_frame, bytes_to_read);

      if (len != bytes_to_read) break;

      int16_t buf_data[2];
      for (uint32_t i = 0; i < r_len; i++)
      {
        if (header.NumChannels == 2)
        {
          buf_data[0] = (int16_t)((int32_t)buf_frame[i * 2 + 0] * m_volume / 100);
          buf_data[1] = (int16_t)((int32_t)buf_frame[i * 2 + 1] * m_volume / 100);
        }
        else
        {
          buf_data[0] = (int16_t)((int32_t)buf_frame[i] * m_volume / 100);
          buf_data[1] = (int16_t)((int32_t)buf_frame[i] * m_volume / 100);
        }
        i2sWrite(ch, buf_data, 2);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }

  fp.close();
}

void I2S_MGR::processBeep(uint32_t freq_hz, uint32_t duration_ms)
{
  i2sPlayBeep(freq_hz, m_volume, duration_ms);    
}
