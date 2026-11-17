/*
 * ============================================================
 *  PORTE POULAILLER AUTOMATIQUE — v2
 *  ESP32 + BTS7960 + MC-38 NF + ACS712 + BH1750
 *  Home Assistant via MQTT
 *
 *  4 modes de gestion :
 *    soleil     — lever/coucher du soleil + offsets
 *    luminosite — seuils lux + durée de confirmation
 *    heure_fixe — heures définies + offsets optionnels
 *    manuel     — commandes manuelles uniquement
 *
 * ============================================================
 *  Dépendances :
 *    - PubSubClient         (Nick O'Leary)
 *    - ArduinoJson          (Benoit Blanchon)
 *    - BH1750               (Christopher Laws)
 *    - ESPAsyncWebServer    (ESP32Async)
 *    - AsyncTCP             (ESP32Async)
 * ============================================================
 */

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <time.h>
#include <math.h>
#include <esp_system.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

// ============================================================
//  PROTOTYPES — fonctions définies dans web_server_v2.ino
// ============================================================
extern AsyncWebSocket ws;  // déclaré dans web_server_v2.ino
void setupWebServer();
void wsBroadcastState();
void wsLog(const char* msg, const char* type);
void commandeOuvrir();
void commandeFermer();
void resetErreur();
void stopUrgence();
void sauvegarderConfig();
void publishDiscovery();
void publishWifi();
void publishEtat();
void publishMode();
void publishParams();
void paramModifie();
void publishHeuresSoleil();
void traiterFileWeb();
void demanderRedemarrage();

// ============================================================
//  CONFIGURATION
// ============================================================
// Identifiants et position par défaut dans secrets.h (non publié — voir secrets.h.example).
// Ils servent au premier démarrage et de secours ; ils sont ensuite modifiables depuis
// l'onglet « Réglages » de la page web (stockés en NVS, espace « reseau »).
#include "secrets.h"

#define MQTT_ACTIF_DEFAUT  true   // MQTT actif tant qu'il n'est pas désactivé dans les réglages

// Point d'accès de secours (si aucun Wi-Fi n'est joignable au démarrage)
#define AP_SSID  "Poulailler-Config"
#ifndef SECRET_AP_PASSWORD
#define SECRET_AP_PASSWORD  "poulailler"   // WPA2 : 8 caractères minimum
#endif

#define LEN_SSID   33     // 32 caractères + \0
#define LEN_MDP    64     // 63 caractères + \0
#define LEN_HOTE   64
#define LEN_MDP_WEB 33    // 32 caractères max (ASCII imprimable)
#define MDP_WEB_MIN 6

struct ReseauCfg {
  char  wifiSsid[LEN_SSID];
  char  wifiPass[LEN_MDP];
  bool  mqttOn;
  char  mqttHost[LEN_HOTE];
  int   mqttPort;
  char  mqttUser[LEN_SSID];
  char  mqttPass[LEN_MDP];
  float lat, lon;                // position GPS (calcul lever/coucher du soleil)
  char  webPass[LEN_MDP_WEB];    // mot de passe de l'onglet Réglages ("" = aucun)
};
ReseauCfg reseau;                // valeurs courantes (défauts secrets.h puis NVS)

const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";  // France — heure été/hiver automatique // Winter/Summer timer France by default

// ============================================================
//  PINS
// ============================================================
#define PIN_RPWM      25   // BTS7960 — sens ouverture  (droite→gauche)
#define PIN_LPWM      26   // BTS7960 — sens fermeture  (gauche→droite)
#define PIN_EN        27   // BTS7960 — enable (R_EN + L_EN reliés ensemble)

#define PIN_FDC_OUV   32   // MC-38 NF — porte en position ouverte  (gauche)
#define PIN_FDC_FER   33   // MC-38 NF — porte en position fermée   (droite)

#define PIN_ACS712    34   // ACS712 — mesure courant (analogique)

#define PIN_SDA       21   // I2C SDA (BH1750)
#define PIN_SCL       22   // I2C SCL (BH1750)

// ============================================================
//  PARAMÈTRES MOTEUR
// ============================================================
// PWM_CHANNEL_R et PWM_CHANNEL_L non utilisés en API v3.x (ledcAttach par pin)
#define PWM_FREQ        1000
#define PWM_RESOLUTION  8
#define PWM_DUTY_9V     191       // ouverture — ~75% → ~9V sur alim 12V
#define PWM_DUTY_FER    140       // fermeture — ~58% → ~7V (plus doux)

#define TIMEOUT_OUV_MS   15000    // timeout ouverture (cycle ~7s + marge)
#define TIMEOUT_FER_MS   20000    // timeout fermeture (cycle ~10s + marge)

#define ACS712_ZERO_ADC     3050  // Calibré sur le module réel
#define ACS712_SEUIL_ADC    310   // ~2.7A → entre nominal (~2A/delta 246) et blocage (~3.3A/delta 375)
#define SURCOURANT_MASQUAGE_MS    500  // ignore le pic de démarrage après la rampe
#define SURCOURANT_CONFIRMATIONS  3    // mesures consécutives au-dessus du seuil avant arrêt

// ============================================================
//  TOPICS MQTT
// ============================================================
#define MQTT_PREFIX             "poulailler"
#define TOPIC_ETAT_PORTE        MQTT_PREFIX "/porte/etat"
#define TOPIC_COMMANDE_PORTE    MQTT_PREFIX "/porte/commande"
#define TOPIC_LUMINOSITE        MQTT_PREFIX "/capteurs/lux"
#define TOPIC_COURANT           MQTT_PREFIX "/capteurs/courant"
#define TOPIC_HEURE_SOLEIL      MQTT_PREFIX "/soleil/heures"
#define TOPIC_RSSI              MQTT_PREFIX "/wifi/rssi"
#define TOPIC_WIFI              MQTT_PREFIX "/wifi/qualite"
#define TOPIC_MODE              MQTT_PREFIX "/mode"
#define TOPIC_CONFIG            MQTT_PREFIX "/config"       // subscribe — JSON complet

// -- État détaillé + fins de course (publish) --
#define TOPIC_ETAT_TEXTE        MQTT_PREFIX "/porte/etat_texte"
#define TOPIC_FDC_OUV           MQTT_PREFIX "/fdc/ouvert"
#define TOPIC_FDC_FER           MQTT_PREFIX "/fdc/ferme"

// -- Paramètres réglables (set = subscribe, state = publish) --
#define TOPIC_OFFSET_OUV        MQTT_PREFIX "/param/offset_ouv"
#define TOPIC_OFFSET_FER        MQTT_PREFIX "/param/offset_fer"
#define TOPIC_LUX_OUV           MQTT_PREFIX "/param/lux_ouv"
#define TOPIC_LUX_FER           MQTT_PREFIX "/param/lux_fer"
#define TOPIC_HFX_OUV_H         MQTT_PREFIX "/param/hfx_ouv_h"
#define TOPIC_HFX_OUV_M         MQTT_PREFIX "/param/hfx_ouv_m"
#define TOPIC_HFX_FER_H         MQTT_PREFIX "/param/hfx_fer_h"
#define TOPIC_HFX_FER_M         MQTT_PREFIX "/param/hfx_fer_m"
#define TOPIC_LUX_CONF          MQTT_PREFIX "/param/lux_conf"
#define TOPIC_DISPO             MQTT_PREFIX "/status"       // online / offline (LWT)
#define MQTT_CLIENT_ID          "esp32_poulailler"

// Bornes des paramètres réglables (MQTT, page web, NVS)
#define OFFSET_SOLEIL_MIN   -60
#define OFFSET_SOLEIL_MAX   120
#define OFFSET_FIXE_MIN    -120
#define OFFSET_FIXE_MAX     120
#define LUX_MIN               0
#define LUX_MAX           65535
#define LUX_CONF_MIN         10
#define LUX_CONF_MAX        300

// ============================================================
//  TYPES
// ============================================================
enum EtatPorte {
  PORTE_FERMEE,
  PORTE_OUVERTE,
  PORTE_EN_OUVERTURE,
  PORTE_EN_FERMETURE,
  PORTE_ERREUR
};

enum ModeGestion {
  MODE_SOLEIL,       // lever/coucher + offsets
  MODE_LUMINOSITE,   // seuils lux + confirmation
  MODE_HEURE_FIXE,   // heures fixes + offsets
  MODE_MANUEL        // commandes manuelles uniquement
};

struct HeureSoleil {
  int leverH, leverM;
  int coucherH, coucherM;
};

// ============================================================
//  CONFIGURATION DES MODES
// ============================================================

// -- Mode SOLEIL --
int offsetOuv = 15;   // minutes après lever  (peut être négatif)
int offsetFer = 15;   // minutes après coucher (peut être négatif)

