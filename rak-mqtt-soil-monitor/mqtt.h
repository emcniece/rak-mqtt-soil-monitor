#include <PubSubClient.h>  // https://github.com/knolleary/pubsubclient

// Define an ID to indicate the device, If it is the same as other devices which connect the same mqtt server,
// it will lead to the failure to connect to the mqtt server
const char* mqttClientId = "rak_test_client";
PubSubClient mqttClient(espClient);
const int ledPin = WB_LED1;

/* callback of receiving subscribed message
   it can only deal with topics that he has already subscribed.
*/
void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp = "";
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");

  // get and print message recieved
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic RAK11200/led, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "RAK11200/led") {
    Serial.print("Changing led status to ");
    if (messageTemp == "on") {
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    } else if (messageTemp == "off") {
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }

  // Feel free to add more to handle more topics
}

/*
    reconnect to mqtt broker if disconnected
    when connected to mqtt broker, subscribe topic here.
*/
void reconnect() {
  // Loop until reconnected
  while (!mqttClient.connected()) {
    Serial.print("Connecting to  MQTT broker...");
    if (mqttClient.connect(mqttClientId, SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
      Serial.println("connected");

      // Subscribe topics which you want to receive.
      mqttClient.subscribe("RAK11200/led");
      // you can add other Subscribe here
    } else {
      Serial.print("failed, code=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup_mqtt(){
  Serial.println("MQTT setup... ");
  // set LED pin mode
  pinMode(ledPin, OUTPUT);

  // connect mqtt broker
  mqttClient.setServer(SECRET_MQTT_SERVER, SECRET_MQTT_PORT);
  // set callback of receiving message
  mqttClient.setCallback(callback);
  Serial.println("MQTT setup complete");
}
