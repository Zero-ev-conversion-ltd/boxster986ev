
//High for anti-clockwise
int fuel_direction_pin = 6;
int fuel_fuel_step_pin = 7;

int full_sweep_steps = 392;

int desiredposition = 0;
int actualposition = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up!");
  pinMode(fuel_direction_pin, OUTPUT);
  pinMode(fuel_fuel_step_pin, OUTPUT);

  //Wind dial anticlockwise at startup to calibrate
  digitalWrite(fuel_direction_pin, HIGH);
    for (int i = 0; i <= full_sweep_steps; i++) {
      digitalWrite(fuel_fuel_step_pin, HIGH);
      delay(1);
      digitalWrite(fuel_fuel_step_pin, LOW);
    }

}

void loop() {

  if (Serial.available() > 0){
      int tempinput = Serial.parseInt();
      Serial.println(tempinput);
      if(tempinput > 0 && tempinput < 101){
        desiredposition = map(tempinput, 0, 100, full_sweep_steps, 0);
      }
  }

  //Need to move clockwise
  if(desiredposition > actualposition){
    digitalWrite(fuel_direction_pin, LOW);
    int num_steps_calculated = desiredposition - actualposition;
    //Serial.print(num_steps_calculated);
    //Serial.println(" steps moved clockwise");
    for (int i = 0; i <= num_steps_calculated; i++) {
      digitalWrite(fuel_fuel_step_pin, HIGH);
      delay(1);
      digitalWrite(fuel_fuel_step_pin, LOW);
    }
    actualposition = desiredposition; //Move has been completed!
  }

  //Need to move anti-clockwise
  if(desiredposition < actualposition){
    digitalWrite(fuel_direction_pin, HIGH);
    int num_steps_calculated = actualposition - desiredposition;
    //Serial.print(num_steps_calculated);
    //Serial.println(" steps moved anti-clockwise");
    for (int i = 0; i <= num_steps_calculated; i++) {
      digitalWrite(fuel_fuel_step_pin, HIGH);
      delay(1);
      digitalWrite(fuel_fuel_step_pin, LOW);
    }
    actualposition = desiredposition; //Move has been completed!
  }
}
