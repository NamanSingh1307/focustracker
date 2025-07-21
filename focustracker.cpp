#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
#include <vector>
#include <iomanip> // For std::put_time, std::get_time
#include <map>
#include <sstream> // For std::stringstream
#include <chrono>  // For std::chrono::seconds, minutes
#include <thread>  // For std::this_thread::sleep_for
#include <limits>  // For std::numeric_limits
#include <algorithm> // For std::sort and std::unique

// --- Global Utility Functions ---

// Simple hash function for passwords (NOT cryptographically secure for real-world apps)
// In a production environment, use robust hashing libraries like Argon2, bcrypt, or scrypt.
std::string hashPassword(const std::string& password) {
    long long hash = 0;
    for (char c : password) {
        hash = (hash * 31 + c) % 1000000007; // A simple polynomial rolling hash
    }
    return std::to_string(hash);
}

// Function to clear input buffer, essential after numeric inputs followed by getline
void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Get current time as tm struct (platform-aware)
tm getCurrentTm() {
    time_t now = time(nullptr);
    tm current_tm;
#ifdef _WIN32 // For Windows, use standard localtime. Note: localtime is not thread-safe.
    current_tm = *localtime(&now);
#else // For Linux/macOS, use thread-safe localtime_r
    localtime_r(&now, &current_tm);
#endif
    return current_tm;
}

// Convert tm to time_t
time_t tmToTimeT(tm& t) {
    return mktime(&t);
}

// Convert time_t to string (YYYY-MM-DD)
std::string timeTToDateString(time_t t) {
    tm local_tm;
#ifdef _WIN32
    local_tm = *localtime(&t);
#else
    localtime_r(&t, &local_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d");
    return oss.str();
}

// --- Structures ---

// Represents a single focus session
struct Session {
    std::string category;
    time_t startTime;
    time_t endTime;
    int duration; // in minutes

    // Computes the duration of the session in minutes
    void computeDuration() {
        duration = static_cast<int>(difftime(endTime, startTime) / 60);
    }

    // Displays the session details
    void display() const {
        tm start_tm;
#ifdef _WIN32
        start_tm = *localtime(&startTime);
#else
        localtime_r(&startTime, &start_tm);
#endif

        char start_time_str[100];
        strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M:%S", &start_tm);

        std::cout << "Category: " << category
                  << ", Duration: " << duration << " minutes"
                  << ", Start Time: " << start_time_str << "\n";
    }
};

// Represents a user with username and hashed password
struct User {
    std::string username;
    std::string hashedPassword;
};

// --- FocusTracker Class ---

class FocusTracker {
private:
    std::string currentUser; // Stores the username of the currently logged-in user
    std::map<std::string, User> users; // Map of usernames to User objects
    std::string usersFile = "users.txt"; // File to store user credentials
    std::string logFile; // Will be dynamically set based on currentUser

    // Private helper methods for user management
    void loadUsers();
    void saveUsers();
    bool registerUser();
    bool loginUser();
    void setLogFileForCurrentUser(); // Sets the log file path for the current user

public:
    // Constructor: Loads existing users when the application starts
    FocusTracker();
    
    // Public methods for session management
    void startSession(const std::string& category = ""); // Starts a manual focus session
    void endSession(Session &session); // Ends a session and logs it
    void logSessionToFile(const Session &session); // Writes a session to the user's log file
    void loadDailySummary(); // Displays a summary of today's focus sessions
    void startPomodoroSession(); // Initiates a Pomodoro timer session
    void generateWeeklyReport(); // Generates a weekly focus report in CSV format
    void trackStreaks(); // Tracks and displays focus streaks
    void menu(); // Main application menu
};

// Constructor implementation
FocusTracker::FocusTracker() {
    loadUsers(); // Load user data on startup
}

// --- User Management Implementations ---

// Loads user credentials from the users file
void FocusTracker::loadUsers() {
    std::ifstream fin(usersFile);
    if (!fin.is_open()) {
        std::cout << "Users file not found. A new one will be created when you register.\n";
        return;
    }

    std::string line;
    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string username, hashedPassword;
        // Parse username and hashed password separated by a comma
        if (std::getline(ss, username, ',') && std::getline(ss, hashedPassword)) {
            users[username] = {username, hashedPassword};
        }
    }
    fin.close();
}

