#include "contiki.h"

/********************libSENSORES***********************/
#include "dev/sht11/sht11-sensor.h"
#include "dev/battery-sensor.h"
#include "dev/light-sensor.h"

#ifdef X_NUCLEO_IKS01A1
#include "dev/temperature-sensor.h"
#include "dev/humidity-sensor.h"
#include "dev/pressure-sensor.h"
#include "dev/magneto-sensor.h"
#include "dev/acceleration-sensor.h"
#include "dev/gyroscope-sensor.h"
#endif /*X_NUCLEO_IKS01A1*/

/********************libLEDS*************************/
#include "dev/leds.h"
/*****************************************************/

#include <stdio.h>

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "HELLO SENSORES");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/


/*--------------------------SENSORIAMENTO------------------------------------*/
//light-sensor
static int get_light(void)
{
  return (10 * light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) / 7);
}

static int get_sol(void){
return (light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR));
}

//sht11-sensor
static int get_temp(void)
{
  return ((sht11_sensor.value(SHT11_SENSOR_TEMP) / 10) - 396) / 10;
}

static int get_hum(void)
{
  return (sht11_sensor.value(SHT11_SENSOR_HUMIDITY));
}

/*
static int get_bat1(void){
 return (sht11_sensor.value(SHT11_SENSOR_BATTERY_INDICATOR));
}
*/

//battery-sensor (TENSÃO DA BATERIA)
static int get_bat(void){
 	uint32_t battery;
	int fbatterycharge = 2500*2; //2500mV== 2,5V
	int fbatterychargeADC = 4096; //12 bit ADC, 2^12 = 4096
	
	battery=battery_sensor.value(0);
	
	battery *= fbatterycharge; 
	battery /= fbatterychargeADC;
 	
 	return (battery%1000);
}

/*--------------------------LED---------------------------------------------*/
static void led_green(void)
{
	leds_on(LEDS_GREEN);
	leds_off(LEDS_BLUE);
	leds_off(LEDS_RED);
	
	printf("FORTE");
}

static void led_blue(void)
{
	leds_off(LEDS_GREEN);
	leds_on(LEDS_BLUE);
	leds_off(LEDS_RED);
	
	printf("MEDIO");
}

static void led_red(void)
{
	leds_off(LEDS_GREEN);
	leds_off(LEDS_BLUE);
	leds_on(LEDS_RED);
	
	printf("FRACO");
}

/*--------------------------CASE--------------------------------------------*/
static void sensor_avaliator(int potencia){
    //CASES:
    if (potencia >= 100){
    	led_green();
    }
    if ((potencia > 30)&&(potencia<100)){
    	led_blue();
    }
    if (potencia < 30){
    	led_red();
    }
    printf("\n\n");  
}

PROCESS_THREAD(hello_world_process, ev, data)
{

  int light,solar,temp,humi;
  uint32_t batte;

  PROCESS_BEGIN();
	
  //--LIGA SENSORES
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(battery_sensor);
  SENSORS_ACTIVATE(sht11_sensor);
  
  while(1) {

    light = get_light();
    solar = get_sol();
    temp  = get_temp();
    humi  = get_hum();
    batte = get_bat();
   //int batte1 = get_bat1();
  
    printf("LUMINOSIDADE: %d \n", light);
    printf("TOTAL SOLAR: %d \n", solar);
    printf("TEMPERATURA: %d \n", temp);
    printf("UMIDADE: %d \n", humi);
    printf("BATERIA: %u.%02uV\n",(uint16_t)batte/1000,(uint16_t)batte%1000); 
    
    //--PARA  MODELOS DE SENSORES COMPATIVEL COM NUCLEO_IKS01A1 (ARDUINO)
    #ifdef X_NUCLEO_IKS01A1
    sensor_value = temperature_sensor.value(0);
    printf("Temperature:\t%d.%d C\n", sensor_value / 10, ABS_VALUE(sensor_value) % 10);

    sensor_value = humidity_sensor.value(0);
    printf("Humidity:\t%d.%d rH\n", sensor_value / 10, ABS_VALUE(sensor_value) % 10);

    sensor_value = pressure_sensor.value(0);
    printf("Pressure:\t%d.%d mbar\n", sensor_value / 10, ABS_VALUE(sensor_value) % 10);

    printf("Magneto:\t%d/%d/%d (X/Y/Z) mgauss\n", magneto_sensor.value(X_AXIS),
           magneto_sensor.value(Y_AXIS),
           magneto_sensor.value(Z_AXIS));

    printf("Acceleration:\t%d/%d/%d (X/Y/Z) mg\n", acceleration_sensor.value(X_AXIS),
           acceleration_sensor.value(Y_AXIS),
           acceleration_sensor.value(Z_AXIS));

    printf("Gyroscope:\t%d/%d/%d (X/Y/Z) mdps\n", gyroscope_sensor.value(X_AXIS),
           gyroscope_sensor.value(Y_AXIS),
           gyroscope_sensor.value(Z_AXIS));
    #endif /*X_NUCLEO_IKS01A1*/

    
    printf("STATUS:");
    sensor_avaliator(light);   
  }  
  
  //--DESLIGA SENSORES
  SENSORS_DEACTIVATE(light_sensor);
  SENSORS_DEACTIVATE(battery_sensor);
  SENSORS_DEACTIVATE(sht11_sensor);
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
