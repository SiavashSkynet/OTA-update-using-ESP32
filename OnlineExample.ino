#include <Wire.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <HTTPUpdate.h>
#include <Update.h>   
#include <CRC32.h>

// ESP32 LilyGo-T-Call-SIM800 SIM800L_IP5306_VERSION_20190610 (v1.3) pins definition

#define MODEM_UART_BAUD 115200
#define MODEM_RST 5
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22
#define LED_PIN 13
#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00
#define TOUCHPIN 2


// Set serial for debug console (to the Serial Monitor)
#define SerialMon Serial
// Set serial for AT commands (to the SIM800 module)
#define SerialAT Serial1

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon


// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

// Include after TinyGSM definitions
#include <TinyGsmClient.h>

// Your GPRS credentials (leave empty, if missing)
const char apn[] = "airtelgprs.com";    // Your APN
const char gprs_user[] = ""; // User
const char gprs_pass[] = ""; // Password
const char simPIN[] = "";    // SIM card PIN code, if any
#define GSM_PIN ""

// Server details
const char hostname[] = "engeletron-cloud-default-rtdb.firebaseio.com";
const char fota_teste[] = "/fota/teste.json";    // This line added -----------
const char update_server[]   = "firebasestorage.googleapis.com";
const char* update_resource;

char data[1024];
int port = 443;
const char* ch;
bool resetflag = false;
bool isValidContentType = false;
uint32_t knownCRC32    = 0x6f50d767;
uint32_t knownFileSize = 0; //599904


//wifi mode


const char* FirmwareVer="1.0";
String firmwareURl;
String path_version = "/fota/teste/version";
String path_firmware = "/fota/teste/firmware";

int cnt;
String value_string;

// Layers stack

TinyGsm sim_modem(SerialAT);
TinyGsmClientSecure client(sim_modem);
DynamicJsonDocument doc(1024);

void setupModem()
{
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TOUCHPIN,INPUT_PULLUP);
  
  // Reset pin high
  digitalWrite(MODEM_RST, HIGH);

  // Turn on the Modem power first
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Pull down PWRKEY for more than 1 second according to manual requirements
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(200);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1200);
  digitalWrite(MODEM_PWRKEY, HIGH);

  // Initialize the indicator as an output
  digitalWrite(LED_PIN, LOW);
}


void setup() {
SerialMon.begin(115200);
  delay(1000);
  if(!SPIFFS.begin(true))
  {
    Serial.println(" >>> An Error has occured while Mounting SPIFFS <<< ");
    return;
  }
    
  if(!SPIFFS.format())
  {
    Serial.println(">>>>>>>>> Failed to Format File <<<<<<<<<<<");
  }

  // Set SIM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  // SIM modem initial setup
  setupModem();
  
  
}

void printPercent(uint32_t readLength, uint32_t contentLength) {
  // If we know the total length
  if (contentLength != -1) {
    SerialMon.print("\r ");
    SerialMon.print((100.0 * readLength) / contentLength);
    SerialMon.print('%');
  } else {
    SerialMon.println(readLength);
  }
}


