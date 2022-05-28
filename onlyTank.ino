#include <math.h>

// мин. сигнал, при котором мотор начинает вращение
#define MIN_DUTY 50

// пины блютуз
#define RX 0
#define TX 1

// номера кнопок
#define RED_BTN 1
#define GREEN_BTN 4
#define YELLOW_BTN 3
#define BLUE_BTN 2

// пины драйвера
#define MOT_RA 9
#define MOT_RB 10
#define MOT_LA 11
#define MOT_LB 12

// ===========================
#include <GyverMotor.h>
// (тип, пин, ШИМ пин, уровень)
GMotor motorR(DRIVER2WIRE, MOT_RA, MOT_RB, HIGH);
GMotor motorL(DRIVER2WIRE, MOT_LA, MOT_LB, HIGH);

// ===========================
#include <Servo.h>
// подготавливаем сервы
Servo servos[2];
int servosAngles[2];
int servosMinAngles[2];
int servosMaxAngles[2];

#define SERVO_X 5
#define SERVO_Y 6

// режимы работы
enum {
  MOVE_MODE,
  TARGET_MODE,
  FIRE_MODE,
  SET_SPEED_MODE,
} modeFlag;

// скорости танка
#define SPEED_HIGH 255;
#define SPEED_MEDIUM 200;
#define SPEED_LOW 150;

// скорости серв
#define SERVO_HIGH 3
#define SERVO_LOW 1

int currentMaxSpeed = SPEED_LOW;
int servoSpeed = SERVO_HIGH;

// перезарядка
#define RELOAD_TIME 1000

// время выстрела
#define FIRE_TIME 500

unsigned long lastTimeFired;
bool reloaded = true;

// пин лазера
#define LASER 7

// пины светодиодов
#define MOVE_LED 2
#define TARGET_LED 3
#define FIRE_LED 4

void setup() {
  motorR.setMode(AUTO);
  motorL.setMode(AUTO);

  // НАПРАВЛЕНИЕ ГУСЕНИЦ (зависит от подключения)
  motorR.setDirection(NORMAL);
  motorL.setDirection(NORMAL);

  // мин. сигнал вращения
  motorR.setMinDuty(MIN_DUTY);
  motorL.setMinDuty(MIN_DUTY);

  // плавность скорости моторов
  motorR.setSmoothSpeed(80);
  motorL.setSmoothSpeed(80);

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
        return;
      }

      if (modeFlag == FIRE_MODE)
      {
        // pow-pow
        fireLaser();
        return;
      }

      if (modeFlag == MOVE_MODE)
      {
        moveTank(angle, strength);
      }

      if (modeFlag == TARGET_MODE)
      {
        aimServos(angle, strength);
      }

      changeModeFlag(button);

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
      motorR.smoothTick(0);
      motorL.smoothTick(0);
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
  motorR.smoothTick(dutyR);
  motorL.smoothTick(dutyL);
}

void aimServos(int angle, int strength)
{
  if (strength < 50) return;
  
  int servoDirection = (angle <= 135 || angle > 315) ? 1 : -1;
  int servoIndex = ((angle > 45 && angle <= 135) || (angle > 225 && angle <= 315)) ? 1 : 0;

  int currentServoAngle = servosAngles[servoIndex];
  int newServoAngle = constrain(currentServoAngle + servoSpeed*servoDirection, servosMinAngles[servoIndex], servosMaxAngles[servoIndex]);
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
  servosAngles[1] = 180;
  
  servos[0].attach(SERVO_X);
  delay(100);
  servos[0].write(90);
  delay(50);
  servos[0].detach();

  servos[1].attach(SERVO_Y);
  delay(100);
  servos[1].write(180);
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
    prepareMoving();
    modeFlag = MOVE_MODE;
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