// Saves current user credentials to the users file
void FocusTracker::saveUsers() {
    std::ofstream fout(usersFile);
    if (!fout.is_open()) {
        std::cerr << "Error: Could not open users file for writing.\n";
        return;
    }
    for (const auto& pair : users) {
        fout << pair.second.username << "," << pair.second.hashedPassword << "\n";
    }
    fout.close();
}

// Allows a new user to register
bool FocusTracker::registerUser() {
    std::string username, password;
    std::cout << "\n--- Register New User ---\n";
    std::cout << "Enter desired username: ";
    std::cin >> username;
    clearInputBuffer(); // Clear newline after username input

    if (users.count(username)) {
        std::cout << "Username already exists. Please choose a different one.\n";
        return false;
    }

    std::cout << "Enter password: ";
    std::cin >> password;
    clearInputBuffer(); // Clear newline after password input

    users[username] = {username, hashPassword(password)}; // Hash and store password
    saveUsers(); // Save updated user list
    std::cout << "User '" << username << "' registered successfully!\n";
    return true;
}

// Allows an existing user to log in
bool FocusTracker::loginUser() {
    std::string username, password;
    std::cout << "\n--- Login ---\n";
    std::cout << "Enter username: ";
    std::cin >> username;
    clearInputBuffer(); // Clear newline after username input

    std::cout << "Enter password: ";
    std::cin >> password;
    clearInputBuffer(); // Clear newline after password input

    // Check if username exists and password matches the hashed stored password
    if (users.count(username) && users[username].hashedPassword == hashPassword(password)) {
        currentUser = username; // Set the current user
        setLogFileForCurrentUser(); // Set the log file path for this user
        std::cout << "Welcome, " << currentUser << "!\n";
        return true;
    } else {
        std::cout << "Invalid username or password.\n";
        return false;
    }
}

// Sets the log file path based on the current user's username
void FocusTracker::setLogFileForCurrentUser() {
    logFile = "focus_log_" + currentUser + ".txt";
}

// --- Session Management Implementations ---

// Starts a focus session, either manual or with a predefined category (for Pomodoro)
void FocusTracker::startSession(const std::string& predefinedCategory) {
    Session s;
    if (predefinedCategory.empty()) {
        std::cout << "\nEnter focus category (Study/Work/Reading/etc.): ";
        std::cin >> std::ws; // Consume leading whitespace before getline
        std::getline(std::cin, s.category);
    } else {
        s.category = predefinedCategory;
    }
    
    s.startTime = time(nullptr); // Record start time

    tm start_tm;
#ifdef _WIN32
    start_tm = *localtime(&s.startTime);
#else
    localtime_r(&s.startTime, &start_tm);
#endif
    std::cout << "Session started at " << std::put_time(&start_tm, "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "Press ENTER to end session...";
    std::cin.get(); // Wait for ENTER key press to end session

    endSession(s); // Call endSession to finalize and log
}

// Ends a focus session, computes duration, and logs it
void FocusTracker::endSession(Session &session) {
    session.endTime = time(nullptr); // Record end time
    session.computeDuration(); // Calculate duration
    logSessionToFile(session); // Log the session to file

    std::cout << "\nSession ended. Summary:\n";
    session.display(); // Display session details
}

// Logs a session's data to the user's specific log file
void FocusTracker::logSessionToFile(const Session &session) {
    std::ofstream fout(logFile, std::ios::app); // Open in append mode
    if (!fout.is_open()) {
        std::cerr << "Error: Could not open log file '" << logFile << "' for writing.\n";
        return;
    }
    fout << session.category << ","
         << session.startTime << ","
         << session.endTime << ","
         << session.duration << "\n";
    fout.close();
}

// Loads and displays a summary of focus sessions for the current day
void FocusTracker::loadDailySummary() {
    std::ifstream fin(logFile);
    if (!fin.is_open()) {
        std::cout << "No focus sessions logged yet for " << currentUser << ".\n";
        return;
    }

    std::string line;
    std::map<std::string, int> categoryDuration; // Map to store total duration per category
    tm now_tm = getCurrentTm(); // Get current date for comparison

    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string cat_str, start_str, end_str, dur_str;

        // Parse the comma-separated values from the log file line
        if (std::getline(ss, cat_str, ',') &&
            std::getline(ss, start_str, ',') &&
            std::getline(ss, end_str, ',') &&
            std::getline(ss, dur_str)) {
            
            time_t start_time = static_cast<time_t>(std::stoll(start_str)); // Convert string to time_t
            int duration = std::stoi(dur_str); // Convert string to int

            tm start_tm_session;
#ifdef _WIN32
            start_tm_session = *localtime(&start_time);
#else
            localtime_r(&start_time, &start_tm_session);
#endif
            // Check if the session occurred on the current day
            if (start_tm_session.tm_mday == now_tm.tm_mday &&
                start_tm_session.tm_mon == now_tm.tm_mon &&
                start_tm_session.tm_year == now_tm.tm_year) {
                categoryDuration[cat_str] += duration; // Aggregate duration for the category
            }
        }
    }
    fin.close();

    std::cout << "\nToday's Focus Summary for " << currentUser << ":\n";
    if (categoryDuration.empty()) {
        std::cout << "No sessions recorded today.\n";
    } else {
        // Corrected for C++14 compatibility (no structured bindings)
        for (auto const& pair : categoryDuration) {
            std::cout << " - " << pair.first << ": " << pair.second << " minutes\n";
        }
    }
}

