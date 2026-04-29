#include "internal_bridge.h"

#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <string>

// ========================================
// CONFIG
// ========================================
static const std::string BASE_URL =
    "https://origin-ai-six.vercel.app";

// ========================================
// CURL HELPERS
// ========================================
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

static std::string EscapeJson(const std::string &input)
{
    std::string out;

    for (char c : input)
    {
        switch (c)
        {
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }

    return out;
}

static std::string ExtractString(const std::string &json, const std::string &key)
{
    std::string token = "\"" + key + "\"";
    size_t pos = json.find(token);

    if (pos == std::string::npos)
        return "";

    pos = json.find(":", pos);
    if (pos == std::string::npos)
        return "";

    pos = json.find("\"", pos);
    if (pos == std::string::npos)
        return "";

    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos)
        return "";

    return json.substr(pos + 1, end - pos - 1);
}

static int ExtractInt(const std::string &json, const std::string &key)
{
    std::string token = "\"" + key + "\"";
    size_t pos = json.find(token);

    if (pos == std::string::npos)
        return 0;

    pos = json.find(":", pos);
    if (pos == std::string::npos)
        return 0;

    pos++;

    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n'))
        pos++;

    std::string number;

    while (pos < json.size() && isdigit(json[pos]))
    {
        number += json[pos];
        pos++;
    }

    if (number.empty())
        return 0;

    return std::stoi(number);
}

// ========================================
// LOGIN
// ========================================
OriginSession AuthenticateOrigin(
    const std::string &email,
    const std::string &password)
{
    OriginSession session{};
    session.isValid = false;
    session.seconds = 0;

    CURL *curl = curl_easy_init();

    if (!curl)
        return session;

    std::string response;

    std::string url = BASE_URL + "/api/session";

    std::string body =
        "{"
        "\"email\":\"" +
        EscapeJson(email) + "\","
                            "\"password\":\"" +
        EscapeJson(password) + "\""
                               "}";

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    CURLcode res = curl_easy_perform(curl);

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    std::cout << "📡 LOGIN RESPONSE:\n"
              << response << "\n";

    if (res != CURLE_OK || code != 200)
    {
        std::cout << "❌ Login failed\n";
        return session;
    }

    session.isValid = true;
    session.token = ExtractString(response, "sessionToken");
    session.decartKey = ExtractString(response, "decartKey");
    session.userId = ExtractString(response, "id");
    session.name = ExtractString(response, "name");
    session.seconds = ExtractInt(response, "seconds");

    if (session.name.empty())
        session.name = "User";

    std::cout << "✅ LOGIN SUCCESS\n";
    std::cout << "User: " << session.name << "\n";
    std::cout << "Seconds: " << session.seconds << "\n";

    return session;
}

// ========================================
// UPDATE TIME TO DB
// ========================================

bool UpdateUserSeconds(
    const std::string &userId,
    int seconds,
    const std::string &token)
{
    CURL *curl = curl_easy_init();

    if (!curl)
        return false;

    std::string response;

    std::string url =
        "https://origin-ai-six.vercel.app/api/update-time";

    // PERFECT JSON
    std::string json =
        std::string("{") +
        "\"user_id\":\"" + userId + "\"," +
        "\"remaining_seconds\":" + std::to_string(seconds) +
        "}";

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers,
                                "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // IMPORTANT
    curl_easy_setopt(curl,
                     CURLOPT_POSTFIELDS,
                     json.c_str());

    curl_easy_setopt(curl,
                     CURLOPT_POSTFIELDSIZE,
                     json.size());

    curl_easy_setopt(curl,
                     CURLOPT_HTTPHEADER,
                     headers);

    curl_easy_setopt(curl,
                     CURLOPT_WRITEFUNCTION,
                     WriteCallback);

    curl_easy_setopt(curl,
                     CURLOPT_WRITEDATA,
                     &response);

    curl_easy_setopt(curl,
                     CURLOPT_TIMEOUT,
                     20L);

    CURLcode res = curl_easy_perform(curl);

    long code = 0;
    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    std::cout << "📡 SENT JSON:\n"
              << json << "\n";

    std::cout << "💾 SAVE RESPONSE:\n"
              << response << "\n";

    if (res != CURLE_OK || code != 200)
    {
        std::cout << "❌ DB SAVE FAILED\n";
        return false;
    }

    std::cout << "✅ DB UPDATED\n";
    return true;
}

// ========================================
// LOCAL DEDUCT
// ========================================
void DeductTimeFromDB(OriginSession &session, int seconds)
{
    session.seconds -= seconds;

    if (session.seconds < 0)
        session.seconds = 0;

    UpdateUserSeconds(
        session.userId,
        session.seconds,
        session.token);
}