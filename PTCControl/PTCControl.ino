#include <OneWire.h>
#include <DallasTemperature.h>
 
#define ONE_WIRE_BUS 2
#define PTC_OUTPUT   12

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

double currentTemperature;
double desiredTemperature = 19.80;
double tempHysteris = 0.1;

 
void setup(void) {
  Serial.begin(115200);
  Serial.println("Boxster Simple PTC Control");
  pinMode(PTC_OUTPUT, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(PTC_OUTPUT, LOW);
  sensors.begin();
  delay(1000);
}
 
 
void loop(void) {
  sensors.requestTemperatures();
  currentTemperature = sensors.getTempCByIndex(0);

  Serial.print("Temp: ");
  Serial.println(currentTemperature);

  if(desiredTemperature > currentTemperature){
    turnonPTC();
  }
  if(currentTemperature > (desiredTemperature + tempHysteris)){
    turnoffPTC();
  }
  
  delay(500);
}

void turnonPTC(){
  digitalWrite(PTC_OUTPUT, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("PTC: ON");
}

void turnoffPTC(){
  digitalWrite(PTC_OUTPUT, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("PTC: OFF");
}
