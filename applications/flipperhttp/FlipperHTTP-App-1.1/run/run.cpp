#include "run/run.hpp"
#include "app.hpp"

FlipperHTTPRun::FlipperHTTPRun(void *appContext) : appContext(appContext), commandStatus(RequestStatusNotStarted), connectionType(ConnectionTypeConnection), connectStatus(RequestStatusNotStarted),
                                                   currentMenuIndex(0), currentSSIDIndex(0), currentView(AppViewMainMenu), keyboard(nullptr),
                                                   lastInput(InputKeyMAX), loading(nullptr), saveWiFiStatus(RequestStatusNotStarted),
                                                   scanStatus(RequestStatusNotStarted), shouldReturnToMenu(false), statusStatus(RequestStatusNotStarted)
{
    // create networks folder
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flipper_http/data/networks");
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

FlipperHTTPRun::~FlipperHTTPRun()
{
    // nothing to do
}

void FlipperHTTPRun::drawCommandsView(Canvas *canvas)
{

    static bool loadingStarted = false;
    switch (commandStatus)
    {
    case RequestStatusNotStarted:
    {
        const char *menuItems[] = {
            "WIFI/CONNECT]",
            "[WIFI/DISCONNECT]",
            "[IP/ADDRESS]",
            "[WIFI/IP]",
            "[WIFI/SCAN]",
            "[LIST]",
            "[LED/ON]",
            "[LED/OFF]",
            "[PING]",
            "[VERSION]",
            "[WIFI/STATUS]",
            "[REBOOT]",
            "[WIFI/SSID]",
            "[WIFI/LIST]",
        };
        drawMenu(canvas, currentCommandIndex, menuItems, 14, "Commands");
        break;
    }
    case RequestStatusWaiting:
    {
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loading->setText("Executing command...");
        }
        loadingStarted = true;
        if (loading)
        {
            loading->animate();
        }
        FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
        furi_check(app);

        HTTPState state = app->getHttpState();
        if (state == RECEIVING || state == SENDING)
        {
            // still processing, keep showing loading
            return;
        }

        loading->stop();
        loadingStarted = false;

        if (state == ISSUE)
        {
            commandStatus = RequestStatusRequestError;
            return;
        }
        commandStatus = RequestStatusSuccess;
        break;
    }
    case RequestStatusSuccess:
    {
        FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
        furi_check(app);
        const char *response = app->getHttpResponse();
        size_t response_len = strlen(response);
        size_t chars_per_line = 32;
        if (response_len == 0)
        {
            canvas_draw_str(canvas, 0, 10, "Command executed!");
        }
        else if (response_len < chars_per_line)
        {
            canvas_draw_str(canvas, 0, 10, response);
        }
        else
        {
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            char buffer[chars_per_line + 1];
            size_t offset = 0;
            int line = 0;
            while (offset < response_len && line < 9) // max 9 lines
            {
                size_t chunk_size = (response_len - offset > chars_per_line) ? chars_per_line : (response_len - offset);
                snprintf(buffer, sizeof(buffer), "%.*s", (int)chunk_size, response + offset);
                canvas_draw_str(canvas, 0, 5 + (line * 7), buffer);
                offset += chunk_size;
                line++;
            }
        }
        break;
    }
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Command request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Executing command...");
        break;
    }
}

void FlipperHTTPRun::drawConnectView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (connectStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Connecting...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                connectStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                /* This returns:
                 - [SUCCESS] Connected to Wifi.
                 - [ERROR] Failed to connect to Wifi.
                 - [INFO] Already connected to WiFi.
                */
                if (strstr(response, "[ERROR]") == NULL)
                {
                    connectStatus = RequestStatusSuccess;
                }
                else
                {
                    connectStatus = RequestStatusRequestError;
                }
            }
            else
            {
                connectStatus = RequestStatusRequestError;
            }
        }
        break;
    case RequestStatusSuccess:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 10, "WiFi saved and connected!!");
        canvas_draw_str(canvas, 0, 60, "Press 'BACK' to leave.");
        canvas_set_font(canvas, FontPrimary);
        break;
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Connect request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Connecting...");
        break;
    }
}

void FlipperHTTPRun::drawMainMenuView(Canvas *canvas)
{
    const char *menuItems[] = {"Status", "Connect", "Scan", "Commands", "Saved Networks"};
    drawMenu(canvas, currentMenuIndex, menuItems, 5);
}