// --- Pomodoro Timer Implementation ---

void FocusTracker::startPomodoroSession() {
    int focusDuration, breakDuration, numCycles;
    std::cout << "\n--- Start Pomodoro Session ---\n";
    std::cout << "Enter focus duration (minutes): ";
    std::cin >> focusDuration;
    std::cout << "Enter break duration (minutes): ";
    std::cin >> breakDuration;
    std::cout << "Enter number of cycles: ";
    std::cin >> numCycles;
    clearInputBuffer(); // Clear newline after numeric input

    std::cout << "Enter focus category for Pomodoro sessions: ";
    std::string category;
    std::cin >> std::ws;
    std::getline(std::cin, category);

    for (int i = 1; i <= numCycles; ++i) {
        std::cout << "\n--- Cycle " << i << "/" << numCycles << " ---\n";
        std::cout << "Focus Time! (" << focusDuration << " minutes) - Category: " << category << "\n";
        
        Session s;
        s.category = category;
        s.startTime = time(nullptr); // Start time for this focus block

        // Countdown for focus time
        for (int j = focusDuration; j > 0; --j) {
            std::cout << "Time remaining: " << j << " minutes...\r" << std::flush;
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
        std::cout << "Focus time ended!                                   \n"; // Clear line after countdown

        s.endTime = time(nullptr); // End time for this focus block
        s.computeDuration();
        logSessionToFile(s); // Log each focus session from Pomodoro

        if (i < numCycles) { // Don't take a break after the last cycle
            std::cout << "Break Time! (" << breakDuration << " minutes)\n";
            // Countdown for break time
            for (int j = breakDuration; j > 0; --j) {
                std::cout << "Time remaining: " << j << " minutes...\r" << std::flush;
                std::this_thread::sleep_for(std::chrono::minutes(1));
            }
            std::cout << "Break time ended!                                   \n"; // Clear line
        }
    }
    std::cout << "\nPomodoro session completed!\n";
}

// --- Weekly Report Implementation ---

void FocusTracker::generateWeeklyReport() {
    std::ifstream fin(logFile);
    if (!fin.is_open()) {
        std::cout << "No focus sessions logged yet for " << currentUser << ".\n";
        return;
    }

    std::map<std::string, std::map<std::string, int>> weeklyData; // Date -> Category -> Duration
    tm now_tm = getCurrentTm();

    // Find the start of the current week (Monday)
    tm start_of_week_tm = now_tm;
    // Adjust tm_mday to get to Monday. tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday
    // If today is Sunday (0), subtract 6 days. If Monday (1), subtract 0. If Tuesday (2), subtract 1.
    start_of_week_tm.tm_mday -= (start_of_week_tm.tm_wday == 0 ? 6 : start_of_week_tm.tm_wday - 1);
    mktime(&start_of_week_tm); // Normalize the tm struct (adjusts month/year if mday goes out of range)

    time_t start_of_week_time = tmToTimeT(start_of_week_tm);

    std::string line;
    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string cat_str, start_str, end_str, dur_str;

        if (std::getline(ss, cat_str, ',') &&
            std::getline(ss, start_str, ',') &&
            std::getline(ss, end_str, ',') &&
            std::getline(ss, dur_str)) {
            
            time_t session_start_time = static_cast<time_t>(std::stoll(start_str));
            int duration = std::stoi(dur_str);

            // Check if session falls within the current week (from Monday to now)
            if (session_start_time >= start_of_week_time && session_start_time <= time(nullptr)) {
                std::string date_str = timeTToDateString(session_start_time);
                weeklyData[date_str][cat_str] += duration; // Aggregate data
            }
        }
    }
    fin.close();

    std::ofstream fout("weekly_report_" + currentUser + ".csv");
    if (!fout.is_open()) {
        std::cerr << "Error: Could not open weekly report file for writing.\n";
        return;
    }

    fout << "Date,Category,Total Duration (minutes)\n"; // CSV Header
    
    // Collect and sort dates for consistent CSV output
    std::vector<std::string> sortedDates;
    // Corrected for C++14 compatibility (no structured bindings)
    for(auto const& pair : weeklyData) {
        sortedDates.push_back(pair.first);
    }
    std::sort(sortedDates.begin(), sortedDates.end());

    // Write aggregated data to CSV
    for (const std::string& date : sortedDates) {
        // Corrected for C++14 compatibility (no structured bindings)
        for (auto const& pair : weeklyData[date]) {
            fout << date << "," << pair.first << "," << pair.second << "\n";
        }
    }
    fout.close();

    std::cout << "\nWeekly report generated successfully for " << currentUser 
              << " at weekly_report_" << currentUser << ".csv\n";
}

