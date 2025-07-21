#include <iostream>
#include <chrono>
#include <thread>

int main() {
    std::cout << "Sleeping for 2 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Awake!\n";
    return 0;
}