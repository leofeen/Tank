#include <math.h>

// Обратить движение моторчиков
#define R_MOTOR_REVERSE false
#define L_MOTOR_REVERSE false

// Обратить движение серв
#define SERVO_Y_REVERSE false
#define SERVO_X_REVERSE true

// пины блютуз
#define RX 0
#define TX 1

// номера кнопок
#define RED_BTN 1
#define GREEN_BTN 4
#define YELLOW_BTN 3
#define BLUE_BTN 2

// пины сервомоторочиков
#define SERVO_X 9
#define SERVO_Y 10

// скорости танка
#define SPEED_HIGH 255;
#define SPEED_MEDIUM 175;
#define SPEED_LOW 100;

// скорости серв
#define SERVO_HIGH 3
#define SERVO_LOW 1

// перезарядка
#define RELOAD_TIME 1000

// время выстрела
#define FIRE_TIME 2000

// пин лазера
#define LASER 7

// пины светодиодов
#define MOVE_LED A3
#define TARGET_LED A0
#define FIRE_LED A1

#include <AFMotor.h>
AF_DCMotor motorR(1);
AF_DCMotor motorL(2);

#include <Servo.h>

// подготавливаем сервы
Servo servos[2];
int servosAngles[2];
int servosMinAngles[2];
int servosMaxAngles[2];

// режимы работы
enum {
  MOVE_MODE,
  TARGET_MODE,
  FIRE_MODE,
  SET_SPEED_MODE,
} modeFlag;

int currentMaxSpeed = SPEED_LOW;
int servoSpeed = SERVO_HIGH;

unsigned long lastTimeFired = 0;
bool reloaded = true;

void setup() {
  // configure servos
  servosMinAngles[0] = 0;
  servosMinAngles[1] = 40;
  servosMaxAngles[0] = 180;
  servosMaxAngles[1] = servosMinAngles[1] + 90;
  prepareMoving();

  // enable bluetooth
  Serial.begin(9600);

  // configure laser pin
  pinMode(LASER, OUTPUT);

  //configure LED pins
  pinMode(MOVE_LED, OUTPUT);
  pinMode(TARGET_LED, OUTPUT);
  pinMode(FIRE_LED, OUTPUT);
}

void loop() {
  if (!reloaded) checkReloading();
  if (modeFlag == FIRE_MODE && reloaded)
  {
    fireLaser();
  }
  
  // read from bluetooth
  if (Serial.available() > 0) {
    String value = Serial.readStringUntil('#');
  
    if (value.length() == 7)
    {
      if (!reloaded)
      {
        value = "";
        Serial.flush();
        checkReloading();
      }
      
      String angleStr = value.substring(0, 3);
      String strengthStr = value.substring(3, 6);
      String buttonStr = value.substring(6, 7);

      int angle = angleStr.toInt();
      int strength = strengthStr.toInt();
      int button = buttonStr.toInt();

      if (modeFlag == SET_SPEED_MODE)
      {
        changeSpeed(button);
      }
      else if (modeFlag == MOVE_MODE)
      {
        moveTank(angle, strength);
        changeModeFlag(button);
      }
      else if (modeFlag == TARGET_MODE)
      {
        aimServos(angle, strength);
        changeModeFlag(button);
      }

      Serial.flush();
      value = "";
    }
  }
}

void changeSpeed(int button)
{
  if (button == RED_BTN)
  {
    currentMaxSpeed = SPEED_LOW;
  }
  else if (button == GREEN_BTN)
  {
    currentMaxSpeed = SPEED_MEDIUM;
  }
  else if (button == YELLOW_BTN)
  {
    currentMaxSpeed = SPEED_HIGH;
  }
  else
  {
    return;
  }

  modeFlag = MOVE_MODE;
}

void changeModeFlag(int button)
{
  if (button == GREEN_BTN)
  {
    prepareMoving();
    modeFlag = MOVE_MODE;
  }
  else if (button == YELLOW_BTN)
  {
    prepareTargeting();
    modeFlag = TARGET_MODE;
  }
  else if (button == RED_BTN)
  {
    if (modeFlag == TARGET_MODE)
    {
       modeFlag = FIRE_MODE;
       return;
    }
  }
  else if (button == BLUE_BTN)
  {
    if (modeFlag == TARGET_MODE)
    {
      changeServoSpeed();
    }
    else
    {
      modeFlag = SET_SPEED_MODE;
      motorR.setSpeed(0);
      motorL.setSpeed(0);
    }
  }
}

