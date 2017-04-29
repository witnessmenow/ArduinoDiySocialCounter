/*********************************************************************
 *  A device for fetching and displaying social media stats from     *
 *  the following: Facebook, YouTube, Twitter, Instructables         *
 *  Output is displayed on 4 LED matrix boards                       *
 *                                                                   *
 *  By Brian Lough                                                   *
 *  https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA         *
 *********************************************************************/

#include <FacebookApi.h>
#include <YoutubeApi.h>
#include <TwitterApi.h>
#include <InstructablesApi.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <SPI.h>
#include "LedMatrix.h"

#include "FS.h"

#include <ArduinoJson.h>

#define NUMBER_OF_DEVICES 4
#define CS_PIN D3

// Wiring for MAX7219LedMatrix is as follows
// VCC -> 3.3v
// GND -> Ground
// DIN -> D7 (GPIO13)
// DS -> Defined by CS_PIN above
// CLK -> D5 (GPIO14)

#define CHAR_WIDTH 7
LedMatrix ledMatrix = LedMatrix(NUMBER_OF_DEVICES, CS_PIN);

//------- Replace the following! ------
char ssid[] = "ssid";       // your network SSID (name)
char password[] = "password";  // your network key

// Instructions to get the keys or Tokens can be found on the libraries readme

// Facebook : https://github.com/witnessmenow/arduino-facebook-api
String FACEBOOK_ACCESS_TOKEN = "ACCESS_TOKEN";
#define FACEBOOK_APP_ID "APP_ID"
#define FACEBOOK_APP_SECRET "APP_SECRET"

// Youtube : https://github.com/witnessmenow/arduino-youtube-api
#define YOU_TUBE_API_KEY "API_KEY"

// Twitter : https://github.com/witnessmenow/arduino-twitter-api
#define TWITTER_BEARER_TOKEN "TWITTER_BEARER_TOKEN"


// YouTube Channel ID of the channel you want to check
#define CHANNEL_ID "UCezJOfu7OtqGzd5xrP3q6WA"

//Twitter user name (e.g. @witnessmenow)
#define TWITTER_NAME "witnessmenow"

// Instructable username
#define INSTRUCTABLE_SCREEN_NAME "witnessmenow"


WiFiClientSecure secureClient;
FacebookApi *facebook;
// FacebookApi facebook(secureClient, FACEBOOK_ACCESS_TOKEN);
YoutubeApi youtube(YOU_TUBE_API_KEY, secureClient);
TwitterApi twitter(secureClient);

WiFiClient client;
InstructablesApi instructables(client);

unsigned long display_delay = 20000; //time between api requests (20 seconds)
unsigned long display_due_time;

unsigned long api_delay = 5 * 60000; //time between api requests (5mins)
unsigned long facebook_due_time;
unsigned long youtube_due_time;
unsigned long twitter_due_time;
unsigned long instructable_due_time;

int facebookFriends = 0;
int youtubeSubscribers = 0;
int twitterFollowers = 0;
int instructableFollowers = 0;
int instructableViews = 0;

void setup() {

  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  ledMatrix.init();
  ledMatrix.setCharWidth(CHAR_WIDTH);
  scrollValueAndStop("DIY");

   if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  loadConfig();
  twitter.setBearerToken(TWITTER_BEARER_TOKEN);
  facebook = new FacebookApi(secureClient, FACEBOOK_ACCESS_TOKEN);
}

