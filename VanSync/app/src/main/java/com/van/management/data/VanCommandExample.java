package com.van.management.data;

import com.van.management.ble.VanBleService;

/**
* Exemples d'utilisation de VanCommand
* Cette classe montre comment créer différents types de commandes
*/
public class VanCommandExample {

   /**
    * Exemple 1: Allumer toutes les LEDs du toit en blanc
    */

    public static VanCommand createRoof1LED1StripCommand() {
        // Allumer seulement la LED numéro 10 du ROOF1 en blanc
        
        // Créer un tableau de 120 LEDs pour ROOF1 (toutes éteintes sauf une)
        LedData[] roof1Leds = new LedData[120];
        for (int i = 0; i < 120; i++) {
            if (i == 10) {
                // LED allumée en blanc brillant
                roof1Leds[i] = new LedData(0, 0, 0, 10, 100);
            } 
        
            else {
                // LEDs éteintes
                roof1Leds[i] = LedData.off();
            }
        }
        
        // Créer un tableau de 120 LEDs pour ROOF2 (toutes éteintes)
        LedData[] roof2Leds = new LedData[120];
        for (int i = 0; i < 120; i++) {
            roof2Leds[i] = LedData.off();
        }
        
        // Créer la commande pour ROOF_LED1 avec createRoof
        // Même si on cible ROOF_LED1, on doit fournir les 2 tableaux
        LedStaticCommand staticCmd = LedStaticCommand.createRoof(
            LedStaticCommand.StaticTarget.ROOF_LED1,
            roof1Leds,
            roof2Leds
        );
        
        LedCommand ledCmd = LedCommand.createStatic(staticCmd);
        return VanCommand.createLedCommand(ledCmd);
    } 

   public static VanCommand createRoofBlueCommand() {
    // Utiliser jaune tamisé (dim) via le constructeur RGB + brightness
    LedData blue = new LedData(0, 0, 255, 0, 128);
       LedStaticCommand staticCmd = LedStaticCommand.createUniformRoof(
           LedStaticCommand.StaticTarget.ROOF_LED_ALL,
           blue
       );
       LedCommand ledCmd = LedCommand.createStatic(staticCmd);
       return VanCommand.createLedCommand(ledCmd);
   } 

   public static VanCommand createRoofYellowCommand() {
    // Utiliser jaune tamisé (dim) via le constructeur RGB + brightness
    LedData yellow = new LedData(0, 255, 0, 0, 128);
       LedStaticCommand staticCmd = LedStaticCommand.createUniformRoof(
           LedStaticCommand.StaticTarget.ROOF_LED_ALL,
           yellow
       );
       LedCommand ledCmd = LedCommand.createStatic(staticCmd);
       return VanCommand.createLedCommand(ledCmd);
   }

   public static VanCommand createRoofWhiteCommand() {
       LedData white = LedData.white(255);
       LedStaticCommand staticCmd = LedStaticCommand.createUniformRoof(
           LedStaticCommand.StaticTarget.ROOF_LED_ALL,
           white
       );
       LedCommand ledCmd = LedCommand.createStatic(staticCmd);
       return VanCommand.createLedCommand(ledCmd);
   }

