/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../StockLimitFileIo.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace rothko::charging;

static unsigned checks;
static void check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
    ++checks;
}

struct TemporaryIo : StockLimitFileIo {
    explicit TemporaryIo(const std::string& path) : StockLimitFileIo(path) {}
    bool admitNode(int, const struct stat& info) const override { return S_ISREG(info.st_mode); }
    int writeCalls = 0;
    int failure = 0;
    bool shortRead = false;
    ssize_t readAt(int fd, void* data, size_t size, off_t offset) override {
        return StockLimitFileIo::readAt(fd, data, shortRead && offset == 0 ? 1 : size, offset);
    }
    ssize_t writeAtZero(int fd, const void* data, size_t size) override {
        ++writeCalls;
        if (failure == 1) return size - 1;
        if (failure == 2) { errno = EINTR; return -1; }
        return StockLimitFileIo::writeAtZero(fd, data, size);
    }
};

// Keeps the real sysfs admission policy; it must reject the temporary filesystem.
struct StrictIo : StockLimitFileIo {
    explicit StrictIo(const std::string& path) : StockLimitFileIo(path) {}
};

struct ReplacingIo : TemporaryIo {
    std::string path, moved;
    ReplacingIo(const std::string& path, const std::string& moved)
        : TemporaryIo(path), path(path), moved(moved) {}
    ssize_t writeAtZero(int fd, const void* data, size_t count) override {
        const auto result = TemporaryIo::writeAtZero(fd, data, count);
        check(rename(path.c_str(), moved.c_str()) == 0, "replacement during write");
        std::ofstream out(path);
        out << "2\n";
        check(out.good(), "new node created during write");
        return result;
    }
};

static void put(const std::string& path, const std::string& value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << value;
    check(out.good(), "temporary fixture write");
}
static std::string contents(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int main() {
    check(parseStockFlags("0\n") == 0, "zero flags");
    check(parseStockFlags(" \t589834\r\n") == 589834, "decimal flags with whitespace");
    check(parseStockFlags("2147483647") == INT32_MAX, "signed driver integer maximum");
    for (auto text : {"", " \n", "-1", "+2", "0x2", "2x", "2 3", "2147483648", "999999999999999999999"}) {
        check(!parseStockFlags(text), "reject malformed/overflow flags");
    }
    check(!parseStockFlags(std::string_view("2\0x", 3)), "reject embedded NUL");

    char pattern[] = "/tmp/rothko-limit-io-XXXXXX";
    const char* dir = mkdtemp(pattern);
    check(dir != nullptr, "temporary directory creation");
    const std::string path = std::string(dir) + "/smart_chg";
    const std::string old = std::string(dir) + "/old";
    const auto packet = *encodeLimit({75, 80});
    {
        TemporaryIo io(path);
        check(!io.readFlags(), "missing node fails");
        check(!io.writePacket(packet), "write without pre-read never opens a node");
        put(path, "2\n");
        StrictIo strict(path);
        check(!strict.readFlags(), "production admission rejects regular test filesystem");
        auto first = io.readFlags();
        check(first && first->flags == 2 && first->generation != 0, "first binding has identity");
        check(!io.writePacket(0) && !io.writePacket((80 << 16) | 7), "reject global/multiselector commands");
        check(io.writeCalls == 0 && contents(path) == "2\n", "invalid commands do not write");
        check(io.writePacket(packet) && contents(path) == "5242883\n", "one complete decimal command");
        auto same = io.readFlags();
        check(same && same->generation == first->generation, "stable binding retains generation");

        check(rename(path.c_str(), old.c_str()) == 0, "move original node fixture");
        put(path, "2\n");
        check(!io.writePacket(packet) && contents(path) == "2\n", "replacement before write is not rebound blindly");
        auto replacement = io.readFlags();
        check(replacement && replacement->flags == 2 && replacement->generation > first->generation,
              "replacement with same flags gets fresh generation");
        io.invalidateBinding();
        check(!io.writePacket(packet), "explicit reset drops the binding without writing");
        auto reset = io.readFlags();
        check(reset && reset->generation > replacement->generation,
              "explicit in-place reset forces a fresh generation");
        io.shortRead = true;
        check(!io.readFlags(), "valid integer prefix from a short read is not accepted");
        check(!io.writePacket(packet), "short read discards binding");
        io.shortRead = false;
        check(io.readFlags().has_value(), "full read can rebind after a short response");

        io.failure = 1;
        check(!io.writePacket(packet) && contents(path) == "2\n", "short write fails without remainder retry");
        check(!io.writePacket(packet), "short write discards binding");
        check(io.readFlags().has_value(), "rebind after short write");
        const int before = io.writeCalls;
        io.failure = 2;
        check(!io.writePacket(packet) && io.writeCalls == before + 1, "EINTR fails without automatic command retry");
        io.failure = 0;
        put(path, "invalid\n");
        check(!io.readFlags() && !io.writePacket(packet), "parse error invalidates binding");
        put(path, std::string(65, '1'));
        check(!io.readFlags(), "oversize response rejected");
        put(path, "");
        check(!io.readFlags(), "empty response rejected");
        check(unlink(path.c_str()) == 0 && symlink(old.c_str(), path.c_str()) == 0, "symlink fixture");
        check(!io.readFlags() && !io.writePacket(packet), "final symlink is not followed");
        check(unlink(path.c_str()) == 0 && mkfifo(path.c_str(), 0600) == 0, "FIFO fixture");
        check(!io.readFlags(), "nonregular node rejected without blocking");
    }
    check(unlink(path.c_str()) == 0, "remove FIFO fixture");
    put(path, "2\n");
    const std::string duringWrite = std::string(dir) + "/during-write";
    {
        ReplacingIo io(path, duringWrite);
        auto before = io.readFlags();
        check(before.has_value(), "bind before in-flight replacement");
        check(!io.writePacket(packet) && contents(path) == "2\n" &&
                  contents(duringWrite) == "5242883\n",
              "write to retired node is not acknowledged as a write to its replacement");
        auto after = io.readFlags();
        check(after && after->generation > before->generation,
              "in-flight replacement is rebound with a fresh generation");
    }
    check(unlink(duringWrite.c_str()) == 0, "retired in-flight fixture removed");
    check(unlink(path.c_str()) == 0 && unlink(old.c_str()) == 0 && rmdir(dir) == 0,
          "temporary fixtures removed");
    std::cout << "PASS: " << checks << " node I/O and fixture checks; no sysfs access\n";
}