// -- Mode LUMINOSITE --
float luxSeuilOuverture   = 800.0;   // ouvre si lux > seuil pendant confirmationSec
float luxSeuilFermeture   = 100.0;   // ferme si lux < seuil pendant confirmationSec
int   luxConfirmationSec  = 60;      // durée de confirmation en secondes

// -- Mode HEURE FIXE --
// Heure de base + offset (offset peut être négatif)
// Heure effective = heureBase + offset minutes
int  heureFixeOuvH  = 7,  heureFixeOuvM  = 30;   // heure de base ouverture
int  offsetFixeOuv  = 0;                           // offset en minutes (−60 à +60)
int  heureFixeFerH  = 20, heureFixeFerM  = 0;     // heure de base fermeture
int  offsetFixeFer  = 0;                           // offset en minutes

// ============================================================
//  VARIABLES GLOBALES
// ============================================================
WiFiClient   espClient;
PubSubClient mqtt(espClient);
BH1750       bh1750;
Preferences  prefs;
EtatPorte   etatPorte   = PORTE_FERMEE;
ModeGestion modeActuel  = MODE_SOLEIL;
ModeGestion modePending = MODE_SOLEIL;  // appliqué après fin de mouvement
bool        modePendingActif = false;

unsigned long tempsDebutMouvement   = 0;
unsigned long dernierPublish        = 0;
unsigned long dernierCheckAuto      = 0;

// Confirmation luminosité
unsigned long debutConditionLux     = 0;
bool          conditionLuxActive    = false;
bool          conditionLuxCible     = false;  // true=ouverture, false=fermeture

// Override manuel temporaire (durée relative : insensible au débordement de millis())
bool          forceManuel          = false;
unsigned long forceManuelDebut     = 0;
unsigned long forceManuelDuree     = 0;

// Ignore les messages MQTT retain au démarrage
unsigned long ignoreRetainUntil     = 0;

// Surintensité : nombre de mesures consécutives au-dessus du seuil
int           compteurSurcourant   = 0;

// Demande de diffusion d'état (posée par la tâche réseau, traitée dans loop())
volatile bool wsBroadcastDemande   = false;

// ============================================================
//  UTILITAIRES
// ============================================================
const char* MODES_STR[] = { "soleil", "luminosite", "heure_fixe", "manuel" };

bool enMouvement() {
  return etatPorte == PORTE_EN_OUVERTURE || etatPorte == PORTE_EN_FERMETURE;
}

const char* etatVersStr(EtatPorte e) {
  switch (e) {
    case PORTE_OUVERTE:      return "open";
    case PORTE_FERMEE:       return "closed";
    case PORTE_EN_OUVERTURE: return "opening";
    case PORTE_EN_FERMETURE: return "closing";
    default:                 return "error";
  }
}

// Convertit un nom de mode ; false si inconnu (le mode courant est alors conservé)
bool strVersMode(const char* s, ModeGestion& out) {
  for (int i = 0; i <= MODE_MANUEL; i++) {
    if (strcmp(s, MODES_STR[i]) == 0) { out = (ModeGestion)i; return true; }
  }
  return false;
}

long borner(long v, long mn, long mx) { return v < mn ? mn : (v > mx ? mx : v); }

// Met à jour un paramètre borné ; retourne true seulement s'il a changé
bool majInt(int& var, long val, long mn, long mx) {
  int v = (int)borner(val, mn, mx);
  if (v == var) return false;
  var = v;
  return true;
}
bool majFloat(float& var, float val, float mn, float mx) {
  float v = val < mn ? mn : (val > mx ? mx : val);
  if (fabsf(v - var) < 0.5f) return false;
  var = v;
  return true;
}

// Vrai si "maintenant" est dans la plage [ouv, fer[ — gère une plage qui passe minuit
bool dansFenetre(int maintenant, int ouv, int fer) {
  if (ouv <= fer) return maintenant >= ouv && maintenant < fer;
  return maintenant >= ouv || maintenant < fer;
}

// Minutes jusqu'à la prochaine occurrence de "cible" (1..1440)
int minutesAvant(int maintenant, int cible) {
  int d = ((cible - maintenant) % 1440 + 1440) % 1440;
  return d == 0 ? 1440 : d;
}

// Heure locale sans attente (getLocalTime() bloque jusqu'à 5 s si l'heure n'est pas synchronisée)
bool heureLocale(struct tm* t) { return getLocalTime(t, 0); }

// ============================================================
//  CALCUL LEVER / COUCHER DU SOLEIL (NOAA simplifié)
// ============================================================
HeureSoleil calculSoleil(int annee, int mois, int jour, float lat, float lon) {
  int a = (14 - mois) / 12;
  int y = annee - a;
  int m = mois + 12 * a - 3;
  long jdn = jour + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 + 1721119;
  float jd  = jdn - 0.5;
  float n   = jd - 2451545.0;
  float L   = fmod(280.460 + 0.9856474 * n, 360.0);
  if (L < 0) L += 360.0;
  float g   = fmod(357.528 + 0.9856003 * n, 360.0) * M_PI / 180.0;
  float lam = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * M_PI / 180.0;
  float eps = 23.439 * M_PI / 180.0;
  float decl = asin(sin(eps) * sin(lam));
  float latR  = lat * M_PI / 180.0;
  float cosH  = (cos(90.833 * M_PI / 180.0) - sin(latR) * sin(decl)) / (cos(latR) * cos(decl));

  HeureSoleil h = {0, 0, 0, 0};
  if (cosH < -1 || cosH > 1) return h;

  float H    = acos(cosH) * 180.0 / M_PI;
  float RA   = atan2(cos(eps) * sin(lam), cos(lam)) * 180.0 / M_PI;
  if (RA < 0) RA += 360.0;
  float EoT  = (L - 0.0057183 - RA) * 4.0;  // minutes — corrigé pour lon négative
  float tNoon    = 12.0 - EoT / 60.0 - lon / 15.0;
  float tLever   = tNoon - H / 15.0;
  float tCoucher = tNoon + H / 15.0;
  // Décalage UTC dynamique (gère heure été/hiver automatiquement)
  time_t maintenant = time(nullptr);
  struct tm loc, utc;
  localtime_r(&maintenant, &loc);
  gmtime_r(&maintenant, &utc);
  int offsetMin = (loc.tm_hour * 60 + loc.tm_min) - (utc.tm_hour * 60 + utc.tm_min);
  if (offsetMin < -720) offsetMin += 1440;
  if (offsetMin >  720) offsetMin -= 1440;
  tLever   += offsetMin / 60.0;
  tCoucher += offsetMin / 60.0;

  // Normaliser dans la plage 0-24h (longitude négative peut donner des valeurs < 0)
  while (tLever   < 0)    tLever   += 24.0;
  while (tLever   >= 24)  tLever   -= 24.0;
  while (tCoucher < 0)    tCoucher += 24.0;
  while (tCoucher >= 24)  tCoucher -= 24.0;

  h.leverH   = (int)tLever;
  h.leverM   = (int)((tLever   - h.leverH)   * 60);
  h.coucherH = (int)tCoucher;
  h.coucherM = (int)((tCoucher - h.coucherH) * 60);
  return h;
}

// Applique un offset en minutes à une heure H:M, retourne en minutes depuis minuit
int heureEnMinutes(int h, int m, int offsetMin) {
  int total = h * 60 + m + offsetMin;
  // Boucler proprement (ex: 00:30 - 45min = 23:45 de la veille → on garde dans journée)
  if (total < 0)    total += 1440;
  if (total >= 1440) total -= 1440;
  return total;
}

// ============================================================
//  MOTEUR
// ============================================================
void stopMoteur() {
  ledcWrite(PIN_RPWM, 0);
  ledcWrite(PIN_LPWM, 0);
}

void stopUrgence() {
  ledcWrite(PIN_RPWM, 0);
  ledcWrite(PIN_LPWM, 0);
  // Relire les FDC pour déterminer l'état réel
  delay(50);
  if      (digitalRead(PIN_FDC_OUV) == LOW) etatPorte = PORTE_OUVERTE;
  else if (digitalRead(PIN_FDC_FER) == LOW) etatPorte = PORTE_FERMEE;
  else                                       etatPorte = PORTE_ERREUR;
  publishEtat();
  wsBroadcastState();
  wsLog("Stop d'urgence — moteur arrêté", "warn");
  Serial.println("[STOP] Arrêt d'urgence");
}


