#pragma once
#include <chrono>
#include <memory>
#include <string>

namespace gda_test {

class GodotProcess {
public:
    struct Impl;

    struct Options {
        std::string godot_path;
        std::string project_path;
        int port = 0;
        bool headless = true;
    };

    explicit GodotProcess(Options opts);
    ~GodotProcess();

    bool start(std::chrono::seconds ready_timeout = std::chrono::seconds(90));

    void stop();

    bool alive() const;
    int port() const;
    std::string capture_logs();
    std::string last_error() const;

    static std::string resolve_godot_path();

private:
    std::unique_ptr<Impl> impl_;
};

}
