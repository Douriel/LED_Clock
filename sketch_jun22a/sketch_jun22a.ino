

#include <Config.h>


// En la siguiente sección incluimos las librerías

#include <DS1302.h>           // Esta librería importada se utiliza para controlar el reloj, 1302
#include <LedControlMS.h>     // Librería con la que controlamos las cuatro matrices LED
#include <LowPower.h>         // Esta librería nos permitirá apagar la pantalla si el PIR no detecta movimiento
#include <stdio.h>            // Este header nos permite trabajar con archivos de entrada y de salida
#include <Wire.h>             // Esta librería nos permite comunicarnos con otros dispositivos
#include <AM2320.h>           // Esta librería se utiliza para poder utilizar el dispositivo que mide la temperatura y la humedad
#include <EasyBuzzer.h>       // Librería de control del zumbador.
#include <RTClib.h>
AM2320 th;                    // Pone las conexiones pin de la siguiente forma: 1-5V, SDA-A4, 3-SCL, 4-A5




// Define pins
#define TEMPPIN A6            // Este pin corresponderá al sensor de temperatura
#define PINPIR 2              // Este pin es el que corresponde al modulo PIR que nos permitirá volver a lanzar el sistema si este detecta movimiento, lo hemos localizado en el PIN 2
#define PCLKPIN 10            // Pin de carga del la pantalla, num 10
#define PINPCLK 11            // Pin CLK, num 11
#define PINPINPUT 12          // Pin de envio de datos, num 12
#define PINCERELOJ 5          // Pin con el que habilitamos el reloj, num 5
#define PININOUTRELOJ 6       // Pin de transmisión input/output con el reloj, num 6
#define PINCLKRELOJ 7         // Pin de reloj en serie, num 7
#define PINBUZZER  3         



// Creamos el controlador de las 4 pantallas leds, para ello pasamos los pins que hemos configurado previamente
LedControl controlLEDs = LedControl(PINPINPUT, PINPCLK, PCLKPIN, 4);

// Creamos un objeto tipo reloj,para crearlo necesitamos los tres pins que hemos configurado antes antes
DS1302 rtc(PINCERELOJ, PININOUTRELOJ, PINCLKRELOJ);

// Creación de las variables del programa.
int brilloLed = 0;        // Los valores posibles son entre 0 y 15
int tiempoEntreBucle = 2;        // Conjuntos de 2 segundos que usaremos cuando mostrmos la hora, haremos que parpadee el divisor : 
int tiempoTexto = 2000;    // Tiempo en que mostramos el texto
int delayAnimaciones = 50;          // Tiempo de retraso entre las transacciones, como la cortina.
int tiempoPintadoColumnas = 18;      // Tiempo de retarado entre el printado de cada una de las columnas de los led, encendido de izquierda a derecha.
int tiempoEntreLineas = 1400;      // Tiempo de exposicón entre los textos que tienen varias lineas, como por ejemplo el días  de la semana
int numeroIteraciones = 1;               //  Número de veces que se repite el bucle principal sin apagarse la pantalla, el PIR volverá a reiniciar el contador a 0.
int limpiarPantalla = false;   

int destMatrix;  // Esta variable se utilizarán para contar el número de pantalla en el que estamos del los leds
int destCol; // Esta variable se usa para saber en que columna estamos dentro de una de las pantallas de los leds
int animType = 0; // Esta variable se usa para seleccionar cual de las dos animaciones que queremos emplear.
int sleeping = true; // Inicializamos a true esta variable y cuando termite el loop, si el PIR no ha detectado nada se apagará la pantalla.

volatile int sleepLoopCounter = 0;

// Algunos simbolos como el asterístico no se pueden poder tan facilmente, más abajo explicamos como
const char * DS1302ErrorMessage = "*RTC ERROR"; //error en el reloj
const char * AM2320ErrorMessage = "*TEMPERROR"; //error en el sensor de humedad y temperatura

String datetimeCompare = "14:20";
char datetimeBuffer[5] = "";

// Configuramos todo y miramos si funciona, en caso de fallar sacará los errores de arriba por pantalla
void setup() {
  setTime(); // Configuramos la fecha con esta función, no se puede hacer manualmente, podemos más adelante añadir un módulo de red para que se conecte al wifi y consiga la hora automáticamente
  int devices = controlLEDs.getDeviceCount();
  for(int address = 0; address < devices; address++) {
    controlLEDs.shutdown(address, false); // Encendemos
    controlLEDs.setIntensity(address, brilloLed);  // Seleccionamos la intensidad de brillo
    controlLEDs.clearDisplay(address); //limpiamos
  }
  EasyBuzzer.setPin(PINBUZZER);
  EasyBuzzer.beep(
    2000          // Frecuencia en herzios
   );
  EasyBuzzer.stopBeep();
  Serial.begin(9600);
  pinMode (PINPIR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PINPIR), PIRmotionDetected, RISING); 
  pruebaDispositivos();     
}


