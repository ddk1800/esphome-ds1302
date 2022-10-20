#include "ds1302.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://datasheets.maximintegrated.com/en/ds/DS1307.pdf

namespace esphome {
namespace ds1302 {

static const char *const TAG = "ds1302";

void DS1302Component::setup() {
  // uint8_t varI;
  ESP_LOGCONFIG(TAG, "Setting up DS1302...");
  this->begin_transmission();
  transfer_byte(0x81);
  varI=this->read_byte(true); 
  this->end_transmission();
  
  if( varI & 0b10000000){
    this->begin_transmission();
    transfer_byte(0x81);
    transfer_byte(varI&~0b10000000);
    this->end_transmission();
  } //    (если установлен 7 бит в 129 регистре, то сбрасываем его - запускаем генератор)
  
  this->begin_transmission();
  transfer_byte(0x85);
  varI=this->read_byte(true);
  this->end_transmission();

  if( varI & 0b10000000){
    this->begin_transmission();
    transfer_byte(0x85);
    transfer_byte(varI&~0b10000000);
    this->end_transmission();
  } //    (если установлен 7 бит в 133 регистре, то сбрасываем его - переводим модуль в 24 часовой режим)
  
  this->begin_transmission();
  transfer_byte(0x8F);
  varI=this->read_byte(true); 
  this->end_transmission();
  
  if( varI & 0b10000000){
    this->begin_transmission();
    transfer_byte(0x8F);
    transfer_byte(varI&~0b10000000);
    this->end_transmission();
  } //    (если установлен 7 бит в 143 регистре, то сбрасываем его - разрешаем запись в регистры модуля)

  this->pinCLK->pin_mode(gpio::FLAG_OUTPUT);
  this->pinDATA->pin_mode(gpio::FLAG_INPUT);
  this->pinRESET->pin_mode(gpio::FLAG_OUTPUT);
  this->pinCLK->digital_write(0);
  this->pinRESET->digital_write(0);
  
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
}

void DS1302Component::update() { this->read_time(); }

void DS1302Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS1302:");
//  LOG_SPI_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS1302 failed!");
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float DS1302Component::get_setup_priority() const { return setup_priority::DATA; }

void DS1302Component::read_time() {
  if (!this->read_rtc_()) {
    return;
  }
  if (ds1302_.reg.ch) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  time::ESPTime rtc_time{.second = uint8_t(ds1302_.reg.second + 10 * ds1302_.reg.second_10),
                         .minute = uint8_t(ds1302_.reg.minute + 10u * ds1302_.reg.minute_10),
                         .hour = uint8_t(ds1302_.reg.hour + 10u * ds1302_.reg.hour_10),
                         .day_of_week = uint8_t(ds1302_.reg.weekday),
                         .day_of_month = uint8_t(ds1302_.reg.day + 10u * ds1302_.reg.day_10),
                         .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
                         .month = uint8_t(ds1302_.reg.month + 10u * ds1302_.reg.month_10),
                         .year = uint16_t(ds1302_.reg.year + 10u * ds1302_.reg.year_10 + 2000)};
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void DS1302Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }
  ds1302_.reg.year = (now.year - 2000) % 10;
  ds1302_.reg.year_10 = (now.year - 2000) / 10 % 10;
  ds1302_.reg.month = now.month % 10;
  ds1302_.reg.month_10 = now.month / 10;
  ds1302_.reg.day = now.day_of_month % 10;
  ds1302_.reg.day_10 = now.day_of_month / 10;
  ds1302_.reg.weekday = now.day_of_week;
  ds1302_.reg.hour = now.hour % 10;
  ds1302_.reg.hour_10 = now.hour / 10;
  ds1302_.reg.minute = now.minute % 10;
  ds1302_.reg.minute_10 = now.minute / 10;
  ds1302_.reg.second = now.second % 10;
  ds1302_.reg.second_10 = now.second / 10;
  ds1302_.reg.ch = false;

  this->write_rtc_();
}

bool DS1302Component::read_rtc_() {
  uint8_t t;
  for(uint8_t i=0;i<sizeof(this->ds1302_.raw)-1;i++){
    this->begin_transmission();
    transfer_byte(arrTimeRegAddr[i]);
    ds1302_.raw[i]=read_byte(true) & arrTimeRegMack[i];
    this->end_transmission();
    // ESP_LOGD(TAG,"%0u Read bit %0u: %0u - %0u", i, arrTimeRegAddr[i], t, ds1302_.raw[i]);
  }
  
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  CH:%s RS:%0u SQWE:%s OUT:%s", ds1302_.reg.hour_10,
           ds1302_.reg.hour, ds1302_.reg.minute_10, ds1302_.reg.minute, ds1302_.reg.second_10, ds1302_.reg.second,
           ds1302_.reg.year_10, ds1302_.reg.year, ds1302_.reg.month_10, ds1302_.reg.month, ds1302_.reg.day_10,
           ds1302_.reg.day, ONOFF(ds1302_.reg.ch), ds1302_.reg.rs, ONOFF(ds1302_.reg.sqwe), ONOFF(ds1302_.reg.out));

  return true;
}

bool DS1302Component::write_rtc_() {
  for(uint8_t i=0;i<sizeof(this->ds1302_.raw)-1;i++){
    this->begin_transmission();
    transfer_byte(arrTimeRegAddr[i]-1);
    transfer_byte(this->ds1302_.raw[i]);
    this->end_transmission();
  }
  this->end_transmission();
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u  CH:%s RS:%0u SQWE:%s OUT:%s", ds1302_.reg.hour_10,
           ds1302_.reg.hour, ds1302_.reg.minute_10, ds1302_.reg.minute, ds1302_.reg.second_10, ds1302_.reg.second,
           ds1302_.reg.year_10, ds1302_.reg.year, ds1302_.reg.month_10, ds1302_.reg.month, ds1302_.reg.day_10,
           ds1302_.reg.day, ONOFF(ds1302_.reg.ch), ds1302_.reg.rs, ONOFF(ds1302_.reg.sqwe), ONOFF(ds1302_.reg.out));
  return true;
}
}  // namespace ds1307
}  // namespace esphome

