#pragma once
#include "easy_flipper/easy_flipper.h"
#include "flipper_http/flipper_http.h"
#include "loading/loading.hpp"
#include "keyboard/keyboard.hpp"
#include "memory"
#include "vector"

typedef enum
{
    AppViewMainMenu = -1,
    AppViewStatus = 0,
    AppViewConnect = 1,
    AppViewScan = 2,
    AppViewCommands = 3,
    AppViewNetworkList = 4,
    AppViewSaveWiFi = 5,
} AppView;

typedef enum
{
    RequestStatusSuccess = 0,      // Request successful
    RequestStatusRequestError = 1, // Request error
    RequestStatusNotStarted = 2,   // Request not started
    RequestStatusWaiting = 3,      // Waiting for request response
    RequestStatusParseError = 4,   // Error parsing request info
    RequestStatusKeyboard = 5,     // Keyboard for input (only used in scan for now)
} RequestStatus;

typedef enum
{
    RequestTypeStatusConnection, // Check if Wi-Fi is connected
    RequestTypeStatusSSID,       // Get current SSID
    RequestTypeStatusIP,         // Get current IP address of the device
    RequestTypeConnect,          // Connect to Wi-Fi
    RequestTypeScan,             // Scan for Wi-Fi networks
    RequestTypeSaveWiFi,         // Save Wi-Fi credentials
} RequestType;

typedef enum
{
    ConnectionTypeConnection, // check connection status
    ConnectionTypeSSID,       // get current SSID
    ConnectionTypeIP,         // get current IP address of the device
} ConnectionType;

class FlipperHTTPApp;

class FlipperHTTPRun
{
    void *appContext;                   // reference to the app context
    bool connectInfoStatus;             // true/false if is connected
    RequestStatus commandStatus;        // status of the current command
    std::string currentSSID;            // current connected SSID
    std::string currentIP;              // current IP address of the device
    ConnectionType connectionType;      // type of connection info to retrieve
    RequestStatus connectStatus;        // status of the Connect view
    uint8_t currentCommandIndex;        // current command index in the Commands view
    uint8_t currentMenuIndex;           // current menu index
    uint8_t currentNetworkListIndex;    // current network list index in the Network List view
    uint8_t currentSSIDIndex;           // current SSID index for scan view
    AppView currentView;                // current view of the social run
    std::unique_ptr<Keyboard> keyboard; // keyboard instance for input handling
    InputKey lastInput;                 // last input key pressed
    std::unique_ptr<Loading> loading;   // loading animation instance
    RequestStatus networkListStatus;    // status of the Network List view
    RequestStatus saveWiFiStatus;       // status of the Save WiFi view
    RequestStatus scanStatus;           // status of the Scan view
    std::vector<std::string> ssidList;  // list of scanned SSIDs
    bool shouldReturnToMenu;            // Flag to signal return to menu
    RequestStatus statusStatus;         // status of the Status view
public:
    FlipperHTTPRun(void *appContext);
    ~FlipperHTTPRun();
    //
    bool isActive() const { return shouldReturnToMenu == false; } // Check if the run is active
    void drawConnectView(Canvas *canvas);                         // Draw the Connect view
    void drawCommandsView(Canvas *canvas);                        // Draw the Commands view
    void drawMainMenuView(Canvas *canvas);                        // Draw the main menu view

    // Generic menu drawer
    void drawMenu(
        Canvas *canvas,
        uint8_t selectedIndex,
        const char **menuItems,
        uint8_t menuCount,
        const char *title = "FlipperHTTP");

    void drawNetworkListView(Canvas *canvas);  // Draw the Network List view
    void drawSaveWiFiView(Canvas *canvas);     // Draw the Save WiFi view
    void drawScanView(Canvas *canvas);         // Draw the Scan view
    void drawStatusView(Canvas *canvas);       // Draw the Status view
    bool httpRequestIsFinished();              // check if the HTTP request is finished
    bool loadNetworkList();                    // load the list of saved networks from storage
    void sendCommand(HTTPCommand command);     // send a command to the device
    void updateDraw(Canvas *canvas);           // update and draw the run
    void updateInput(InputEvent *event);       // update input for the run
    void userRequest(RequestType requestType); // Send a user request to the server based on the request type
};