// ============================================================
//  RESET ERREUR — relit les FDC et remet l'état à jour
// ============================================================
void resetErreur() {
  stopMoteur();
  delay(100);
  if      (digitalRead(PIN_FDC_OUV) == LOW)  { etatPorte = PORTE_OUVERTE; Serial.println("[RESET] Porte OUVERTE"); }
  else if (digitalRead(PIN_FDC_FER) == LOW)  { etatPorte = PORTE_FERMEE;  Serial.println("[RESET] Porte FERMÉE");  }
  else                                        { etatPorte = PORTE_ERREUR;  Serial.println("[RESET] Position toujours inconnue"); }
  publishEtat();
  wsBroadcastState();
  wsLog("Reset erreur — état relu depuis les fins de course", "info");
}

// Arrêt sur défaut (timeout, surintensité) — publie l'état complet
void passerEnErreur(const char* raison) {
  stopMoteur();
  etatPorte = PORTE_ERREUR;
  compteurSurcourant = 0;
  Serial.printf("[ERREUR] %s\n", raison);
  publishEtat();
  wsLog(raison, "error");
  wsBroadcastState();
}

// Démarrage progressif — rampe PWM de 0 à PWM_DUTY_9V en RAMPE_MS ms
#define RAMPE_MS  300   // durée de la rampe en millisecondes

void demarrerMoteur(int pinActif, int pinInactif, int dutyMax) {
  ledcWrite(pinInactif, 0);
  int steps = 20;
  for (int i = 1; i <= steps; i++) {
    int duty = (dutyMax * i) / steps;
    ledcWrite(pinActif, duty);
    delay(RAMPE_MS / steps);
  }
}

// Ouverture = porte coulisse de droite vers gauche
void ouvrirPorte() {
  if (digitalRead(PIN_FDC_OUV) == LOW) {
    Serial.println("[MOTEUR] Déjà ouverte (gauche).");
    return;
  }
  if (enMouvement()) {
    Serial.println("[MOTEUR] Mouvement déjà en cours.");
    return;
  }
  Serial.println("[MOTEUR] Ouverture (droite → gauche)...");
  etatPorte = PORTE_EN_OUVERTURE;
  tempsDebutMouvement = millis();
  compteurSurcourant = 0;
  publishEtat();
  demarrerMoteur(PIN_RPWM, PIN_LPWM, PWM_DUTY_9V);
  wsLog("Ouverture déclenchée", "info");
}

// Fermeture = porte coulisse de gauche vers droite
void fermerPorte() {
  if (digitalRead(PIN_FDC_FER) == LOW) {
    Serial.println("[MOTEUR] Déjà fermée (droite).");
    return;
  }
  if (enMouvement()) {
    Serial.println("[MOTEUR] Mouvement déjà en cours.");
    return;
  }
  Serial.println("[MOTEUR] Fermeture (gauche → droite)...");
  etatPorte = PORTE_EN_FERMETURE;
  tempsDebutMouvement = millis();
  compteurSurcourant = 0;
  publishEtat();
  demarrerMoteur(PIN_LPWM, PIN_RPWM, PWM_DUTY_FER);
  wsLog("Fermeture déclenchée", "info");
}

// ============================================================
//  LECTURE ACS712
// ============================================================
float lireCourant() {
  int raw = analogRead(PIN_ACS712);
  return abs(raw - ACS712_ZERO_ADC) / 115.0;
}
bool surcourant() {
  // Moyenne sur 5 lectures pour filtrer le bruit ADC
  long somme = 0;
  for (int i = 0; i < 5; i++) {
    somme += analogRead(PIN_ACS712);
    delay(2);
  }
  int moyenne = somme / 5;
  int delta = abs(moyenne - ACS712_ZERO_ADC);
  return delta > ACS712_SEUIL_ADC;
}

// ============================================================
//  APPLICATION DU MODE PENDING
// ============================================================
void appliquerModePending() {
  if (!modePendingActif) return;
  if (enMouvement()) return;
  modePendingActif = false;
  if (modePending != modeActuel) {
    modeActuel = modePending;
    publishMode();
    sauvegarderConfig();
  }
  conditionLuxActive = false;  // reset confirmation lux
  Serial.printf("[MODE] Appliqué : %s\n", MODES_STR[modeActuel]);
  wsBroadcastState();
}

// Changement de mode (MQTT ou page web) — mis en attente si la porte bouge
void changerMode(ModeGestion nouveau) {
  if (enMouvement()) {
    modePending = nouveau;
    modePendingActif = true;
    Serial.println("[MODE] En mouvement — mode mis en attente");
  } else {
    modePendingActif = false;
    if (nouveau != modeActuel) {   // ne publier/sauver que si le mode change réellement
      modeActuel = nouveau;
      conditionLuxActive = false;
      publishMode();
      sauvegarderConfig();
      Serial.printf("[MODE] Nouveau mode : %s\n", MODES_STR[modeActuel]);
    }
  }
  wsBroadcastState();
}



// ============================================================
//  PERSISTANCE CONFIG (NVS Flash)
// ============================================================
void sauvegarderConfig() {
  prefs.begin("poulailler", false);
  prefs.putInt("offsetOuv",      offsetOuv);
  prefs.putInt("offsetFer",      offsetFer);
  prefs.putFloat("luxOuv",       luxSeuilOuverture);
  prefs.putFloat("luxFer",       luxSeuilFermeture);
  prefs.putInt("luxConf",        luxConfirmationSec);
  prefs.putInt("hfxOuvH",        heureFixeOuvH);
  prefs.putInt("hfxOuvM",        heureFixeOuvM);
  prefs.putInt("hfxOuvOff",      offsetFixeOuv);
  prefs.putInt("hfxFerH",        heureFixeFerH);
  prefs.putInt("hfxFerM",        heureFixeFerM);
  prefs.putInt("hfxFerOff",      offsetFixeFer);
  prefs.putInt("mode",           (int)modeActuel);
  prefs.end();
  Serial.println("[NVS] Config sauvegardée");
}

void chargerConfig() {
  prefs.begin("poulailler", true);  // read-only
  offsetOuv          = prefs.getInt("offsetOuv",   offsetOuv);
  offsetFer          = prefs.getInt("offsetFer",   offsetFer);
  luxSeuilOuverture  = prefs.getFloat("luxOuv",    luxSeuilOuverture);
  luxSeuilFermeture  = prefs.getFloat("luxFer",    luxSeuilFermeture);
  luxConfirmationSec = prefs.getInt("luxConf",     luxConfirmationSec);
  heureFixeOuvH      = prefs.getInt("hfxOuvH",    heureFixeOuvH);
  heureFixeOuvM      = prefs.getInt("hfxOuvM",    heureFixeOuvM);
  offsetFixeOuv      = prefs.getInt("hfxOuvOff",  offsetFixeOuv);
  heureFixeFerH      = prefs.getInt("hfxFerH",    heureFixeFerH);
  heureFixeFerM      = prefs.getInt("hfxFerM",    heureFixeFerM);
  offsetFixeFer      = prefs.getInt("hfxFerOff",  offsetFixeFer);
  int mode           = prefs.getInt("mode", (int)modeActuel);
  prefs.end();

  // Valeurs NVS corrompues ou hors bornes → ramenées dans les limites
  if (mode < MODE_SOLEIL || mode > MODE_MANUEL) mode = MODE_SOLEIL;
  modeActuel = (ModeGestion)mode;
  offsetOuv          = borner(offsetOuv,          OFFSET_SOLEIL_MIN, OFFSET_SOLEIL_MAX);
  offsetFer          = borner(offsetFer,          OFFSET_SOLEIL_MIN, OFFSET_SOLEIL_MAX);
  luxSeuilOuverture  = borner(luxSeuilOuverture,  LUX_MIN, LUX_MAX);
  luxSeuilFermeture  = borner(luxSeuilFermeture,  LUX_MIN, LUX_MAX);
  luxConfirmationSec = borner(luxConfirmationSec, LUX_CONF_MIN, LUX_CONF_MAX);
  heureFixeOuvH      = borner(heureFixeOuvH, 0, 23);
  heureFixeOuvM      = borner(heureFixeOuvM, 0, 59);
  heureFixeFerH      = borner(heureFixeFerH, 0, 23);
  heureFixeFerM      = borner(heureFixeFerM, 0, 59);
  offsetFixeOuv      = borner(offsetFixeOuv, OFFSET_FIXE_MIN, OFFSET_FIXE_MAX);
  offsetFixeFer      = borner(offsetFixeFer, OFFSET_FIXE_MIN, OFFSET_FIXE_MAX);
  Serial.println("[NVS] Config chargée");
}