// --- Streak Tracking Implementation ---

void FocusTracker::trackStreaks() {
    std::ifstream fin(logFile);
    if (!fin.is_open()) {
        std::cout << "No focus sessions logged yet for " << currentUser << ".\n";
        return;
    }

    std::vector<time_t> sessionTimestamps; // Store all session start times
    std::string line;
    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string cat_str, start_str, end_str, dur_str;

        if (std::getline(ss, cat_str, ',') &&
            std::getline(ss, start_str, ',') &&
            std::getline(ss, end_str, ',') &&
            std::getline(ss, dur_str)) {
            
            time_t start_time = static_cast<time_t>(std::stoll(start_str));
            sessionTimestamps.push_back(start_time);
        }
    }
    fin.close();

    if (sessionTimestamps.empty()) {
        std::cout << "\nNo sessions recorded to track streaks.\n";
        return;
    }

    // Convert timestamps to unique YYYY-MM-DD date strings
    std::vector<std::string> uniqueDates;
    for (time_t t : sessionTimestamps) {
        uniqueDates.push_back(timeTToDateString(t));
    }
    std::sort(uniqueDates.begin(), uniqueDates.end());
    // Remove duplicate dates to get one entry per day
    uniqueDates.erase(std::unique(uniqueDates.begin(), uniqueDates.end()), uniqueDates.end());

    int currentStreak = 0;
    int maxStreak = 0;
    
    if (!uniqueDates.empty()) {
        currentStreak = 1; // Start with 1 if there's at least one unique date
        maxStreak = 1;

        // Iterate through sorted unique dates to find consecutive days
        for (size_t i = 1; i < uniqueDates.size(); ++i) {
            // Corrected: Create a named istringstream for std::get_time
            tm prev_tm = {};
            std::istringstream iss_prev(uniqueDates[i-1]);
            iss_prev >> std::get_time(&prev_tm, "%Y-%m-%d");
            time_t prev_time_t = tmToTimeT(prev_tm);

            tm curr_tm = {};
            std::istringstream iss_curr(uniqueDates[i]);
            iss_curr >> std::get_time(&curr_tm, "%Y-%m-%d");
            time_t curr_time_t = tmToTimeT(curr_tm);

            // Calculate difference in days (seconds / (seconds in a day))
            double diff_seconds = difftime(curr_time_t, prev_time_t);
            int diff_days = static_cast<int>(diff_seconds / (60 * 60 * 24));

            if (diff_days == 1) { // Consecutive day
                currentStreak++;
            } else if (diff_days > 1) { // Gap in days, reset streak
                currentStreak = 1;
            }
            maxStreak = std::max(maxStreak, currentStreak); // Update max streak
        }
    }

    // Check if a session was recorded today
    tm today_tm = getCurrentTm();
    bool hadSessionToday = false;
    for (const std::string& date_str : uniqueDates) {
        tm t = {};
        std::istringstream iss_date(date_str); // Corrected: Named istringstream
        iss_date >> std::get_time(&t, "%Y-%m-%d");
        if (t.tm_mday == today_tm.tm_mday && t.tm_mon == today_tm.tm_mon && t.tm_year == today_tm.tm_year) {
            hadSessionToday = true;
            break;
        }
    }

    std::cout << "\n--- Focus Streaks for " << currentUser << " ---\n";
    std::cout << "Current Streak: " << currentStreak << " consecutive days\n";
    std::cout << "Longest Streak: " << maxStreak << " consecutive days\n";
    if (!hadSessionToday) {
        std::cout << "Note: No session recorded today. Your current streak might reset tomorrow if you don't log a session.\n";
    }
}

