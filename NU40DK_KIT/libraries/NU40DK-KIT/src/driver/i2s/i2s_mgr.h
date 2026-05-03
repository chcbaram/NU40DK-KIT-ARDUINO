#ifndef _I2S_MGR_H_
#define _I2S_MGR_H_

#include <Arduino.h>


#define I2S_MAX_WORKERS  4


class I2S_MGR
{
  public:
    I2S_MGR();
    ~I2S_MGR();
    
    bool begin(void);

    uint32_t playFile(const char *file_name);
    uint32_t beep(uint32_t freq_hz, uint32_t duration_ms);
    
    void stop(uint32_t id); 
    void stopAll(void);

    void setVolume(uint8_t vol);
    uint8_t getVolume(void) const;

    bool isBusy(uint32_t id = 0) const; 

  private:
    bool           is_init;
    uint8_t        m_volume;

    uint32_t          m_next_id;    // 다음 발급할 고유 ID
    volatile uint32_t m_stop_id;    // 현재 정지 요청된 ID를 임시 보관
    volatile uint32_t m_active_ids[I2S_MAX_WORKERS]; // 현재 워커들이 재생 중인 ID 목록

    QueueHandle_t  m_cmd_queue;
    TaskHandle_t   m_worker_handles[I2S_MAX_WORKERS];

    static void i2sWorkerTask(void *pvParameters);

    void processPlayFile(uint32_t id, const char *file_name);
    void processBeep(uint32_t id, uint32_t freq_hz, uint32_t duration_ms);
    bool setSampleRate(uint32_t sample_rate);
};



#endif