// Applique les clés présentes d'un JSON de configuration (bornées).
// Retourne true si au moins un paramètre a réellement changé.
bool appliquerConfigJson(JsonDocument& doc) {
  bool modifie = false;
  JsonVariantConst v;
  if (!(v = doc["offset_ouv"]).isNull())  modifie |= majInt(offsetOuv,          v.as<long>(),  OFFSET_SOLEIL_MIN, OFFSET_SOLEIL_MAX);
  if (!(v = doc["offset_fer"]).isNull())  modifie |= majInt(offsetFer,          v.as<long>(),  OFFSET_SOLEIL_MIN, OFFSET_SOLEIL_MAX);
  if (!(v = doc["lux_ouv"]).isNull())     modifie |= majFloat(luxSeuilOuverture, v.as<float>(), LUX_MIN, LUX_MAX);
  if (!(v = doc["lux_fer"]).isNull())     modifie |= majFloat(luxSeuilFermeture, v.as<float>(), LUX_MIN, LUX_MAX);
  if (!(v = doc["lux_conf"]).isNull())    modifie |= majInt(luxConfirmationSec, v.as<long>(),  LUX_CONF_MIN, LUX_CONF_MAX);
  if (!(v = doc["hfx_ouv_h"]).isNull())   modifie |= majInt(heureFixeOuvH,      v.as<long>(),  0, 23);
  if (!(v = doc["hfx_ouv_m"]).isNull())   modifie |= majInt(heureFixeOuvM,      v.as<long>(),  0, 59);
  if (!(v = doc["hfx_ouv_off"]).isNull()) modifie |= majInt(offsetFixeOuv,      v.as<long>(),  OFFSET_FIXE_MIN, OFFSET_FIXE_MAX);
  if (!(v = doc["hfx_fer_h"]).isNull())   modifie |= majInt(heureFixeFerH,      v.as<long>(),  0, 23);
  if (!(v = doc["hfx_fer_m"]).isNull())   modifie |= majInt(heureFixeFerM,      v.as<long>(),  0, 59);
  if (!(v = doc["hfx_fer_off"]).isNull()) modifie |= majInt(offsetFixeFer,      v.as<long>(),  OFFSET_FIXE_MIN, OFFSET_FIXE_MAX);
  return modifie;
}

// ============================================================
//  RÉSEAU / POSITION / MOT DE PASSE (NVS, espace « reseau »)
// ============================================================
// Valeurs d'usine (secrets.h) puis surcharge par ce qui a été enregistré depuis la page web
void chargerReseau() {
  memset(&reseau, 0, sizeof(reseau));
  strlcpy(reseau.wifiSsid, SECRET_WIFI_SSID,     sizeof(reseau.wifiSsid));
  strlcpy(reseau.wifiPass, SECRET_WIFI_PASSWORD, sizeof(reseau.wifiPass));
  reseau.mqttOn   = MQTT_ACTIF_DEFAUT;
  strlcpy(reseau.mqttHost, SECRET_MQTT_SERVER,   sizeof(reseau.mqttHost));
  reseau.mqttPort = 1883;
  strlcpy(reseau.mqttUser, SECRET_MQTT_USER,     sizeof(reseau.mqttUser));
  strlcpy(reseau.mqttPass, SECRET_MQTT_PASS,     sizeof(reseau.mqttPass));
  reseau.lat = SECRET_LATITUDE;
  reseau.lon = SECRET_LONGITUDE;

  if (!prefs.begin("reseau", true)) { Serial.println("[NVS] Réseau : valeurs par défaut (secrets.h)"); return; }
  if (prefs.isKey("wSsid"))  prefs.getString("wSsid", reseau.wifiSsid, sizeof(reseau.wifiSsid));
  if (prefs.isKey("wPass"))  prefs.getString("wPass", reseau.wifiPass, sizeof(reseau.wifiPass));
  if (prefs.isKey("mOn"))    reseau.mqttOn   = prefs.getBool("mOn", reseau.mqttOn);
  if (prefs.isKey("mHost"))  prefs.getString("mHost", reseau.mqttHost, sizeof(reseau.mqttHost));
  if (prefs.isKey("mPort"))  reseau.mqttPort = prefs.getInt("mPort", reseau.mqttPort);
  if (prefs.isKey("mUser"))  prefs.getString("mUser", reseau.mqttUser, sizeof(reseau.mqttUser));
  if (prefs.isKey("mPass"))  prefs.getString("mPass", reseau.mqttPass, sizeof(reseau.mqttPass));
  if (prefs.isKey("lat"))    reseau.lat      = prefs.getFloat("lat", reseau.lat);
  if (prefs.isKey("lon"))    reseau.lon      = prefs.getFloat("lon", reseau.lon);
  if (prefs.isKey("webPass")) prefs.getString("webPass", reseau.webPass, sizeof(reseau.webPass));
  prefs.end();

  // Valeurs NVS corrompues → ramenées dans les limites
  reseau.mqttPort = borner(reseau.mqttPort, 1, 65535);
  if (!(reseau.lat >= -90.0f  && reseau.lat <= 90.0f))   reseau.lat = SECRET_LATITUDE;
  if (!(reseau.lon >= -180.0f && reseau.lon <= 180.0f))  reseau.lon = SECRET_LONGITUDE;
  Serial.println("[NVS] Réseau chargé");
}

void sauvegarderReseau() {
  prefs.begin("reseau", false);
  prefs.putString("wSsid", reseau.wifiSsid);
  prefs.putString("wPass", reseau.wifiPass);
  prefs.putBool("mOn",     reseau.mqttOn);
  prefs.putString("mHost", reseau.mqttHost);
  prefs.putInt("mPort",    reseau.mqttPort);
  prefs.putString("mUser", reseau.mqttUser);
  prefs.putString("mPass", reseau.mqttPass);
  prefs.putFloat("lat",    reseau.lat);
  prefs.putFloat("lon",    reseau.lon);
  prefs.putString("webPass", reseau.webPass);
  prefs.end();
  Serial.println("[NVS] Réseau sauvegardé");
}

// Redémarrage différé : laisse partir la réponse HTTP, et attend que la porte soit immobile
static volatile bool redemarrageDemande = false;
static unsigned long redemarrageInstant = 0;
void demanderRedemarrage() { redemarrageDemande = true; }

void gererRedemarrage() {
  if (!redemarrageDemande || enMouvement()) return;
  if (redemarrageInstant == 0) { redemarrageInstant = millis(); return; }
  if (millis() - redemarrageInstant < 1500) return;
  Serial.println("[SYS] Redémarrage demandé");
  Serial.flush();
  ESP.restart();
}

// Applique une configuration validée (appelé depuis loop(), porte immobile)
void appliquerReseau(const ReseauCfg& n) {
  bool wifiChg = strcmp(n.wifiSsid, reseau.wifiSsid) != 0 || strcmp(n.wifiPass, reseau.wifiPass) != 0;
  bool mqttChg = n.mqttOn != reseau.mqttOn || n.mqttPort != reseau.mqttPort
              || strcmp(n.mqttHost, reseau.mqttHost) != 0
              || strcmp(n.mqttUser, reseau.mqttUser) != 0
              || strcmp(n.mqttPass, reseau.mqttPass) != 0;
  bool posChg  = n.lat != reseau.lat || n.lon != reseau.lon;
  bool mdpChg  = strcmp(n.webPass, reseau.webPass) != 0;
  if (!wifiChg && !mqttChg && !posChg && !mdpChg) return;   // rien de nouveau : pas d'écriture flash

  reseau = n;
  sauvegarderReseau();
  if (posChg) {            // position : appliquée tout de suite (recalcul lever/coucher)
    publishHeuresSoleil();
    wsBroadcastState();
    wsLog("Position GPS mise à jour", "ok");
  }
  if (mdpChg) wsLog("Mot de passe des réglages modifié", "ok");
  if (wifiChg || mqttChg) {
    wsLog("Réglages réseau enregistrés — redémarrage", "warn");
    demanderRedemarrage();
  }
}

// File d'une demande de réglages (1 place) : remplie par la tâche réseau, vidée par loop()
static QueueHandle_t fileReseau = nullptr;
void traiterFileReseau() {
  if (fileReseau == nullptr || enMouvement()) return;   // en attente tant que la porte bouge
  ReseauCfg n;
  if (xQueueReceive(fileReseau, &n, 0) == pdTRUE) appliquerReseau(n);
}

// ============================================================
//  WI-FI — connexion, repli et point d'accès de secours
// ============================================================
static volatile bool apActif = false;
static char wifiSsidActif[LEN_SSID] = "";   // identifiants qui ont réellement fonctionné
static char wifiPassActif[LEN_MDP]  = "";

bool essayerWifi(const char* ssid, const char* pass) {
  if (ssid[0] == '\0') return false;
  Serial.printf("[WiFi] Connexion à %s...", ssid);
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 30) { delay(500); Serial.print("."); t++; }
  if (WiFi.status() != WL_CONNECTED) { Serial.println(" échec"); return false; }
  Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  strlcpy(wifiSsidActif, ssid, sizeof(wifiSsidActif));
  strlcpy(wifiPassActif, pass, sizeof(wifiPassActif));
  return true;
}