void FlipperHTTPRun::drawMenu(Canvas *canvas, uint8_t selectedIndex, const char **menuItems, uint8_t menuCount, const char *title)
{
    // Draw title
    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
    int title_width = canvas_string_width(canvas, title);
    int title_x = (128 - title_width) / 2;
    canvas_draw_str(canvas, title_x, 12, title);

    // Draw underline for title
    canvas_draw_line(canvas, title_x, 14, title_x + title_width, 14);

    // Draw decorative horizontal pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 18);
    }

    // Menu items with word wrapping
    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    const char *currentItem = menuItems[selectedIndex];

    const int box_padding = 10;
    const int box_width = 108;
    const int usable_width = box_width - (box_padding * 2); // Text area inside box
    const int line_height = 8;                              // Typical line height for medium font
    const int max_lines = 2;                                // Maximum lines to prevent overflow

    int menu_y = 40;

    // Calculate word wrapping
    char lines[max_lines][64];
    int line_count = 0;

    // word wrap
    const char *text = currentItem;
    int text_len = strlen(text);
    int current_pos = 0;

    while (current_pos < text_len && line_count < max_lines)
    {
        int line_start = current_pos;
        int last_space = -1;
        int current_width = 0;
        int char_pos = 0;

        // Find how much text fits on this line
        while (current_pos < text_len && char_pos < 63) // Leave room for null terminator
        {
            if (text[current_pos] == ' ')
            {
                last_space = char_pos;
            }

            lines[line_count][char_pos] = text[current_pos];
            char_pos++;

            // Check if adding this character exceeds width
            lines[line_count][char_pos] = '\0'; // Temporary null terminator
            current_width = canvas_string_width(canvas, lines[line_count]);

            if (current_width > usable_width)
            {
                // Text is too wide, need to break
                if (last_space > 0)
                {
                    // Break at last space
                    lines[line_count][last_space] = '\0';
                    current_pos = line_start + last_space + 1; // Skip the space
                }
                else
                {
                    // No space found, break at previous character
                    char_pos--;
                    lines[line_count][char_pos] = '\0';
                    current_pos = line_start + char_pos;
                }
                break;
            }

            current_pos++;
        }

        // If we reached end of text
        if (current_pos >= text_len)
        {
            lines[line_count][char_pos] = '\0';
        }

        line_count++;
    }

    // If there's still more text and we're at max lines, add ellipsis
    if (current_pos < text_len && line_count == max_lines)
    {
        int last_line = line_count - 1;
        int line_len = strlen(lines[last_line]);
        if (line_len > 3)
        {
            strcpy(&lines[last_line][line_len - 3], "...");
        }
    }

    // Calculate box height based on number of lines, but keep minimum height
    int box_height = (line_count * line_height) + 8;
    if (box_height < 16)
        box_height = 16;

    // Dynamic box positioning based on content height
    int box_y_offset;
    if (line_count > 1)
    {
        box_y_offset = -22;
    }
    else
    {
        box_y_offset = -12;
    }

    // Draw main selection box
    canvas_draw_rbox(canvas, 10, menu_y + box_y_offset, box_width, box_height, 4);
    canvas_set_color(canvas, ColorWhite);

    // Draw each line of text centered
    for (int i = 0; i < line_count; i++)
    {
        int line_width = canvas_string_width(canvas, lines[i]);
        int line_x = (128 - line_width) / 2;
        int text_y_offset = (line_count > 1) ? -18 : -4;
        int line_y = menu_y + (i * line_height) + 4 + text_y_offset;
        canvas_draw_str(canvas, line_x, line_y, lines[i]);
    }

    canvas_set_color(canvas, ColorBlack);

    // Draw navigation arrows
    if (selectedIndex > 0)
    {
        canvas_draw_str(canvas, 2, menu_y + 4, "<");
    }
    if (selectedIndex < (menuCount - 1))
    {
        canvas_draw_str(canvas, 122, menu_y + 4, ">");
    }

    const int MAX_DOTS = 15;
    const int dots_spacing = 6;
    int indicator_y = 52;

    if (menuCount <= MAX_DOTS)
    {
        // Show all dots if they fit
        int dots_start_x = (128 - (menuCount * dots_spacing)) / 2;
        for (int i = 0; i < menuCount; i++)
        {
            int dot_x = dots_start_x + (i * dots_spacing);
            if (i == selectedIndex)
            {
                canvas_draw_box(canvas, dot_x, indicator_y, 4, 4);
            }
            else
            {
                canvas_draw_frame(canvas, dot_x, indicator_y, 4, 4);
            }
        }
    }
    else
    {
        // condensed indicator with current position
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        char position_text[16];
        snprintf(position_text, sizeof(position_text), "%d/%d", selectedIndex + 1, menuCount);
        int pos_width = canvas_string_width(canvas, position_text);
        int pos_x = (128 - pos_width) / 2;
        canvas_draw_str(canvas, pos_x, indicator_y + 3, position_text);

        // progress bar
        int bar_width = 60;
        int bar_x = (128 - bar_width) / 2;
        int bar_y = indicator_y - 6;
        canvas_draw_frame(canvas, bar_x, bar_y, bar_width, 3);
        int progress_width = (selectedIndex * (bar_width - 2)) / (menuCount - 1);
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, progress_width, 1);
    }

    // Draw decorative bottom pattern
    for (int i = 0; i < 128; i += 4)
    {
        canvas_draw_dot(canvas, i, 58);
    }
}

