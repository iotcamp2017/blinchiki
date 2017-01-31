/*
 * Листинг 5, основной.  
 * 1. Выбираем упражнение.
 * 2. Выбираем максимальное число повторов для него.
 * 3. Считаем число повторов для упражнения "изолированное сгибание руки".
 *    или для упражнения "вертикальная (боковая) тяга".
 * 4. Фиксируем верхнюю и нижнюю точку, выводим число повторов в порт.
 *    и на мобильное приложение через BLE.
 * 5. Фиксируем ошибку в течение 3-4. Ошибку выводим голосом и светодиодом.     
 * 
 */
#include <CurieBLE.h>

#include <CurieIMU.h>

#include <SoftwareSerial.h>
#include <CurieTimerOne.h>
#include <LiquidCrystal.h>

/*** BLE data  ***/

//Создаем перефирийное устройство: 
BLEPeripheral blePeripheral; 
//Создаем сервис и присваиваем ему UUID: 
BLEService dumbbellService("19B10010-E8F2-537E-4F6C-D104768A1214");

//Создаем характеристику типа char, в которую будет записываться
//1) Номер упражнения;
//2) Желаемое количество повторов (rpt_limit: смартфон -> гантеля) 
//BLERead - запрос значение характеристики из мобильного приложения; 
//BLENotify - непрерывный опрос характеристики телефоном; 
//BLEWrite - отправка сообщения от телефона плате.
BLECharCharacteristic rptLimitCharacteristic
    ("19B10011-E8F2-537E-4F6C-D104768A1215", BLERead | BLENotify | BLEWrite);
// Создание характеристики типа int, в которую будет записываться 
// текущее число повторов (rpt: гантеля -> смартфон) 
// и подтверждающий сигнал о выборе упражнения
BLEIntCharacteristic rptCharacteristic
    ("19B10012-E8F2-537E-4F6C-D104768A1216", BLERead | BLENotify);

void initBLE (void);

/*** BLE data end  ***/

//Используем таймер, чтобы моргать светодиодом. 
//Количество микросекунд в секунде:
const int oneSecInUsec = 1000000;  
//Порт светодиода: 
#define RED_PIN 13
#define GREEN_PIN 12
//Порт кнопки
#define BUT_ONE 2

#define holdtime 2000


//Инициирующее значение по ключевой оси. Его достижение означает 
//прохождение нижней точки в упражнении (0 - инициализиция):
//И инициирующее значение по осям контроля. Отклонение от этих значений на величину, 
//большую чем err_limit означает ошибку выполнения упражнения.
//(0.0 - просто инициализиция переменной):
float y_init = 0.0, x_init = 0.0, z_init = 0.0;


//Значение МАКСИМАЛЬНО допустимой разницы между текущим
//положением гантели и начальным y_init.
//Если значение стало меньше dy_down_limit,
//значит мы пришли в нижнюю точку.
float dy_down_limit = 0.2;

//Значение МИНИМАЛЬНО допустимой разницы между текущим
//положением гантели и начальным y_init.
//Если разница между текущим y и y_init стала 
//больше dy_up_limit_isolated_flexion (dy_up_limit_vertical_traction),
//мы пришли в верхнюю точку.
//Зачение dy_up_limit_isolated_flexion (dy_up_limit_vertical_traction) 
//калибруется в зависимости от abs(y_init).
float dy_up_limit_isolated_flexion  = 0;
float dy_up_limit_vertical_traction = 0;


//Переменная кнопки One
int ButOneChecker = 0;

void calibrate_isolated_flexion (bool voice);
void calibrate_vertical_traction (bool voice);

void ex_isolated_flexion (int);
void ex_vertical_traction (int);

bool test_error_isolated_flexion (void);
bool test_error_vertical_traction (void);

void LEDLight(char color) ; 

void ShowExScreen (int ex_number);

void ShowUpScreen(int rpt_limit);
int str_number;
int rpt_limit = 0;
#define ISOLATED_FLECTION 1
#define VERTICAL_TRACTION 2

//Переменные для сигнала на цифровых входах
#define OFF LOW
#define ON HIGH
unsigned long eventTime=0;
#define MAX_EX 2

// Инициализируем объект-экран, передаём использованные 
// для подключения контакты на Arduino в порядке:
// RS, E, DB4, DB5, DB6, DB7
LiquidCrystal lcd(4, 5, 10, 11, 12, 13);