// Main 
void loop() {
  //EasyBuzzer.stopBeep();
  if (sleepLoopCounter == 0 && sleeping == true) {
    displayAnim(); //Función de animación, tenemos dos
    sleeping = false;
  }

  if (sleepLoopCounter < numeroIteraciones) {
    sleepLoopCounter++; //Según lo tenemos configurado ahora mismo solo datá una vuelta.
    displayTime(); //Función dar la hora
    Time taux = rtc.time();
    sprintf(datetimeBuffer, "%02d:%02d",taux.hr, taux.min);
     Serial.println(datetimeBuffer);
     Serial.println(datetimeCompare.c_str());
     Serial.println(strcmp(datetimeBuffer, datetimeCompare.c_str()));

    if(strcmp(datetimeBuffer, datetimeCompare.c_str()) == 0)
    {
      //analogWrite(PINBUZZER, 10);
      EasyBuzzer.update();
    }else{
      EasyBuzzer.stopBeep();
    }
    displayDate(); //Función dar fecha
    displayTemp(); //Función dar temperatura
    displayAnim(); //Función segúnda animación
  }
  else if (sleepLoopCounter == numeroIteraciones) {
    //Si ya hemos igualado el numero de Iteraciones entramos en reposo
    displayAnimClear();  
    sleeping = true;
    sleepLoopCounter++;
    
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  // Dejamos que entre en modo de reposo, pero en cuanto el PIR detecte algo continuamos con la ejecución normal

  }
  Serial.println(digitalRead(PINPIR));
}

// Functions
void setTime() {
  // Inicializamos el chip del reloj a la fecha que queramos, sino, por defecto serán las dos de la mañana del 1 de enero del 2000
  rtc.writeProtect(false);
  rtc.halt(false);

  // Creamos un objeto tipo tiempo para establecer una fecha. Hay que introducir los siguientes parámetros.
  // Año, més, día, hr, min, seg, día de la semana
  Time t(2021, 6, 22, 14, 19, 50, Time::kSunday);
  rtc.time(t); // Seteamos el chip con el nuevo objeto 
}

void pruebaDispositivos() {
  Time t = rtc.time();
  if (dayAsString(t.day) == DS1302ErrorMessage ) {
      displayStringCondensed(0, DS1302ErrorMessage );  
  }

  switch(th.Read())
    case 1 or 2:
      displayStringCondensed(0, AM2320ErrorMessage);  
}

void PIRmotionDetected() {
  if (digitalRead(PINPIR) == HIGH) {
    Serial.println("Motion detected");
  
    sleepLoopCounter = 0;
  
    controlLEDs.setLed(3,0,0,true);
    controlLEDs.setLed(3,0,0,false);
  }
}

void displayDate() {
  Time t = rtc.time();

  String day = dayAsString(t.day);
  char dayString[20];
  snprintf(dayString, sizeof(dayString), "%s ", day.c_str());
  displayStringCondensed(1, dayString);  
  delayAndClearLed();

  String month = monthAsString(t.mon);
  char dateString[8];
  snprintf(dateString, sizeof(dateString), "%02d||%s", t.date, month.c_str());
  displayStringCondensed(1, dateString);  
  delayAndClearLed();

  char yearString[5];
  snprintf(yearString, sizeof(yearString), "%04d", t.yr);
  displayStringCondensed(4, yearString);  
  delayAndClearLed();
}

void displayTime() {
  for (int i = 0; i <= tiempoEntreBucle; i++) {    
    Time t = rtc.time();
    char timeString[9];
    snprintf(timeString, sizeof(timeString), "%02d||||%02d", t.hr, t.min);
    displayStringCondensed(2, timeString);  

    controlLEDs.setLed(1,5,0,false);
    controlLEDs.setLed(1,2,0,false);
    delay(1000);
    controlLEDs.setLed(1,5,0,true);
    controlLEDs.setLed(1,2,0,true);
    delay(1000);
  }  

  if (limpiarPantalla) 
    controlLEDs.clearAll();
}

void displayTemp () {
  int intValue, fracValue;
  
  if(th.Read() == 0) {
    char tempString[10];
    intValue = th.t;
    fracValue = th.t * 10 - intValue * 10;
    
    snprintf(tempString, sizeof(tempString), "%02d.|%01d||C", intValue, fracValue);
    displayStringCondensed(2, tempString);
    delayAndClearLed();
  }
  else
    displayStringCondensed(0, AM2320ErrorMessage);  

  if(th.Read() == 0) {
    char humString[10];    
    intValue = th.h;

    snprintf(humString, sizeof(humString), "%02d||/ ", intValue);
    displayStringCondensed(7, humString);
    delayAndClearLed();
  }
  else
    displayStringCondensed(0, AM2320ErrorMessage);  
}

