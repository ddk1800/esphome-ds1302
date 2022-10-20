#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/time/real_time_clock.h"

#define _BV(b) (1UL << (b))

namespace esphome {
namespace ds1302 {

class DS1302Component : public time::RealTimeClock{
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void read_time();
  void write_time();
  void set_pinDATA(GPIOPin *pin) { pinDATA = pin; }
  void set_pinCLK(GPIOPin *pin) { pinCLK = pin; }
  void set_pinRESET(GPIOPin *pin) { pinRESET = pin; }

 protected:
  bool read_rtc_();
  bool write_rtc_();
  union DS1302Reg {
    struct {
      //0
      uint8_t second : 4;
      uint8_t second_10 : 3;
      bool ch : 1;

      //1
      uint8_t minute : 4;
      uint8_t minute_10 : 3;
      uint8_t unused_1 : 1;

      //2
      uint8_t hour : 4;
      uint8_t hour_10 : 2;
      uint8_t unused_2 : 2;

      //3
      uint8_t weekday : 3;
      uint8_t unused_3 : 5;

      //4
      uint8_t day : 4;
      uint8_t day_10 : 2;
      uint8_t unused_4 : 2;

      //5
      uint8_t month : 4;
      uint8_t month_10 : 1;
      uint8_t unused_5 : 3;
      
      //6
      uint8_t year : 4;
      uint8_t year_10 : 4;
      
      //7
      uint8_t rs : 2;
      uint8_t unused_6 : 2;
      bool sqwe : 1;
      uint8_t unused_7 : 2;
      bool out : 1;
    } reg;
    uint8_t raw[sizeof(reg)];
  } ds1302_;
  private:
    GPIOPin *pinRESET;                         //  Определяем переменную для хранения номера вывода RST трехпроводной шины
    GPIOPin *pinCLK;                         //  Определяем переменную для хранения номера вывода CLK трехпроводной шины
    GPIOPin *pinDATA;                         //  Определяем переменную для хранения номера вывода DAT трехпроводной шины
    uint8_t arrTimeRegAddr[8]   = {0x81,0x83,0x85,0x8B,0x87,0x89,0x8D,0x8F};       //  Определяем массив с адресами регистров чтения даты и времени    (сек, мин, час, день, месяц, год, день недели)
    uint8_t arrTimeRegMack[8]   = {0x7F,0x7F,0x3F,0x3F,0x3F,0x1F,0xFF,0x07};       //  Определяем маскировочный массив для регистров даты и времени    (при чтении/записи, нужно совершить побитовое «и»)
    uint8_t busRate         = 10;                         //  Скорость передачи данных трехпроводной шине в кГц         (до 255 кГц)
    uint8_t varI;                                     //
    
    void begin_transmission() {
        this->pinRESET->digital_write(0); // default, not enabled
        this->pinRESET->pin_mode(gpio::FLAG_OUTPUT);

        this->pinCLK->digital_write(0); // default, clock low
        this->pinCLK->pin_mode(gpio::FLAG_OUTPUT);
        
        this->pinDATA->pin_mode(gpio::FLAG_OUTPUT);

        this->pinRESET->digital_write(1); // start the session
        delayMicroseconds(4);           // tCC = 4us
    }
    
    void end_transmission() {
        this->pinRESET->digital_write(0);
        delayMicroseconds(4);           // tCWH = 4us
    }

    void transfer_byte(uint8_t j)                           /*  Передача  одного байта    (байт для передачи)                 */  
    {
      uint8_t i=0, n=500/busRate+1; 
      this->pinDATA->pin_mode(gpio::FLAG_OUTPUT); 
      while(i>=0 && i<8){
        this->pinDATA->digital_write(j & _BV(i));
        delayMicroseconds(n); 
        this->pinCLK->digital_write(1); 
        delayMicroseconds(n); 
        this->pinCLK->digital_write(0); i++;
      } 
      // this->pinDATA->pin_mode(gpio::FLAG_INPUT);
    }

    uint8_t read_byte(bool j)
    /*  Получение одного байта    (флаг чтения предустановленного бита с линии DAT) */  
    {
      uint8_t i=0, k=0, n=500/busRate+1; 
      this->pinDATA->pin_mode(gpio::FLAG_INPUT); 
      if(j){
        if(this->pinDATA->digital_read()){
          k |= _BV(i);
        } 
        i++;
      } 
      while(i>=0 && i<8){
        this->pinCLK->digital_write(1); 
        delayMicroseconds(n); 
        this->pinCLK->digital_write(0); 
        delayMicroseconds(n); 
        if(this->pinDATA->digital_read()){
          k |= _BV(i);
        } 
        i++;
      }
      return k;
    }
    
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DS1302Component> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<DS1302Component> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};


}  // namespace ds1302
}  // namespace esphome
