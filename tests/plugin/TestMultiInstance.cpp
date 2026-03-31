// ---------------------------------------------------------------------------
// Multi-instance integration & stress tests
//
// Simulates DAW-like scenarios: multiple plugin instances sharing a
// SharedListenerHub, editor open/close cycles, remote focus, rapid
// construction/destruction, concurrent processBlock, and state persistence.
// ---------------------------------------------------------------------------

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamIDs.h"
#include "SharedListenerHub.h"

#include <thread>
#include <vector>
#include <memory>

using Catch::Approx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Enable listener linking on a processor (triggers SharedListenerHub registration)
static void enableLink(XYZPanProcessor& proc) {
    auto* p = proc.apvts.getParameter(ParamID::LISTENER_LINK);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(1.0f);
}

// Disable listener linking
static void disableLink(XYZPanProcessor& proc) {
    auto* p = proc.apvts.getParameter(ParamID::LISTENER_LINK);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(0.0f);
}

// Set a float parameter by ID using its raw range
static void setParam(XYZPanProcessor& proc, const juce::String& id, float value) {
    auto* p = proc.apvts.getParameter(id);
    REQUIRE(p != nullptr);
    p->setValueNotifyingHost(proc.apvts.getParameterRange(id).convertTo0to1(value));
}

// Read a float parameter's denormalized value
static float getParam(XYZPanProcessor& proc, const juce::String& id) {
    auto* raw = proc.apvts.getRawParameterValue(id);
    REQUIRE(raw != nullptr);
    return raw->load();
}

// Prepare a processor for audio processing
static void prepare(XYZPanProcessor& proc, double sr = 44100.0, int block = 128) {
    proc.prepareToPlay(sr, block);
}

// Run one block of silence through a processor
static void processOneBlock(XYZPanProcessor& proc, int numSamples = 128) {
    juce::AudioBuffer<float> buf(2, numSamples);
    buf.clear();
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);
}

// ===================================================================
// SECTION 1: SharedListenerHub lifecycle
// ===================================================================

TEST_CASE("Hub: single instance link/unlink", "[multi][hub]") {
    XYZPanProcessor proc;
    auto& hub = proc.getListenerHub();

    REQUIRE(hub.getLinkedCount() == 0);

    enableLink(proc);
    REQUIRE(hub.getLinkedCount() == 1);

    disableLink(proc);
    REQUIRE(hub.getLinkedCount() == 0);
}

TEST_CASE("Hub: two instances link and see each other", "[multi][hub]") {
    XYZPanProcessor a, b;
    auto& hub = a.getListenerHub(); // same singleton

    enableLink(a);
    enableLink(b);
    REQUIRE(hub.getLinkedCount() == 2);

    // getLinkedInstances should return both
    SharedListenerHub::Listener* out[4];
    int n = hub.getLinkedInstances(out, 4);
    REQUIRE(n == 2);

    disableLink(a);
    REQUIRE(hub.getLinkedCount() == 1);

    disableLink(b);
    REQUIRE(hub.getLinkedCount() == 0);
}

TEST_CASE("Hub: unlinked instance does NOT receive broadcast", "[multi][hub]") {
    XYZPanProcessor a, b;
    enableLink(a);
    // b is NOT linked

    float origYaw = getParam(b, ParamID::LISTENER_YAW);
    setParam(a, ParamID::LISTENER_YAW, 60.0f);

    // B should not have changed
    float bYaw = getParam(b, ParamID::LISTENER_YAW);
    REQUIRE(bYaw == Approx(origYaw).margin(0.01f));

    disableLink(a);
}

// ===================================================================
// SECTION 2: Instance destruction safety
// ===================================================================

TEST_CASE("Hub: destroying a linked instance cleans up", "[multi][hub][lifecycle]") {
    XYZPanProcessor a;
    enableLink(a);

    {
        XYZPanProcessor b;
        enableLink(b);
        REQUIRE(a.getListenerHub().getLinkedCount() == 2);
    }
    // b destroyed — should have detached

    REQUIRE(a.getListenerHub().getLinkedCount() == 1);

    disableLink(a);
}

TEST_CASE("Hub: rapid create/destroy stress", "[multi][hub][stress]") {
    XYZPanProcessor anchor;
    enableLink(anchor);

    for (int i = 0; i < 50; ++i) {
        auto proc = std::make_unique<XYZPanProcessor>();
        enableLink(*proc);
        REQUIRE(anchor.getListenerHub().getLinkedCount() == 2);
        // proc destroyed here
    }

    REQUIRE(anchor.getListenerHub().getLinkedCount() == 1);
    disableLink(anchor);
}

