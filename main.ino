#include <Arduino_FreeRTOS.h>
#include <semphr.h>

SemaphoreHandle_t interruptSemaphore;

TaskHandle_t taskComputeHandle;

void setup() {
  
  pinMode(2, INPUT_PULLUP);   // enable pin 2 as an input

  Serial.begin(9600);         // initialize serial at baud rate of 9600
  while (!Serial) {}

  xTaskCreate(Compute,
              "Compute something",
              128,
              NULL,
              1,
              &taskComputeHandle
  );

  xTaskCreate(ButtonPressed,
              "Button pressed task",
              256,
              NULL,
              2,
              NULL
  );

  // creates binary semaphore, aka a mutex
  interruptSemaphore = xSemaphoreCreateBinary();
  if (interruptSemaphore != NULL) {
    // 
    attachInterrupt(digitalPinToInterrupt(2), interruptHandler, CHANGE);
  }

  vTaskStartScheduler();
}

void loop() {
  // put your main code here, to run repeatedly:

}

void interruptHandler() {
  // BaseType_t higherPrioTaskExists = pdFalse;

  xSemaphoreGiveFromISR(interruptSemaphore, NULL);
  Serial.println("ISR");
  portYIELD_FROM_ISR();
}

void ButtonPressed(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    // wait until semaphore is given
    if (xSemaphoreTake(interruptSemaphore, portMAX_DELAY) == pdTRUE) {
      Serial.println("Button was pressed.");
      
      // .2 seconds of computation
      volatile uint32_t x = 0;
      for (uint32_t i = 0; i < 200UL * 1000UL; i++) {
      x += i;
    }
    }
  }
}

void Compute(void *pvParameters) {
  (void) pvParameters;
  TickType_t lastAwake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(5000);

  // initialize LED
  pinMode(LED_BUILTIN, OUTPUT);

  for (;;) 
  {
    TickType_t currentTick = xTaskGetTickCount();
    if (currentTick > lastAwake + period) {
      Serial.println("Periodic deadline miss");
    }


    unsigned long start = micros();
    Serial.print("Task ");
    Serial.print(pcTaskGetName(taskComputeHandle)); // print this task's name
    Serial.println(" is beginning computation");
    
    digitalWrite(LED_BUILTIN, HIGH);
    // do computation for some time
    volatile uint32_t x = 0;
    // 5000 * 1000 => 5sec of compute
    for (uint32_t i = 0; i < 2500UL * 1000UL; i++) {
      x += i;
      if ((i & 0xFF) == 0) taskYIELD();
    }

    // turns the LED off
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("Computation took ");
    unsigned long end = micros();
    Serial.print(end - start);
    Serial.println(" us");

    // schedule to "arrive" again in 5 seconds
    vTaskDelayUntil(&lastAwake, period);
  }

}