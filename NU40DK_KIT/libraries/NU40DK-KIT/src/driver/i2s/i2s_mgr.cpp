#include "i2s_mgr.h"
#include "i2s.h"

#include <SD.h>
#include "driver/cli/cli.h"

// I2S 명령어 타입 정의
typedef enum
{
  I2S_CMD_PLAY_FILE,
  I2S_CMD_BEEP,
  I2S_CMD_WRITE,
  I2S_CMD_STOP
} i2s_cmd_type_t;

// 큐에 전달할 메시지 구조체
typedef struct
{
  i2s_cmd_type_t cmd;
  uint32_t       id;
  char           file_name[64]; 

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



I2S_MGR::I2S_MGR() : is_init(false), m_volume(100), m_next_id(1), m_stop_id(0), m_cmd_queue(NULL)
{
  for (int i = 0; i < I2S_MAX_WORKERS; i++)
  {
    m_worker_handles[i] = NULL;
    m_active_ids[i] = 0; // 0은 대기 중(ID 없음)을 의미
  }
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
  
  m_cmd_queue = xQueueCreate(8, sizeof(i2s_msg_t));
  if (m_cmd_queue == NULL) 
    return false;

  for (int i = 0; i < I2S_MAX_WORKERS; i++)
  {
    BaseType_t task_ret = xTaskCreate(
    I2S_MGR::i2sWorkerTask,
    "i2s_worker",
    4096,
    this,
    configMAX_PRIORITIES - 1,
    &m_worker_handles[i]);

    if (task_ret != pdPASS) return false;
  }

  is_init = true;

  return ret;
}

bool I2S_MGR::isBusy(uint32_t id) const
{
  if (id == 0)
  {
    for (int i = 0; i < I2S_MAX_WORKERS; i++)
    {
      if (m_active_ids[i] != 0) return true;
    }    
  }
  else
  {
    for (int i = 0; i < I2S_MAX_WORKERS; i++)
    {
      if (m_active_ids[i] == id) return true;
    }
  }
  return false;
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

uint32_t I2S_MGR::playFile(const char *file_name)
{
  if (!is_init || m_cmd_queue == NULL) return 0;

  i2s_msg_t msg;
  msg.cmd = I2S_CMD_PLAY_FILE;
  vTaskSuspendAll();
  msg.id  = m_next_id++;
  if (m_next_id == 0) m_next_id = 1; 
  xTaskResumeAll();

  strncpy(msg.file_name, file_name, sizeof(msg.file_name) - 1);
  msg.file_name[sizeof(msg.file_name) - 1] = '\0';

  if (xQueueSend(m_cmd_queue, &msg, 0) != pdPASS)
  {
    return 0;
  }
  return msg.id; // 할당된 ID 반환
}

uint32_t I2S_MGR::beep(uint32_t freq_hz, uint32_t duration_ms)
{
  if (!is_init || m_cmd_queue == NULL) return 0;

  i2s_msg_t msg;
  msg.cmd = I2S_CMD_BEEP;
  vTaskSuspendAll();
  msg.id  = m_next_id++;
  if (m_next_id == 0) m_next_id = 1;
  xTaskResumeAll();

  msg.beep.freq_hz     = freq_hz;
  msg.beep.duration_ms = duration_ms;

  if (xQueueSend(m_cmd_queue, &msg, 0) != pdPASS)
  {
    return 0;
  }
  return msg.id;
}

void I2S_MGR::stop(uint32_t id)
{
  if (!is_init || id == 0) return;

  if (!isBusy(id)) return;

  m_stop_id = id;

  uint32_t start_time = millis();
  while (isBusy(id))
  {
    if (millis() - start_time >= 100) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (m_stop_id == id)
  {
    m_stop_id = 0;
  }
}

void I2S_MGR::stopAll(void)
{
  if (!is_init) return;

  for (int i = 0; i < I2S_MAX_WORKERS; i++)
  {
    uint32_t id = m_active_ids[i];
    if (id != 0)
    {
      stop(id);
    }
  }
}

void I2S_MGR::i2sWorkerTask(void *pvParameters)
{
  I2S_MGR  *p_mgr = (I2S_MGR *)pvParameters;
  i2s_msg_t msg;

  // 현재 태스크가 관리할 인덱스 찾기
  int          worker_idx     = -1;
  TaskHandle_t current_handle = xTaskGetCurrentTaskHandle();
  for (int i = 0; i < I2S_MAX_WORKERS; i++)
  {
    if (p_mgr->m_worker_handles[i] == current_handle)
    {
      worker_idx = i;
      break;
    }
  }

  while (1)
  {
    if (xQueueReceive(p_mgr->m_cmd_queue, &msg, portMAX_DELAY) == pdPASS)
    {
      if (msg.cmd == I2S_CMD_STOP) continue;

      // 워커가 할당받은 ID를 등록하여 활성 상태로 전환
      if (worker_idx != -1)
      {
        p_mgr->m_active_ids[worker_idx] = msg.id;
      }

      switch (msg.cmd)
      {
        case I2S_CMD_PLAY_FILE:
          p_mgr->processPlayFile(msg.id, msg.file_name);
          break;

        case I2S_CMD_BEEP:
          p_mgr->processBeep(msg.id, msg.beep.freq_hz, msg.beep.duration_ms);
          break;

        default:
          break;
      }

      // 작업 완료 후 ID 해제
      if (worker_idx != -1)
      {
        p_mgr->m_active_ids[worker_idx] = 0;
      }
    }
  }
}

void I2S_MGR::processPlayFile(uint32_t id, const char *file_name)
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

  while (fp.available())
  {
    // 정지 요청된 ID가 내 ID인지 확인
    if (m_stop_id == id)
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

void I2S_MGR::processBeep(uint32_t id, uint32_t freq_hz, uint32_t duration_ms)
{
  uint32_t num_samples = i2sGetFrameSize(); // 프레임 사이즈
  float    sample_point;
  int16_t  sample[num_samples];
  uint16_t sample_index = 0;
  float    div_freq;
  int8_t   mix_ch;
  int32_t  volume_out;


  uint16_t current_vol = constrain(m_volume, 0, 100);
  volume_out = (INT16_MAX / 40) * current_vol / 100;

  // 비어있는 I2S 믹서 채널 할당받음
  mix_ch = i2sGetEmptyChannel();
  if (mix_ch < 0)
  {
    return;
  }

  div_freq = (float)i2sGetSampleRate() / (float)freq_hz;

  uint32_t pre_time = millis();

  while (millis() - pre_time <= duration_ms)
  {
    if (m_stop_id == id)
    {
      break;
    }

    if (i2sAvailableForWrite(mix_ch) >= num_samples)
    {
      for (uint32_t i = 0; i < num_samples; i += 2)
      {
        sample_point = i2sSin(2.0f * (float)M_PI * (float)sample_index / div_freq);
        
        sample[i + 0] = (int16_t)(sample_point * volume_out); // Left
        sample[i + 1] = (int16_t)(sample_point * volume_out); // Right
        
        sample_index = (sample_index + 1) % (int)div_freq;
      }

      i2sWrite(mix_ch, (int16_t *)sample, num_samples);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