   /**
    * Exemple 2: Créer un effet arc-en-ciel qui se déplace sur le toit
    * Les deux strips font la MÊME animation en synchronisation
    * Animation fluide avec 17 keyframes (16 + 1 final) sur 5 secondes
    */
   public static VanCommand createRoofRainbowDynamicCommand() {
       final int numLedsPerStrip = 120;  // Chaque strip a 120 LEDs
       final int numKeyframes = 8;
       final int totalDurationMs = 5000;
       final int brightness = 255;
       
       LedDynamicCommand.Builder builder = new LedDynamicCommand.Builder(
           LedDynamicCommand.DynamicTarget.ROOF_LED_ALL,
           totalDurationMs
       ).setLoopBehavior(LedDynamicCommand.LoopBehavior.PING_PONG);

       // Créer les keyframes + un dernier identique au premier pour boucler sans saut
       for (int k = 0; k <= numKeyframes; k++) {
           int timeMs = (k * totalDurationMs) / numKeyframes;
           int offset = (k * 256) / numKeyframes;  // Offset de 0 à 256
           
           // Les deux strips ont le MÊME pattern (synchronisés)
           LedData[] roof1 = new LedData[numLedsPerStrip];
           LedData[] roof2 = new LedData[numLedsPerStrip];
           
           for (int i = 0; i < numLedsPerStrip; i++) {
               // Même calcul pour les deux strips = animation identique
               int wheelPos = (i * 256 / numLedsPerStrip + offset) & 0xFF;
               LedData color = colorWheel(wheelPos, brightness);
               roof1[i] = color;
               roof2[i] = color;  // Même couleur = synchronisation parfaite
           }
           
           // Ajouter le keyframe
           builder.addKeyframe(LedKeyframe.createRoofAll(
               timeMs,
               LedKeyframe.TransitionMode.LINEAR,
               roof1,
               roof2
           ));
       }

       LedDynamicCommand dynamicCmd = builder.build();
       LedCommand ledCmd = LedCommand.createDynamic(dynamicCmd);
       return VanCommand.createLedCommand(ledCmd);
   }
/**
 * Exemple 2b: Crée une commande statique qui colore les 240 LEDs du toit
 * en un dégradé arc-en-ciel (chaque LED a une couleur fixe différente).
 * ROOF1 = 120 LEDs, ROOF2 = 120 LEDs (total 240 LEDs)
 */
public static VanCommand createRoofRainbowStaticCommand() {
    // Créer le tableau pour toutes les LEDs (240 total)
    LedData[] allLeds = new LedData[240];

    // Remplir avec un arc-en-ciel complet
    for (int i = 0; i < 240; i++) {
        float hue = (i * 360.0f) / 240; // Répartir sur 360 degrés
        allLeds[i] = hsvToLedData(hue, 1.0f, 1.0f, 255);
    }

    // Séparer en roof1 (0-119) et roof2 (120-239)
    LedData[] roof1Leds = new LedData[120];
    LedData[] roof2Leds = new LedData[120];
    System.arraycopy(allLeds, 0, roof1Leds, 0, 120);
    System.arraycopy(allLeds, 120, roof2Leds, 0, 120);

    // Utiliser la méthode createRoof avec les 2 strips séparés
    LedStaticCommand staticCmd = LedStaticCommand.createRoof(
        LedStaticCommand.StaticTarget.ROOF_LED_ALL,
        roof1Leds,
        roof2Leds
    );
    
    LedCommand ledCmd = LedCommand.createStatic(staticCmd);
    return VanCommand.createLedCommand(ledCmd);
}

/**
 * Algorithme color_wheel identique au code C
 * Génère une couleur arc-en-ciel à partir d'une position (0-255)
 * @param pos Position dans la roue des couleurs (0-255)
 * @param brightness Luminosité (0-255)
 * @return LedData avec les couleurs RGB ajustées par la luminosité
 */
private static LedData colorWheel(int pos, int brightness) {
    pos = 255 - pos;
    int r, g, b;
    
    if (pos < 85) {
        r = (255 - pos * 3) * brightness / 255;
        g = 0;
        b = (pos * 3) * brightness / 255;
    } else if (pos < 170) {
        pos -= 85;
        r = 0;
        g = (pos * 3) * brightness / 255;
        b = (255 - pos * 3) * brightness / 255;
    } else {
        pos -= 170;
        r = (pos * 3) * brightness / 255;
        g = (255 - pos * 3) * brightness / 255;
        b = 0;
    }
    
    return new LedData(r, g, b, brightness);
}

/** Convertit HSV en LedData (r,g,b,brightness). */
private static LedData hsvToLedData(float h, float s, float v, int brightness) {
    // h en degrés [0,360)
    float c = v * s;
    float hh = (h / 60.0f) % 6;
    float x = c * (1 - Math.abs(hh % 2 - 1));
    float r1 = 0, g1 = 0, b1 = 0;

    if (0 <= hh && hh < 1) { r1 = c; g1 = x; b1 = 0; }
    else if (1 <= hh && hh < 2) { r1 = x; g1 = c; b1 = 0; }
    else if (2 <= hh && hh < 3) { r1 = 0; g1 = c; b1 = x; }
    else if (3 <= hh && hh < 4) { r1 = 0; g1 = x; b1 = c; }
    else if (4 <= hh && hh < 5) { r1 = x; g1 = 0; b1 = c; }
    else { r1 = c; g1 = 0; b1 = x; }

    float m = v - c;
    int r = Math.round((r1 + m) * 255.0f);
    int g = Math.round((g1 + m) * 255.0f);
    int b = Math.round((b1 + m) * 255.0f);

    return new LedData(r, g, b, brightness);
}

   /**
    * Exemple 3: Allumer les LEDs extérieures en rouge
    */
   public static VanCommand createExtRedCommand() {
       LedData red = new LedData(255, 0, 0, 255);
       LedStaticCommand staticCmd = LedStaticCommand.createUniformExt(
           LedStaticCommand.StaticTarget.EXT_LED_ALL,
           red
       );
       LedCommand ledCmd = LedCommand.createStatic(staticCmd);
       return VanCommand.createLedCommand(ledCmd);
   }

   /**
    * Exemple 4: Éteindre toutes les LEDs du toit
    */
   public static VanCommand createRoofOffCommand() {
       LedData off = LedData.off();
       LedStaticCommand staticCmd = LedStaticCommand.createUniformRoof(
           LedStaticCommand.StaticTarget.ROOF_LED_ALL,
           off
       );
       LedCommand ledCmd = LedCommand.createStatic(staticCmd);
       return VanCommand.createLedCommand(ledCmd);
   }

   /**
    * Exemple 5: Allumer le chauffage
    */
   public static VanCommand createHeaterOnCommand() {
       HeaterCommand heaterCmd = HeaterCommand.turnOn(60.0f, 20.0f);
       return VanCommand.createHeaterCommand(heaterCmd);
   }

   /**
    * Exemple 6: Éteindre le chauffage
    */
   public static VanCommand createHeaterOffCommand() {
       HeaterCommand heaterCmd = HeaterCommand.turnOff();
       return VanCommand.createHeaterCommand(heaterCmd);
   }

   /**
    * Exemple 7: Allumer la hotte
    */
   public static VanCommand createHoodOnCommand() {
       return VanCommand.createHoodCommand(HoodCommand.ON);
   }

   /**
    * Exemple 8: Commande pour le système d'eau - Cas 3
    */
   public static VanCommand createWaterCase3Command() {
       WaterCaseCommand waterCmd = new WaterCaseCommand(WaterCaseCommand.SystemCase.CASE_3);
       return VanCommand.createWaterCaseCommand(waterCmd);
   }

   /**
    * Exemple d'utilisation avec VanBleService
    */
   public static void sendExampleCommand(VanBleService service) {
       // Créer une commande
       VanCommand command = createRoofWhiteCommand();

       // L'envoyer via BLE
       boolean success = service.sendBinaryCommand(command);

       if (success) {
           System.out.println("Commande envoyée: " + command.toString());
       } else {
           System.err.println("Échec d'envoi de la commande");
       }
   }
}
