// 244-470-AL - Communication des Objets 4.0 
// Hiver 2024 - Laboratoire 04 - Partie 1 - Simple Sketch MQTT client
/*
 *  Par: Yh - janvier 2022 - revu 27 février 2023 - révisé en mars 2024
 *  
 * 
 * Crédits:
 *  Joël Gähwiler - https://github.com/256dpi/arduino-mqtt
 */

//[code_pour_inclure_la_lib_Ethernet]
#include <Ethernet.h>  
#include <MQTT.h>                //Lib by Joël Gähwiler : https://github.com/256dpi/arduino-mqtt  v2.5.2
//[code_pour_inclure_les_secrets]
#include "secrets.h"


//Version du programme :
#define VERSION "2.0.5"

// Broches pour la connectivité SPI au module Ethernet
#define SS 5    //broche 5 sur le module DFRobot (shield Ethernet) + protoTPhys

//À compléter avec l'adresse mac que l'on vous fournira
//format 6 octets séparés par une virgule, ex: [octet],[octet],...
byte mac[] = { 0x24, 0x6f, 0x28, 0x01, 0x01, 0xA4 };

// Lier les définitions disponibles dans secrets.h aux variables suivantes:
// Configuration de la connectivité à un service MQTT:
// *** SVP: UTILISER LES DÉFINITIONS DU FICHIER secrets.h ***//
const char mqttHostProvider[]= "mqtt://" ;  //adresse du serveur MQTT (FQDN)
const char mqttUsername[]= "technophyscal";  //usager du service MQTT de shiftr.io
const char mqttDevice[]= "etudiant04" ;  //nom du dispositif connectant au service (etudiantXX)
const char mqttUserPass[]= "xFDAtDxZJhxVLUqo" ; //token du dispositif (associé au device "etudiantXX")
const char mqttSubscribeTo[]="LABO_4_MQTT_YE_MM";  //Nom du canal (topic) d'abonnement (peut commencer par "/")
const char mqttPublishTo[]="LABO_4_MQTT_YE_MM";  //Nom du canal (topic) de publication (peut commencer par "/")

//Compteurs de messages envoyés et recus:
uint16_t recMsgCount=0;
uint16_t senMsgCount=0;

//Mettre ici la déclaration d'un objet client TCP dont le nom devra remplacer '[nom_objet_Client_MQTT]':
//[code_clientTCP_selon_le_mode_de_transport_Eth-Wifi-autre]
//MQTTClient [nom_objet_Client_MQTT](512); //réservation de mémoire pour l'échange de data

MQTTClient clientMQTT;
EthernetClient clientEth;

bool rtcTimeSet=false;
bool etherStatus=false;

//Minuterie entre les publications: 20 secondes
uint32_t currentMillis = 0;
const uint32_t msgInterval=20000; 

//Minuterie de vérification de la connectivité: 10 secondes
uint32_t checkEtherStatusTimer =0;
const uint32_t checkEtherStatusDelay = 10000;

//Routine d'établissement de la connectivité au service MQTT:
bool connectMQTT() {
  bool retCode = false;

  Serial.print("connecting...");
  // Tant que la connexion avec MQTT broker n'est pas établie
  // synopsis (réf. librairie arduino-mqtt, MQTTClient.h, ligne 151): 
  //   bool connect(const char clientID[], const char username[], const char password[], bool skip = false);
  while (!clientMQTT.connect( mqttDevice, mqttUsername, mqttUserPass)) {
    Serial.print(".");
    delay(1000);
  }
 
  Serial.println("\nconnected!");

  //Réussi! On s'abonne donc au topic (à vous de déterminer):
  retCode = clientMQTT.subscribe(mqttSubscribeTo);

  return retCode;
}

// Routine de traitement d'un message recu, appellé de façon asynchrone, par [objet].loop()
void messageReceived(String &topic, String &payload) {
  recMsgCount++;
  Serial.println("incoming("+String(recMsgCount)+"): " + topic + " - " + payload);
  
  // Note: Do not use the [nom_objet_Client_MQTT] in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `[nom_objet_Client_MQTT].loop()`.
}