void loop() {
  Serial.print(F("Waiting for network..."));
  SerialMon.print("Initializing modem...");
  if (!sim_modem.init())
  {
    SerialMon.print(" Failed... restarting modem...");
    setupModem();
    // Restart takes quite some time
    // Use modem.init() if you don't need the complete restart
    if (!sim_modem.restart())
    {
      SerialMon.println(" Fail... even after restart");
      return;
    }
  }
  SerialMon.println(" OK");

  // General information
  String name = sim_modem.getModemName();
  Serial.println("Modem Name: " + name);
  String modem_info = sim_modem.getModemInfo();
  Serial.println("Modem Info: " + modem_info);

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && sim_modem.getSimStatus() != 3)
  {
    sim_modem.simUnlock(simPIN);
  }

  // Wait for network availability
  SerialMon.print("Waiting for network...");
  if (!sim_modem.waitForNetwork(240000L))
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");
  
  // Connect to the GPRS network
  SerialMon.print("Connecting to network...");
  if (!sim_modem.isNetworkConnected())
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");

  // Connect to APN
  SerialMon.print(F("Connecting to APN: "));
  SerialMon.print(apn);
  if (!sim_modem.gprsConnect(apn, gprs_user, gprs_pass))
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }  
  SerialMon.println(" OK");

  // More info..
  Serial.println("");
  String ccid = sim_modem.getSimCCID();
  Serial.println("CCID: " + ccid);
  String imei = sim_modem.getIMEI();
  Serial.println("IMEI: " + imei);
  String cop = sim_modem.getOperator();
  Serial.println("Operator: " + cop);
  IPAddress local = sim_modem.localIP();
  Serial.println("Local IP: " + String(local));
  int csq = sim_modem.getSignalQuality();
  Serial.println("Signal quality: " + String(csq));
 //  ----------------------------------------------------------
  DynamicJsonDocument elements(1024);
  bool startPrint = false;
  int i = 0;

  client.connect(hostname,port,30);
  if(client.connected())
  {
    Serial.println("*************************");
    Serial.println("\n Checking for new Updates ...");
    client.print(String("GET ") + fota_teste + " HTTP/1.1\r\n");
    client.print(String("Host: ") + hostname + "\r\n");
    client.print("Accept: application/json \r\n");
    client.println();
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client Timeout !");
        client.stop();
        return;
      }
    }
    Serial.println("\n Data Received - ");
    while (client.available()) {
      char c = client.read();
      if(c == '{' || startPrint)
      {
        startPrint = true;
        data[i] = c;
        i++;
      }
    }
  }
  else
  {
    Serial.println("Not connected to the host!!");
  }
  deserializeJson(elements,data);
  const char* ch = elements["version"];
  String result(ch);
  if(strcmp(ch,FirmwareVer))
  {
    Serial.println("--- Firmware version not matching ---");
    update_resource = elements["firmware"];
    client.stop();
  }


 // ------------------------------------------------------------


  Serial.print(F("Connecting to "));
  Serial.print(update_server);
  if (!client.connect(update_server, port)) {
    Serial.println(" fail");
    delay(10000);
    return;
  }
  Serial.println(" OK");

  // Make a HTTP GET request:
  client.print(String("GET ") + update_resource + " HTTP/1.1\r\n");
  client.print(String("Host: ") + update_server + "\r\n\r\n\r\n\r\n");
  client.print("Connection: close\r\n\r\n\r\n\r\n");

  long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000L) {
      Serial.println(F(">>> Client Timeout !"));
      client.stop();
      delay(10000L);
      return;
    }
  }

  Serial.println(F("Reading response header"));
  uint32_t contentLength = 0;
  
  File file = SPIFFS.open("/firmware.bin", FILE_APPEND);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    //Serial.println(line);    // Uncomment this to show response header
    line.toLowerCase();
    if (line.startsWith("content-length:")) {
      contentLength = line.substring(line.lastIndexOf(':') + 1).toInt();
    } else if (line.length() == 0) {
      break;
    }
  }

  Serial.println(F("Reading response data"));
  timeout = millis();
  uint32_t readLength = 0;
  CRC32 crc;

  unsigned long timeElapsed = millis();
  printPercent(readLength, contentLength);
  while (/*!(readLength > contentLength) &&*/ client.connected() /*&& millis() - timeout < 10000L*/) {
      if (client.available()) {
       if (!file.write(char(client.read())))
      {
          Serial.println("error writing character to SPIFFS");
      }
      readLength++;
//      Serial.println(String(readLength));
//      if (readLength % (contentLength / 13) == 0) {
//        printPercent(readLength, contentLength);
//      }
//      timeout = millis();   

      }
  }
//  file.close();
  printPercent(readLength, contentLength);
  timeElapsed = millis() - timeElapsed;
  Serial.println();

  // Shutdown

  //client.stop();
  Serial.println(F("Server disconnected"));
  sim_modem.gprsDisconnect();
  Serial.println(F("GPRS disconnected"));

  float duration = float(timeElapsed) / 1000;

  Serial.println();
  Serial.print("Content-Length: ");   Serial.println(contentLength);
  Serial.print("Actually read:  ");   Serial.println(readLength);
  Serial.print("Calc. CRC32:    0x"); Serial.println(crc.finalize(), HEX);
  Serial.print("Known CRC32:    0x"); Serial.println(knownCRC32, HEX);
  Serial.print("Duration:       ");   Serial.print(duration); Serial.println("s");
  
  Serial.println("starting Update after 3 seconds  ");
  for (int i = 0; i < 3; i++)
  {
      Serial.print(String(i) + "...");
      delay(1000);
  }
  //readFile(SPIFFS, "/firmware.bin");
  updateFromFS();



  // Do nothing forevermore
  while (true) {
    delay(1000);
  }
}

void readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while (file.available())
    {
        Serial.write(file.read());
        delayMicroseconds(100);
    }
}

void updateFromFS()
{
    client.stop();
    File updateBin = SPIFFS.open("/firmware.bin", "r");
    if (updateBin)
    {


        size_t updateSize = updateBin.size();
        Serial.println("##### Size of the file - " + String(updateSize));

        if (updateSize > 0)
        {
            Serial.println("start of updating");
            performUpdate(updateBin, updateSize);
        }
        else
        {
            Serial.println("Error, file is empty");
        }

        updateBin.close();

        // whe finished remove the binary from sd card to indicate end of the process
        //fs.remove("/firmware.bin bin");
    }
    else
    {
        Serial.println("can't open firmware.bin bin");
    }
}


void performUpdate(Stream &updateSource, size_t updateSize)
{
    if (Update.begin(updateSize))
    {
        size_t written = Update.writeStream(updateSource);
        if (written == updateSize)
        {
            Serial.println("writings : " + String(written) + " successfully");
        }
        else
        {
            Serial.println("only writing : " + String(written) + "/" + String(updateSize) + ". Retry?");
        }
        Serial.println("#### Ending Update ####");
        if (Update.end())
        {
            Serial.println("OTA accomplished!");
            if (Update.isFinished())
            {
                Serial.println("OTA ended. restarting!");
                ESP.restart();
            }
            else
            {
                Serial.println("OTA didn't finish? something went wrong!");
            }
        }
        else
        {
            Serial.println("Error occured #: " + String(Update.getError()));
        }
    }
    else
    {
        Serial.println("without enough space to do OTA");
    }
}
