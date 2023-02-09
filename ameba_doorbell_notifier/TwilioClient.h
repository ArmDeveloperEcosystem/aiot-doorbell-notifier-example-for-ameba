// 
// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: MIT
//

#include <ArduinoHttpClient.h>

// DigiCert Global Root CA
// from: https://cacerts.digicert.com/DigiCertGlobalRootCA.crt.pem
const char DIGICERT_GLOBAL_ROOT_CA[] = R"(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)"; 

class TwilioClient {
  public:
    TwilioClient(WiFiSSLClient& client, const char accountSid[], const char authToken[]) :
      _httpClient(client, "api.twilio.com", 443),
      _accountSid(accountSid),
      _authToken(authToken)
    {
      client.setRootCA((unsigned char*)DIGICERT_GLOBAL_ROOT_CA);
    }

    ~TwilioClient()
    {
    }

    int sendMessage(const char to[], const char from[], const char message[]) {
      String path;
      String body;

      path += "/2010-04-01/Accounts/";
      path += _accountSid;
      path += "/Messages.json";

      body += "To=";
      body += URLEncoder.encode(to);
      body += "&";
      body += "From=";
      body += URLEncoder.encode(from);
      body += "&";
      body += "Body=";
      body += URLEncoder.encode(message);

      _httpClient.beginRequest();
      _httpClient.post(path);
      _httpClient.sendBasicAuth(_accountSid, _authToken);
      _httpClient.sendHeader("Content-Type", "application/x-www-form-urlencoded");
      _httpClient.sendHeader("Content-Length", body.length());

      _httpClient.beginBody();
      _httpClient.print(body);
      _httpClient.endRequest();

      // read the status code and body of the response
      int statusCode = _httpClient.responseStatusCode();
      String response = _httpClient.responseBody();

      _httpClient.stop();

      return (statusCode / 100) == 2;
    }

  private:
    HttpClient _httpClient;
    String _accountSid;
    String _authToken;
};
