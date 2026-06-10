/*
 * sst-voicemanager - a header only library providing synth
 * voice management in response to midi and clap event streams
 * with support for a variety of play, trigger, and midi nodes
 *
 * Copyright 2023-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-voicemanager is released under the MIT license, available
 * as LICENSE.md in the root of this repository.
 *
 * All source in sst-voicemanager available at
 * https://github.com/surge-synthesizer/sst-voicemanager
 */

#include "catch2.hpp"

#include "sst/voicemanager/voicemanager.h"
#include "test_player.h"

namespace
{
// Group ids: parent G holds children A, B, C. Keys are routed to a child by range so a
// voice's child is identifiable from its key.
constexpr uint64_t G{100}, A{1979}, B{21121984}, C{8675309};

auto inA = [](const auto &v) { return v.key() >= 10 && v.key() < 20; };
auto inB = [](const auto &v) { return v.key() >= 20 && v.key() < 30; };
auto inC = [](const auto &v) { return v.key() >= 30 && v.key() < 40; };

template <typename TP> void routeChildrenByKey(TP &tp)
{
    tp.polyGroupForKey = [](int16_t k) -> uint64_t
    {
        if (k >= 10 && k < 20)
            return A;
        if (k >= 20 && k < 30)
            return B;
        if (k >= 30 && k < 40)
            return C;
        return 0;
    };
}

// Parent G=8 over three children A/B/C=6 (3x6=18 overcommit is legal).
template <typename TP> void makeOvercommitTree(TP &tp)
{
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(A, 6);
    vm.setPolyphonyGroupVoiceLimit(B, 6);
    vm.setPolyphonyGroupVoiceLimit(C, 6);
    vm.setPolyphonyGroupVoiceLimit(G, 8);
    vm.setPolyphonyGroupParent(A, G);
    vm.setPolyphonyGroupParent(B, G);
    vm.setPolyphonyGroupParent(C, G);
    routeChildrenByKey(tp);
}
} // namespace

TEST_CASE("Hierarchy - parent caps the subtree")
{
    SECTION("Overcommit configures with no pre-steal")
    {
        TestPlayer<64> tp;
        auto &vm = tp.voiceManager;
        makeOvercommitTree(tp);

        // 3x6 = 18 > 8 is legal; nothing is stolen at configure time.
        REQUIRE_NO_VOICES;

        // Fill A to its own limit 6 - parent G binds at 8, but A alone is only 6 so all land.
        for (int i = 0; i < 6; ++i)
            vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(6, 6);
        REQUIRE(tp.activeVoicesMatching(inA) == 6);
    }

    SECTION("9th voice steals to hold the parent at 8, leaf grows")
    {
        TestPlayer<64> tp;
        auto &vm = tp.voiceManager;
        makeOvercommitTree(tp);

        // 4 in A (keys 10-13), 2 in B (20-21), 2 in C (30-31): parent exactly full at 8.
        for (int i = 0; i < 4; ++i)
            vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
        for (int i = 0; i < 2; ++i)
            vm.processNoteOnEvent(0, 0, 20 + i, -1, 0.8, 0.0);
        for (int i = 0; i < 2; ++i)
            vm.processNoteOnEvent(0, 0, 30 + i, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(8, 8);
        REQUIRE(tp.activeVoicesMatching(inA) == 4);

        // 9th note into C: C is 2/6 (not full), G is 8/8 (full) -> steal oldest in G's
        // subtree (the very first note, key 10 in A), and C grows to 3. Total stays 8.
        vm.processNoteOnEvent(0, 0, 32, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(8, 8);
        REQUIRE(tp.activeVoicesMatching(inA) == 3); // lost its oldest
        REQUIRE(tp.activeVoicesMatching(inB) == 2);
        REQUIRE(tp.activeVoicesMatching(inC) == 3); // grew
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 10; }) == 0);
    }

    SECTION("Note into an empty child steals from a full sibling, not itself")
    {
        TestPlayer<64> tp;
        auto &vm = tp.voiceManager;
        makeOvercommitTree(tp);

        // 4 in A, 4 in B = parent full at 8; C empty.
        for (int i = 0; i < 4; ++i)
            vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
        for (int i = 0; i < 4; ++i)
            vm.processNoteOnEvent(0, 0, 20 + i, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(8, 8);

        // Note into empty C: steal oldest in G (key 10, in A); C becomes 1.
        vm.processNoteOnEvent(0, 0, 30, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(8, 8);
        REQUIRE(tp.activeVoicesMatching(inA) == 3);
        REQUIRE(tp.activeVoicesMatching(inB) == 4);
        REQUIRE(tp.activeVoicesMatching(inC) == 1);
    }
}

TEST_CASE("Hierarchy - a full leaf sheds its own voice")
{
    TestPlayer<64> tp;
    auto &vm = tp.voiceManager;
    // Parent G=8, children A/B=6. Fill A to its own limit 6 (parent only 6/8).
    vm.setPolyphonyGroupVoiceLimit(A, 6);
    vm.setPolyphonyGroupVoiceLimit(B, 6);
    vm.setPolyphonyGroupVoiceLimit(G, 8);
    vm.setPolyphonyGroupParent(A, G);
    vm.setPolyphonyGroupParent(B, G);
    routeChildrenByKey(tp);

    for (int i = 0; i < 6; ++i)
        vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
    REQUIRE(tp.activeVoicesMatching(inA) == 6);

    // 7th into A: A is the deepest full level (6/6) while G is 6/8 - steal A's own oldest.
    vm.processNoteOnEvent(0, 0, 16, -1, 0.8, 0.0);
    REQUIRE(tp.activeVoicesMatching(inA) == 6);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 10; }) == 0); // own oldest
    REQUIRE_VOICE_COUNTS(6, 6);

    // 1st into sibling B: G has room (6/8), B empty - no steal.
    vm.processNoteOnEvent(0, 0, 20, -1, 0.8, 0.0);
    REQUIRE(tp.activeVoicesMatching(inA) == 6);
    REQUIRE(tp.activeVoicesMatching(inB) == 1);
    REQUIRE_VOICE_COUNTS(7, 7);
}