void setup() {
    // устанавливаем размер (количество столбцов и строк) экрана
    lcd.begin(16, 2);
    lcd.print("Hello, friend");
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BUT_ONE, INPUT);
    //Мигаем светодиодом 4 раза в секунду с помощью прерывания таймера:
    //CurieTimerOne.start(oneSecInUsec / 4,  &timed_blink_isr);            
    //Последовательный порт:
    Serial.begin(9600);

    
    //Инициализируем гироскопы-акселерометры:
    CurieIMU.begin();
    CurieIMU.setGyroRate(25);
    CurieIMU.setAccelerometerRate(25);
    //Устанавливаем акселерометр на диапазон 2G:
    CurieIMU.setAccelerometerRange(2);
    //Устанавливаем гироскоп на диапазон 250 градусов:
    CurieIMU.setGyroRange(250);
    
    

    //Запускаем BLE:
    initBLE();
}



void loop() {

    //Выбираем упражнение (по умолчанию - изолированное сгибание):
    int ex_number = ISOLATED_FLECTION;

    //Вывод на экран "Выберите
    lcd.print("Vibrat'");
    lcd.setCursor(0, 1);
    //Вывод на экран "упражнение
    lcd.print("upragnenie");

    while(true){
      if(BUT_ONE == ON && !eventTime) {
        eventTime=millis(); // засекли когда произошло событие
        
      }
      if(BUT_ONE == OFF && eventTime) {
        eventTime = 0;
        break;
      }
      if(eventTime && (millis()-eventTime>holdtime)){ // проверям прошло ли 2000 миллесекунд с события
        if(ex_number == MAX_EX) ex_number = 1;
        else ex_number++;
        ShowExScreen(ex_number);
        eventTime = 0;
      }
      
    }
    
    lcd.setCursor(0,0);
    lcd.print("Podem");
     while(true){
      if(BUT_ONE == ON && !eventTime) {
        eventTime=millis(); // засекли когда произошло событие
        
      }
      if(BUT_ONE == OFF && eventTime) {
        eventTime = 0;
        break;
      }
      if(eventTime && (millis()-eventTime>holdtime)){ // проверям прошло ли 2000 миллесекунд с события
         rpt_limit++;
         ShowUpScreen(rpt_limit);
         eventTime = 0;
      }
      
    }
    

    

    

    //Дождались номера упражнения, если он корректный, ждем число повторов:    
    if ((ex_number == ISOLATED_FLECTION) or (ex_number == VERTICAL_TRACTION)) {
        
        

        
        
       

        if (ISOLATED_FLECTION == ex_number) {
            lcd.print("Calibrate... ");
         
            delay(1500);
            calibrate_isolated_flexion(true);  
            lcd.print("Calibrated!");
            CurieTimerOne.pause();
            
            //
            ex_isolated_flexion(rpt_limit);            
        
        } else if (VERTICAL_TRACTION == ex_number) {
            lcd.print("Calibrate... ");
           
            delay(1500);
            calibrate_vertical_traction(true);  
            lcd.print("Calibrated!");
            CurieTimerOne.pause();
           
            //
            ex_vertical_traction(rpt_limit);    
        }
        
    }  else {
        
        
        lcd.print("error");
        
    }

}


//УПРАЖНЕНИЯ:

void ex_isolated_flexion (int rpt_limit) {
    
   
    
    //Текущая координата по ключевой оси:
    float y;
    //Текущая (абсолютная) разница между данными показаниями по оси 
    //и их инициирующим значением.
    //Чем она больше, тем на больший угол мы отклонились по оси:
    float dy;
    
    
    
    for (int rpt = 1; rpt <= rpt_limit; rpt++) {    
       bool error = false;
        //Пока движемся наверх и dy меньше предела, 
        //проверяем текущую ошибку по вспомогательным осям:
        do {
            y  = CurieIMU.readAccelerometerScaled(Y_AXIS);
            dy = abs(y - y_init);

            //Проверяем на ошибку, уведомляем, если ошиблись:
            error = test_error_isolated_flexion();            
        } while ((dy < dy_up_limit_isolated_flexion) || error);
        
        
        

        //Движемся вниз:
        do {
            y  = CurieIMU.readAccelerometerScaled(Y_AXIS);
            dy = abs(y - y_init);

            //Снова проверяем на ошибку, уведомляем, если ошиблись:
            error = test_error_isolated_flexion(); 
        } while ((dy > dy_down_limit) || error);
        
        //Тут, в принципе, можно перекалибровать данные y_init
        //в нижней точке вызовом calibrate_isolated_flexion(false);
        
        
         
        //Количество сделанных повторов выводим на экран:
       
        lcd.print(rpt);

        
    }    
    
    lcd.print("Finished!");
    while(true){
      if(BUT_ONE==ON) break;
    }
}

