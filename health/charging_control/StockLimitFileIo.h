/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "StockLimitController.h"

#include <charconv>
#include <climits>
#include <fcntl.h>
#include <limits>
#include <linux/magic.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <utility>
#include <unistd.h>

namespace rothko::charging {

inline std::optional<uint32_t> parseStockFlags(std::string_view text) {
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    while (!text.empty() && space(text.front())) text.remove_prefix(1);
    while (!text.empty() && space(text.back())) text.remove_suffix(1);
    if (text.empty()) return std::nullopt;
    uint32_t value;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        value > INT32_MAX) {
        return std::nullopt;
    }
    return value;
}

// Not installed or instantiated by any product/service. Production construction
// binds only the verified stock attribute. Tests use a protected path constructor
// and replace node admission; production always requires a root-owned sysfs file.
class StockLimitFileIo : public StockLimitIo {
  public:
    StockLimitFileIo() : StockLimitFileIo(kNodePath) {}
    ~StockLimitFileIo() override { dropBinding(); }
    StockLimitFileIo(const StockLimitFileIo&) = delete;
    StockLimitFileIo& operator=(const StockLimitFileIo&) = delete;

    std::optional<DriverFlags> readFlags() override {
        std::lock_guard<std::mutex> guard(mMutex);
        if (!ensureBinding()) return std::nullopt;
        char buffer[65];
        const ssize_t count = readAt(mFd, buffer, sizeof(buffer), 0);
        if (count <= 0 || count >= static_cast<ssize_t>(sizeof(buffer))) {
            dropBinding();
            return std::nullopt;
        }
        char extra;
        if (readAt(mFd, &extra, 1, count) != 0) {
            // A syntactically valid prefix is not a complete node response.
            dropBinding();
            return std::nullopt;
        }
        const auto flags = parseStockFlags(std::string_view(buffer, count));
        if (!flags || !bindingIsCurrent()) {
            dropBinding();
            return std::nullopt;
        }
        return DriverFlags{*flags, mGeneration};
    }

    bool writePacket(int32_t packet) override {
        // This method cannot open/rebind a node or write a global selector.
        // A successful pre-read must establish the binding first.
        if (packet < 0) return false;
        const int max = packet >> 16;
        const auto encoded = encodeLimit({(packet & 1) ? max - 5 : 0, max});
        if (!encoded || *encoded != packet) return false;

        std::lock_guard<std::mutex> guard(mMutex);
        if (!bindingIsCurrent()) {
            dropBinding();
            return false;
        }
        const std::string command = std::to_string(packet) + "\n";
        // One complete sysfs command only. Do not retry a partial write or
        // EINTR by sending a second command that could clear triggered state.
        const ssize_t count = writeAtZero(mFd, command.data(), command.size());
        if (count != static_cast<ssize_t>(command.size()) || !bindingIsCurrent()) {
            dropBinding();
            return false;
        }
        return true;
    }

    // For an explicit driver reset notification that does not replace the node.
    // The next read rebinds with a new generation; this method never writes.
    void invalidateBinding() {
        std::lock_guard<std::mutex> guard(mMutex);
        dropBinding();
    }

  protected:
    explicit StockLimitFileIo(std::string path) : mPath(std::move(path)) {}

    virtual bool admitNode(int fd, const struct stat& info) const {
        struct statfs filesystem {};
        return fstatfs(fd, &filesystem) == 0 && filesystem.f_type == SYSFS_MAGIC &&
                S_ISREG(info.st_mode) && info.st_uid == 0 && !(info.st_mode & S_IWOTH);
    }

    virtual ssize_t writeAtZero(int fd, const void* data, size_t count) {
        return pwrite(fd, data, count, 0);
    }

    virtual ssize_t readAt(int fd, void* data, size_t count, off_t offset) {
        return pread(fd, data, count, offset);
    }

  private:
    static constexpr const char* kNodePath =
            "/sys/devices/platform/11d02000.i2c/i2c-5/5-0034/"
            "11d02000.i2c:mt6375@34:mtk-gauge/power_supply/battery/smart_chg";

    bool bindingIsCurrent() const {
        if (mFd < 0) return false;
        struct stat pathInfo {}, fdInfo {};
        return lstat(mPath.c_str(), &pathInfo) == 0 && fstat(mFd, &fdInfo) == 0 &&
                S_ISREG(pathInfo.st_mode) && pathInfo.st_dev == fdInfo.st_dev &&
                pathInfo.st_ino == fdInfo.st_ino && admitNode(mFd, fdInfo);
    }

    bool ensureBinding() {
        if (bindingIsCurrent()) return true;
        // Keep the old fd alive until replacement has been observed. A fresh
        // binding always gets a new generation, even if an inode is reused.
        dropBinding();
        if (mGeneration == std::numeric_limits<uint64_t>::max()) return false;
        mFd = open(mPath.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (!bindingIsCurrent()) {
            dropBinding();
            return false;
        }
        ++mGeneration;
        return true;
    }

    void dropBinding() {
        if (mFd >= 0) close(mFd);
        mFd = -1;
    }

    const std::string mPath;
    std::mutex mMutex;
    int mFd = -1;
    uint64_t mGeneration = 0;
};

}  // namespace rothko::charging
