#include <Arduino.h>
#include "EntprellterTaster.h"

// ========== KONFIGURATION ==========
const int BUTTON_PIN = 32;  // HIER DEN PIN EINTRAGEN!

// ========== GLOBALE VARIABLEN ==========
EntprellterTaster taster(BUTTON_PIN);

// Task-Handles
TaskHandle_t taskHauptHandle = NULL;
TaskHandle_t taskAHandle = NULL;
TaskHandle_t taskBHandle = NULL;
TaskHandle_t taskCHandle = NULL;

// Aktueller aktiver Task (0=A, 1=B, 2=C)
volatile int aktiverTask = 0;

// ========== TASK-FUNKTIONEN ==========

// Task A: Gibt "a" im Sekundentakt aus
void taskA(void *parameter) {
  delay(5);  // Wichtig: Initiales Delay damit Task-Handle gesetzt wird
  
  Serial.println("Task A gestartet");
  
  while (true) {
    Serial.println("a");
    delay(1000);  // 1 Sekunde warten
  }
}

// Task B: Gibt "b" im Sekundentakt aus
void taskB(void *parameter) {
  delay(5);  // Wichtig: Initiales Delay damit Task-Handle gesetzt wird
  
  Serial.println("Task B gestartet");
  
  while (true) {
    Serial.println("b");
    delay(1000);  // 1 Sekunde warten
  }
}

// Task C: Gibt "c" im Sekundentakt aus
void taskC(void *parameter) {
  delay(5);  // Wichtig: Initiales Delay damit Task-Handle gesetzt wird
  
  Serial.println("Task C gestartet");
  
  while (true) {
    Serial.println("c");
    delay(1000);  // 1 Sekunde warten
  }
}

// Haupttask: Prüft auf langen Tastendruck und schaltet Tasks um
void taskHaupt(void *parameter) {
  delay(5);  // Wichtig: Initiales Delay damit Task-Handle gesetzt wird
  
  Serial.println("Haupttask gestartet - Warte auf langen Tastendruck...");
  
  while (true) {
    // Taster aktualisieren
    taster.aktualisiere();
    
    // Prüfen ob langer Tastendruck erkannt wurde
    if (taster.wurdeLangeGedrueckt()) {
      Serial.println("\n=== Langer Tastendruck erkannt! ===");
      
      // Aktuellen Task suspendieren
      if (aktiverTask == 0 && taskAHandle != NULL) {
        vTaskSuspend(taskAHandle);
        Serial.println("Task A suspendiert");
      } else if (aktiverTask == 1 && taskBHandle != NULL) {
        vTaskSuspend(taskBHandle);
        Serial.println("Task B suspendiert");
      } else if (aktiverTask == 2 && taskCHandle != NULL) {
        vTaskSuspend(taskCHandle);
        Serial.println("Task C suspendiert");
      }
      
      // Zum nächsten Task wechseln
      aktiverTask = (aktiverTask + 1) % 3;
      
      // Neuen Task aktivieren
      if (aktiverTask == 0 && taskAHandle != NULL) {
        vTaskResume(taskAHandle);
        Serial.println("Task A aktiviert\n");
      } else if (aktiverTask == 1 && taskBHandle != NULL) {
        vTaskResume(taskBHandle);
        Serial.println("Task B aktiviert\n");
      } else if (aktiverTask == 2 && taskCHandle != NULL) {
        vTaskResume(taskCHandle);
        Serial.println("Task C aktiviert\n");
      }
    }
    
    delay(10);  // Kurze Pause für Task-Wechsel
  }
}

// ========== SETUP ==========
void setup() {
  // Serielle Kommunikation starten
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== FreeRTOS Task-Umschaltung ===");
  Serial.println("Langer Tastendruck schaltet zwischen Tasks um");
  Serial.println("=====================================\n");
  
  // Tasks erstellen
  xTaskCreate(
    taskHaupt,      // Task-Funktion
    "Haupttask",    // Name
    2048,           // Stack-Größe
    NULL,           // Parameter
    2,              // Priorität (höher als die anderen)
    &taskHauptHandle // Handle
  );
  
  xTaskCreate(
    taskA,
    "Task A",
    2048,
    NULL,
    1,              // Niedrigere Priorität
    &taskAHandle
  );
  
  xTaskCreate(
    taskB,
    "Task B",
    2048,
    NULL,
    1,
    &taskBHandle
  );
  
  xTaskCreate(
    taskC,
    "Task C",
    2048,
    NULL,
    1,
    &taskCHandle
  );
  
  // Warten bis alle Tasks initialisiert sind
  delay(100);
  
  // Task B und C initial suspendieren (Task A startet aktiv)
  if (taskBHandle != NULL) {
    vTaskSuspend(taskBHandle);
  }
  if (taskCHandle != NULL) {
    vTaskSuspend(taskCHandle);
  }
  
  Serial.println("Alle Tasks erstellt - Task A ist aktiv\n");
}

// ========== LOOP ==========
void loop() {
  // Loop bleibt leer, da FreeRTOS die Tasks verwaltet
  delay(1000);
}