bool loadConfig() {
  File configFile = SPIFFS.open("/counterConfig.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  FACEBOOK_ACCESS_TOKEN = json["facebookToken"].as<String>();
  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["facebookToken"] = FACEBOOK_ACCESS_TOKEN;

  File configFile = SPIFFS.open("/counterConfig.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

void scrollValueAndStop(String value){
  int lengthToScroll = value.length() * CHAR_WIDTH + 1;
  ledMatrix.setText(value);
  for (int i = 0; i < lengthToScroll; i ++){
    ledMatrix.clear();
    ledMatrix.scrollTextLeft();
    ledMatrix.drawText();
    ledMatrix.commit();
    delay(50);
  }
}

void scrollValueTilPassed(String value){
  int lengthToScroll = (value.length() * CHAR_WIDTH) + (NUMBER_OF_DEVICES * 8);
  ledMatrix.setText(value);
  for (int i = 0; i < lengthToScroll; i ++){
    ledMatrix.clear();
    ledMatrix.scrollTextLeft();
    ledMatrix.drawText();
    ledMatrix.commit();
    delay(50);
  }
}

bool getFacebookFriendsIfDue(){
  if (millis() > facebook_due_time)  {
    String token = facebook->extendAccessToken(FACEBOOK_APP_ID, FACEBOOK_APP_SECRET);
    //Save the newely generated token to config for reload on startup
    if(token != ""){
      FACEBOOK_ACCESS_TOKEN = token;
      saveConfig();
    }
    int friendsCount = facebook->getTotalFriends();
    if(friendsCount >= 0)
    {
      Serial.print("Facebook Friends: ");
      Serial.println(friendsCount);
      facebookFriends = friendsCount;
    } else {
      Serial.println("Error getting friends count");
    }
    facebook_due_time = millis() + api_delay;
    return true;
  }

  return false;
}

bool getYoutubeFriendsIfDue(){
  if (millis() > youtube_due_time)  {

    if(youtube.getChannelStatistics(CHANNEL_ID))
    {
      Serial.print("Youtube Subs: ");
      Serial.println(youtube.channelStats.subscriberCount);
      youtubeSubscribers = youtube.channelStats.subscriberCount;
    } else {
      Serial.println("Error getting YT count");
    }
    youtube_due_time = millis() + api_delay;
    return true;
  }

  return false;
}

bool getTwitterFollowersIfDue(){
  if (millis() > twitter_due_time)  {
    String responseString = twitter.getUserStatistics(TWITTER_NAME);;
    DynamicJsonBuffer jsonBuffer;
    JsonObject& response = jsonBuffer.parseObject(responseString);
    if (response.success() && response.containsKey("followers_count")) {
      twitterFollowers = response["followers_count"].as<int>();
    } else {
      Serial.println("Failed to parse Json for twitter");
    }

    twitter_due_time = millis() + api_delay;
    return true;
  }

  return false;
}

bool getInstructableFollowersIfDue(){
  if (millis() > instructable_due_time)  {
    instructablesAuthorStats stats;
    stats = instructables.getAuthorStats(INSTRUCTABLE_SCREEN_NAME);
    if(stats.error.equals(""))
    {
      instructableFollowers = stats.followersCount;
      instructableViews = stats.views;
    } else {
      Serial.println("Failed to get stats from instructables");
      Serial.println(stats.error);
    }

    instructable_due_time = millis() + api_delay;
    return true;
  }

  return false;
}

int current = -1;
int max = 5;
#define FACEBOOK_INDEX 0
#define YOUTUBE_INDEX 1
#define TWITTER_INDEX 2
#define INSTRUCTABLE_INDEX 3
#define INSTRUCTABLE_VIEWS_INDEX 4
void displayNextData(){
  current++;
  if(current > max) {
    current = 0;
  }

  switch(current){
    case FACEBOOK_INDEX:
      Serial.println("Facebook");
      scrollValueTilPassed("Facebook Friends:");
      scrollValueAndStop(formatData(facebookFriends));
    break;
    case YOUTUBE_INDEX:
      Serial.println("Youtube");
      scrollValueTilPassed("YouTube Subscribers:");
      scrollValueAndStop(formatData(youtubeSubscribers));
    break;
    case TWITTER_INDEX:
      Serial.println("Twitter");
      scrollValueTilPassed("Twitter Followers:");
      scrollValueAndStop(formatData(twitterFollowers));
    break;
    case INSTRUCTABLE_INDEX:
      Serial.println("Instructable");
      scrollValueTilPassed("Instructable Followers:");
      scrollValueAndStop(formatData(instructableFollowers));
    break;
    case INSTRUCTABLE_VIEWS_INDEX:
      Serial.println("Instructable");
      scrollValueTilPassed("Instructable Views:");
      scrollValueAndStop(formatData(instructableViews));
    break;
  }
}

String formatData(int data){
  int beforeDecimal;
  int afterDecimal;
  if(data < 10000) {
    return String(data);
  }
  if(data < 1000000) {
    return String(data/1000) + "K";
  } else {
    return String(data/1000000) + "M";
  }
}

void loop() {

  //Offeset the gathering of data so the the screen can update more often
  if(getFacebookFriendsIfDue()) {
    //Got Facebook
  } else if (getYoutubeFriendsIfDue()) {
    // Got Youtube
  } else if (getTwitterFollowersIfDue()) {
    // Got Twitter
  } else if (getInstructableFollowersIfDue()) {
    // Got Instructables
  }

  //Every 20 seconds switch Datapoint
  if (millis() > display_due_time)  {
    displayNextData();
    display_due_time = millis() + display_delay;
  }
}
