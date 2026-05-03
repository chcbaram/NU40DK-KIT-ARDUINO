#ifndef _I2S_MGR_H_
#define _I2S_MGR_H_

#include <Arduino.h>


class I2S_MGR
{
  public:
    I2S_MGR();
    ~I2S_MGR();
    
    bool begin(void);

    bool playFile(const char* file_name);
    bool beep(uint32_t freq_hz, uint32_t duration_ms);
    void stop(void);

    void setVolume(uint8_t vol);
    uint8_t getVolume(void) const;

    bool isBusy(void) const;

  private:
    bool           is_init;
    uint8_t        m_volume;
    volatile bool  m_is_busy;

    QueueHandle_t  m_cmd_queue;
    TaskHandle_t   m_task_handle;

    static void i2sTask(void *pvParameters);

    void processPlayFile(const char* file_name);
    void processBeep(uint32_t freq_hz, uint32_t duration_ms);
    bool setSampleRate(uint32_t sample_rate);
};



#endif