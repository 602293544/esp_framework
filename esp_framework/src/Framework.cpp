#include "Framework.h"
#include "Module.h"
#include "Rtc.h"
#include "Http.h"
#include "Util.h"

uint16_t Framework::rebootCount = 0;
#ifndef DISABLE_MQTT
void Framework::callback(char *topic, byte *payload, unsigned int length)
{
    Debug::AddInfo(PSTR("Subscribe: %s payload: %.*s"), topic, length, payload);

    char *cmnd = strrchr(topic, '/');
    if (cmnd == nullptr)
    {
        return;
    }
    cmnd++;
    payload[length] = 0;

    if (strcmp(cmnd, "ota") == 0)
    {
        String str = String((char *)payload);
        Http::OTA(str.endsWith(F(".bin")) ? str : OTA_URL);
    }
    else if (strcmp(cmnd, "restart") == 0)
    {
        ESP.reset();
    }
    else if (module)
    {
        module->mqttCallback(topic, (char *)payload, cmnd);
    }

    Led::led(200);
}

void Framework::connectedCallback()
{
    Mqtt::subscribe(Mqtt::getCmndTopic(F("#")));
    Led::blinkLED(40, 8);
    if (module)
    {
        module->mqttConnected();
    }
}
#endif

void Framework::tickerPerSecondDo()
{
    perSecond++;
    if (perSecond == 30)
    {
        Rtc::rtcReboot.fast_reboot_count = 0;
        Rtc::rtcRebootSave();
    }
    if (rebootCount == 3)
    {
        return;
    }
    Rtc::perSecondDo();

    Config::perSecondDo();
#ifndef DISABLE_MQTT
    Mqtt::perSecondDo();
#endif
    module->perSecondDo();
}

void Framework::one(unsigned long baud)
{
    Rtc::rtcRebootLoad();
    Rtc::rtcReboot.fast_reboot_count++;
    Rtc::rtcRebootSave();
    rebootCount = Rtc::rtcReboot.fast_reboot_count > BOOT_LOOP_OFFSET ? Rtc::rtcReboot.fast_reboot_count - BOOT_LOOP_OFFSET : 0;

    Serial.begin(baud);
    globalConfig.debug.type = 1;
}

void Framework::setup()
{
    Debug::AddError(PSTR("---------------------  v%s  %s  -------------------"), module->getModuleVersion().c_str(), Rtc::GetBuildDateAndTime().c_str());
    if (rebootCount == 1)
    {
        Config::readConfig();
        module->resetConfig();
    }
    else if (rebootCount == 2)
    {
        Config::readConfig();
        module->resetConfig();
    }
    else
    {
        Config::readConfig();
    }
    if (globalConfig.uid[0] != '\0')
    {
        strcpy(UID, globalConfig.uid);
    }
    else
    {
        uint8_t mac[6];
        wifi_get_macaddr(STATION_IF, mac);
        sprintf(UID, "%s_%02x%02x%02x", module->getModuleName().c_str(), mac[3], mac[4], mac[5]);
    }
    Util::strlowr(UID);

    Debug::AddInfo(PSTR("UID: %s"), UID);
    // Debug::AddInfo(PSTR("Config Len: %d"), GlobalConfigMessage_size + 6);

    //Config::resetConfig();
    if (MQTT_MAX_PACKET_SIZE == 128)
    {
        Debug::AddError(PSTR("WRONG PUBSUBCLIENT LIBRARY USED PLEASE INSTALL THE ONE FROM OMG LIB FOLDER"));
    }

    if (rebootCount == 3)
    {
        module = NULL;

        tickerPerSecond = new Ticker();
        tickerPerSecond->attach(1, tickerPerSecondDo);

        Http::begin();
        Wifi::connectWifi();
    }
    else
    {
#ifndef DISABLE_MQTT
        Mqtt::setClient(Wifi::wifiClient);
        Mqtt::mqttSetConnectedCallback(connectedCallback);
        Mqtt::mqttSetLoopCallback(callback);
#endif
        module->init();
        tickerPerSecond = new Ticker();
        tickerPerSecond->attach(1, tickerPerSecondDo);
        Http::begin();
        Wifi::connectWifi();
        Rtc::init();
    }
}

void Framework::loop()
{
    if (rebootCount == 3)
    {
        Wifi::loop();
        Http::loop();
    }
    else
    {
        yield();
        Led::loop();
#ifndef DISABLE_MQTT
        yield();
        Mqtt::loop();
#endif
        yield();
        module->loop();
        yield();
        Wifi::loop();
        yield();
        Http::loop();
        yield();
        Rtc::loop();
    }
}