void demarrerPointAcces() {
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP(AP_SSID, SECRET_AP_PASSWORD)) {
    apActif = true;
    Serial.printf("[WiFi] Point d'accès « %s » — http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[WiFi] Point d'accès : échec");
  }
}

// Démarrage : Wi-Fi des réglages → Wi-Fi de secrets.h → point d'accès « Poulailler-Config »
void connecterWifi() {
  if (essayerWifi(reseau.wifiSsid, reseau.wifiPass)) return;
  bool memes = strcmp(reseau.wifiSsid, SECRET_WIFI_SSID) == 0 && strcmp(reseau.wifiPass, SECRET_WIFI_PASSWORD) == 0;
  if (!memes) {
    Serial.println("[WiFi] Réglages injoignables — essai avec secrets.h");
    if (essayerWifi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD)) return;
  }
  Serial.println("[WiFi] Échec — mode hors ligne");
  demarrerPointAcces();
}

// loop() : coupe le point d'accès dès que le Wi-Fi est revenu, sinon retente de temps en temps
void surveillerWifi() {
  static unsigned long dernierEssai = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (apActif) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apActif = false;
      Serial.println("[WiFi] Wi-Fi rétabli — point d'accès coupé");
    }
    return;
  }
  if (enMouvement()) return;
  unsigned long delai = apActif ? 300000UL : 60000UL;   // 5 min avec le point d'accès (évite de le perturber)
  if (millis() - dernierEssai < delai) return;
  dernierEssai = millis();
  const char* ssid = wifiSsidActif[0] ? wifiSsidActif : reseau.wifiSsid;
  const char* pass = wifiSsidActif[0] ? wifiPassActif : reseau.wifiPass;
  Serial.printf("[WiFi] Nouvelle tentative vers %s\n", ssid);
  WiFi.begin(ssid, pass);   // non bloquant
}

// ============================================================
//  OVERRIDE MANUEL — suspend l'auto jusqu'à la prochaine échéance
// ============================================================
void activerForceManuel() {
  if (modeActuel == MODE_MANUEL) return;  // inutile en mode manuel

  // Calculer la durée jusqu'à la prochaine échéance (max 12h)
  unsigned long dureeMs = 12UL * 3600UL * 1000UL;  // fallback 12h

  if (modeActuel == MODE_LUMINOSITE) {
    dureeMs = 30UL * 60UL * 1000UL;  // luminosité : suspension de 30 minutes
  } else {
    struct tm t;
    if (heureLocale(&t)) {
      int maintenant = t.tm_hour * 60 + t.tm_min;
      int heureOuv, heureFer;
      if (modeActuel == MODE_SOLEIL) {
        HeureSoleil s = calculSoleil(t.tm_year+1900, t.tm_mon+1, t.tm_mday, reseau.lat, reseau.lon);
        heureOuv = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
        heureFer = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);
      } else {
        heureOuv = heureEnMinutes(heureFixeOuvH, heureFixeOuvM, offsetFixeOuv);
        heureFer = heureEnMinutes(heureFixeFerH, heureFixeFerM, offsetFixeFer);
      }
      // Prochaine échéance = la plus proche des deux (fonctionne même si la plage passe minuit)
      int delta = min(minutesAvant(maintenant, heureOuv), minutesAvant(maintenant, heureFer));
      dureeMs = (unsigned long)delta * 60UL * 1000UL;
    }
  }

  forceManuel = true;
  forceManuelDebut = millis();
  forceManuelDuree = dureeMs;

  char msg[64];
  snprintf(msg, sizeof(msg), "Mode auto suspendu jusqu'à la prochaine échéance");
  Serial.println(msg);
  wsLog(msg, "info");
}

// ============================================================
//  GESTION AUTOMATIQUE
// ============================================================

// Retourne true si l'override manuel est toujours actif (et le lève s'il a expiré)
bool overrideActif() {
  if (forceManuel && millis() - forceManuelDebut >= forceManuelDuree) {
    forceManuel = false;
    Serial.println("[AUTO] Override manuel expiré — reprise automatique");
    wsLog("Reprise automatique", "info");
    wsBroadcastState();
  }
  return forceManuel;
}

// Ouvre ou ferme selon la plage horaire [heureOuv, heureFer[
void appliquerPlage(int maintenant, int heureOuv, int heureFer) {
  bool doitEtreOuverte = dansFenetre(maintenant, heureOuv, heureFer);
  if (doitEtreOuverte  && etatPorte == PORTE_FERMEE) ouvrirPorte();
  if (!doitEtreOuverte && etatPorte == PORTE_OUVERTE) fermerPorte();
}

// ── Mode SOLEIL ──────────────────────────────────────────────
void gestionSoleil() {
  if (overrideActif()) return;
  struct tm t; if (!heureLocale(&t)) return;
  HeureSoleil s = calculSoleil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, reseau.lat, reseau.lon);
  appliquerPlage(t.tm_hour * 60 + t.tm_min,
                 heureEnMinutes(s.leverH,   s.leverM,   offsetOuv),
                 heureEnMinutes(s.coucherH, s.coucherM, offsetFer));
}

// ── Mode LUMINOSITE ──────────────────────────────────────────
void gestionLuminosite() {
  if (overrideActif()) return;

  float lux = bh1750.readLightLevel();

  // Déterminer quelle condition est active
  bool devraitOuvrir = (lux > luxSeuilOuverture && etatPorte == PORTE_FERMEE);
  bool devraitFermer = (lux < luxSeuilFermeture && etatPorte == PORTE_OUVERTE);

  if (!devraitOuvrir && !devraitFermer) {
    // Plus de condition — reset timer
    conditionLuxActive = false;
    debutConditionLux  = 0;
    return;
  }

  bool cibleActuelle = devraitOuvrir; // true=ouverture, false=fermeture

  if (!conditionLuxActive || conditionLuxCible != cibleActuelle) {
    // Nouvelle condition → démarrer le timer
    conditionLuxActive = true;
    conditionLuxCible  = cibleActuelle;
    debutConditionLux  = millis();
    Serial.printf("[LUX] Condition %s — confirmation dans %ds (%.0f lux)\n",
      cibleActuelle ? "ouverture" : "fermeture", luxConfirmationSec, lux);
    return;
  }

  // Condition stable depuis assez longtemps ?
  if (millis() - debutConditionLux >= (unsigned long)luxConfirmationSec * 1000) {
    conditionLuxActive = false;
    if (cibleActuelle) {
      Serial.printf("[LUX] Ouverture confirmée (%.0f lux > %.0f)\n", lux, luxSeuilOuverture);
      wsLog("Ouverture — luminosité confirmée", "info");
      ouvrirPorte();
    } else {
      Serial.printf("[LUX] Fermeture confirmée (%.0f lux < %.0f)\n", lux, luxSeuilFermeture);
      wsLog("Fermeture — luminosité confirmée", "info");
      fermerPorte();
    }
  }
}

// ── Mode HEURE FIXE ──────────────────────────────────────────
void gestionHeureFixe() {
  if (overrideActif()) return;
  struct tm t; if (!heureLocale(&t)) return;
  appliquerPlage(t.tm_hour * 60 + t.tm_min,
                 heureEnMinutes(heureFixeOuvH, heureFixeOuvM, offsetFixeOuv),
                 heureEnMinutes(heureFixeFerH, heureFixeFerM, offsetFixeFer));
}


// ============================================================
//  MONITORING WIFI
// ============================================================
// Convertit le RSSI (dBm) en qualité (%)
int wifiQualite(int rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50)  return 100;
  return 2 * (rssi + 100);
}

// Texte descriptif de la qualité du signal
const char* wifiQualiteTexte(int rssi) {
  if (rssi >= -50) return "excellent";
  if (rssi >= -60) return "tres bon";
  if (rssi >= -70) return "bon";
  if (rssi >= -80) return "faible";
  return "tres faible";
}

void publishWifi() {
  if (!reseau.mqttOn) return;
  if (WiFi.status() != WL_CONNECTED) return;
  int rssi = WiFi.RSSI();
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", rssi);
  mqtt.publish(TOPIC_RSSI, buf);
  snprintf(buf, sizeof(buf), "%d", wifiQualite(rssi));
  mqtt.publish(TOPIC_WIFI, buf);
}