void ex_vertical_traction (int rpt_limit) {  
    
    
    //Текущая координата по ключевой оси:
    float y;
    //Текущая (абсолютная) разница между данными показаниями по оси 
    //и их инициирующим значением.
    //Чем она больше, тем на больший угол мы отклонились по оси:
    float dy;
    
    
    
    for (int rpt = 1; rpt <= rpt_limit; rpt++) {    
       bool error = false;
        //Пока движемся наверх и dy меньше предела, 
        //проверяем текущую ошибку по вспомогательным осям:
        do {
            y  = CurieIMU.readAccelerometerScaled(Y_AXIS);
            dy = abs(y - y_init);

            //Проверяем на ошибку, уведомляем, если ошиблись:
            error = test_error_vertical_traction();            
        } while ((dy < dy_up_limit_vertical_traction) || error);
        
        
       

        //Движемся вниз:
        do {
            y  = CurieIMU.readAccelerometerScaled(Y_AXIS);
            dy = abs(y - y_init);

            //Снова проверяем на ошибку, уведомляем, если ошиблись:
            error = test_error_vertical_traction(); 
        } while ((dy > dy_down_limit) || error);
        
        //Тут, в принципе, можно перекалибровать данные y_init
        //в нижней точке вызовом calibrate_isolated_flexion(false);
        
        
            
        //Количество сделанных повторов выводим на экран:
        
        lcd.print(rpt);

        
    }    
    
   
    lcd.print("Finished!");
    while(true){
      if(BUT_ONE==ON) break;
    }
}


//ОШИБКИ:

/*
 *  Проверяем текущую ошибку по вспомогательным осям (здесь X).
 *  Если ошибка вышла за предел err_limit,
 *  выводим голосовое предупреждение.
 */
bool test_error_isolated_flexion (void) {
    //Максимально допустимое отклонение по вспомогательным осям. 
    //Выход за это отклонение означает ошибку выполнения упражнения:
    float err_limit = 0.45;
    
    static bool err_flag = false;    
    static unsigned long int err_time = millis();

    
    float x  = CurieIMU.readAccelerometerScaled(X_AXIS);       
    float dx = abs(x - x_init);

    const int SIZE = 20;
    static int i = -1;
    //Ошибки собираем в массив и считаем среднее, чтобы
    //защититься от ситуации пограничных флуктуаций
    //когда текущее dx находится вблизи предела err_limit
    static float error_list[SIZE];

    //Инициализация:
    if (-1 == i) {
        for (int j = 0; j < SIZE; j++) error_list[j] = 0; 
    }

    //ФИКСИРУЕМ НОВУЮ ОШИБКУ:
    i = (i+1)%SIZE;
    error_list[i] = dx;

    
    float error = 0.0;
    for (int j = 0; j < SIZE; j++) {
        error += error_list[j];
    }
    error /= SIZE;

    //Если функция ошибки вышла за предел и сделала это ТОЛЬКО ЧТО
    //(err_flag опущен; так защищаемся от "атаки ошибок"), 
    //то обновляем измерение и оповещаем спортсмена.
    if ((error > err_limit) && !err_flag && (millis() > err_time+1000)) {

        err_time = millis();
        err_flag = true;

        char buffer[255];
        sprintf(
            buffer, 
            "%.2f: err_flag ON with error = %.2f; %c", err_time/1000.0, 
            error, 0
        );

        LEDLight('R');
        delay(1500);
           
    }    

    //Если же мы в корректной области, то сбрасываем флаг ошибки,
    //чтобы иметь возможность реагировать на новые:   
    if ((error < err_limit) && err_flag) {
        err_flag = false;  
        LEDLight('G');              
        char buffer[255];
        sprintf(buffer, "%.2f: err_flag OFF %c", millis()/1000.0, 0);
        Serial.println(buffer);
    }

    

    return err_flag;
}


