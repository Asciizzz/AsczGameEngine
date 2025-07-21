#pragma once

#include <AzVulk/Pipeline.hpp>

namespace AzVulk {

class Manager {
public: 
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    Manager();
    ~Manager();

    // Not copyable or movable
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;

    void run();

private:
    Device device;
    SwapChain swapChain;
    Pipeline pipeline;
    Model model;

};

} // namespace AzVulk