// ============================================================
//  MQTT
// ============================================================
void publishEtat() {
  if (!reseau.mqttOn || !mqtt.connected()) return;
  mqtt.publish(TOPIC_ETAT_PORTE, etatVersStr(etatPorte), true);

  // État en texte lisible (pour capteur HA)
  const char* txt;
  switch (etatPorte) {
    case PORTE_OUVERTE:        txt = "Ouverte";      break;
    case PORTE_FERMEE:         txt = "Fermee";       break;
    case PORTE_EN_OUVERTURE:   txt = "Ouverture...";  break;
    case PORTE_EN_FERMETURE:   txt = "Fermeture...";  break;
    default:                   txt = "Erreur";       break;
  }
  mqtt.publish(TOPIC_ETAT_TEXTE, txt, true);

  // État des fins de course (ON = actif = aimant présent = LOW)
  mqtt.publish(TOPIC_FDC_OUV, digitalRead(PIN_FDC_OUV) == LOW ? "ON" : "OFF", true);
  mqtt.publish(TOPIC_FDC_FER, digitalRead(PIN_FDC_FER) == LOW ? "ON" : "OFF", true);
}

// Publie l'état de tous les paramètres réglables (pour les entités number HA)
void publishParams() {
  if (!reseau.mqttOn) return;
  char b[12];
  snprintf(b, sizeof(b), "%d", offsetOuv);           mqtt.publish(TOPIC_OFFSET_OUV, b, true);
  snprintf(b, sizeof(b), "%d", offsetFer);           mqtt.publish(TOPIC_OFFSET_FER, b, true);
  snprintf(b, sizeof(b), "%.0f", luxSeuilOuverture); mqtt.publish(TOPIC_LUX_OUV, b, true);
  snprintf(b, sizeof(b), "%.0f", luxSeuilFermeture); mqtt.publish(TOPIC_LUX_FER, b, true);
  snprintf(b, sizeof(b), "%d", heureFixeOuvH);       mqtt.publish(TOPIC_HFX_OUV_H, b, true);
  snprintf(b, sizeof(b), "%d", heureFixeOuvM);       mqtt.publish(TOPIC_HFX_OUV_M, b, true);
  snprintf(b, sizeof(b), "%d", heureFixeFerH);       mqtt.publish(TOPIC_HFX_FER_H, b, true);
  snprintf(b, sizeof(b), "%d", heureFixeFerM);       mqtt.publish(TOPIC_HFX_FER_M, b, true);
  snprintf(b, sizeof(b), "%d", luxConfirmationSec);  mqtt.publish(TOPIC_LUX_CONF,  b, true);
}

void publishMode() {
  if (!reseau.mqttOn) return;
  mqtt.publish(TOPIC_MODE, MODES_STR[modeActuel], false);  // pas de retain — évite les boucles
}

void publishCapteurs() {
  if (!reseau.mqttOn) return;
  float lux = bh1750.readLightLevel();
  float amp = lireCourant();
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f", lux);
  mqtt.publish(TOPIC_LUMINOSITE, buf);
  snprintf(buf, sizeof(buf), "%.2f", amp);
  mqtt.publish(TOPIC_COURANT, buf);
}

void publishHeuresSoleil() {
  if (!reseau.mqttOn) return;
  struct tm t; if (!heureLocale(&t)) return;
  HeureSoleil s = calculSoleil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, reseau.lat, reseau.lon);
  int ouvMin = heureEnMinutes(s.leverH,   s.leverM,   offsetOuv);
  int ferMin = heureEnMinutes(s.coucherH, s.coucherM, offsetFer);
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"lever\":\"%02d:%02d\",\"coucher\":\"%02d:%02d\","
    "\"ouverture\":\"%02d:%02d\",\"fermeture\":\"%02d:%02d\"}",
    s.leverH, s.leverM, s.coucherH, s.coucherM,
    ouvMin / 60, ouvMin % 60, ferMin / 60, ferMin % 60);
  mqtt.publish(TOPIC_HEURE_SOLEIL, buf, true);
}


// ============================================================
//  COMMANDES MANUELLES (depuis MQTT ou WebSocket)
// ============================================================
void commandeOuvrir() {
  activerForceManuel();
  ouvrirPorte();
}

void commandeFermer() {
  activerForceManuel();
  fermerPorte();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[256];
  unsigned int n = length < sizeof(msg) - 1 ? length : sizeof(msg) - 1;
  memcpy(msg, payload, n);
  msg[n] = 0;
  String s(msg);
  s.trim();
  Serial.printf("[MQTT] %s → %s\n", topic, s.c_str());

  // Ignorer les retain au démarrage (mode + paramètres) pour éviter
  // que les valeurs retain HA n'écrasent celles chargées depuis la NVS.
  // Les commandes porte (OPEN/CLOSE/STOP/RESET) ne sont jamais ignorées.
  bool estCommande = strcmp(topic, TOPIC_COMMANDE_PORTE) == 0;
  if (millis() < ignoreRetainUntil && !estCommande) {
    Serial.println("[MQTT] Retain ignoré (démarrage)");
    return;
  }

  if (estCommande) {
    if      (s == "OPEN")  commandeOuvrir();
    else if (s == "CLOSE") commandeFermer();
    else if (s == "STOP")  stopUrgence();
    else if (s == "RESET") resetErreur();
    return;
  }

  if (strcmp(topic, TOPIC_MODE) == 0) {
    // Notre propre publication revient aussi ici : changerMode() ignore un mode inchangé
    ModeGestion nouveau;
    if (strVersMode(s.c_str(), nouveau)) changerMode(nouveau);
    else Serial.println("[MODE] Mode inconnu ignoré");
    return;
  }

  if (strcmp(topic, TOPIC_CONFIG) == 0) {
    // JSON de configuration complet
    // ex: {"offset_ouv":15,"offset_fer":-10,"lux_ouv":800,"lux_fer":100,"lux_conf":60,
    //       "hfx_ouv_h":7,"hfx_ouv_m":30,"hfx_ouv_off":0,
    //       "hfx_fer_h":20,"hfx_fer_m":0,"hfx_fer_off":0}
    JsonDocument doc;
    if (deserializeJson(doc, msg) == DeserializationError::Ok && appliquerConfigJson(doc))
      paramModifie();
    return;
  }

  // ── Paramètres individuels (entités number HA) ──────────────
  // L'ESP32 republie ces topics (retain) et les reçoit en retour : on ne
  // réagit que si la valeur change, sinon boucle infinie publication/réception.
  bool modifie = false;
  if      (strcmp(topic, TOPIC_OFFSET_OUV) == 0) modifie = majInt(offsetOuv,          s.toInt(),   OFFSET_SOLEIL_MIN, OFFSET_SOLEIL_MAX);
  else if (strcmp(topic, TOPIC_OFFSET_FER) == 0) modifie = majInt(offsetFer,          s.toInt(),   OFFSET_SOLEIL_MIN, OFFSET_SOLEIL_MAX);
  else if (strcmp(topic, TOPIC_LUX_OUV)    == 0) modifie = majFloat(luxSeuilOuverture, s.toFloat(), LUX_MIN, LUX_MAX);
  else if (strcmp(topic, TOPIC_LUX_FER)    == 0) modifie = majFloat(luxSeuilFermeture, s.toFloat(), LUX_MIN, LUX_MAX);
  else if (strcmp(topic, TOPIC_LUX_CONF)   == 0) modifie = majInt(luxConfirmationSec, s.toInt(),   LUX_CONF_MIN, LUX_CONF_MAX);
  else if (strcmp(topic, TOPIC_HFX_OUV_H)  == 0) modifie = majInt(heureFixeOuvH,      s.toInt(),   0, 23);
  else if (strcmp(topic, TOPIC_HFX_OUV_M)  == 0) modifie = majInt(heureFixeOuvM,      s.toInt(),   0, 59);
  else if (strcmp(topic, TOPIC_HFX_FER_H)  == 0) modifie = majInt(heureFixeFerH,      s.toInt(),   0, 23);
  else if (strcmp(topic, TOPIC_HFX_FER_M)  == 0) modifie = majInt(heureFixeFerM,      s.toInt(),   0, 59);
  if (modifie) paramModifie();
}

// Appelé après modification d'un paramètre : sauvegarde + republie état
void paramModifie() {
  sauvegarderConfig();
  publishParams();
  publishHeuresSoleil();
  wsBroadcastState();
}

