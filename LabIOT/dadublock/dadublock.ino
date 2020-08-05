/**********************************************************************************
 * The following software may be included in this software : orion_firmware.ino
 * from http://www.makeblock.cc/
 * This software contains the following license and notice below:
 * CC-BY-SA 3.0 (https://creativecommons.org/licenses/by-sa/3.0/)
 * Author : Ander, Mark Yan
 * Updated : Ander, Mark Yan
 * Date : 01/09/2016
 * Description : Firmware for Makeblock Electronic modules with Scratch.
 * Copyright (C) 2013 - 2016 Maker Works Technology Co., Ltd. All right reserved. 
 **********************************************************************************/
// 서보 라이브러리
//#include <Servo.h>

// 동작 상수
#define ALIVE 0
#define DIGITAL 1
#define ANALOG 2
#define PWM 3
#define SERVO_PIN 4
#define TONE 5
#define PULSEIN 6
#define ULTRASONIC 7
#define TIMER 8

// 상태 상수
#define GET 1
#define SET 2
#define RESET 3

// val Union
union{
  byte byteVal[4];
  float floatVal;
  long longVal;
}val;

// valShort Union
union{
  byte byteVal[2];
  short shortVal;
}valShort;

// 전역변수 선언 시작
//Servo servos[8];  

//서보모터 관련 전역변수
char g_ServoPin;
 
// This is the time since the last rising edge in units of 0.5us.
uint16_t volatile servoTime = 0;
 
// This is the pulse width we want in units of 0.5us.
uint16_t volatile servoHighTime = 3000;
 
// This is true if the servo pin is currently high.
boolean volatile servoHigh = false;

//울트라 소닉 포트
int trigPin = 13;
int echoPin = 12;

//포트별 상태
int analogs[4]={0,0,0,0};
int digitals[17]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int servo_pins[8]={0,0,0,0,0,0,0,0};

// 울트라소닉 최종 값
float lastUltrasonic = 0;

// 버퍼
char buffer[52];
unsigned char prevc=0;

byte index = 0;
byte dataLen;

double lastTime = 0.0;
double currentTime = 0.0;

uint8_t command_index = 0;

boolean isStart = false;
boolean isUltrasonic = false;
// 전역변수 선언 종료

void setup(){
  Serial.begin(115200);
  Serial1.begin(115200);
  initPorts();
  delay(200);
}

void initPorts() {
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
  for (int pinNumber = 2; pinNumber < 17; pinNumber++) {
    pinMode(pinNumber, OUTPUT);
    digitalWrite(pinNumber, LOW);
  }
}

void loop(){
  while (Serial.available()) {
    if (Serial.available() > 0) {
      char serialRead = Serial.read();
      setPinValue(serialRead&0xff);
    }
  } 
  while (Serial1.available()) {
    if (Serial1.available() > 0) {
      char serialRead = Serial1.read();
      setPinValue(serialRead&0xff);
    }
  } 
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);
  delay(15);
  sendPinValues();
  delay(10);
}

void setPinValue(unsigned char c) {
  if(c==0x55&&isStart==false){
    if(prevc==0xff){
      index=1;
      isStart = true;
    }    
  } else {    
    prevc = c;
    if(isStart) {
      if(index==2){
        dataLen = c; 
      } else if(index>2) {
        dataLen--;
      }
      
      writeBuffer(index,c);
    }
  }
    
  index++;
  
  if(index>51) {
    index=0; 
    isStart=false;
  }
    
  if(isStart&&dataLen==0&&index>3){  
    isStart = false;
    parseData(); 
    index=0;
  }
}

unsigned char readBuffer(int index){
  return buffer[index]; 
}

void parseData() {
  isStart = false;
  int idx = readBuffer(3);
  command_index = (uint8_t)idx;
  int action = readBuffer(4);
  int device = readBuffer(5);
  int port = readBuffer(6);
  switch(action){
    case GET:{
      if(device == ULTRASONIC){
        if(!isUltrasonic) {
          setUltrasonicMode(true);
          trigPin = readBuffer(6);
          echoPin = readBuffer(7);
          digitals[trigPin] = 1;
          digitals[echoPin] = 1;
          pinMode(trigPin, OUTPUT);
          pinMode(echoPin, INPUT);
          delay(50);
        } else {
          int trig = readBuffer(6);
          int echo = readBuffer(7);
          if(trig != trigPin || echo != echoPin) {
            trigPin = trig;
            echoPin = echo;
            digitals[trigPin] = 1;
            digitals[echoPin] = 1;
            pinMode(trigPin, OUTPUT);            
            pinMode(echoPin, INPUT);
            delay(50);
          }
        }
      } else if(port == trigPin || port == echoPin) {
        setUltrasonicMode(false);
        digitals[port] = 0;
      } else {
        digitals[port] = 0;
      }
    }
    break;
    case SET:{
      runModule(device);
      callOK();
    }
    break;
    case RESET:{
      callOK();
    }
    break;
  }
}

