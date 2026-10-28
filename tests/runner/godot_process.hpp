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
        std::string allow_list;
        bool run_import = true;
        std::string console_log_path;
    };

    explicit GodotProcess(Options opts);
    ~GodotProcess();

    bool start(std::chrono::seconds ready_timeout = std::chrono::seconds(90));

    void stop();

    void detach();

    bool alive() const;
    int port() const;
    long long pid() const;
    std::string capture_logs();
    std::string last_error() const;

    static std::string resolve_godot_path();

private:
    std::unique_ptr<Impl> impl_;
};

}
