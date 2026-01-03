#include <iostream>
#include <fstream>
#include <ctime>
#include <string>
#include <vector>
#include <iomanip>
#include <map>
#include <sstream>
#include <chrono>
#include <thread>
#include <limits>
#include <algorithm>

std::string hashPassword(const std::string& password) {
    long long hash = 0;
    for (char c : password) {
        hash = (hash * 31 + c) % 1000000007;
    }
    return std::to_string(hash);
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

tm getCurrentTm() {
    time_t now = time(nullptr);
    tm current_tm;
#ifdef _WIN32
    current_tm = *localtime(&now);
#else
    localtime_r(&now, &current_tm);
#endif
    return current_tm;
}

time_t tmToTimeT(tm& t) {
    return mktime(&t);
}

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

struct Session {
    std::string category;
    time_t startTime;
    time_t endTime;
    int duration;

    void computeDuration() {
        duration = static_cast<int>(difftime(endTime, startTime) / 60);
    }

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

struct User {
    std::string username;
    std::string hashedPassword;
};

class FocusTracker {
private:
    std::string currentUser;
    std::map<std::string, User> users;
    std::string usersFile = "users.txt";
    std::string logFile;

    void loadUsers();
    void saveUsers();
    bool registerUser();
    bool loginUser();
    void setLogFileForCurrentUser();

public:
    FocusTracker();
    void startSession(const std::string& category = "");
    void endSession(Session &session);
    void logSessionToFile(const Session &session);
    void loadDailySummary();
    void startPomodoroSession();
    void generateWeeklyReport();
    void trackStreaks();
    void menu();
};

FocusTracker::FocusTracker() {
    loadUsers();
}

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
        if (std::getline(ss, username, ',') && std::getline(ss, hashedPassword)) {
            users[username] = {username, hashedPassword};
        }
    }
    fin.close();
}

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

bool FocusTracker::registerUser() {
    std::string username, password;
    std::cout << "\n--- Register New User ---\n";
    std::cout << "Enter desired username: ";
    std::cin >> username;
    clearInputBuffer();

    if (users.count(username)) {
        std::cout << "Username already exists. Please choose a different one.\n";
        return false;
    }

    std::cout << "Enter password: ";
    std::cin >> password;
    clearInputBuffer();

    users[username] = {username, hashPassword(password)};
    saveUsers();
    std::cout << "User '" << username << "' registered successfully!\n";
    return true;
}

bool FocusTracker::loginUser() {
    std::string username, password;
    std::cout << "\n--- Login ---\n";
    std::cout << "Enter username: ";
    std::cin >> username;
    clearInputBuffer();

    std::cout << "Enter password: ";
    std::cin >> password;
    clearInputBuffer();

    if (users.count(username) && users[username].hashedPassword == hashPassword(password)) {
        currentUser = username;
        setLogFileForCurrentUser();
        std::cout << "Welcome, " << currentUser << "!\n";
        return true;
    } else {
        std::cout << "Invalid username or password.\n";
        return false;
    }
}

void FocusTracker::setLogFileForCurrentUser() {
    logFile = "focus_log_" + currentUser + ".txt";
}