//Routine d'affichage de l'adresse MAC Ethernet:
void getNPrintMAC(void) {
  byte macBuffer[6];  // create a buffer to hold the MAC address
  Ethernet.MACAddress(macBuffer); // fill the buffer
//  Serial.print("The MAC address is: ");
  for (byte octet = 0; octet < 6; octet++) {
    if (macBuffer[octet] < 0x10) Serial.print('0');
    Serial.print(macBuffer[octet], HEX);
    if (octet < 5) {
      Serial.print('-');
    }
  }
  Serial.println("");
}

// --- Setup et Loop ----------------------------------------------------------------

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 client MQTT");
  Serial.println("** Initialisation **");

  //Affiche le nom du programme au démarrage :
  Serial.println("> ESP32 Client Ethernet. Version du code = "+String(VERSION));

 // start the Ethernet connection and the server:
  Ethernet.init(SS);

  //Affiche dans le moniteur série l'adresse MAC
  Serial.print("> Adresse MAC : "); getNPrintMAC();

  //Affiche dans le moniteur série l'adresse IP attribuée par le réseau
  Serial.print("> Adresse IP : "); Serial.println(Ethernet.localIP());
  
  Serial.print("\n> Branchement réussi avec le réseau: "); Serial.println("Ethernet");

  Serial.println("> Préparation de la connexion au broker MQTT: "+ String(1));

  //Initialisation de la librairie: quel serveur MQTT utiliser:
  clientMQTT.begin( "technophyscal.cloud.shiftr.io", clientEth);

  //Initialisation de la librairie: quelle fonction appeller lorsqu'un msg est recu:
  clientMQTT.onMessage(messageReceived);
 
  // Connexion au service MQTT tel que configuré, si ca passe, on publie sur le topic un message.
  if (connectMQTT())
    clientMQTT.publish(mqttPublishTo, "Bonjour, je suis prêt");
}
//Fin de setup
 
void loop() {
  //Élément obligatoire pour vérifier les transactions MQTT:
  clientMQTT.loop();

  //Vérification à intervalle régulière de la connectivité du transport et rétablir s'il y a lieu
  if ((millis() - checkEtherStatusTimer) > checkEtherStatusDelay) {
    checkEtherStatusTimer = millis();
    if (Ethernet.maintain() == 0) { 
      if (!clientMQTT.connected()) {
        connectMQTT(); 
      }
    } else Serial.println(F("> Probleme de transport"));
  }
 
  // publication d'un message à intervalle régulière, basé sur la variable 'msgInterval'.
  if (millis() - currentMillis > msgInterval) { 

     currentMillis = millis(); 

    //Teste si la connectivité est tjrs là:
    if (clientMQTT.connected()) { 

      //Confection du message à envoyer:
     String msg = "bla bla " ; //forger "Iteration: 0", puis "Iteration 1" ... //TODO

      //Publication du message sur un topic:
      if ( clientMQTT.publish(mqttPublishTo, msg) ) {
       // [code]  //Incrémente le compteur de message envoyé. Affiche l'information sur le moniteur série: //TODO
        //Serial.println("> Publication msg("+String(senMsgCount)+") au sujet "+String(mqttPublishTo[])+" de "+String(dataSize)+" octets.");      
      } else {
        //En cas d'erreur d'envoi, on affiche les détails:
        Serial.println("> ERREUR publication: "+String(clientMQTT.lastError()));
        if (clientMQTT.connected())
          Serial.println("> clientMQTT connecté");
        else
          Serial.println("> clientMQTT NON-connecté");
        //Serial.println("> Msg ("+String(dataSize)+") longueur est: "+msg); //TODO
      }//Fin if-then-else publication MQTT

    } else {
      Serial.println("> Ne peut procéder: clientMQTT non-connecté!");
    }//Fin if-then-else test de connectivité MQTT
  } //Fin de la minuterie
}//Fin de loop