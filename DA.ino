  #include <SPI.h>
  #include <MFRC522.h>
  #include <ESP32Servo.h>
  #include <LiquidCrystal_I2C.h>
  #include <FirebaseESP32.h>

  // WiFi thông tin
  #define WIFI_SSID "NHU"
  #define WIFI_PASSWORD "23042007"

  // Firebase thông tin
  #define FIREBASE_HOST "parkingsystem-9d434-default-rtdb.firebaseio.com"//"webs-943c5-default-rtdb.firebaseio.com"
  #define FIREBASE_AUTH "066bluyAcyeQySyX6W4RTaUw5SkC0FZPGiUOaBr2"//"AyUxXwk7jGSO6rJ4nDyGwSEK7Pwiuwy9qxdeaNg0"

  // Firebase config
  FirebaseData firebaseData;
  FirebaseConfig config;
  FirebaseAuth auth;
  FirebaseJson json;
  String path = "/ParkingData";

  // RFID
  #define RST_PIN_R 4
  #define SS_PIN_R 5
  #define RST_PIN_L 16
  #define SS_PIN_L 17

  // LEDs
  #define LED_PIN_R 32
  #define LED_PIN_L 2

  // Cảm biến
  #define IR_SENSOR_PIN 33
  #define TRIG_PIN_1 12
  #define ECHO_PIN_1 14
  #define TRIG_PIN_2 27
  #define ECHO_PIN_2 26

  // Servo
  Servo servo1, servo2;
  #define SERVO_PIN_R 13
  #define SERVO_PIN_L 15

  // LCD
  LiquidCrystal_I2C lcd(0x27, 16, 2);

  // Biến trạng thái
  MFRC522 mfrc522_R(SS_PIN_R, RST_PIN_R);
  MFRC522 mfrc522_L(SS_PIN_L, RST_PIN_L);
  MFRC522 mfrc522_R2(SS_PIN_R, RST_PIN_R);
  int totalSlots = 4;
  int availableSlots = totalSlots;
  int available1 = totalSlots;
  bool gateOpen_R = false, gateOpen_L = false;
  int vehicleCount = 0;
  bool barrierState = false;
  int lastAvailableSlots = -1; // Trạng thái trước đó
  std::vector<String> rightUidList;
  // kiểm tra UID
  String uid_R = ""; // UID bên phải
  String uid_L = ""; // UID bên trái
  String uid_R2 = "";


  // Biến lưu trữ trạng thái trước đó
  bool lastGateState_R = false;
  bool lastGateState_L = false;
  String lastUid_R = ""; // UID_Right trước đó
  int currentNode = 0; // Node Firebase hiện tại  
// *** CẤU HÌNH THỜI GIAN ***
  unsigned long previousMillis = 0;
  const long interval = 2000;  // 2 giây
  // Thời gian cho loop
  unsigned long lastMillis = 0;


// *** HÀM SETUP ***

  void setup() {
      Serial.begin(9600);
      SPI.begin();
      // WiFi
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
      }
      Serial.println("\nWiFi connected! IP address: " + WiFi.localIP());
      // Firebase
      config.host = FIREBASE_HOST;
      config.signer.tokens.legacy_token = FIREBASE_AUTH;
      Firebase.begin(&config, &auth);
      Firebase.reconnectWiFi(true);

      if (Firebase.beginStream(firebaseData, path)) {
          Serial.println("Firebase stream started successfully.");
      } else {
          Serial.println("Failed to start Firebase stream: " + firebaseData.errorReason());
      }

      // RFID
      mfrc522_R.PCD_Init();
      mfrc522_L.PCD_Init();

      // Servo và LCD
      servo1.attach(SERVO_PIN_R);
      servo2.attach(SERVO_PIN_L);
      servo1.write(0);
      servo2.write(0);
      lcd.init();
      lcd.backlight();

      // Pin cảm biến
      pinMode(TRIG_PIN_1, OUTPUT);
      pinMode(ECHO_PIN_1, INPUT);
      pinMode(TRIG_PIN_2, OUTPUT);
      pinMode(ECHO_PIN_2, INPUT);

      Serial.println("Setup hoàn tất.");
  }


