#include <WiFi.h>

WiFiClient espClient;
//byte mac[6];
char mac[20];
IPAddress ip;

void setup_wifi() {
  Serial.print("Wifi setup... ");
  delay(10);
  Serial.println();
  Serial.print("  Connecting to ");
  Serial.print(SECRET_WIFI_SSID);

  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  byte b_mac[6];
  WiFi.macAddress(b_mac);
  sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",b_mac[0], b_mac[1], b_mac[2], b_mac[3], b_mac[4],b_mac[5]);
  ip = WiFi.localIP();

  Serial.print("  IP address: "); Serial.println(ip);
  Serial.print("  MAC address: "); Serial.println(mac);
}