TEST_CASE("Hierarchy - grandparent binds (depth 3)")
{
    // G1 (leaf) -> G2 -> G3, limits 8 / 8 / 4. The grandparent G3 caps the subtree at 4.
    constexpr uint64_t G1{11}, G2{12}, G3{13};
    TestPlayer<64> tp;
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(G1, 8);
    vm.setPolyphonyGroupVoiceLimit(G2, 8);
    vm.setPolyphonyGroupVoiceLimit(G3, 4);
    vm.setPolyphonyGroupParent(G1, G2);
    vm.setPolyphonyGroupParent(G2, G3);
    tp.polyGroupForKey = [](int16_t) -> uint64_t { return G1; };

    for (int i = 0; i < 8; ++i)
        vm.processNoteOnEvent(0, 0, 40 + i, -1, 0.8, 0.0);
    // Even though G1/G2 allow 8, the grandparent G3=4 caps it.
    REQUIRE_VOICE_COUNTS(4, 4);
    // The four survivors are the four most recent (older ones stolen as G3 stayed full).
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() >= 44; }) == 4);
}

TEST_CASE("Hierarchy - lowering a parent limit sheds across the subtree")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    makeOvercommitTree(tp);

    // 4/2/2 across A/B/C, parent full at 8.
    for (int i = 0; i < 4; ++i)
        vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
    for (int i = 0; i < 2; ++i)
        vm.processNoteOnEvent(0, 0, 20 + i, -1, 0.8, 0.0);
    for (int i = 0; i < 2; ++i)
        vm.processNoteOnEvent(0, 0, 30 + i, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(8, 8);

    // Lower G to 5: excess 3 stolen from the subtree, oldest first (keys 10,11,12 in A).
    vm.setPolyphonyGroupVoiceLimit(G, 5);
    REQUIRE_VOICE_COUNTS(5, 5);
    REQUIRE(tp.activeVoicesMatching(inA) == 1); // 4 -> 1
    REQUIRE(tp.activeVoicesMatching(inB) == 2);
    REQUIRE(tp.activeVoicesMatching(inC) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 13; }) == 1); // newest A kept
}

TEST_CASE("Hierarchy - multi-voice note stolen as a unit under a tight parent")
{
    // Keys > 72 create 3 voices each. Parent G=6 over child A; two 3-voice notes fill it,
    // a third must steal the oldest 3-voice note as a group.
    TestPlayer<64> tp;
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(A, 12);
    vm.setPolyphonyGroupVoiceLimit(G, 6);
    vm.setPolyphonyGroupParent(A, G);
    tp.polyGroupForKey = [](int16_t) -> uint64_t { return A; };

    vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(3, 3);
    vm.processNoteOnEvent(0, 0, 91, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(6, 6);

    // Parent full at 6; a third 3-voice note steals the first note's 3 voices together.
    vm.processNoteOnEvent(0, 0, 92, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(6, 6);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 90; }) == 0);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 92; }) == 3);
}