// *** HÀM VÒNG LẶP CHÍNH ***
  void loop() {
      // Đảm bảo kết nối Firebase và WiFi
      ensureFirebaseConnection();
      handleFirebaseStream();
      // checkIRSensor();
        // updateFirebase(availableSlots, gateOpen_L, gateOpen_R, uid_R, uid_L,uid_R2);
      // checkAndOpenLeftBarrierFromFirebase();
      // Kiểm tra khoảng cách bãi trống
      CheckSlotEmpty();

      // Kiểm tra RFID bên phải
      if (mfrc522_R.PICC_IsNewCardPresent() && mfrc522_R.PICC_ReadCardSerial()) {
          handleCard(mfrc522_R, servo1, LED_PIN_R, gateOpen_R, "Right");
      }

      // Kiểm tra RFID bên trái
      if (mfrc522_L.PICC_IsNewCardPresent() && mfrc522_L.PICC_ReadCardSerial()) {
          handleCard(mfrc522_L, servo2, LED_PIN_L, gateOpen_L, "Left");
      }
      // // Kiểm tra RFID bên phải 2
      // if (mfrc522_R2.PICC_IsNewCardPresent() && mfrc522_R2.PICC_ReadCardSerial()) {
      //      handleCard(mfrc522_R2, servo1, LED_PIN_R, gateOpen_R, "Right2");
      // }

      // Kiểm tra và cập nhật trạng thái Firebase
  // Kiểm tra và cập nhật trạng thái Firebase
if (availableSlots != lastAvailableSlots || gateOpen_R != lastGateState_R || gateOpen_L != lastGateState_L || uid_R != lastUid_R) {
    // Tăng node nếu UID_Right thay đổi
    if (uid_R != lastUid_R || available1 == 0) {
        currentNode++; // Node mới cho UID_Right
        lastUid_R = uid_R; // Cập nhật lastUid_R
    }

    // Cập nhật Firebase với trạng thái mới nếu có sự thay đổi
    updateFirebase(currentNode, gateOpen_L, gateOpen_R, uid_R, uid_L, available1, vehicleCount);

    // Cập nhật trạng thái trước đó
    // lastAvailableSlots = availableSlots;
    lastGateState_R = gateOpen_R;
    lastGateState_L = gateOpen_L;
}

      // Hiển thị thông tin mỗi 2 giây
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
          // Lưu thời gian hiện tại
          previousMillis = currentMillis;

          // Hiển thị các giá trị hiện tại
          Serial.print("Available Slots: ");
          Serial.println(availableSlots);
          Serial.print("Gate Left Open: ");
          Serial.println(gateOpen_L ? "true" : "false");
          Serial.print("Gate Right Open: ");
          Serial.println(gateOpen_R ? "true" : "false");
      }

      delay(100);  // Tránh quá tải CPU
  }

  // Đảm bảo kết nối WiFi và Firebase
  void ensureFirebaseConnection() {
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi disconnected. Reconnecting...");
          WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
          while (WiFi.status() != WL_CONNECTED) {
              delay(500);
              Serial.print("...");
          }
          Serial.println("\nWiFi reconnected.");
      }
      if (!Firebase.ready()) {
          Serial.println("Firebase not ready. Reinitializing...");
          Firebase.begin(&config, &auth);
      }
  }

  // Xử lý luồng Firebase
  void handleFirebaseStream() {
      if (Firebase.readStream(firebaseData)) {
          if (firebaseData.streamTimeout()) {
              Serial.println("Stream timeout, reconnecting...");
              Firebase.beginStream(firebaseData, path);
          } else if (firebaseData.streamAvailable()) {
              Serial.println("Stream available");
              String key = firebaseData.dataPath();
              String value = firebaseData.stringData();
              // In ra chỉ những dữ liệu bạn muốn
              if (key == "/availableSlots") {
                  availableSlots = value.toInt();
                  Serial.println("Available Slots: " + String(availableSlots));
              } 
              else if (key == "/openGate_L") {
                  gateOpen_L = value == "true";
                  Serial.println("Open Gate Left: " + String(gateOpen_L));
              }
              else if (key == "/openGate_R") {
                  gateOpen_R = value == "true";
                  Serial.println("Open Gate Right: " + String(gateOpen_R));
              }
          }
      } else {
          Serial.println("Error in Firebase stream: " + firebaseData.errorReason());
      }
  }