void runModule(int device) {
  //0xff 0x55 0x6 0x0 0x1 0xa 0x9 0x0 0x0 0xa
  int port = readBuffer(6);
  int pin = port;

  if(pin == trigPin || pin == echoPin) {
    setUltrasonicMode(false);
  }
  
  switch(device){
    case DIGITAL:{      
      setPortWritable(pin);
      int v = readBuffer(7);
      digitalWrite(pin,v);
    }
    break;
    case PWM:{
      setPortWritable(pin);
      int v = readBuffer(7);
      analogWrite(pin,v);
    }
    break;
    case TONE:{
      setPortWritable(pin);
      int hz = readShort(7);
      int ms = readShort(9);
      if(ms>0) {
        tone(pin, hz, ms);
      } else {
        noTone(pin);
      }
    }
    break;
    case SERVO_PIN:{
      setPortWritable(pin);
      int v = readBuffer(7);
      if(v>=0&&v<=180){
        //Servo sv = servos[searchServoPin(pin)]; //2017.02.21 삭제, 기존 서보라이브러리 timer1 겹침
        //sv.attach(pin); //2017.02.21 삭제, 기존 서보라이브러리 timer1 겹침
        //sv.write(v);    //2017.02.21 삭제, 기존 서보라이브러리 timer1 겹침
        g_ServoPin = pin; // 전역번수
        servoInit(); // 서보모터 초기화
        servoWrite(v); // 각도설정
      }
    }
    break;
    case TIMER:{
      lastTime = millis()/1000.0; 
    }
    break;
  }
}

void sendPinValues() {  
  int pinNumber = 0;
  for (pinNumber = 0; pinNumber < 17; pinNumber++) {
    if(digitals[pinNumber] == 0) {
      sendDigitalValue(pinNumber);
      callOK();
    }
  }
  for (pinNumber = 0; pinNumber < 4; pinNumber++) {
    if(analogs[pinNumber] == 0) {
      sendAnalogValue(pinNumber);
      callOK();
    }
  }
  
  if(isUltrasonic) {
    sendUltrasonic();  
    callOK();
  }
}

void setUltrasonicMode(boolean mode) {
  isUltrasonic = mode;
  if(!mode) {
    lastUltrasonic = 0;
  }
}

void sendUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  float value = pulseIn(echoPin, HIGH, 30000) / 29.0 / 2.0;

  if(value == 0) {
    value = lastUltrasonic;
  } else {
    lastUltrasonic = value;
  }
  writeHead();
  sendFloat(value);
  writeSerial(trigPin);
  writeSerial(echoPin);
  writeSerial(ULTRASONIC);
  writeEnd();
}

void sendDigitalValue(int pinNumber) {
  pinMode(pinNumber,INPUT);
  writeHead();
  sendFloat(digitalRead(pinNumber));  
  writeSerial(pinNumber);
  writeSerial(DIGITAL);
  writeEnd();
}

void sendAnalogValue(int pinNumber) {
  writeHead();
  sendFloat(analogRead(pinNumber));  
  writeSerial(pinNumber);
  writeSerial(ANALOG);
  writeEnd();
}

void writeBuffer(int index,unsigned char c){
  buffer[index]=c;
}

void writeHead(){
  writeSerial(0xff);
  writeSerial(0x55);
}

void writeEnd(){
  Serial.println();
  Serial1.println();
}

void writeSerial(unsigned char c){
  Serial.write(c);
  Serial1.write(c);
}


void sendString(String s){
  int l = s.length();
  writeSerial(4);
  writeSerial(l);
  for(int i=0;i<l;i++){
    writeSerial(s.charAt(i));
  }
}

