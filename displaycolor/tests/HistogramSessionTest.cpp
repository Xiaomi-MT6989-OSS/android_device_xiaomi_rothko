// SPDX-License-Identifier: Apache-2.0
#include "HistogramSession.h"
#include <cstdlib>
#include <iostream>
using namespace rothko::color;
int main() {
    int assertions = 0;
    auto check = [&](bool value) {
        ++assertions;
        if (!value) { std::cerr << "failed assertion " << assertions << '\n'; std::abort(); }
    };
    HistogramSession session;
    PqReadback sample;
    sample.readStartedAt = 10; sample.transportOk = true; sample.shapeValid = true;
    sample.frame = 2; sample.bins[0] = 500;
    check(!session.command(0));
    check(!session.readable(0));
    session.scene(true, false);
    session.request(1, true, 0);
    check(!session.command(0)); // No OEM registration yet.
    session.oemConnected();
    auto command = session.command(0);
    check(command && !command->enable); // Clean ownership after process restart.
    session.complete(*command, true);
    command = session.command(0);
    check(command && command->enable);
    check(!session.readable(0)); // An unacknowledged command grants nothing.
    session.complete(*command, true);
    check(session.readable(0));
    auto generation = session.generation();
    check(session.publish(generation, sample, 10));
    check(session.snapshot(10).frame == 2);
    check(!session.readable(1200)); // Exact lease boundary, without worker cleanup.
    check(session.snapshot(1200).frame == 0);
    session.request(1, true, 1200);
    check(session.generation() != generation); // Expired renewal starts a new session.
    check(!session.publish(generation, sample, 1200));
    command = session.command(1200);
    check(command && !command->enable);
    session.complete(*command, true);
    command = session.command(1200);
    check(command && command->enable);
    generation = command->generation;
    // OFF arrives while enabling is in flight. It must remain un-readable;
    // the eventual success still needs a disabling command for the hardware.
    session.scene(false, false);
    session.complete(*command, true);
    check(!session.readable(1201));
    check(!session.publish(generation, sample, 1201));
    command = session.command(1201);
    check(command && !command->enable);
    session.complete(*command, true);
    check(!session.command(1201));
    session.scene(true, false);
    command = session.command(1202);
    check(command && command->enable);
    session.complete(*command, false); // May have reached the OEM despite failure.
    check(!session.readable(1202));
    command = session.command(1203);
    check(command && !command->enable);
    session.complete(*command, true);
    command = session.command(1204);
    check(command && command->enable);
    session.complete(*command, true);
    check(session.readable(1204));
    generation = session.generation();
    session.scene(true, true);
    check(session.sceneKnown() && session.hdr());
    check(!session.readable(1205));
    check(!session.publish(generation, sample, 1205));
    command = session.command(1205);
    check(command && !command->enable);
    session.complete(*command, true);
    check(!session.command(1205)); // No use of SDR histogram for HDR.
    session.scene(true, false);
    command = session.command(1206); session.complete(*command, true);
    generation = session.generation();
    session.request(2, true, 1206);
    check(!session.readable(1206));
    check(!session.publish(generation, sample, 1206));
    command = session.command(1206); check(command && !command->enable);
    session.complete(*command, true);
    command = session.command(1207); session.complete(*command, true);
    generation = session.generation();
    session.oemLost();
    check(!session.sceneKnown() && !session.readable(1208));
    check(!session.command(1208));
    check(!session.publish(generation, sample, 1208));
    session.oemConnected();
    command = session.command(1209); check(command && !command->enable);
    session.complete(*command, true);
    command = session.command(1209); session.complete(*command, true);
    session.release();
    check(!session.readable(1209));
    command = session.command(1209); check(command && !command->enable);
    session.complete(*command, true);
    check(!session.command(1209));
    session.request(3, true, 1210);
    command = session.command(1210); session.complete(*command, true);
    generation = session.generation();
    session.invalidate(); // New PQ connection, before old IO returns.
    check(!session.publish(generation, sample, 1211));
    check(!session.readable(1211));
    command = session.command(1211); check(command && !command->enable);
    session.complete(*command, true);
    command = session.command(1211); session.complete(*command, true);
    check(session.publish(session.generation(), sample, 1211));
    session.request(3, false, 1212);
    check(!session.readable(1212) && session.snapshot(1212).frame == 0);
    command = session.command(1212); check(command && !command->enable);
    session.complete(*command, true);
    session.request(4, true, 1213);
    command = session.command(1213); session.complete(*command, true);
    check(session.command(2413).has_value()); // Lease expiry also drives disable.
    check(!session.wanted(2413));
    std::cout << "PASS: " << assertions << " histogram ownership/lease/generation assertions\n";
}