void handleCard(MFRC522 &reader, Servo &servo, int ledPin, bool &gateState, const char *side) {
    String uid = "";
    for (byte i = 0; i < reader.uid.size; i++) {
        if (reader.uid.uidByte[i] < 0x10) uid += "0"; // Đảm bảo luôn có 2 chữ số
        uid += String(reader.uid.uidByte[i], HEX);
    }
    uid.toUpperCase(); // Chuyển UID sang chữ in hoa
    Serial.print("UID (");
    Serial.print(side);
    Serial.print("): ");
    Serial.println(uid);
    int nodeToUse = findEmptyNode();
    if (strcmp(side, "Right") == 0) {
        // Kiểm tra node trống trước khi thêm UID_Right
        
        if (nodeToUse == -1) {
            // Không có node trống, tạo node mới
            currentNode++;
            nodeToUse = currentNode;
        }

        // Nếu UID mới khác UID cũ, lưu vào node
        if (uid != lastUid_R || available1 == 4) {
            uid_R = uid;
            lastUid_R = uid;
            rightUidList.push_back(uid);

            // Mở cổng
            if (vehicleCount < totalSlots && available1 > 0) {
                openGate(servo, ledPin, gateState);
                if (gateOpen_R == true) {
                    vehicleCount++;
                    available1--;
                    Serial.println("Available slots decreased. Current count: " + String(available1));
                    updateFirebase(nodeToUse, gateOpen_L, gateOpen_R, uid_R, "", available1, vehicleCount);
                    delay(3000); // Chờ 3 giây trước khi đóng barrier
                }
            }

            // Đóng barrier
            closeGate(servo, gateState);
            gateOpen_R = false;
            updateFirebase(nodeToUse, gateOpen_L, gateOpen_R, uid_R, "", available1, vehicleCount);
        } else {
            Serial.println("UID Right trùng với UID trước đó, không ghi lại.");
        }
    } else if (strcmp(side, "Left") == 0) {
        uid_L = uid;
        Serial.println("Thẻ quét tại cổng Left.");

        // Kiểm tra UID_Left trùng với UID_Right
        for (size_t i = 0; i < rightUidList.size(); i++) {
            if (rightUidList[i] == uid) {
                nodeToUse = i + 1;
                if (vehicleCount > 0 && available1 <= totalSlots) {
                    available1++;
                    vehicleCount--;
                }
                updateFirebase(nodeToUse, gateOpen_L, gateOpen_R, rightUidList[i], uid_L, available1, vehicleCount);
                Serial.println("Đã cập nhật UID_Left cho node trùng khớp.");

                // Xóa node sau khi hoàn tất
                deleteNodeFromFirebase(nodeToUse);
                rightUidList[i] = "DELETED";
                //rightUidList.erase(rightUidList.begin() + i);
                break;
            }
        }
    }

    reader.PICC_HaltA();
}

// Hàm tìm node trống
int findEmptyNode() {
    for (int i = 1; i <= currentNode; i++) {
        String pathNode = "ParkingData/" + String(i);
        if (Firebase.get(firebaseData, pathNode + "/UID_Right") && firebaseData.stringData() == "") {
            return i; // Node trống
        } else if (Firebase.get(firebaseData, pathNode + "/UID_Right") && firebaseData.stringData() == "DELETED") {
            continue; // Bỏ qua các node bị đánh dấu là "DELETED"
        }
    }
    return -1; // Không tìm thấy node trống
}

// Hàm xóa node khỏi Firebase
void deleteNodeFromFirebase(int nodeNumber) {
    if (nodeNumber <= 0) {
        Serial.println("Invalid node number: " + String(nodeNumber));
        return;
    }

    String pathNode = "ParkingData/" + String(nodeNumber);
    // Chỉ đánh dấu các UID là "DELETED"
    if (Firebase.set(firebaseData, pathNode + "/UID_Right", "DELETED") && 
        Firebase.set(firebaseData, pathNode + "/UID_Left", "DELETED")) {
        Serial.println("Node " + String(nodeNumber) + " được đánh dấu là đã xóa.");
    } else {
        Serial.println("Không thể đánh dấu node " + String(nodeNumber) + " là đã xóa.");
    }
}