void FocusTracker::startSession(const std::string& predefinedCategory) {
    Session s;
    if (predefinedCategory.empty()) {
        std::cout << "\nEnter focus category (Study/Work/Reading/etc.): ";
        std::cin >> std::ws;
        std::getline(std::cin, s.category);
    } else {
        s.category = predefinedCategory;
    }

    s.startTime = time(nullptr);

    tm start_tm;
#ifdef _WIN32
    start_tm = *localtime(&s.startTime);
#else
    localtime_r(&s.startTime, &start_tm);
#endif
    std::cout << "Session started at " << std::put_time(&start_tm, "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "Press ENTER to end session...";
    std::cin.get();

    endSession(s);
}

void FocusTracker::endSession(Session &session) {
    session.endTime = time(nullptr);
    session.computeDuration();
    logSessionToFile(session);

    std::cout << "\nSession ended. Summary:\n";
    session.display();
}

void FocusTracker::logSessionToFile(const Session &session) {
    std::ofstream fout(logFile, std::ios::app);
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

void FocusTracker::loadDailySummary() {
    std::ifstream fin(logFile);
    if (!fin.is_open()) {
        std::cout << "No focus sessions logged yet for " << currentUser << ".\n";
        return;
    }

    std::string line;
    std::map<std::string, int> categoryDuration;
    tm now_tm = getCurrentTm();

    while (std::getline(fin, line)) {
        std::stringstream ss(line);
        std::string cat_str, start_str, end_str, dur_str;

        if (std::getline(ss, cat_str, ',') &&
            std::getline(ss, start_str, ',') &&
            std::getline(ss, end_str, ',') &&
            std::getline(ss, dur_str)) {

            time_t start_time = static_cast<time_t>(std::stoll(start_str));
            int duration = std::stoi(dur_str);

            tm start_tm_session;
#ifdef _WIN32
            start_tm_session = *localtime(&start_time);
#else
            localtime_r(&start_time, &start_tm_session);
#endif
            if (start_tm_session.tm_mday == now_tm.tm_mday &&
                start_tm_session.tm_mon == now_tm.tm_mon &&
                start_tm_session.tm_year == now_tm.tm_year) {
                categoryDuration[cat_str] += duration;
            }
        }
    }
    fin.close();

    std::cout << "\nToday's Focus Summary for " << currentUser << ":\n";
    if (categoryDuration.empty()) {
        std::cout << "No sessions recorded today.\n";
    } else {
        for (auto const& pair : categoryDuration) {
            std::cout << " - " << pair.first << ": " << pair.second << " minutes\n";
        }
    }
}

void FocusTracker::startPomodoroSession() {
    int focusDuration, breakDuration, numCycles;
    std::cout << "\n--- Start Pomodoro Session ---\n";
    std::cout << "Enter focus duration (minutes): ";
    std::cin >> focusDuration;
    std::cout << "Enter break duration (minutes): ";
    std::cin >> breakDuration;
    std::cout << "Enter number of cycles: ";
    std::cin >> numCycles;
    clearInputBuffer();

    std::cout << "Enter focus category for Pomodoro sessions: ";
    std::string category;
    std::cin >> std::ws;
    std::getline(std::cin, category);

    for (int i = 1; i <= numCycles; ++i) {
        std::cout << "\n--- Cycle " << i << "/" << numCycles << " ---\n";
        std::cout << "Focus Time! (" << focusDuration << " minutes) - Category: " << category << "\n";

        Session s;
        s.category = category;
        s.startTime = time(nullptr);

        for (int j = focusDuration; j > 0; --j) {
            std::cout << "Time remaining: " << j << " minutes...\r" << std::flush;
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
        std::cout << "Focus time ended!                                   \n";

        s.endTime = time(nullptr);
        s.computeDuration();
        logSessionToFile(s);

        if (i < numCycles) {
            std::cout << "Break Time! (" << breakDuration << " minutes)\n";
            for (int j = breakDuration; j > 0; --j) {
                std::cout << "Time remaining: " << j << " minutes...\r" << std::flush;
                std::this_thread::sleep_for(std::chrono::minutes(1));
            }
            std::cout << "Break time ended!                                   \n";
        }
    }
    std::cout << "\nPomodoro session completed!\n";
}

void FocusTracker::generateWeeklyReport() {
    std::ifstream fin(logFile);
    if (!fin.is_open()) {
        std::cout << "No focus sessions logged yet for " << currentUser << ".\n";
        return;
    }

    std::map<std::string, std::map<std::string, int>> weeklyData;
    tm now_tm = getCurrentTm();

    tm start_of_week_tm = now_tm;
    start_of_week_tm.tm_mday -= (start_of_week_tm.tm_wday == 0 ? 6 : start_of_week_tm.tm_wday - 1);
    mktime(&start_of_week_tm);

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

            if (session_start_time >= start_of_week_time && session_start_time <= time(nullptr)) {
                std::string date_str = timeTToDateString(session_start_time);
                weeklyData[date_str][cat_str] += duration;
            }
        }
    }
    fin.close();

    std::ofstream fout("weekly_report_" + currentUser + ".csv");
    if (!fout.is_open()) {
        std::cerr << "Error: Could not open weekly report file for writing.\n";
        return;
    }

    fout << "Date,Category,Total Duration (minutes)\n";

    std::vector<std::string> sortedDates;
    for (auto const& pair : weeklyData) {
        sortedDates.push_back(pair.first);
    }
    std::sort(sortedDates.begin(), sortedDates.end());

    for (const std::string& date : sortedDates) {
        for (auto const& pair : weeklyData[date]) {
            fout << date << "," << pair.first << "," << pair.second << "\n";
        }
    }
    fout.close();

    std::cout << "\nWeekly report generated successfully for " << currentUser
              << " at weekly_report_" << currentUser << ".csv\n";
}

void FocusTracker::trackStreaks() {
    std::ifstream fin(logFile);
    if (!fin.is_open()) {
        std::cout << "No focus sessions logged yet for " << currentUser << ".\n";
        return;
    }

    std::vector<time_t> sessionTimestamps;
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

    std::vector<std::string> uniqueDates;
    for (time_t t : sessionTimestamps) {
        uniqueDates.push_back(timeTToDateString(t));
    }
    std::sort(uniqueDates.begin(), uniqueDates.end());
    uniqueDates.erase(std::unique(uniqueDates.begin(), uniqueDates.end()), uniqueDates.end());

    int currentStreak = 0;
    int maxStreak = 0;

    if (!uniqueDates.empty()) {
        currentStreak = 1;
        maxStreak = 1;

        for (size_t i = 1; i < uniqueDates.size(); ++i) {
            tm prev_tm = {};
            std::istringstream iss_prev(uniqueDates[i - 1]);
            iss_prev >> std::get_time(&prev_tm, "%Y-%m-%d");
            time_t prev_time_t = tmToTimeT(prev_tm);

            tm curr_tm = {};
            std::istringstream iss_curr(uniqueDates[i]);
            iss_curr >> std::get_time(&curr_tm, "%Y-%m-%d");
            time_t curr_time_t = tmToTimeT(curr_tm);

            int diff_days = static_cast<int>(difftime(curr_time_t, prev_time_t) / (60 * 60 * 24));

            if (diff_days == 1) {
                currentStreak++;
            } else if (diff_days > 1) {
                currentStreak = 1;
            }
            maxStreak = std::max(maxStreak, currentStreak);
        }
    }

    tm today_tm = getCurrentTm();
    bool hadSessionToday = false;
    for (const std::string& date_str : uniqueDates) {
        tm t = {};
        std::istringstream iss_date(date_str);
        iss_date >> std::get_time(&t, "%Y-%m-%d");
        if (t.tm_mday == today_tm.tm_mday &&
            t.tm_mon == today_tm.tm_mon &&
            t.tm_year == today_tm.tm_year) {
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

void FocusTracker::menu() {
    int choice;
    bool loggedIn = false;

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
            case 3: std::cout << "Exiting...\n"; return;
            default: std::cout << "Invalid option! Please try again.\n";
        }
    }

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
        clearInputBuffer();

        switch (choice) {
            case 1: startSession(); break;
            case 2: startPomodoroSession(); break;
            case 3: loadDailySummary(); break;
            case 4: generateWeeklyReport(); break;
            case 5: trackStreaks(); break;
            case 6:
                currentUser = "";
                loggedIn = false;
                std::cout << "Logged out successfully.\n";
                menu();
                return;
            case 7: std::cout << "Exiting...\n"; break;
            default: std::cout << "Invalid option! Please try again.\n";
        }
    } while (choice != 7);
}

int main() {
    FocusTracker app;
    app.menu();
    return 0;
}
