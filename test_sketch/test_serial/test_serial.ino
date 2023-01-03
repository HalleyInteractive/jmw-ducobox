#include <HardwareSerial.h>

String command = "";

HardwareSerial LocalLog(0);
HardwareSerial DucoConsole(1);

void setup() {
  LocalLog.begin(115200);
  DucoConsole.begin(115200, SERIAL_8N1, 16, 17);
  
  LocalLog.println("Start listening...");
}

void processMessage(char const* message) {
  LocalLog.println("Sending message back");
  while(!DucoConsole.availableForWrite()) {
    LocalLog.println("Waiting for console to be available for write...");
    delay(50);
  }
  DucoConsole.println("FanSpeed: Actual 394 [rpm] - Filtered 394 [rpm]: ");
  DucoConsole.println(message);
  DucoConsole.println("Done");
  DucoConsole.flush();
  LocalLog.println("Done");
  
}

void loop() {
  while (DucoConsole.available()) {
    LocalLog.println("Serial available, reading string...");
    command = DucoConsole.readString();
    LocalLog.println("Command received: ");
    LocalLog.println(command);
  }

  if(command.length() > 0) {
    processMessage(command.c_str());
    command = "";
  }

}