void updateFirebase(int nodeNumber, bool gate_L, bool gate_R, String uid_R, String uid_L, int available, int count) {
    // Tạo JSON cho node mới
    FirebaseJson jsonNode;
    jsonNode.set("/UID_Right", uid_R);
    jsonNode.set("/UID_Left", uid_L);

    // Đường dẫn node
    String pathNode = "ParkingData/" + String(nodeNumber); // Node tăng dần

    // Lưu thông tin node vào Firebase
    if (Firebase.set(firebaseData, pathNode, jsonNode)) {
        Serial.println("Firebase node updated successfully at " + pathNode);
    } else {
        Serial.println("Error updating Firebase node: " + firebaseData.errorReason());
    }

    // Tạo JSON cho trạng thái cổng
    FirebaseJson jsonStatus;
    jsonStatus.set("/barrierLeft", gate_L);
    jsonStatus.set("/barrierRight", gate_R);

    // Đường dẫn thư mục status
    String pathStatus = "ParkingData/status";

    // Lưu trạng thái cổng vào Firebase
    if (Firebase.set(firebaseData, pathStatus, jsonStatus)) {
        Serial.println("Firebase status updated successfully at " + pathStatus);
    } else {
        Serial.println("Error updating Firebase status: " + firebaseData.errorReason());
    }

    // Tạo JSON cho carCount và lưu vào ParkingData
    FirebaseJson jsonCarCount;
    jsonCarCount.set("/vehicleCount", count);
    jsonCarCount.set("/available slot", available); // Thêm carCount vào json riêng biệt

    // Đường dẫn để lưu carCount
    String pathCarCount = "ParkingData/carCount";

    // Lưu carCount vào Firebase
    if (Firebase.set(firebaseData, pathCarCount, jsonCarCount)) {
        Serial.println("Firebase carCount updated successfully at " + pathCarCount);
    } else {
        Serial.println("Error updating carCount: " + firebaseData.errorReason());
    }
}


// Mở và đóng cổng
void openGate(Servo &servo, int led, bool &gate) {
    digitalWrite(led, HIGH);
    servo.write(90);
    gate = true;
    lcd.clear();
    lcd.setCursor(4, 1);
    lcd.print("OPEN GATE");
    delay(2000);
}

void closeGate(Servo &servo, bool &gate) {
    servo.write(0);
    gate = false;
    lcd.clear();
    lcd.setCursor(4, 1);
    lcd.print("CLOSE GATE");
}

// void checkIRSensor() {
//     // Đọc tín hiệu cảm biến hồng ngoại
//     int irSensorValue = digitalRead(IR_SENSOR_PIN);

//     // Nếu cảm biến phát hiện vật cản
//     if (irSensorValue == HIGH) { 
//         Serial.println("Phát hiện xe qua cảm biến hồng ngoại.");
//         delay(3000);  // Đợi 3 giây để xe đi qua hoàn toàn
        
//         // Đóng barrier bên phải nếu đang mở
//         if (gateOpen_R) {
//             Serial.println("Đang đóng barrier bên phải...");
//             closeGate(servo1, gateOpen_R);
//         }

//         // Đóng barrier bên trái nếu đang mở
//         if (gateOpen_L) {
//             Serial.println("Đang đóng barrier bên trái...");
//             closeGate(servo2, gateOpen_L);
//         }

//         Serial.println("Barrier đã đóng.");
//     } else {
//         // Không phát hiện tín hiệu từ cảm biến
//         Serial.println("Không có xe qua cảm biến hồng ngoại.");
//     }
// }

  // Kiểm tra bãi trống
  void CheckSlotEmpty() {
      int occupiedSlots = 0;
      float distance1 = measureDistance(TRIG_PIN_1, ECHO_PIN_1);
      float distance2 = measureDistance(TRIG_PIN_2, ECHO_PIN_2);
      if (distance1 < 50) occupiedSlots++;
      if (distance2 < 50) occupiedSlots++;
      availableSlots = totalSlots - occupiedSlots;

      if (availableSlots != lastAvailableSlots) {
          lcd.clear();
          if (availableSlots == 0) {
              lcd.setCursor(3, 1);
              lcd.print("FULL SLOT!");
          } else {
              lcd.setCursor(0, 0);
              lcd.print("Empty slots: ");
              lcd.setCursor(12, 0);
              lcd.print(availableSlots);
          }
          lastAvailableSlots = availableSlots;
      }
  }

  // Đo khoảng cách cảm biến siêu âm
  float measureDistance(int trigPin, int echoPin) {
      digitalWrite(trigPin, LOW);
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);
      long duration = pulseIn(echoPin, HIGH, 30000);
      return duration * 0.0344 / 2;  // Tính khoảng cách (cm)
  }



// void checkAndOpenLeftBarrierFromFirebase() {
//     char uid_L[32] = {0}; // UID của cổng Left
//       char uidRight[32] = {0};
//      char uidRight2[32] = {0};

//     // Lấy UID từ Firebase cho cổng trái
//     if (Firebase.getString(firebaseData, path + "/Left/UID_Left", uid_L)) {
//         // Serial.println("Left UID: " + String(uid_L));
//     } else {
//         // Serial.println("Error reading Left UID: " + firebaseData.errorReason());
//         return;
//     }

//     // Lấy UID từ Firebase cho cổng phải (Right)
//     if (Firebase.getString(firebaseData, path + "/Right/UID_Right", uidRight)) {
//         // Serial.println("Right UID: " + String(uidRight));
//     } else {
//         // Serial.println("Error reading Right UID: " + firebaseData.errorReason());
//         return;
//     }

