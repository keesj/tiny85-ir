#include <TinyPinChange.h>
#include <SoftSerial.h>

/* Pin configuration */
#define IR_PIN                  3
#define LED_PIN                 1

#define RELAY_UP_PIN          0  
#define RELAY_DOWN_PIN        2  


/* Infrared dependent */
#define IR_PACKET_TRANSITION_SIZE 70
#define IR_PACKET_US 100000
#define NEC_BASE 562

#if defined(SERIAL_DEBUG)
#define SERIAL_BAUD_RATE        9600 
SoftSerial ser(DEBUG_TX_RX_PIN, DEBUG_TX_RX_PIN, true); 
#define DEBUG_TX_RX_PIN         2  

#endif /* SERIAL_DEBUG */

int irPin=IR_PIN;
int ledPin = LED_PIN;



unsigned long data[IR_PACKET_TRANSITION_SIZE];
unsigned int data_count;

#if defined(SERIAL_DEBUG)
/** 
 *  Dump the IR data to the serial port in a format that allows gnuplot
 *  to visualize it.
 *  
 *  The idea is based on the sample code found here
 *  http://playground.arduino.cc/Code/InfraredReceivers
 *  
 *  the gnuplot script used was
 *  set yrange[-0.2:1.2]
 *  plot "data.dat" using 1:2 with lines
 **/
void ser_dump(unsigned long *data, unsigned int c){
  //http://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol
  long t0 = data[0];
  int i;
  for (i =0 ; i <data_count-1; i++){
    //Time in , status
    ser.print(data[i] - t0,DEC);
    ser.print(" ");
    ser.println( (i % 2) , DEC);

    ser.print(data[i +1] - t0,DEC);
    ser.print(" ");
    ser.println( (i % 2) , DEC);
  }  
}
#endif /* SERIAL_DEBUG */

enum states {
 STATE_INITIAL,
 STATE_UP_PRESSED,
 STATE_UP,
 STATE_DOWN_PRESSED,
 STATE_DOWN,
 STATE_STOP_PRESSED,
 STATE_STOPPED,
} app_state;

int ff =0;
int led_speed =0;
void handle_state(){
  ff++;  
  /* Combined state machine where evens are also states....*/
  switch (app_state){
    case STATE_INITIAL:
      led_speed =2;
      digitalWrite(ledPin, (ff % led_speed == 1)?1:0); 
      digitalWrite(RELAY_UP_PIN,LOW);
      digitalWrite(RELAY_DOWN_PIN,LOW);
      break;
    case STATE_UP_PRESSED:
      ff=0;
      led_speed = 4;
      digitalWrite(RELAY_UP_PIN,HIGH);
      digitalWrite(RELAY_DOWN_PIN,LOW);      
      app_state = STATE_UP;
      break;
    case STATE_UP:
    case STATE_DOWN:
      digitalWrite(ledPin, (ff % led_speed == 1)?1:0); 
      break;      
    case STATE_DOWN_PRESSED:
      ff=0;
      led_speed = 8;
      digitalWrite(RELAY_UP_PIN,LOW);
      digitalWrite(RELAY_DOWN_PIN,HIGH);
      app_state = STATE_DOWN;
      break;
    case STATE_STOP_PRESSED:
      ff=0;
      led_speed = 0;
      digitalWrite(ledPin, LOW);
      digitalWrite(RELAY_UP_PIN,LOW);
      digitalWrite(RELAY_DOWN_PIN,LOW);
      app_state = STATE_STOPPED;
     break;
   case STATE_STOPPED:
      ff =0;
     break;

  }

}


void setup()
{
 pinMode(irPin,INPUT);
 pinMode(ledPin,OUTPUT);
 digitalWrite(ledPin,HIGH);
 
 pinMode(RELAY_UP_PIN,OUTPUT);
 pinMode(RELAY_DOWN_PIN,OUTPUT);
 digitalWrite(RELAY_UP_PIN,LOW);
 digitalWrite(RELAY_DOWN_PIN,LOW);
#if defined(SERIAL_DEBUG)
 ser.begin(SERIAL_BAUD_RATE); 
 ser.txMode();
#endif
 app_state = STATE_INITIAL;
}


void loop()
{ 
  long stime;/* Sampling start time */

  int val;

  //ser.println("Ready");
  long time = micros();
  while(digitalRead(irPin) ==1){
    if (time + 100000 < micros()){
      handle_state();
      time= micros();
    }
  }
  stime = micros();

  data_count = 0;
  val = digitalRead(irPin);
  //record pin changes for 100 millis()
  while( stime + IR_PACKET_US  > micros() ) {
    while(val == digitalRead(irPin) && stime + IR_PACKET_US > micros());
    data_count++;
    val = digitalRead(irPin);
    data[data_count] = micros() -stime;
    if (data_count >= IR_PACKET_TRANSITION_SIZE){
#if defined(SERIAL_DEBUG)      
      ser.println("Maxx IR");
#endif /* SERIAL_DEBUG */
      break;
    }
  }

  
#if defined(SERIAL_DEBUG)
  if (data_count != 68){
    //ser_dump(data,data_count);
    ser.print("items read: ");
    ser.println(data_count,DEC);
    ser.print("timer passed: ");
    ser.println(micros(), DEC);
  } 
#endif  
  long item = 0;
  
  if (data_count == 68){ //Assume NEC   
    long i;    
    long v;

    for (i =0 ; i < data_count-1; i++){
      if ( i % 2 ==1){
        v = ((data[i+1] - data[i])   );
        //ser.print(i % 1000,DEC);
//        ser.print(i ,DEC);
//        ser.print(" "); 
//        ser.print(v);
//        ser.println();
        item = (item << 1) | (v < 1000);
#if defined(SERIAL_DEBUG)        
        if (v < 1000) {
          ser.print("0");
        } else {
          ser.print("1");
        }
#endif /* SERIAL_DEBUG */
      }
    }
#if defined(SERIAL_DEBUG)
    ser.println();
#endif
    
    if ( (item & 0xffff) == 0x7F80){
      app_state = STATE_UP_PRESSED;
#if defined(SERIAL_DEBUG)
      ser.println("UP");
#endif      
    }
    if ( (item & 0xffff) == 0x5FA0){
      app_state = STATE_DOWN_PRESSED;
#if defined(SERIAL_DEBUG)
      ser.println("DOWN");
#endif      
    }
    if ( (item & 0xffff) == 0x6F90){
      app_state = STATE_STOP_PRESSED;
#if defined(SERIAL_DEBUG)
      ser.println("STOP");
#endif      
    }
    
#if defined(SERIAL_DEBUG)
    ser.println(item & 0xffff,HEX);
#endif /* SERIAL_DEBUG */
  }
  delay(100);
  
}