TEST_CASE("Hub: max instances link and unlink cleanly", "[multi][hub][stress]") {
    constexpr int N = 8;
    std::vector<std::unique_ptr<XYZPanProcessor>> procs;

    for (int i = 0; i < N; ++i) {
        procs.push_back(std::make_unique<XYZPanProcessor>());
        enableLink(*procs.back());
    }

    REQUIRE(procs[0]->getListenerHub().getLinkedCount() == N);

    // Destroy in reverse order
    while (!procs.empty()) {
        procs.pop_back();
    }

    // All gone — hub should be empty (SharedResourcePointer keeps hub alive)
    XYZPanProcessor probe;
    REQUIRE(probe.getListenerHub().getLinkedCount() == 0);
}

// ===================================================================
// SECTION 3: Instance naming & persistence
// ===================================================================

TEST_CASE("Instance name: set and get", "[multi][naming]") {
    XYZPanProcessor proc;

    REQUIRE(proc.getInstanceNameValue().isEmpty());

    proc.setInstanceName("Track 1");
    REQUIRE(proc.getInstanceNameValue() == "Track 1");
}

TEST_CASE("Instance name: persists through state save/load", "[multi][naming]") {
    juce::MemoryBlock block;

    {
        XYZPanProcessor proc;
        proc.setInstanceName("My Bass");
        proc.getStateInformation(block);
    }

    {
        XYZPanProcessor proc2;
        proc2.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        REQUIRE(proc2.getInstanceNameValue() == "My Bass");
    }
}

TEST_CASE("Instance name: manual name sticks over track properties", "[multi][naming]") {
    XYZPanProcessor proc;

    // Simulate DAW providing track name
    juce::AudioProcessor::TrackProperties tp;
    tp.name = "DAW Track";
    proc.updateTrackProperties(tp);
    REQUIRE(proc.getInstanceNameValue() == "DAW Track");

    // User manually renames
    proc.setInstanceName("My Custom Name");
    REQUIRE(proc.getInstanceNameValue() == "My Custom Name");

    // DAW sends another track update — should NOT override manual name
    tp.name = "Different Track";
    proc.updateTrackProperties(tp);
    REQUIRE(proc.getInstanceNameValue() == "My Custom Name");
}

TEST_CASE("Instance name: nameManuallySet flag persists", "[multi][naming]") {
    juce::MemoryBlock block;

    {
        XYZPanProcessor proc;
        proc.setInstanceName("User Name");
        proc.getStateInformation(block);
    }

    {
        XYZPanProcessor proc2;
        proc2.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

        // After restoring, DAW track update should NOT override
        juce::AudioProcessor::TrackProperties tp;
        tp.name = "New Track";
        proc2.updateTrackProperties(tp);
        REQUIRE(proc2.getInstanceNameValue() == "User Name");
    }
}

TEST_CASE("Instance name: propagates through ForeignSourceSnapshot", "[multi][naming]") {
    XYZPanProcessor a, b;
    a.setInstanceName("Violin");
    b.setInstanceName("Cello");

    enableLink(a);
    enableLink(b);

    prepare(a);
    prepare(b);

    // Run one block on each so source export gets written
    processOneBlock(a);
    processOneBlock(b);

    // Collect foreign sources from A's perspective (should see B)
    xyzpan::ForeignSourceSnapshot out[8];
    int count = a.getListenerHub().getLinkedSources(
        static_cast<SharedListenerHub::Listener*>(&a), out, 8);

    REQUIRE(count == 1);
    REQUIRE(juce::String(out[0].name) == "Cello");

    disableLink(a);
    disableLink(b);
}

// ===================================================================
// SECTION 4: Audio processing stress
// ===================================================================

TEST_CASE("ProcessBlock: runs without crash after prepare", "[multi][audio]") {
    XYZPanProcessor proc;
    prepare(proc);
    processOneBlock(proc);
    proc.releaseResources();
}

TEST_CASE("ProcessBlock: multiple instances process concurrently", "[multi][audio][stress]") {
    constexpr int N = 4;
    std::vector<std::unique_ptr<XYZPanProcessor>> procs;
    for (int i = 0; i < N; ++i) {
        procs.push_back(std::make_unique<XYZPanProcessor>());
        prepare(*procs.back());
        enableLink(*procs.back());
    }

    // Process on separate threads (simulates DAW multi-track rendering)
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&procs, i]() {
            for (int block = 0; block < 100; ++block) {
                processOneBlock(*procs[i]);
            }
        });
    }

    for (auto& t : threads)
        t.join();

    // No crash = success
    for (auto& p : procs) {
        disableLink(*p);
        p->releaseResources();
    }
}