// --- Main Menu ---

void FocusTracker::menu() {
    int choice;
    bool loggedIn = false;

    // Initial login/register loop
    while (!loggedIn) {
        std::cout << "\n==== Welcome to FocusTracker++ ====\n";
        std::cout << "1. Login\n";
        std::cout << "2. Register\n";
        std::cout << "3. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;
        clearInputBuffer(); // Clear newline after numeric input

        switch (choice) {
            case 1: loggedIn = loginUser(); break;
            case 2: registerUser(); break;
            case 3: std::cout << "Exiting...\n"; return; // Exit application
            default: std::cout << "Invalid option! Please try again.\n"; break;
        }
    }

    // Main application menu loop (after successful login)
    do {
        std::cout << "\n==== FocusTracker++ Menu (" << currentUser << ") ====\n";
        std::cout << "1. Start Manual Focus Session\n";
        std::cout << "2. Start Pomodoro Session\n";
        std::cout << "3. View Today's Summary\n";
        std::cout << "4. Generate Weekly Report (CSV)\n";
        std::cout << "5. Track Streaks\n";
        std::cout << "6. Logout\n";
        std::cout << "7. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;
        clearInputBuffer(); // Clear newline after numeric input

        switch (choice) {
            case 1: startSession(); break;
            case 2: startPomodoroSession(); break;
            case 3: loadDailySummary(); break;
            case 4: generateWeeklyReport(); break;
            case 5: trackStreaks(); break;
            case 6: 
                currentUser = ""; // Clear current user
                loggedIn = false; // Set loggedIn to false to re-enter login loop
                std::cout << "Logged out successfully.\n";
                // Re-enter the initial login/register loop
                while (!loggedIn) {
                    std::cout << "\n==== Welcome to FocusTracker++ ====\n";
                    std::cout << "1. Login\n";
                    std::cout << "2. Register\n";
                    std::cout << "3. Exit\n";
                    std::cout << "Enter your choice: ";
                    std::cin >> choice;
                    clearInputBuffer();

                    switch (choice) {
                        case 1: loggedIn = loginUser(); break;
                        case 2: registerUser(); break;
                        case 3: std::cout << "Exiting...\n"; return; // Exit application
                        default: std::cout << "Invalid option! Please try again.\n"; break;
                    }
                }
                break;
            case 7: std::cout << "Exiting...\n"; break; // Exit application
            default: std::cout << "Invalid option! Please try again.\n"; break;
        }
    } while (choice != 7);
}

// Main function to run the application
int main() {
    FocusTracker app; // Create an instance of the FocusTracker
    app.menu(); // Start the application menu
    return 0;
}
