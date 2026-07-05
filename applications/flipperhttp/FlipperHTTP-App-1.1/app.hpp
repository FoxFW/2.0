#pragma once
#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "run/run.hpp"
#include "settings/settings.hpp"
#include "about/about.hpp"

#define TAG "FlipperHTTP"
#define VERSION "1.1"
#define VERSION_TAG TAG " " VERSION
#define APP_ID "flipper_http"

typedef enum
{
    FlipperHTTPSubmenuRun = 0,
    FlipperHTTPSubmenuAbout = 1,
    FlipperHTTPSubmenuSettings = 2,
} FlipperHTTPSubmenuIndex;

typedef enum
{
    FlipperHTTPViewMain = 0,
    FlipperHTTPViewSubmenu = 1,
    FlipperHTTPViewAbout = 2,
    FlipperHTTPViewSettings = 3,
    FlipperHTTPViewTextInput = 4,
} FlipperHTTPView;

class FlipperHTTPApp
{
private:
    std::unique_ptr<FlipperHTTPAbout> about;       // About class instance
    FlipperHTTP *flipperHttp = nullptr;            // FlipperHTTP instance for HTTP requests
    std::unique_ptr<FlipperHTTPRun> run;           // Run class instance
    std::unique_ptr<FlipperHTTPSettings> settings; // Settings class instance
    Submenu *submenu = nullptr;                    // Submenu for the app
    FuriTimer *timer = nullptr;                    // Timer for run updates
    //
    static uint32_t callbackExitApp(void *context);                    // Callback to exit the app
    void callbackSubmenuChoices(uint32_t index);                       // Callback for submenu choices
    void createAppDataPath();                                          // Create the app data path in storage
    void settingsItemSelected(uint32_t index);                         // Handle settings item selection
    static void submenuChoicesCallback(void *context, uint32_t index); // Callback for submenu choices
    static void timerCallback(void *context);                          // Timer callback for run updates

public:
    FlipperHTTPApp();
    ~FlipperHTTPApp();
    //
    Gui *gui = nullptr;                                                                                         // GUI instance for the app
    ViewDispatcher *viewDispatcher = nullptr;                                                                   // ViewDispatcher for managing views
    ViewPort *viewPort = nullptr;                                                                               // ViewPort for drawing and input handling (run instance)
                                                                                                                //
    void clearHttpResponse();                                                                                   // clear the last HTTP response
    size_t getBytesReceived() const noexcept { return flipperHttp ? flipperHttp->bytes_received : 0; }          // get the number of bytes received
    size_t getContentLength() const noexcept { return flipperHttp ? flipperHttp->content_length : 0; }          // get the content length of the last response
    HTTPState getHttpState() const noexcept { return flipperHttp ? flipperHttp->state : INACTIVE; }             // get the HTTP state
    const char *getHttpResponse() const noexcept { return flipperHttp ? flipperHttp->last_response : nullptr; } // get the last HTTP response
    bool hasWiFiCredentials();                                                                                  // check if WiFi credentials are set
    bool hasUserCredentials();                                                                                  // check if user credentials are set
    FuriString *httpRequest(                                                                                    // synchronous HTTP request
        const char *url,                                                                                        // URL to send the request to
        HTTPMethod method = GET,                                                                                // HTTP method to use (GET, POST, etc.)
        const char *headers = "{\"Content-Type\": \"application/json\"}",                                       // Headers to include in the request
        const char *payload = nullptr);                                                                         // Payload to send with the request (for POST, PUT, etc.)
    bool httpRequestAsync(                                                                                      // asynchronous HTTP request (check the HttpState to see if the request is finished)
        const char *saveLocation,                                                                               // location to save the response (filename)
        const char *url,                                                                                        // URL to send the request to
        HTTPMethod method = GET,                                                                                // HTTP method to use (GET, POST, etc.)
        const char *headers = "{\"Content-Type\": \"application/json\"}",                                       // Headers to include in the request
        const char *payload = nullptr);                                                                         // Payload to send with the request (for POST, PUT, etc.)
    bool isBoardConnected();                                                                                    // check if the board is connected
    bool loadChar(const char *path_name, char *value, size_t value_size);                                       // load a string from storage
    bool loadFileChunk(const char *filePath, char *buffer, size_t sizeOfChunk, uint8_t iteration);              // Load a file chunk from storage
    void runDispatcher();                                                                                       // run the app's view dispatcher to handle views and events
    bool saveChar(const char *path_name, const char *value);                                                    // save a string to storage
    bool setHttpState(HTTPState state = IDLE) noexcept;                                                         // set the HTTP state
    bool sendHttpCommand(HTTPCommand command);                                                                  // send a command to the board
    bool sendWiFiCredentials(const char *ssid, const char *password);                                           // send WiFi credentials to the board
    static void viewPortDraw(Canvas *canvas, void *context);                                                    // draw callback for the ViewPort (used in run instance)
    static void viewPortInput(InputEvent *event, void *context);                                                // input callback for the ViewPort (used in run instance)
};