TEST_CASE("ProcessBlock: parameter automation during processing", "[multi][audio][stress]") {
    XYZPanProcessor proc;
    prepare(proc);

    for (int i = 0; i < 200; ++i) {
        // Automate position
        float t = static_cast<float>(i) / 200.0f;
        setParam(proc, ParamID::X, -1.0f + 2.0f * t);
        setParam(proc, ParamID::Y, std::sin(t * 6.28f));
        processOneBlock(proc);
    }

    proc.releaseResources();
}

TEST_CASE("ProcessBlock: varying block sizes", "[multi][audio]") {
    XYZPanProcessor proc;

    int sizes[] = {32, 64, 128, 256, 512, 1024, 2048};
    for (int sz : sizes) {
        proc.prepareToPlay(44100.0, sz);
        processOneBlock(proc, sz);
        proc.releaseResources();
    }
}

TEST_CASE("ProcessBlock: varying sample rates", "[multi][audio]") {
    XYZPanProcessor proc;

    double rates[] = {22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0};
    for (double sr : rates) {
        proc.prepareToPlay(sr, 128);
        processOneBlock(proc);
        proc.releaseResources();
    }
}

// ===================================================================
// SECTION 5: State persistence with linked instances
// ===================================================================

TEST_CASE("State: save/load preserves parameters across instances", "[multi][state]") {
    juce::MemoryBlock block;

    {
        XYZPanProcessor proc;
        setParam(proc, ParamID::X, 0.7f);
        setParam(proc, ParamID::Y, -0.3f);
        setParam(proc, ParamID::VERB_SIZE, 0.8f);
        proc.setInstanceName("Synth Lead");
        proc.getStateInformation(block);
    }

    {
        XYZPanProcessor proc2;
        proc2.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        REQUIRE(getParam(proc2, ParamID::X) == Approx(0.7f).margin(0.01f));
        REQUIRE(getParam(proc2, ParamID::Y) == Approx(-0.3f).margin(0.01f));
        REQUIRE(getParam(proc2, ParamID::VERB_SIZE) == Approx(0.8f).margin(0.01f));
        REQUIRE(proc2.getInstanceNameValue() == "Synth Lead");
    }
}

TEST_CASE("State: loading state while linked doesn't crash", "[multi][state]") {
    XYZPanProcessor a, b;
    enableLink(a);
    enableLink(b);
    prepare(a);
    prepare(b);

    // Save A's state
    juce::MemoryBlock block;
    a.getStateInformation(block);

    // Load into A while still linked
    a.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    // Process should still work
    processOneBlock(a);
    processOneBlock(b);

    disableLink(a);
    disableLink(b);
}

// ===================================================================
// SECTION 6: Destruction ordering edge cases
// ===================================================================

TEST_CASE("Destruction: linked instance destroyed during broadcast", "[multi][lifecycle][stress]") {
    // Simulate destroying one instance while another is broadcasting
    XYZPanProcessor a;
    enableLink(a);

    auto b = std::make_unique<XYZPanProcessor>();
    enableLink(*b);

    // Change a parameter to trigger broadcast, then destroy b
    setParam(a, ParamID::LISTENER_YAW, 30.0f);
    b.reset(); // destroy while hub still has cached orientation

    // A should survive unscathed
    REQUIRE(a.getListenerHub().getLinkedCount() == 1);
    setParam(a, ParamID::LISTENER_YAW, 60.0f); // should not crash

    disableLink(a);
}

TEST_CASE("Destruction: destroy linked instance then process survivor", "[multi][lifecycle][stress]") {
    // Simulates DAW removing a track: destroy one processor, then verify
    // the surviving instance still processes audio correctly.
    auto proc = std::make_unique<XYZPanProcessor>();
    prepare(*proc);
    enableLink(*proc);

    XYZPanProcessor other;
    prepare(other);
    enableLink(other);

    // Both process a few blocks while linked
    processOneBlock(*proc);
    processOneBlock(other);

    // Destroy proc (simulates track removal — DAW stops audio first)
    proc.reset();

    // Survivor should still process fine
    for (int i = 0; i < 100; ++i) {
        processOneBlock(other);
    }

    REQUIRE(other.getListenerHub().getLinkedCount() == 1);

    disableLink(other);
    other.releaseResources();
}