void FlipperHTTPRun::drawNetworkListView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    switch (networkListStatus)
    {
    case RequestStatusWaiting:
        if (!loadNetworkList())
        {
            networkListStatus = RequestStatusRequestError;
            return;
        }
        networkListStatus = RequestStatusSuccess;
        break;
    case RequestStatusSuccess:
    {
        std::vector<const char *> ssid_cstr_list;
        for (const auto &ssid : ssidList)
        {
            ssid_cstr_list.push_back(ssid.c_str());
        }
        if (!ssidList.empty())
        {
            drawMenu(canvas, currentNetworkListIndex, ssid_cstr_list.data(), ssidList.size(), "Saved Networks");
        }
        else
        {
            canvas_draw_str(canvas, 0, 10, "No saved networks!");
        }
        break;
    }
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Load request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case RequestStatusKeyboard:
        if (!keyboard)
        {
            keyboard = std::make_unique<Keyboard>();
        }
        if (keyboard)
        {
            keyboard->draw(canvas, "Update password:");
        }
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Loading...");
        break;
    };
}

void FlipperHTTPRun::drawSaveWiFiView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (saveWiFiStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Saving...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                saveWiFiStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                /* This returns:
                 - [SUCCESS] Wifi settings saved.
                 - [ERROR] Failed to save Wifi settings.
                 - [ERROR] Failed to parse JSON
                 - [ERROR] JSON does not contain ssid and password.
                 - [ERROR] Failed to save settings to file.
                 - [SUCCESS] Connected to the new Wifi network.
                 - [ERROR] Failed to parse JSON data.
                 - [ERROR] JSON must contain 'ssid' and 'password'.
                 - [ERROR] Failed to write settings to storage.
                 - [SUCCESS] Settings saved.
                */
                if (strstr(response, "[ERROR]") == NULL)
                {
                    saveWiFiStatus = RequestStatusSuccess;

                    // switch to connect
                    currentView = AppViewConnect;
                    connectStatus = RequestStatusWaiting;
                    userRequest(RequestTypeConnect);
                }
                else
                {
                    saveWiFiStatus = RequestStatusRequestError;
                }
            }
            else
            {
                saveWiFiStatus = RequestStatusRequestError;
            }
        }
        break;
    case RequestStatusSuccess:
        canvas_draw_str(canvas, 0, 10, "Saved successfully!");
        canvas_draw_str(canvas, 0, 20, "Press BACK to leave.");
        break;
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Save request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Saving...");
        break;
    }
}