bool test_error_vertical_traction (void) {
    //Максимально допустимое отклонение по вспомогательным осям. 
    //Выход за это отклонение означает ошибку выполнения упражнения:
    float err_limit = 0.33;
    
    static bool err_flag = false;    
    
    static unsigned long int err_time = millis();

    
    float x  = CurieIMU.readAccelerometerScaled(X_AXIS);       
    float dx = abs(x - x_init);

    const int SIZE = 20;
    static int i = -1;
    //Ошибки собираем в массив и считаем среднее, чтобы
    //защититься от ситуации пограничных флуктуаций
    //когда текущее dx находится вблизи предела err_limit
    static float error_list[SIZE];

    //Инициализация:
    if (-1 == i) {
        for (int j = 0; j < SIZE; j++) error_list[j] = 0; 
    }

    //ФИКСИРУЕМ НОВУЮ ОШИБКУ:
    i = (i+1)%SIZE;
    error_list[i] = dx;

    
    float error = 0.0;
    for (int j = 0; j < SIZE; j++) {
        error += error_list[j];
    }
    error /= SIZE;

    //Если функция ошибки вышла за предел и сделала это ТОЛЬКО ЧТО
    //(err_flag опущен; так защищаемся от "атаки ошибок"), 
    //то обновляем измерение и оповещаем спортсмена.
    if ((error > err_limit) && !err_flag && (millis() > err_time+1000)) {

        err_time = millis();
        err_flag = true;

        char buffer[255];
        sprintf(
            buffer, 
            "%.2f: err_flag ON with error = %.2f; %c", 
            err_time/1000.0, 
            error, 
            0
        );
        LEDLight('R');
        delay(1500);   
    }    

    //Если же мы в корректной области, то сбрасываем флаг ошибки,
    //чтобы иметь возможность реагировать на новые:   
    if ((error < err_limit) && err_flag) {
        err_flag = false;  
        LEDLight('G');              
        char buffer[255];
        sprintf(buffer, "%.2f: err_flag OFF %c", millis()/1000.0, 0);
        Serial.println(buffer);
    }

    

    return err_flag;
}


//КАЛИБРОВКИ:

/*
 *  Калибруем начальные значения по ключевой оси.
 *  y_init - некоторое время только нули,
 *  в акселерометре идут какие-то переходные процессы.
 *  Дожидаемся, пока не придет что-то полезное 
 *  и выставляем dy_up_limit_isolated_flexion в зависимости от полученного значения.
 */
void calibrate_isolated_flexion (bool voice) {    
   
     
        //Ждем пару секунд, чтобы спортсмен успел встать 
        //в исходную позицию на калибровку:
        delay(3000);
    

    y_init = 0;
    
    //Калибруем, пока не получим значение, отличное от нуля в 
    //двух знаках после запятой (y_init*100):
    int y = y_init * 100;
    while (y == 0) {
        y_init = CurieIMU.readAccelerometerScaled(Y_AXIS);
        y = y_init * 100;
    }   
    //Наберем сотню значений за секунду и возьмем среднее:
    for (int i = 1; i < 100; i++){
        delay(10);
        y_init += CurieIMU.readAccelerometerScaled(Y_AXIS);
    }
    y_init /= 100;
    
    
    //Калибруем величину dy_up_limit_isolated_flexion в зависимости от y_init 
    //(апостериорные данные).
    //Наибольшим y_init соответствует максимальный лимит:
    if (0.8 < abs(y_init)) {
        dy_up_limit_isolated_flexion = 1.65;
    }
    
    //Постепенно понижаем лимит при уменьшении y_init:
    if (0.8 >= abs(y_init)) {
        dy_up_limit_isolated_flexion = 1.46;
    }
    
    if (0.5 >= abs(y_init)) {
        dy_up_limit_isolated_flexion = 1.1;
    }
    
    if (0.31 >= abs(y_init)) {
        dy_up_limit_isolated_flexion = 0.9;
    }
    
    //То ли это ошибка нормировки, то ли выделенная точка, 
    //но при 0.0 - предельное значение вот такое:
    if (0.0 == abs(y_init)) {
        dy_up_limit_isolated_flexion = 1.8;
    }


    //Теперь калибруем вспомогательные оси. 
    //Вопрос о калибровке предельной ошибки 
    //в зависимости от показаний - остается открытым:
    x_init = 0;
    int x = x_init * 100;
    while (x == 0) {
        x_init = CurieIMU.readAccelerometerScaled(X_AXIS);
        x = x_init * 100;
    }
    z_init = 0;
    int z = z_init * 100;
    while (z == 0) {
        z_init = CurieIMU.readAccelerometerScaled(Z_AXIS);
        z = z_init * 100;
    }    
}