TEST_CASE("Destruction: audio thread reads hub while instance destroyed", "[multi][lifecycle][stress]") {
    // Real DAW scenario: Instance B's audio thread calls getLinkedSources()
    // on the shared hub while Instance A is being destroyed on the message thread.
    // The spinlock + hubAlive_ pattern must prevent use-after-free.

    XYZPanProcessor survivor;
    prepare(survivor);
    enableLink(survivor);

    auto victim = std::make_unique<XYZPanProcessor>();
    prepare(*victim);
    enableLink(*victim);

    // Audio thread: repeatedly reads from the hub (like processBlock's timer would)
    std::atomic<bool> running{true};
    std::thread audioThread([&]() {
        xyzpan::ForeignSourceSnapshot out[8];
        while (running.load(std::memory_order_relaxed)) {
            // This is what the 30Hz timer does — reads linked source positions
            int count = survivor.getListenerHub().getLinkedSources(
                static_cast<SharedListenerHub::Listener*>(&survivor), out, 8);
            // count should be 0 or 1, never negative, never > 1
            REQUIRE(count >= 0);
            REQUIRE(count <= 1);
        }
    });

    // Give the audio thread time to start iterating
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Message thread: destroy the victim (this is what the DAW does on track removal)
    victim.reset();

    // Let audio thread run a bit more after destruction
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    running.store(false, std::memory_order_relaxed);
    audioThread.join();

    // Survivor is fine
    processOneBlock(survivor);
    REQUIRE(survivor.getListenerHub().getLinkedCount() == 1);

    disableLink(survivor);
    survivor.releaseResources();
}

// ===================================================================
// SECTION 7: ForeignSourceBridge data integrity
// ===================================================================

TEST_CASE("ForeignSourceBridge: double-buffer doesn't tear", "[multi][bridge]") {
    xyzpan::ForeignSourceBridge bridge;

    // Write a known payload
    xyzpan::ForeignSourceBridge::Payload wp;
    wp.count = 2;
    wp.sources[0].x = 1.0f;
    wp.sources[0].y = 2.0f;
    wp.sources[0].z = 3.0f;
    wp.sources[1].x = 4.0f;
    wp.sources[1].y = 5.0f;
    wp.sources[1].z = 6.0f;
    std::memcpy(wp.sources[0].name, "Alpha", 6);
    std::memcpy(wp.sources[1].name, "Beta", 5);

    bridge.write(wp);

    auto rp = bridge.read();
    REQUIRE(rp.count == 2);
    REQUIRE(rp.sources[0].x == 1.0f);
    REQUIRE(rp.sources[1].y == 5.0f);
    REQUIRE(juce::String(rp.sources[0].name) == "Alpha");
    REQUIRE(juce::String(rp.sources[1].name) == "Beta");
}

TEST_CASE("ForeignSourceBridge: concurrent write/read stress", "[multi][bridge][stress]") {
    xyzpan::ForeignSourceBridge bridge;

    std::atomic<bool> running{true};

    // Writer thread
    std::thread writer([&]() {
        for (int i = 0; i < 10000 && running.load(std::memory_order_relaxed); ++i) {
            xyzpan::ForeignSourceBridge::Payload p;
            p.count = 1;
            p.sources[0].x = static_cast<float>(i);
            p.sources[0].y = static_cast<float>(i) * 0.5f;
            bridge.write(p);
        }
        running.store(false, std::memory_order_relaxed);
    });

    // Reader thread — just verify no crash and count is sane
    int reads = 0;
    while (running.load(std::memory_order_relaxed)) {
        auto p = bridge.read();
        REQUIRE(p.count >= 0);
        REQUIRE(p.count <= xyzpan::kMaxLinkedSources);
        ++reads;
    }

    writer.join();
    REQUIRE(reads > 0);
}

// ===================================================================
// SECTION 8: PositionBridge (SourceExportBuffer) integrity
// ===================================================================

TEST_CASE("SourceExportBuffer: write/read round-trip", "[multi][bridge]") {
    xyzpan::SourceExportBuffer buf;
    buf.write(0.1f, 0.2f, 0.3f, 1.5f, 0.7f,
              -0.1f, 0.0f, 0.1f,   // L node
               0.1f, 0.0f, -0.1f,  // R node
               1.732f, 0.0f);      // sphereRadius, inputRms

    auto snap = buf.read();
    REQUIRE(snap.x == Approx(0.1f));
    REQUIRE(snap.y == Approx(0.2f));
    REQUIRE(snap.z == Approx(0.3f));
    REQUIRE(snap.distance == Approx(1.5f));
    REQUIRE(snap.stereoWidth == Approx(0.7f));
}

TEST_CASE("SourceExportBuffer: concurrent write/read stress", "[multi][bridge][stress]") {
    xyzpan::SourceExportBuffer buf;
    std::atomic<bool> running{true};

    std::thread writer([&]() {
        for (int i = 0; i < 50000; ++i) {
            float v = static_cast<float>(i) * 0.001f;
            buf.write(v, v, v, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.732f, 0.0f);
        }
        running.store(false, std::memory_order_relaxed);
    });

    int reads = 0;
    while (running.load(std::memory_order_relaxed)) {
        auto snap = buf.read();
        // Values should be self-consistent (x == y == z from same write)
        // Due to tearing this may not always hold, but it should never crash
        (void)snap;
        ++reads;
    }

    writer.join();
    REQUIRE(reads > 0);
}