void FlipperHTTPRun::drawScanView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (scanStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Scanning...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                scanStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                if (strstr(response, "[ERROR]") == NULL)
                {
                    scanStatus = RequestStatusSuccess;

                    // Clear previous SSID list and reset index
                    ssidList.clear();

                    // Parse JSON response and populate SSID list
                    for (uint8_t i = 0; i < 50; i++)
                    {
                        char *ssid_item = get_json_array_value("networks", i, app->getHttpResponse());
                        if (ssid_item == NULL)
                        {
                            // end of the list
                            break;
                        }
                        ssidList.push_back(std::string(ssid_item));
                        free(ssid_item);
                    }
                }
                else
                {
                    scanStatus = RequestStatusRequestError;
                }
            }
            else
            {
                scanStatus = RequestStatusRequestError;
            }
        }
        break;
    case RequestStatusSuccess:
    {
        std::vector<const char *> ssid_cstr_list;
        for (const auto &ssid : ssidList)
        {
            ssid_cstr_list.push_back(ssid.c_str());
        }

        if (!ssidList.empty())
        {
            drawMenu(canvas, currentSSIDIndex, ssid_cstr_list.data(), ssidList.size(), "Scan Results");
        }
        else
        {
            canvas_draw_str(canvas, 0, 30, "No networks found!");
        }
        break;
    }
    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Save request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case RequestStatusKeyboard:
        if (!keyboard)
        {
            keyboard = std::make_unique<Keyboard>();
        }
        if (keyboard)
        {
            keyboard->draw(canvas, "Enter password:");
        }
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Scanning...");
        break;
    }
}

void FlipperHTTPRun::drawStatusView(Canvas *canvas)
{
    /*
    This view sequentially collects three pieces of information:
    1. ConnectionTypeConnection: Check if Wi-Fi is connected (true/false)
    2. ConnectionTypeSSID: Get current connected SSID
    3. ConnectionTypeIP: Get current IP address
    Then displays all three together.
    */
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;

    switch (statusStatus)
    {
    case RequestStatusWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Updating...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
            furi_check(app);
            if (app->getHttpState() == ISSUE)
            {
                statusStatus = RequestStatusRequestError;
                return;
            }
            const char *response = app->getHttpResponse();
            if (response && strlen(response) > 0)
            {
                // Process response based on current connection type
                switch (connectionType)
                {
                case ConnectionTypeConnection:
                    // Parse connection status response
                    if (strstr(response, "[ERROR]") == NULL)
                    {
                        connectInfoStatus = (strstr(response, "true") != NULL ||
                                             strstr(response, "Connected") != NULL ||
                                             strstr(response, "[SUCCESS]") != NULL ||
                                             strstr(response, "[INFO]") != NULL);
                    }
                    else
                    {
                        connectInfoStatus = false;
                    }
                    // Move to next request type
                    connectionType = ConnectionTypeSSID;
                    userRequest(RequestTypeStatusSSID);
                    break;

                case ConnectionTypeSSID:
                    // Parse SSID response
                    if (strstr(response, "[ERROR]") == NULL)
                    {
                        // Extract SSID from response (remove [SUCCESS] or [INFO] prefix if present)
                        const char *ssid_start = response;
                        if (strstr(response, "[SUCCESS]") != NULL)
                        {
                            ssid_start = strstr(response, "]");
                            if (ssid_start)
                                ssid_start++; // skip the ]
                            while (*ssid_start == ' ')
                                ssid_start++; // skip spaces
                        }
                        else if (strstr(response, "[INFO]") != NULL)
                        {
                            ssid_start = strstr(response, "]");
                            if (ssid_start)
                                ssid_start++; // skip the ]
                            while (*ssid_start == ' ')
                                ssid_start++; // skip spaces
                        }
                        currentSSID = std::string(ssid_start);
                    }
                    else
                    {
                        currentSSID = "Not connected";
                    }
                    // Move to next request type
                    connectionType = ConnectionTypeIP;
                    userRequest(RequestTypeStatusIP);
                    break;

                case ConnectionTypeIP:
                    // Parse IP response
                    if (strstr(response, "[ERROR]") == NULL)
                    {
                        // Extract IP from response (remove [SUCCESS] or [INFO] prefix if present)
                        const char *ip_start = response;
                        if (strstr(response, "[SUCCESS]") != NULL)
                        {
                            ip_start = strstr(response, "]");
                            if (ip_start)
                                ip_start++; // skip the ]
                            while (*ip_start == ' ')
                                ip_start++; // skip spaces
                        }
                        else if (strstr(response, "[INFO]") != NULL)
                        {
                            ip_start = strstr(response, "]");
                            if (ip_start)
                                ip_start++; // skip the ]
                            while (*ip_start == ' ')
                                ip_start++; // skip spaces
                        }
                        currentIP = std::string(ip_start);
                    }
                    else
                    {
                        currentIP = "No IP assigned";
                    }
                    // All requests complete, show results
                    statusStatus = RequestStatusSuccess;
                    break;
                }
            }
            else
            {
                statusStatus = RequestStatusRequestError;
            }
        }
        break;

    case RequestStatusSuccess:
    {
        // Display all collected information
        canvas_draw_str(canvas, 0, 10, "WiFi Status:");

        canvas_set_font(canvas, FontSecondary);

        // Connection status
        const char *connectionStatus = connectInfoStatus ? "Connected" : "Disconnected";
        canvas_draw_str(canvas, 70, 10, connectionStatus);

        // SSID
        char ssidBuffer[64];
        snprintf(ssidBuffer, sizeof(ssidBuffer), "SSID: %s", currentSSID.c_str());
        canvas_draw_str(canvas, 0, 22, ssidBuffer);

        // IP Address
        char ipBuffer[64];
        snprintf(ipBuffer, sizeof(ipBuffer), "IP: %s", currentIP.c_str());
        canvas_draw_str(canvas, 0, 34, ipBuffer);

        canvas_draw_str(canvas, 0, 60, "Press 'BACK' to leave.");

        canvas_set_font(canvas, FontPrimary);
        break;
    }

    case RequestStatusRequestError:
        canvas_draw_str(canvas, 0, 10, "Status request failed!");
        canvas_draw_str(canvas, 0, 20, "Reconnect your board and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;

    case RequestStatusNotStarted:
    case RequestStatusParseError:
    default:
        canvas_draw_str(canvas, 0, 10, "Updating...");
        break;
    }
}

