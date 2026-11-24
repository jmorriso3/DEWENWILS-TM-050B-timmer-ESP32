#include <WiFi.h>
#include <WiFiUdp.h>
#include <sys/time.h>

// Set these locally before uploading. Enable cloud control in ECO Plugs.
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_WIFI_PASSWORD";
const char* timerMac = "aa:bb:cc:dd:ee:ff";

WiFiUDP beacons, commands;
IPAddress cloud;
uint16_t port;
uint32_t ecoId, ecoKey, lastSeen, lastClock, requestClock, requestTime, nonce;
uint8_t ecoToken[12], ecoShortId[28];
bool seen=false, relay=false, pending=false, desired=false;
unsigned matches=0;
static uint32_t ecoU32(const uint8_t *p,int i){return uint32_t(p[i])|(uint32_t(p[i+1])<<8)|(uint32_t(p[i+2])<<16)|(uint32_t(p[i+3])<<24);}
static void ecoPut32(uint8_t *p,int i,uint32_t v){for(int k=0;k<4;k++)p[i+k]=(v>>(8*k))&255;}
static String ecoMacText(const uint8_t *p){char b[20];snprintf(b,sizeof(b),"%02x:%02x:%02x:%02x:%02x:%02x",p[0],p[1],p[2],p[3],p[4],p[5]);return String(b);}
static bool ecoPublicCloudAddress(const IPAddress& ip){
  const uint8_t a=ip[0],b=ip[1];
  return a>0&&a<224&&a!=10&&a!=127
    &&!(a==100&&b>=64&&b<=127)&&!(a==169&&b==254)
    &&!(a==172&&b>=16&&b<=31)&&!(a==192&&b==168);
}
static void ecoBuildCommand(uint8_t (&p)[152],uint8_t subtype,uint32_t nonce,bool on){
  ecoPut32(p,0,nonce);
  ecoPut32(p,4,subtype);
  ecoPut32(p,0x0c,ecoId);
  memcpy(p+0x10,ecoToken,sizeof(ecoToken));
  memcpy(p+0x24,ecoShortId,sizeof(ecoShortId));
  ecoPut32(p,0x40,ecoKey);
  ecoPut32(p,0x44,0x12345678);
  const uint8_t mask[4]={9,13,17,19};
  for(int group=0;group<14;group++){
    for(int byte=0;byte<4;byte++){
      p[0x10+4*group+byte]^=p[byte]^uint8_t(group*mask[byte]);
    }
  }
  ecoPut32(p,0x4c,0x6a);
  ecoPut32(p,0x50,on?1:0);
}

void readBeacon() {
  if (beacons.parsePacket() < 272) return;
  uint8_t p[272];
  if (beacons.read(p,sizeof(p)) != sizeof(p)) return;
  if (memcmp(p+5,"\x55\xaa\x55\xaa",4) || memcmp(p+10,"ECO Plugs",9)) return;
  uint8_t key[4];
  memcpy(key,p+0xec,4);
  for (int g=0;g<62;g++) {
    int pos=0x14+4*g;
    if (pos==0xec) continue;
    const uint8_t multipliers[]={2,4,3,1};
    for(int i=0;i<4;i++) p[pos+i]^=key[i]^uint8_t(g*multipliers[i]);
  }
  uint32_t sum=0;
  for(int i=0x14;i<0x10c;i++) sum+=p[i];
  if(sum!=ecoU32(p,0x10c) || !ecoMacText(p+0xe4).equalsIgnoreCase(timerMac)) return;
  uint32_t state=ecoU32(p,0xf0), advertisedPort=ecoU32(p,0x104);
  if(state>1 || advertisedPort==0 || advertisedPort>65535) return;
  cloud=IPAddress(p[0x90],p[0x91],p[0x92],p[0x93]); port=advertisedPort;
  ecoId=ecoU32(p,0x24); ecoKey=ecoU32(p,0x108);
  memcpy(ecoToken,p+0x98,sizeof(ecoToken));
  memset(ecoShortId,0,sizeof(ecoShortId));
  memcpy(ecoShortId,p+0xa4,strnlen((char*)p+0xa4,sizeof(ecoShortId)));
  if(!seen || relay!=bool(state)) Serial.println(state?"Timer reports ON":"Timer reports OFF");
  seen=true; relay=bool(state); lastSeen=millis(); lastClock=ecoU32(p,0x1c);
  if(pending && int32_t(lastClock-requestClock)>0){
    requestClock=lastClock;
    matches=relay==desired?matches+1:0;
    if(matches>=2){pending=false;Serial.println("Relay confirmed");}
  }
}
void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED) delay(100);
  if(!beacons.begin(10228)||!commands.begin(9090)){
    Serial.println("Could not open timer ports"); while(true) delay(1000);
  }
  configTime(0,0,"pool.ntp.org");
  Serial.println("Enter on or off using New Line");
}
void loop(){
  readBeacon();
  if(pending && millis()-requestTime>15000){pending=false;Serial.println("Relay confirmation timed out");}
  if(!Serial.available()) return;
  String action=Serial.readStringUntil('\n');action.trim();action.toLowerCase();
  if(action!="on" && action!="off") return;
  if(!seen||millis()-lastSeen>30000||!ecoPublicCloudAddress(cloud)){
    Serial.println("Waiting for a fresh timer broadcast and cloud endpoint");return;
  }
  timeval t={};gettimeofday(&t,nullptr);
  if(t.tv_sec<1700000000){Serial.println("Waiting for clock sync");return;}
  uint32_t next=uint32_t(uint64_t(t.tv_sec)*1000+t.tv_usec/1000);
  nonce=next==nonce?nonce+1:next;
  desired=action=="on";pending=false;
  uint8_t packet[152]={};ecoBuildCommand(packet,0x66,nonce,desired);
  if(!commands.beginPacket(cloud,port)||commands.write(packet,sizeof(packet))!=sizeof(packet)||!commands.endPacket()){
    Serial.println("Send failed");return;
  }
  requestTime=millis();requestClock=lastClock;matches=0;pending=true;
  Serial.println("Sent; waiting for timer broadcasts");
}