TEST_CASE("Hierarchy - reparenting with live voices recomputes subtree counts")
{
    TestPlayer<64> tp;
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(A, 6);
    vm.setPolyphonyGroupVoiceLimit(B, 6);
    routeChildrenByKey(tp);

    // A and B are roots; put 3 live voices in A before any parent exists.
    for (int i = 0; i < 3; ++i)
        vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
    REQUIRE(tp.activeVoicesMatching(inA) == 3);

    // Now introduce a tight parent and adopt both children while A's voices are sounding.
    // The recompute on reparent must credit those 3 voices to G; if it did not, G would
    // read 0 and wrongly admit 5 more (total 8) instead of capping the subtree at 5.
    vm.setPolyphonyGroupVoiceLimit(G, 5);
    vm.setPolyphonyGroupParent(A, G);
    vm.setPolyphonyGroupParent(B, G);

    // Fill B: subtree climbs 3 -> 5, then holds at 5, shedding A's oldest first.
    for (int i = 0; i < 4; ++i)
        vm.processNoteOnEvent(0, 0, 20 + i, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(5, 5);
    REQUIRE(tp.activeVoicesMatching(inB) == 4);
    REQUIRE(tp.activeVoicesMatching(inA) == 1);
    // The survivor in A is its newest; the two oldest (keys 10, 11) were stolen.
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 12; }) == 1);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 10; }) == 0);
}

TEST_CASE("Hierarchy - only leaf groups may be MONO")
{
    using VM = TestPlayer<32>::voiceManager_t;

    SECTION("Setting MONO on a group with children is rejected")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE(vm.setPolyphonyGroupParent(A, G)); // G now has a child
        REQUIRE(vm.getPlaymode(G) == VM::PlayMode::POLY_VOICES);

        REQUIRE_FALSE(vm.setPlaymode(G, VM::PlayMode::MONO_NOTES)); // rejected
        REQUIRE(vm.getPlaymode(G) == VM::PlayMode::POLY_VOICES);    // unchanged

        // A leaf, by contrast, takes the mode and returns true.
        REQUIRE(vm.setPlaymode(A, VM::PlayMode::MONO_NOTES));
        REQUIRE(vm.getPlaymode(A) == VM::PlayMode::MONO_NOTES);
    }

    SECTION("Attaching a child to a MONO group is rejected")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        // Parent candidate P is MONO with a tight limit; child routes here if attached.
        vm.setPolyphonyGroupVoiceLimit(G, 2);
        REQUIRE(vm.setPlaymode(G, VM::PlayMode::MONO_NOTES)); // G is a leaf here: allowed
        vm.setPolyphonyGroupVoiceLimit(A, 10);
        tp.polyGroupForKey = [](int16_t) -> uint64_t { return A; };

        REQUIRE_FALSE(vm.setPolyphonyGroupParent(A, G)); // rejected: G is MONO

        // A is unparented, so it fills to its own limit, not the (would-be) parent's 2.
        for (int i = 0; i < 5; ++i)
            vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(5, 5);
    }
}

TEST_CASE("Hierarchy - cycles and self-parent are rejected")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(A, 4);
    vm.setPolyphonyGroupVoiceLimit(B, 8);
    REQUIRE(vm.setPolyphonyGroupParent(A, B)); // A under B

    // Self-parent is rejected (would be a 1-cycle).
    REQUIRE_FALSE(vm.setPolyphonyGroupParent(A, A));
    // Closing the loop B -> A is rejected (A is already an ancestor-or-self of B's target).
    REQUIRE_FALSE(vm.setPolyphonyGroupParent(B, A));

    // B is still a root: route to A, fill past A's limit; the binding constraint is A=4,
    // never an accidental cycle (which would hang or miscount).
    tp.polyGroupForKey = [](int16_t) -> uint64_t { return A; };
    for (int i = 0; i < 7; ++i)
        vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(4, 4);
}

TEST_CASE("Hierarchy - detaching restores root behavior")
{
    TestPlayer<64> tp;
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(A, 6);
    vm.setPolyphonyGroupVoiceLimit(G, 3);
    REQUIRE(vm.setPolyphonyGroupParent(A, G));
    tp.polyGroupForKey = [](int16_t) -> uint64_t { return A; };

    // Parented: A is capped by G=3.
    for (int i = 0; i < 6; ++i)
        vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(3, 3);

    vm.allSoundsOff();
    REQUIRE_NO_VOICES;

    // Detach: A is a root again, capped only by its own limit 6.
    REQUIRE(vm.setPolyphonyGroupParent(A, TestPlayer<64>::voiceManager_t::noPolyphonyGroupParent));
    for (int i = 0; i < 8; ++i)
        vm.processNoteOnEvent(0, 0, 10 + i, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(6, 6);
}