//     // Lấy UID từ Firebase cho cổng Right1 (nếu có)
//     if (Firebase.getString(firebaseData, path + "/Right2/UID_Right2", uidRight2)) {
//         // Serial.println("Right2 UID: " + String(uidRight2));
//     } else {
//         // Serial.println("No Right1 UID found.");
//     }

//     // So sánh UID giữa Left và Right
//       bool matchWithRight = false;
//     bool matchWithRight2 = false;

//     // So sánh với Right
//     if (String(uid_L) == String(uidRight)) {
//          matchWithRight = true;
//         // Serial.println("Match found with Right! Opening left barrier...");
//         openGate(servo2, LED_PIN_L, gateOpen_L);
//     }
//     // So sánh với Right1 nếu có
//     else if (String(uid_L) == String(uidRight2)) {
//         matchWithRight2 = true;
//         // Serial.println("Match found with Right1! Opening left barrier...");
//         openGate(servo2, LED_PIN_L, gateOpen_L);
//     }

//     // Nếu không có sự trùng khớp
// if (matchWithRight || matchWithRight2) {
//         if (matchWithRight) {
//             uid_L[0] = '\0';
//             uidRight[0] = '\0';
//             updateFirebase(availableSlots, gateOpen_L, gateOpen_R, "\0", String(uid_L), "\0");
            
//         }
//         else if (matchWithRight2) {
//             uid_L[0] = '\0';
//             uidRight2[0] = '\0';
//             updateFirebase(availableSlots, gateOpen_L, gateOpen_R, "", String(uid_L), "");
//         }
//         // updateFirebase(availableSlots, gateOpen_L, gateOpen_R, "", String(uid_L), "");
//         // Serial.println("No match found between Left and Right or Right1.");
//     }
// }

// void checkAndOpenLeftBarrierFromFirebase() {
//     char uid_L[32] = {0}; // UID của cổng Left
//     char uidRight[32] = {0};
//     char uidRight2[32] = {0};

//     // Lấy UID từ Firebase cho cổng trái
//     if (Firebase.getString(firebaseData, path + "/Left/UID_Left", uid_L)) {
//         // Serial.println("Left UID: " + String(uid_L));
//     } else {
//         return;
//     }

//     // Lấy UID từ Firebase cho cổng phải (Right)
//     if (Firebase.getString(firebaseData, path + "/Right/UID_Right", uidRight)) {
//         // Serial.println("Right UID: " + String(uidRight));
//     } else {
//         return;
//     }

//     // Lấy UID từ Firebase cho cổng Right1 (nếu có)
//     if (Firebase.getString(firebaseData, path + "/Right2/UID_Right2", uidRight2)) {
//         // Serial.println("Right2 UID: " + String(uidRight2));
//     } else {
//         // Không có Right2 UID
//     }

//     // So sánh UID giữa Left và Right
//     bool matchWithRight = false;
//     bool matchWithRight2 = false;

//     // So sánh với Right
//     if (String(uid_L) == String(uidRight)) {
//         matchWithRight = true;
//         openGate(servo2, LED_PIN_L, gateOpen_L);
//     }
//     // So sánh với Right1 nếu có
//     else if (String(uid_L) == String(uidRight2)) {
//         matchWithRight2 = true;
//         openGate(servo2, LED_PIN_L, gateOpen_L);
//     }

//     // Nếu có sự trùng khớp, xóa UID khỏi Firebase
//     if (matchWithRight || matchWithRight2) {
//         if (matchWithRight) {
//             // Xóa UID của cổng trái và cổng phải bằng cách thiết lập giá trị thành chuỗi rỗng
//             Firebase.set(firebaseData, path + "/Left/UID_Left", "");
//             Firebase.set(firebaseData, path + "/Right/UID_Right", "");
//             updateFirebase(availableSlots, gateOpen_L, gateOpen_R, "", "", "");  // Cập nhật lại Firebase với UID trống
//         } else if (matchWithRight2) {
//             // Xóa UID của cổng trái và cổng Right2 bằng cách thiết lập giá trị thành chuỗi rỗng
//             Firebase.set(firebaseData, path + "/Left/UID_Left", "");
//             Firebase.set(firebaseData, path + "/Right2/UID_Right2", "");
//             updateFirebase(availableSlots, gateOpen_L, gateOpen_R, "", "", "");  // Cập nhật lại Firebase với UID trống
//         }
//     }
// }