void sendFloat(float value){ 
  writeSerial(2);
  val.floatVal = value;
  writeSerial(val.byteVal[0]);
  writeSerial(val.byteVal[1]);
  writeSerial(val.byteVal[2]);
  writeSerial(val.byteVal[3]);
}

void sendShort(double value){
  writeSerial(3);
  valShort.shortVal = value;
  writeSerial(valShort.byteVal[0]);
  writeSerial(valShort.byteVal[1]);
}

short readShort(int idx){
  valShort.byteVal[0] = readBuffer(idx);
  valShort.byteVal[1] = readBuffer(idx+1);
  return valShort.shortVal; 
}

float readFloat(int idx){
  val.byteVal[0] = readBuffer(idx);
  val.byteVal[1] = readBuffer(idx+1);
  val.byteVal[2] = readBuffer(idx+2);
  val.byteVal[3] = readBuffer(idx+3);
  return val.floatVal;
}

long readLong(int idx){
  val.byteVal[0] = readBuffer(idx);
  val.byteVal[1] = readBuffer(idx+1);
  val.byteVal[2] = readBuffer(idx+2);
  val.byteVal[3] = readBuffer(idx+3);
  return val.longVal;
}

int searchServoPin(int pin){
  for(int i=0;i<8;i++){
    if(servo_pins[i] == pin){
      return i;
    }
    if(servo_pins[i]==0){
      servo_pins[i] = pin;
      return i;
    }
  }
  return 0;
}

void setPortWritable(int pin) {
  if(digitals[pin] == 0) {
    digitals[pin] = 1;
    pinMode(pin, OUTPUT);
  } 
}

void callOK(){
  writeSerial(0xff);
  writeSerial(0x55);
  writeEnd();
}

void callDebug(char c){
  writeSerial(0xff);
  writeSerial(0x55);
  writeSerial(c);
  writeEnd();
}


// 2017.02.21 추가
// 서보모터 timer가 겹쳐 timer4로 추가하였음

ISR(TIMER4_COMPA_vect)
{
  // The time that passed since the last interrupt is OCR2A + 1
  // because the timer value will equal OCR2A before going to 0.
  servoTime += OCR4A + 1;
   
  static uint16_t highTimeCopy = 3000;
  static uint8_t interruptCount = 0;
   
  if(servoHigh)
  {
    if(++interruptCount == 2)
    {
      OCR4A = 255;
    }
 
    // The servo pin is currently high.
    // Check to see if is time for a falling edge.
    // Note: We could == instead of >=.
    if(servoTime >= highTimeCopy)
    {
      // The pin has been high enough, so do a falling edge.
      digitalWrite(g_ServoPin, LOW);
      servoHigh = false;
      interruptCount = 0;
    }
  } 
  else
  {
    // The servo pin is currently low.
     
    if(servoTime >= 40000)
    {
      // We've hit the end of the period (20 ms),
      // so do a rising edge.
      highTimeCopy = servoHighTime;
      digitalWrite(g_ServoPin, HIGH);
      servoHigh = true;
      servoTime = 0;
      interruptCount = 0;
      OCR4A = ((highTimeCopy % 256) + 256)/2 - 1;
    }
  }
}
 
void servoInit()
{
  digitalWrite(g_ServoPin, LOW);
  pinMode(g_ServoPin, OUTPUT);
   
  // Turn on CTC mode.  Timer 2 will count up to OCR2A, then
  // reset to 0 and cause an interrupt.
  TCCR4A = (1 << WGM41);
  // Set a 1:8 prescaler.  This gives us 0.5us resolution.
  TCCR4B = (1 << CS41) | (1 << CS40);
   
  // Put the timer in a good default state.
  TCNT4 = 0;
  OCR4A = 255;
   
  TIMSK4 |= (1 << OCIE4A);  // Enable timer compare interrupt.
  sei();   // Enable interrupts.
}
 
void servoSetPosition(uint16_t highTimeMicroseconds)
{
  TIMSK4 &= ~(1 << OCIE4A); // disable timer compare interrupt
  servoHighTime = highTimeMicroseconds * 2;
  TIMSK4 |= (1 << OCIE4A); // enable timer compare interrupt
}

void servoWrite(unsigned int deg)
{
    unsigned int Val;
    Val = map(deg,0,180,700,2500); //각도가 맞지 않는다면 700 ~ 2500 값을 조절
    servoSetPosition(Val);
}