void calibrate_vertical_traction (bool voice) {    
   
   
        //Ждем пару секунд, чтобы спортсмен успел встать 
        //в исходную позицию на калибровку:
        delay(3000);
    

    y_init = 0;
    
    //Калибруем, пока не получим значение, отличное от нуля в 
    //двух знаках после запятой (y_init*100):
    int y = y_init * 100;
    while (y == 0) {
        y_init = CurieIMU.readAccelerometerScaled(Y_AXIS);
        y = y_init * 100;
    }   
    //Наберем сотню значений за секунду и возьмем среднее:
    for (int i = 1; i < 100; i++){
        delay(10);
        y_init += CurieIMU.readAccelerometerScaled(Y_AXIS);
    }
    y_init /= 100;
    
    
    //Калибруем величину dy_up_limit_isolated_flexion в зависимости от y_init 
    //(апостериорные данные).
    //Наибольшим y_init соответствует максимальный лимит:
    if (0.8 < abs(y_init)) {
        dy_up_limit_vertical_traction = 1.65;
    }
    
    //Постепенно понижаем лимит при уменьшении y_init:
    if (0.8 >= abs(y_init)) {
        dy_up_limit_vertical_traction = 1.46;
    }
    
    if (0.5 >= abs(y_init)) {
        dy_up_limit_vertical_traction = 1.1;
    }
    
    if (0.31 >= abs(y_init)) {
        dy_up_limit_vertical_traction = 0.9;
    }
    
    //То ли это ошибка нормировки, то ли выделенная точка, 
    //но при 0.0 - предельное значение вот такое:
    if (0.0 == abs(y_init)) {
        dy_up_limit_vertical_traction = 1.8;
    }

    dy_up_limit_vertical_traction *=  0.85;

    //Теперь калибруем вспомогательные оси. 
    //Вопрос о калибровке предельной ошибки 
    //в зависимости от показаний - остается открытым:
    x_init = 0;
    int x = x_init * 100;
    while (x == 0) {
        x_init = CurieIMU.readAccelerometerScaled(X_AXIS);
        x = x_init * 100;
    }
    
    z_init = 0;
    int z = z_init * 100;
    while (z == 0) {
        z_init = CurieIMU.readAccelerometerScaled(Z_AXIS);
        z = z_init * 100;
    }    
}




//Вклю светодиодом с помощью прерываний таймера:
void LEDLight(char color)   
{                  
  if(color == 'R') {
      digitalWrite(RED_PIN, 1);
      digitalWrite(GREEN_PIN, 0);
  }
  if(color == 'G') {
     digitalWrite(RED_PIN, 0);
     digitalWrite(GREEN_PIN, 1);
  }
  if(color == 'D') {
     digitalWrite(RED_PIN, 1);
     digitalWrite(GREEN_PIN, 1);
  }
  if(color == 'O'){
     digitalWrite(RED_PIN, 0);
     digitalWrite(GREEN_PIN, 0);
  }
}



void initBLE (void)
{
  blePeripheral.setLocalName("Dumbbell");
  blePeripheral.setAdvertisedServiceUuid(dumbbellService.uuid());
  blePeripheral.addAttribute(dumbbellService);
  blePeripheral.addAttribute(rptLimitCharacteristic);
  blePeripheral.addAttribute(rptCharacteristic);

  //Ожидаем подключения:
  blePeripheral.begin();
}



void ShowExScreen(int ex_number){
  lcd.setCursor(0, 0);
  switch(ex_number) {
    case 1: 
      lcd.print("Izol sgib");
      break;
    case 2:
      lcd.print("vertik");
      break;
  }
}

void ShowUpScreen(int rpt_limit){
  lcd.setCursor(0, 0);
  String str11 = (String)rpt_limit;
  lcd.print(str11);
}

