package com.van.management.utils;

import android.util.Log;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.van.management.data.VanState;

public class VanStateParser {
    private static final Gson gson = new Gson();

    /**
     * Parse le JSON reçu et met à jour l'instance de VanState passée en argument.
     * @param vanState Instance à modifier
     * @param json Chaîne JSON reçue
     */
    public static void parseVanState(VanState vanState, String json) {
        Log.w("parser", String.valueOf(json));
        JsonObject root = JsonParser.parseString(json).getAsJsonObject();
        if (root.has("mppt")) {
            VanState.MpptData mppt = gson.fromJson(root.get("mppt"), VanState.MpptData.class);
            vanState.mppt = mppt;
        }
        if (root.has("alternator_charger")) {
            VanState.AlternatorChargerData alternatorCharger = gson.fromJson(root.get("alternator_charger"), VanState.AlternatorChargerData.class);
            vanState.alternator_charger = alternatorCharger;
        }
        if (root.has("inverter_charger")) {
            VanState.InverterChargerData inverterCharger = gson.fromJson(root.get("inverter_charger"), VanState.InverterChargerData.class);
            vanState.inverter_charger = inverterCharger;
        }
        if (root.has("battery")) {
            VanState.BatteryData battery = gson.fromJson(root.get("battery"), VanState.BatteryData.class);
            vanState.battery = battery;
        }
        if (root.has("sensors")) {
            VanState.SensorData sensors = gson.fromJson(root.get("sensors"), VanState.SensorData.class);
            vanState.sensors = sensors;
        }
        if (root.has("heater")) {
            VanState.HeaterData heater = gson.fromJson(root.get("heater"), VanState.HeaterData.class);
            vanState.heater = heater;
        }
        if (root.has("leds")) {
            JsonObject ledsObj = root.getAsJsonObject("leds");
            VanState.LedData leds = new VanState.LedData();
            if (ledsObj.has("roof1")) {
                leds.leds_roof1 = gson.fromJson(ledsObj.get("roof1"), VanState.LedStrip.class);
            }
            if (ledsObj.has("roof2")) {
                leds.leds_roof2 = gson.fromJson(ledsObj.get("roof2"), VanState.LedStrip.class);
            }
            if (ledsObj.has("av")) {
                leds.leds_av = gson.fromJson(ledsObj.get("av"), VanState.LedStrip.class);
            }
            if (ledsObj.has("ar")) {
                leds.leds_ar = gson.fromJson(ledsObj.get("ar"), VanState.LedStrip.class);
            }
            vanState.leds = leds;
        }
        if (root.has("system")) {
            VanState.SystemData system = gson.fromJson(root.get("system"), VanState.SystemData.class);
            vanState.system = system;
        }
        if (root.has("slave_pcb")) {
            JsonObject slaveObj = root.getAsJsonObject("slave_pcb");
            VanState.SlavePcbState slave = new VanState.SlavePcbState();
            if (slaveObj.has("timestamp")) slave.timestamp = slaveObj.get("timestamp").getAsLong();
            if (slaveObj.has("current_case")) slave.current_case = VanState.SystemCase.values()[slaveObj.get("current_case").getAsInt()];
            if (slaveObj.has("hood_state")) slave.hood_state = VanState.HoodState.values()[slaveObj.get("hood_state").getAsInt()];
            if (slaveObj.has("water_tanks")) {
                slave.tanks_levels = gson.fromJson(slaveObj.get("water_tanks"), VanState.WaterTanksLevels.class);
            }
            if (slaveObj.has("error_state")) {
                JsonObject errObj = slaveObj.getAsJsonObject("error_state");
                VanState.SlaveErrorState errorState = new VanState.SlaveErrorState();
                if (errObj.has("stats")) {
                    errorState.error_stats = gson.fromJson(errObj.get("stats"), VanState.SlaveErrorStats.class);
                }
                if (errObj.has("last_errors")) {
                    errorState.last_errors = gson.fromJson(errObj.get("last_errors"), VanState.SlaveErrorEvent[].class);
                }
                slave.error_state = errorState;
            }
            if (slaveObj.has("system_health")) {
                slave.system_health = gson.fromJson(slaveObj.get("system_health"), VanState.SlaveHealth.class);
            }
            vanState.slave_pcb = slave;
        }
        if (root.has("videoprojecteur")) {
            JsonObject projectorObj = root.getAsJsonObject("videoprojecteur");
            VanState.ProjectorData projector = new VanState.ProjectorData();
            if (projectorObj.has("state")) {
                int stateValue = projectorObj.get("state").getAsInt();
                projector.state = VanState.ProjectorState.fromValue(stateValue);
            }
            if (projectorObj.has("connected")) {
                projector.connected = projectorObj.get("connected").getAsBoolean();
            }
            if (projectorObj.has("last_update_time")) {
                projector.last_update_time = projectorObj.get("last_update_time").getAsLong();
            }
            if (projectorObj.has("position_percent")) {
                projector.position_percent = projectorObj.get("position_percent").getAsFloat();
            }
            vanState.projector = projector;
        }
    }
}