void moveTank(int angle, int strength)
{
  float inputX = cos(radians(angle)) * strength * 255 / 100;
  float inputY = sin(radians(angle)) * strength * 255 / 100;

  // танковая схема
  int dutyR = inputY + inputX;
  int dutyL = inputY - inputX;
  dutyR = constrain(dutyR, -currentMaxSpeed, currentMaxSpeed);
  dutyL = constrain(dutyL, -currentMaxSpeed, currentMaxSpeed);

  // задаём целевую скорость
  motorR.setSpeed(abs(dutyR));
  motorL.setSpeed(abs(dutyL));

  dutyR = R_MOTOR_REVERSE ? -dutyR : dutyR;
  dutyL = L_MOTOR_REVERSE ? -dutyL : dutyL;

  motorR.run((dutyR > 0) ? FORWARD : BACKWARD);
  motorL.run((dutyL > 0) ? FORWARD : BACKWARD);
}

void aimServos(int angle, int strength)
{
  if (strength < 50) return;
  
  int servoDirection = (angle <= 135 || angle > 315) ? 1 : -1;
  int servoIndex = ((angle > 45 && angle <= 135) || (angle > 225 && angle <= 315)) ? 1 : 0;

  int servoSpeedToUse = servoSpeed;

  if ((servoIndex == 0 && SERVO_X_REVERSE) || (servoIndex == 1 && SERVO_Y_REVERSE))
  {
    servoSpeedToUse *= -1;
  }

  int currentServoAngle = servosAngles[servoIndex];
  int newServoAngle = constrain(currentServoAngle + servoSpeedToUse*servoDirection, servosMinAngles[servoIndex], servosMaxAngles[servoIndex]);
  if (newServoAngle == currentServoAngle) return;
  
  servosAngles[servoIndex] = newServoAngle;

  servos[servoIndex].attach(servoIndex + SERVO_X);

  delay(50);
  servos[servoIndex].write(newServoAngle);
  
  delay(20);
  servos[servoIndex].detach();
}

void changeServoSpeed()
{
  servoSpeed = (servoSpeed == SERVO_HIGH) ? SERVO_LOW : SERVO_HIGH;
}

void prepareMoving()
{ 
  servosAngles[0] = 90;
  servosAngles[1] = 155;
  
  servos[0].attach(SERVO_X);
  delay(100);
  servos[0].write(90);
  delay(50);
  servos[0].detach();

  servos[1].attach(SERVO_Y);
  delay(100);
  servos[1].write(155);
  delay(50);
  servos[1].detach();

  digitalWrite(MOVE_LED, HIGH);
  digitalWrite(TARGET_LED, LOW);
  digitalWrite(FIRE_LED, LOW);
}

void prepareTargeting()
{
  servos[1].attach(SERVO_Y);

  delay(100);

  servos[1].write(servosMaxAngles[1]);
  servosAngles[1] = servosMaxAngles[1];

  delay(50);

  servos[1].detach();

  digitalWrite(MOVE_LED, LOW);
  digitalWrite(TARGET_LED, HIGH);
  digitalWrite(FIRE_LED, LOW);
}

void checkReloading()
{
  if (millis() - lastTimeFired > RELOAD_TIME + FIRE_TIME)
  {
    reloaded = true;
    modeFlag = TARGET_MODE;

    digitalWrite(MOVE_LED, LOW);
    digitalWrite(TARGET_LED, HIGH);
    digitalWrite(FIRE_LED, LOW);
  }
}

void fireLaser()
{
  reloaded = false;
  lastTimeFired = millis();
  digitalWrite(MOVE_LED, LOW);
  digitalWrite(TARGET_LED, LOW);
  digitalWrite(FIRE_LED, HIGH);
  
  digitalWrite(LASER, HIGH);
  delay(FIRE_TIME);
  digitalWrite(LASER, LOW);
}