bool connecterMQTT() {
  if (!reseau.mqttOn) return true;
  if (mqtt.connected()) return true;
  Serial.print("[MQTT] Connexion...");
  // LWT sur un topic de disponibilité dédié (et non sur l'état de la porte)
  if (mqtt.connect(MQTT_CLIENT_ID,
                   reseau.mqttUser[0] ? reseau.mqttUser : nullptr,
                   reseau.mqttUser[0] ? reseau.mqttPass : nullptr,
                   TOPIC_DISPO, 1, true, "offline")) {
    Serial.println(" OK");
    mqtt.subscribe(TOPIC_COMMANDE_PORTE);
    mqtt.subscribe(TOPIC_MODE);
    mqtt.subscribe(TOPIC_CONFIG);
    mqtt.subscribe(TOPIC_OFFSET_OUV);
    mqtt.subscribe(TOPIC_OFFSET_FER);
    mqtt.subscribe(TOPIC_LUX_OUV);
    mqtt.subscribe(TOPIC_LUX_FER);
    mqtt.subscribe(TOPIC_LUX_CONF);
    mqtt.subscribe(TOPIC_HFX_OUV_H);
    mqtt.subscribe(TOPIC_HFX_OUV_M);
    mqtt.subscribe(TOPIC_HFX_FER_H);
    mqtt.subscribe(TOPIC_HFX_FER_M);
    publishDiscovery();      // auto-déclaration des entités HA
    mqtt.publish(TOPIC_DISPO, "online", true);
    publishEtat();
    publishMode();
    publishParams();
    publishWifi();
    publishHeuresSoleil();
    ignoreRetainUntil = millis() + 3000;  // ignorer les retain pendant 3s
    return true;
  }
  Serial.printf(" Échec (%d)\n", mqtt.state());
  return false;
}


// ============================================================
//  MQTT DISCOVERY — Auto-déclaration des entités Home Assistant
// ============================================================
// Publie la config sur homeassistant/<type>/poulailler/<id>/config (retain)
// HA crée automatiquement les entités. Voir docs MQTT Discovery.

#define HA_PREFIX  "homeassistant"
#define HA_COMMUN  "\"availability_topic\":\"" TOPIC_DISPO "\"," \
                   "\"device\":{\"identifiers\":[\"esp32_poulailler\"],\"name\":\"Porte Poulailler\",\"model\":\"ESP32 Retrofit Omlet\",\"manufacturer\":\"DIY\"}"

// Publie une entité : "corps" = champs JSON propres à l'entité (sans accolades)
bool publierEntite(const char* type, const char* id, const char* corps) {
  char topic[96];
  char buf[768];
  snprintf(topic, sizeof(topic), HA_PREFIX "/%s/poulailler/%s/config", type, id);
  int n = snprintf(buf, sizeof(buf), "{%s," HA_COMMUN "}", corps);
  if (n >= (int)sizeof(buf)) {
    Serial.printf("[MQTT] Discovery %s trop long (%d)\n", id, n);
    return false;
  }
  return mqtt.publish(topic, buf, true);
}

// Entité number réglable (command_topic == state_topic)
#define NUMBER(nom, uid, topic, bornes) \
  "\"name\":\"" nom "\",\"unique_id\":\"poulailler_" uid "\"," \
  "\"command_topic\":\"" topic "\",\"state_topic\":\"" topic "\"," bornes

void publishDiscovery() {
  if (!reseau.mqttOn) return;
  bool ok = true;

  ok &= publierEntite("cover", "porte",
    "\"name\":\"Porte\",\"unique_id\":\"poulailler_porte\",\"device_class\":\"door\","
    "\"command_topic\":\"" TOPIC_COMMANDE_PORTE "\",\"state_topic\":\"" TOPIC_ETAT_PORTE "\","
    "\"payload_open\":\"OPEN\",\"payload_close\":\"CLOSE\",\"payload_stop\":\"STOP\","
    "\"state_open\":\"open\",\"state_closed\":\"closed\","
    "\"state_opening\":\"opening\",\"state_closing\":\"closing\"");

  ok &= publierEntite("select", "mode",
    "\"name\":\"Mode\",\"unique_id\":\"poulailler_mode\","
    "\"command_topic\":\"" TOPIC_MODE "\",\"state_topic\":\"" TOPIC_MODE "\","
    "\"options\":[\"soleil\",\"luminosite\",\"heure_fixe\",\"manuel\"]");

  ok &= publierEntite("sensor", "lux",
    "\"name\":\"Luminosite\",\"unique_id\":\"poulailler_lux\","
    "\"state_topic\":\"" TOPIC_LUMINOSITE "\","
    "\"unit_of_measurement\":\"lx\",\"device_class\":\"illuminance\"");

  ok &= publierEntite("sensor", "courant",
    "\"name\":\"Courant moteur\",\"unique_id\":\"poulailler_courant\","
    "\"state_topic\":\"" TOPIC_COURANT "\","
    "\"unit_of_measurement\":\"A\",\"device_class\":\"current\"");

  ok &= publierEntite("sensor", "soleil",
    "\"name\":\"Heures soleil\",\"unique_id\":\"poulailler_soleil\","
    "\"state_topic\":\"" TOPIC_HEURE_SOLEIL "\","
    "\"value_template\":\"{{ value_json.ouverture }} / {{ value_json.fermeture }}\","
    "\"json_attributes_topic\":\"" TOPIC_HEURE_SOLEIL "\",\"icon\":\"mdi:weather-sunset\"");

  ok &= publierEntite("sensor", "rssi",
    "\"name\":\"WiFi RSSI\",\"unique_id\":\"poulailler_rssi\","
    "\"state_topic\":\"" TOPIC_RSSI "\","
    "\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\","
    "\"entity_category\":\"diagnostic\"");

  ok &= publierEntite("sensor", "wifi",
    "\"name\":\"WiFi qualite\",\"unique_id\":\"poulailler_wifi\","
    "\"state_topic\":\"" TOPIC_WIFI "\","
    "\"unit_of_measurement\":\"%\",\"icon\":\"mdi:wifi\","
    "\"entity_category\":\"diagnostic\"");

  ok &= publierEntite("sensor", "etat",
    "\"name\":\"Etat porte\",\"unique_id\":\"poulailler_etat\","
    "\"state_topic\":\"" TOPIC_ETAT_TEXTE "\",\"icon\":\"mdi:door\"");

  ok &= publierEntite("binary_sensor", "fdc_ouv",
    "\"name\":\"Fin de course ouvert\",\"unique_id\":\"poulailler_fdc_ouv\","
    "\"state_topic\":\"" TOPIC_FDC_OUV "\","
    "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:arrow-left-bold\","
    "\"entity_category\":\"diagnostic\"");

  ok &= publierEntite("binary_sensor", "fdc_fer",
    "\"name\":\"Fin de course ferme\",\"unique_id\":\"poulailler_fdc_fer\","
    "\"state_topic\":\"" TOPIC_FDC_FER "\","
    "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"icon\":\"mdi:arrow-right-bold\","
    "\"entity_category\":\"diagnostic\"");

  ok &= publierEntite("number", "offset_ouv", NUMBER("Offset ouverture", "offset_ouv", TOPIC_OFFSET_OUV,
    "\"min\":-60,\"max\":120,\"step\":1,\"unit_of_measurement\":\"min\","
    "\"mode\":\"slider\",\"icon\":\"mdi:weather-sunset-up\""));

  ok &= publierEntite("number", "offset_fer", NUMBER("Offset fermeture", "offset_fer", TOPIC_OFFSET_FER,
    "\"min\":-60,\"max\":120,\"step\":1,\"unit_of_measurement\":\"min\","
    "\"mode\":\"slider\",\"icon\":\"mdi:weather-sunset-down\""));

  ok &= publierEntite("number", "lux_ouv", NUMBER("Seuil lux ouverture", "lux_ouv", TOPIC_LUX_OUV,
    "\"min\":0,\"max\":2000,\"step\":10,\"unit_of_measurement\":\"lx\","
    "\"mode\":\"box\",\"icon\":\"mdi:brightness-6\""));

  ok &= publierEntite("number", "lux_fer", NUMBER("Seuil lux fermeture", "lux_fer", TOPIC_LUX_FER,
    "\"min\":0,\"max\":2000,\"step\":10,\"unit_of_measurement\":\"lx\","
    "\"mode\":\"box\",\"icon\":\"mdi:brightness-4\""));

  ok &= publierEntite("number", "lux_conf", NUMBER("Confirmation luminosite", "lux_conf", TOPIC_LUX_CONF,
    "\"min\":10,\"max\":300,\"step\":10,\"unit_of_measurement\":\"s\","
    "\"mode\":\"box\",\"icon\":\"mdi:timer-outline\""));

  ok &= publierEntite("number", "hfx_ouv_h", NUMBER("Heure fixe ouverture (h)", "hfx_ouv_h", TOPIC_HFX_OUV_H,
    "\"min\":0,\"max\":23,\"step\":1,\"mode\":\"box\",\"icon\":\"mdi:clock-outline\""));

  ok &= publierEntite("number", "hfx_ouv_m", NUMBER("Minute fixe ouverture", "hfx_ouv_m", TOPIC_HFX_OUV_M,
    "\"min\":0,\"max\":59,\"step\":1,\"mode\":\"box\",\"icon\":\"mdi:clock-outline\""));

  ok &= publierEntite("number", "hfx_fer_h", NUMBER("Heure fixe fermeture (h)", "hfx_fer_h", TOPIC_HFX_FER_H,
    "\"min\":0,\"max\":23,\"step\":1,\"mode\":\"box\",\"icon\":\"mdi:clock-outline\""));

  ok &= publierEntite("number", "hfx_fer_m", NUMBER("Minute fixe fermeture", "hfx_fer_m", TOPIC_HFX_FER_M,
    "\"min\":0,\"max\":59,\"step\":1,\"mode\":\"box\",\"icon\":\"mdi:clock-outline\""));

  Serial.println(ok ? "[MQTT] Discovery publié" : "[MQTT] Discovery : au moins une entité en échec");
}