void displayAnim() {
  switch (animType) {
    case 0: 
      for (int row = 0; row <= 7; row++) {
        for (int matrix = 0; matrix <= 3; matrix++) {
          controlLEDs.setRow(matrix, 7 - row, B11111111);
        }
        delay(delayAnimaciones);
      }
      break;
      
    case 1:
      for (int row = 0; row <= 7; row++) {
        for (int matrix = 0; matrix <= 3; matrix++) {
          controlLEDs.setRow(matrix, row, B11111111);
        }
        delay(delayAnimaciones);
      }
      break;
  }

  animType++;
  if (animType > 1)   
    animType = 0;
  
  if (limpiarPantalla) 
    controlLEDs.clearAll();
}

void displayAnimClear() {
  for (int row = 0; row <= 7; row++) {
    for (int matrix = 0; matrix <= 3; matrix++) {
      controlLEDs.setRow(matrix, 7 - row, B00000000);
    }
    delay(delayAnimaciones);
  }
}

void displayStringCondensed (int startCol, char * displayString) {
  int i;
  char c;
  destMatrix = 0;
  destCol = 7 - startCol;

  for (i = 7; i > (7 - startCol); i--)               // Borramos las columnas
    controlLEDs.setColumn(destMatrix, i, B00000000);

  while (displayString[0] != 0) {
    c = displayString[0];

    if (destMatrix == 4 || displayString[0] == '~') {       // Hacemos una nueva linea cuando nos quedamos sin espacio o mandamos el símbolo ~
      clearLastColumns();
      destMatrix = 0;
      destCol = 7 - startCol;

      if (displayString[0] == '~') {
        displayString++;
        c = displayString[0];
      }
      delay(tiempoEntreLineas);
    }

    // Encontramos en la librería ejemplos que nos permitren escribir otros símbolos o simplemente ayudas para manipularlo y dejar un texto de salida más estético

    if (displayString[0] == '|') {                          // Dejamos una columna antes de escribir
      controlLEDs.setColumn(destMatrix, destCol, B00000000);
      increaseCocontrolLEDsounter();
    }
    else if (displayString[0] == '.') {                     
      controlLEDs.setColumn(destMatrix, destCol, B10000000);
      increaseCocontrolLEDsounter();
    }
    else if (displayString[0] == '!') {                     
      controlLEDs.setColumn(destMatrix, destCol, B10111111);
      increaseCocontrolLEDsounter();
    }
    else if (displayString[0] == '/') {                     // Nos permite escribir un signo, sino no nos deja
      controlLEDs.setColumn(destMatrix, destCol, B11000011);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B00110011);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B11001100);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B11000011);
      increaseCocontrolLEDsounter();
    }
    else if (displayString[0] == '*') {                     
      controlLEDs.setColumn(destMatrix, destCol, B00100010);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B00010100);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B01111111);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B00010100);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B00100010);
      increaseCocontrolLEDsounter();
      controlLEDs.setColumn(destMatrix, destCol, B00000000);
      increaseCocontrolLEDsounter();
    }
    else {
      if (destMatrix == 3 && destCol < 4) {                 // Aqui entraremos cuando no haya espacio sufiente para escribir algo en los leds y por tanto haremos una nueva linea
        clearLastColumns();
        destMatrix = 0;
        destCol = 7 - startCol;
        displayString--;
        delay(tiempoEntreLineas);
      }
      else {
        int pos = controlLEDs.getCharArrayPosition(c);
    
        for (i = 0; i < 6; i++) {
          controlLEDs.setColumn(destMatrix, destCol, alphabetBitmap[pos][i]);          
          increaseCocontrolLEDsounter();
        }
      }
    }
    
    displayString++;
  } 

  clearLastColumns();
}

void increaseCocontrolLEDsounter() {
  destCol--;
  
  if (destCol < 0) {
    destMatrix++;
    destCol = 7;
  }
  
  delay(tiempoPintadoColumnas);
}

void clearLastColumns() {
  for (int i = destMatrix * 8 + destCol; i < 4 * 8; i++) {
    controlLEDs.setColumn(destMatrix, destCol, B00000000);
    increaseCocontrolLEDsounter();
  }
}

void delayAndClearLed() {
  delay(tiempoTexto);

  if (limpiarPantalla) 
    controlLEDs.clearAll();
}

String dayAsString(const Time::Day day) {
  switch (day) { 
    case Time::kMonday:    return "Lunes";
    case Time::kTuesday:   return "|||Mar- |||tes";
    case Time::kWednesday: return "||Mier-|coles";
    case Time::kThursday:  return "Jue|||-ves";
    case Time::kFriday:    return "|||Vie-||| rnes";
    case Time::kSaturday:  return " Sab-||| ado";
    case Time::kSunday:    return " |||Do- min-|| ||go";
  }
  return DS1302ErrorMessage ;
}

String monthAsString(int month) {
  switch (month) {
    case 1:  return "Ene";
    case 2:  return "Feb";
    case 3:  return "Mzo";
    case 4:  return "Abr";
    case 5:  return "May";
    case 6:  return "Jun";
    case 7:  return "Jul";
    case 8:  return "Aug";
    case 9:  return "Sep";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
  }
  return DS1302ErrorMessage ;
}