bool FlipperHTTPRun::httpRequestIsFinished()
{
    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return true;
    }
    // for this app, we just need to check the response
    const char *response = app->getHttpResponse();
    if (!response || strlen(response) == 0)
    {
        return false;
    }
    // state should be IDLE in this app
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

bool FlipperHTTPRun::loadNetworkList()
{
    this->ssidList.clear();

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flipper_http/data/networks");

    if (!storage_dir_exists(storage, directory_path))
    {
        FURI_LOG_E(TAG, "Networks directory does not exist");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!storage_dir_open(file, directory_path))
    {
        FURI_LOG_E(TAG, "Failed to open networks directory");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char name_buffer[64];
    while (storage_dir_read(file, NULL, name_buffer, sizeof(name_buffer)))
    {
        // remove last 4 characters (".txt")
        size_t name_len = strlen(name_buffer);
        if (name_len > 4 && strcmp(name_buffer + name_len - 4, ".txt") == 0)
        {
            name_buffer[name_len - 4] = '\0'; // Remove the ".txt" extension
            this->ssidList.push_back(std::string(name_buffer));
        }
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

void FlipperHTTPRun::sendCommand(HTTPCommand command)
{
    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
    furi_check(app);
    app->clearHttpResponse();
    app->sendHttpCommand(command);
}

void FlipperHTTPRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);
    switch (currentView)
    {
    case AppViewMainMenu:
        drawMainMenuView(canvas);
        break;
    case AppViewStatus:
        drawStatusView(canvas);
        break;
    case AppViewConnect:
        drawConnectView(canvas);
        break;
    case AppViewScan:
        drawScanView(canvas);
        break;
    case AppViewSaveWiFi:
        drawSaveWiFiView(canvas);
        break;
    case AppViewCommands:
        drawCommandsView(canvas);
        break;
    case AppViewNetworkList:
        drawNetworkListView(canvas);
        break;
    default:
        break;
    };
}

void FlipperHTTPRun::updateInput(InputEvent *event)
{
    lastInput = event->key;
    switch (currentView)
    {
    case AppViewMainMenu:
        switch (lastInput)
        {
        case InputKeyRight:
            if (currentMenuIndex < 4)
            {
                currentMenuIndex++;
            }
            else
            {
                currentMenuIndex = 0;
            }
            break;
        case InputKeyLeft:
            if (currentMenuIndex > 0)
            {
                currentMenuIndex--;
            }
            else
            {
                currentMenuIndex = 4;
            }
            break;
        case InputKeyBack:
            shouldReturnToMenu = true;
            if (loading)
            {
                loading.reset();
            }
            if (keyboard)
            {
                keyboard.reset();
            }
            currentView = AppViewMainMenu;
            currentMenuIndex = 0;
            break;
        case InputKeyOk:
            switch (currentMenuIndex)
            {
            case AppViewStatus:
                currentView = AppViewStatus;
                connectionType = ConnectionTypeConnection;
                statusStatus = RequestStatusWaiting;
                userRequest(RequestTypeStatusConnection);
                break;
            case AppViewConnect:
                currentView = AppViewSaveWiFi;
                saveWiFiStatus = RequestStatusWaiting;
                userRequest(RequestTypeSaveWiFi);
                break;
            case AppViewScan:
                currentView = AppViewScan;
                currentSSIDIndex = 0;
                scanStatus = RequestStatusWaiting;
                userRequest(RequestTypeScan);
                break;
            case AppViewCommands:
                currentView = AppViewCommands;
                currentCommandIndex = 0;
                commandStatus = RequestStatusNotStarted;
                break;
            case AppViewNetworkList:
                currentView = AppViewNetworkList;
                currentNetworkListIndex = 0;
                networkListStatus = RequestStatusWaiting;
                break;
            default:
                break;
            };
        default:
            break;
        }
        break;
    case AppViewScan:
        if (scanStatus == RequestStatusKeyboard)
        {
            if (keyboard)
            {
                if (keyboard->handleInput(event))
                {
                    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                    furi_check(app);
                    app->saveChar("wifi_ssid", ssidList[currentSSIDIndex].c_str());
                    app->saveChar("wifi_pass", keyboard->getText());
                    std::string networkPathName = "networks/" + ssidList[currentSSIDIndex];
                    app->saveChar(networkPathName.c_str(), keyboard->getText());
                    scanStatus = RequestStatusWaiting;
                    saveWiFiStatus = RequestStatusWaiting;
                    userRequest(RequestTypeSaveWiFi);
                    currentView = AppViewSaveWiFi;
                    currentMenuIndex = 1; // move to connect
                    currentSSIDIndex = 0;
                    keyboard.reset();
                }
            }
            if (lastInput == InputKeyBack && event->type == InputTypeLong)
            {
                scanStatus = RequestStatusWaiting;
                currentView = AppViewMainMenu;
            }
        }
        else
        {
            switch (lastInput)
            {
            case InputKeyRight:
                if (currentSSIDIndex < ssidList.size() - 1)
                {
                    currentSSIDIndex++;
                }
                break;
            case InputKeyLeft:
                if (currentSSIDIndex > 0)
                {
                    currentSSIDIndex--;
                }
                break;
            case InputKeyBack:
                currentView = AppViewMainMenu;
                break;
            case InputKeyOk:
                if (!keyboard)
                {
                    keyboard = std::make_unique<Keyboard>();
                }
                if (keyboard)
                {
                    keyboard->clearText();
                    keyboard->setText(""); // Start with empty text
                }
                scanStatus = RequestStatusKeyboard;
                break;
            default:
                break;
            }
        }
        break;
    case AppViewCommands:
        switch (lastInput)
        {
        case InputKeyBack:
            if (commandStatus != RequestStatusSuccess)
            {
                commandStatus = RequestStatusNotStarted;
                currentView = AppViewMainMenu;
                currentMenuIndex = 3; // move to commands
                currentCommandIndex = 0;
            }
            else
            {
                commandStatus = RequestStatusNotStarted;
            }
            break;
        case InputKeyOk:
            this->sendCommand(static_cast<HTTPCommand>(currentCommandIndex));
            commandStatus = RequestStatusWaiting;
            break;
        case InputKeyRight:
            if (currentCommandIndex < 13)
            {
                currentCommandIndex++;
            }
            else
            {
                currentCommandIndex = 0;
            }
            break;
        case InputKeyLeft:
            if (currentCommandIndex > 0)
            {
                currentCommandIndex--;
            }
            else
            {
                currentCommandIndex = 13;
            }
            break;
        default:
            break;
        };
        break;
    case AppViewNetworkList:
        if (networkListStatus == RequestStatusKeyboard)
        {
            if (keyboard)
            {
                if (keyboard->handleInput(event))
                {
                    // save both the SSID and password to the current WiFi credentials, then move to save view
                    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                    furi_check(app);
                    app->saveChar("wifi_ssid", ssidList[currentNetworkListIndex].c_str());
                    app->saveChar("wifi_pass", keyboard->getText());
                    std::string networkPathName = "networks/" + ssidList[currentNetworkListIndex];
                    app->saveChar(networkPathName.c_str(), keyboard->getText());
                    saveWiFiStatus = RequestStatusWaiting;
                    userRequest(RequestTypeSaveWiFi);
                    currentView = AppViewSaveWiFi;
                    currentMenuIndex = 1; // move to connect
                    currentSSIDIndex = 0;
                    keyboard.reset();
                }
            }
            if (lastInput == InputKeyBack && event->type == InputTypeLong)
            {
                networkListStatus = RequestStatusWaiting;
                currentView = AppViewMainMenu;
            }
        }
        else
        {
            switch (lastInput)
            {
            case InputKeyBack:
                networkListStatus = RequestStatusNotStarted;
                currentView = AppViewMainMenu;
                currentMenuIndex = 4; // move to network list
                currentNetworkListIndex = 0;
                break;
            case InputKeyOk:
            {
                // here we will load the selected network's password
                // then save both the SSID and password to the current WiFi credentials, then move to save view
                char wifi_pass[64] = {0};
                FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
                furi_check(app);
                std::string networkPathName = "networks/" + ssidList[currentNetworkListIndex];
                if (!app->loadChar(networkPathName.c_str(), wifi_pass, sizeof(wifi_pass)))
                {
                    FURI_LOG_E(TAG, "Failed to load network password");
                }
                if (!keyboard)
                {
                    keyboard = std::make_unique<Keyboard>();
                }
                if (keyboard)
                {
                    keyboard->clearText();
                    keyboard->setText(wifi_pass); // Start with the saved password
                }
                networkListStatus = RequestStatusKeyboard;
                break;
            }
            case InputKeyRight:
            {
                auto listSize = ssidList.size();
                if (currentNetworkListIndex < listSize - 1)
                {
                    currentNetworkListIndex++;
                }
                else
                {
                    currentNetworkListIndex = 0;
                }
                break;
            }
            case InputKeyLeft:
            {
                auto listSize = ssidList.size();
                if (currentNetworkListIndex > 0)
                {
                    currentNetworkListIndex--;
                }
                else
                {
                    currentNetworkListIndex = listSize - 1;
                }
                break;
            }
            default:
                break;
            };
        }
        break;
    default:
        if (lastInput == InputKeyBack)
        {
            if (currentView == AppViewStatus)
            {
                connectionType = ConnectionTypeConnection;
            }
            currentView = AppViewMainMenu;
        }
        break;
    };
}

void FlipperHTTPRun::userRequest(RequestType requestType)
{
    // Get app context to access HTTP functionality
    FlipperHTTPApp *app = static_cast<FlipperHTTPApp *>(appContext);
    furi_check(app);

    app->clearHttpResponse();

    switch (requestType)
    {
    case RequestTypeStatusConnection:
        if (!app->sendHttpCommand(HTTP_CMD_STATUS))
        {
            statusStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeStatusSSID:
        if (!app->sendHttpCommand(HTTP_CMD_SSID))
        {
            statusStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeStatusIP:
        if (!app->sendHttpCommand(HTTP_CMD_IP_ADDRESS))
        {
            statusStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeConnect:
        if (!app->sendHttpCommand(HTTP_CMD_WIFI_CONNECT))
        {
            connectStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeScan:
        if (!app->sendHttpCommand(HTTP_CMD_SCAN))
        {
            scanStatus = RequestStatusRequestError;
        }
        break;
    case RequestTypeSaveWiFi:
    {
        char wifi_ssid[64] = {0};
        char wifi_pass[64] = {0};
        if (!app->loadChar("wifi_ssid", wifi_ssid, sizeof(wifi_ssid)))
        {
            FURI_LOG_E(TAG, "Failed to load wifi_ssid");
            connectStatus = RequestStatusRequestError;
            break;
        }
        if (!app->loadChar("wifi_pass", wifi_pass, sizeof(wifi_pass)))
        {
            FURI_LOG_E(TAG, "Failed to load wifi_pass");
            connectStatus = RequestStatusRequestError;
            break;
        }
        if (!app->sendWiFiCredentials(wifi_ssid, wifi_pass))
        {
            FURI_LOG_E(TAG, "Failed to send WiFi credentials");
            connectStatus = RequestStatusRequestError;
        }

        break;
    }
    default:
        FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
        statusStatus = RequestStatusRequestError;
        connectStatus = RequestStatusRequestError;
        scanStatus = RequestStatusRequestError;
        break;
    }
}