// ============================================================
//  WEBSOCKET — déclarations (implémentation dans web_server.ino)
// ============================================================

// ============================================================
//  OTA — Mise à jour sans fil (Over-The-Air)
// ============================================================
void setupOTA() {
  ArduinoOTA.setHostname("poulailler");

  ArduinoOTA.onStart([]() {
    // Couper le moteur par sécurité pendant la mise à jour
    ledcWrite(PIN_RPWM, 0);
    ledcWrite(PIN_LPWM, 0);
    Serial.println("[OTA] Début de la mise à jour");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Terminé");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Erreur %u\n", error);
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Prêt — hostname: poulailler");
}

// ============================================================
//  DIAGNOSTIC SYSTÈME
// ============================================================
#define FIRMWARE_VERSION  __DATE__ " " __TIME__   // date de compilation

const char* causeRedemarrage() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "Mise sous tension";
    case ESP_RST_SW:        return "Redémarrage logiciel";
    case ESP_RST_PANIC:     return "Plantage du programme";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "Chien de garde (blocage)";
    case ESP_RST_BROWNOUT:  return "Chute de tension";
    case ESP_RST_EXT:       return "Bouton reset";
    case ESP_RST_DEEPSLEEP: return "Sortie de veille";
    default:                return "Inconnue";
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== PORTE POULAILLER v2 — Démarrage ===");
  Serial.printf("[SYS] Cause du démarrage : %s\n", causeRedemarrage());

  // Moteur
  // ESP32 Arduino v3.x : nouvelle API PWM (ledcAttach remplace ledcSetup+ledcAttachPin)
  ledcAttach(PIN_RPWM, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_LPWM, PWM_FREQ, PWM_RESOLUTION);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, HIGH);
  stopMoteur();

  // Fins de course NF
  pinMode(PIN_FDC_OUV, INPUT_PULLUP);
  pinMode(PIN_FDC_FER, INPUT_PULLUP);

  // ACS712
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // I2C + BH1750
  Wire.begin(PIN_SDA, PIN_SCL);
  bh1750.begin() ? Serial.println("[BH1750] OK") : Serial.println("[BH1750] Non trouvé");

  // Charger la config depuis NVS (mode, offsets, seuils)
  chargerConfig();
  chargerReseau();
  fileReseau = xQueueCreate(1, sizeof(ReseauCfg));

  // État initial — détection position via fins de course
  delay(100);
  if      (digitalRead(PIN_FDC_OUV) == LOW)  { etatPorte = PORTE_OUVERTE; Serial.println("[INIT] Porte OUVERTE (gauche)"); }
  else if (digitalRead(PIN_FDC_FER) == LOW)  { etatPorte = PORTE_FERMEE;  Serial.println("[INIT] Porte FERMÉE (droite)");  }
  else                                        { etatPorte = PORTE_ERREUR;  Serial.println("[INIT] Position inconnue");       }

  // WiFi (réglages enregistrés, puis secrets.h, puis point d'accès de secours)
  connecterWifi();

  // NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", TZ_INFO, 1);
  tzset();
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000))   // attente max 10 s (au lieu de 10 × 6 s)
    Serial.printf("[NTP] %02d/%02d/%04d %02d:%02d\n",
      timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900,
      timeinfo.tm_hour, timeinfo.tm_min);

  // MQTT
  if (reseau.mqttOn) {
    mqtt.setServer(reseau.mqttHost, reseau.mqttPort);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(1024);  // défaut 256 trop petit : Discovery HA (768) + topic + en-tête
    mqtt.setSocketTimeout(3);  // limite l'attente de réponse du broker
    if (WiFi.status() == WL_CONNECTED) connecterMQTT();
  }

  // Serveur web
  setupWebServer();

  // OTA — mise à jour sans fil
  setupOTA();

  Serial.println("=== Prêt ===");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  // OTA — écoute les demandes de mise à jour
  ArduinoOTA.handle();

  // Commandes reçues de la page web : exécutées ici, pas dans la tâche réseau
  traiterFileWeb();
  traiterFileReseau();
  gererRedemarrage();
  surveillerWifi();

  // MQTT — pas de tentative de reconnexion pendant un mouvement :
  // connect() peut bloquer plusieurs secondes et les fins de course ne seraient plus lues
  if (reseau.mqttOn) {
    if (!mqtt.connected() && !enMouvement() && WiFi.status() == WL_CONNECTED) {
      static unsigned long derniereTentative = 0;
      if (millis() - derniereTentative > 5000) {
        derniereTentative = millis();
        connecterMQTT();
      }
    }
    mqtt.loop();
  }

  // WebSocket
  ws.cleanupClients();
  static unsigned long dernierBroadcast = 0;
  if (wsBroadcastDemande || millis() - dernierBroadcast > 2000) {
    wsBroadcastDemande = false;
    dernierBroadcast = millis();
    wsBroadcastState();
  }

  // ── Fins de course ───────────────────────────────────────
  bool fdcOuv = (digitalRead(PIN_FDC_OUV) == LOW);
  bool fdcFer = (digitalRead(PIN_FDC_FER) == LOW);

  if (etatPorte == PORTE_EN_OUVERTURE && fdcOuv) {
    stopMoteur();
    etatPorte = PORTE_OUVERTE;
    Serial.println("[FDC] Porte ouverte ✓ (position gauche)");
    publishEtat();
    publishHeuresSoleil();
    wsLog("Porte ouverte — fin de course gauche atteint", "ok");
  }
  else if (etatPorte == PORTE_EN_FERMETURE && fdcFer) {
    stopMoteur();
    etatPorte = PORTE_FERMEE;
    Serial.println("[FDC] Porte fermée ✓ (position droite)");
    publishEtat();
    wsLog("Porte fermée — fin de course droit atteint", "ok");
  }

  // ── Sécurité — stopper le moteur si état non-mouvement ────
  if (!enMouvement()) stopMoteur();

  // ── Timeout et surintensité ──────────────────────────────
  if (enMouvement()) {
    unsigned long ecoule  = millis() - tempsDebutMouvement;
    unsigned long timeout = (etatPorte == PORTE_EN_OUVERTURE) ? TIMEOUT_OUV_MS : TIMEOUT_FER_MS;
    if (ecoule > timeout) {
      passerEnErreur("Timeout moteur — fin de course non atteint");
    }
    // Pic de démarrage ignoré, puis plusieurs mesures consécutives exigées
    else if (ecoule > RAMPE_MS + SURCOURANT_MASQUAGE_MS) {
      if (!surcourant())                                        compteurSurcourant = 0;
      else if (++compteurSurcourant >= SURCOURANT_CONFIRMATIONS) passerEnErreur("Surcourant — obstacle détecté, moteur arrêté");
    }
  }

  // Mode demandé pendant un mouvement : appliqué dès que la porte est immobile
  // (fin de course, erreur ou stop)
  if (modePendingActif && !enMouvement()) appliquerModePending();

  // ── Gestion automatique (toutes les 60 s) ────────────────
  if (millis() - dernierCheckAuto > 60000) {
    dernierCheckAuto = millis();
    switch (modeActuel) {
      case MODE_SOLEIL:     gestionSoleil();     break;
      case MODE_HEURE_FIXE: gestionHeureFixe();  break;
      default: break;
    }
  }
  // Mode luminosité : vérification plus fréquente (toutes les 5 s)
  if (modeActuel == MODE_LUMINOSITE) {
    static unsigned long dernierLux = 0;
    if (millis() - dernierLux > 5000) {
      dernierLux = millis();
      gestionLuminosite();
    }
  }

  // ── Publication périodique capteurs (30 s) ───────────────
  if (millis() - dernierPublish > 30000) {
    dernierPublish = millis();
    publishCapteurs();
    publishWifi();
    static unsigned long dernierSoleil = 0;
    if (millis() - dernierSoleil > 3600000) {
      dernierSoleil = millis();
      publishHeuresSoleil();
    }
  }

  delay(10);  // boucle rapide : fins de course lues toutes les ~10-20 ms
}
