// Copyright 2021 Garena Online Private Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file toy_text_bindings.cpp
 * @brief Nanobind Python bindings for toy text environments
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/array.h>

import envpool.env.toy_text.blackjack;
import envpool.env.toy_text.catch;
import envpool.env.toy_text.cliffwalking;
import envpool.env.toy_text.frozen_lake;
import envpool.env.toy_text.nchain;
import envpool.env.toy_text.taxi;

namespace nb = nanobind;
using namespace envpool::toy_text;

NB_MODULE(envpool2_toy_text, m) {
    m.doc() = "EnvPool2 Toy Text Environments - C++20 Modules with Nanobind";

    //==========================================================================
    // Blackjack Environment
    //==========================================================================

    nb::class_<BlackjackState>(m, "BlackjackState")
        .def_ro("player_sum", &BlackjackState::player_sum, "Sum of player's hand")
        .def_ro("dealer_card", &BlackjackState::dealer_card, "Dealer's visible card")
        .def_ro("usable_ace", &BlackjackState::usable_ace, "Has usable ace")
        .def("to_array", &BlackjackState::toArray, "Convert to array");

    nb::class_<BlackjackStepResult>(m, "BlackjackStepResult")
        .def_ro("observation", &BlackjackStepResult::observation, "Next observation")
        .def_ro("reward", &BlackjackStepResult::reward, "Reward")
        .def_ro("done", &BlackjackStepResult::done, "Episode done flag")
        .def_ro("truncated", &BlackjackStepResult::truncated, "Episode truncated flag");

    nb::class_<BlackjackEnv>(m, "BlackjackEnv")
        .def(nb::init<int, int, bool, bool>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("natural") = false,
             nb::arg("sab") = true,
             "Create Blackjack environment")
        .def("reset", &BlackjackEnv::reset, "Reset environment")
        .def("step", &BlackjackEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &BlackjackEnv::isDone, "Check if episode is done")
        .def("id", &BlackjackEnv::id, "Get environment ID")
        .def("set_seed", &BlackjackEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // Catch Environment
    //==========================================================================

    nb::class_<CatchState>(m, "CatchState")
        .def_ro("grid", &CatchState::grid, "Grid representation")
        .def_ro("height", &CatchState::height, "Grid height")
        .def_ro("width", &CatchState::width, "Grid width")
        .def("to_vector", &CatchState::toVector, "Convert to vector");

    nb::class_<CatchStepResult>(m, "CatchStepResult")
        .def_ro("observation", &CatchStepResult::observation, "Next observation")
        .def_ro("reward", &CatchStepResult::reward, "Reward")
        .def_ro("done", &CatchStepResult::done, "Episode done flag")
        .def_ro("truncated", &CatchStepResult::truncated, "Episode truncated flag");

    nb::class_<CatchEnv>(m, "CatchEnv")
        .def(nb::init<int, int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("height") = 10,
             nb::arg("width") = 5,
             "Create Catch environment")
        .def("reset", &CatchEnv::reset, "Reset environment")
        .def("step", &CatchEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &CatchEnv::isDone, "Check if episode is done")
        .def("id", &CatchEnv::id, "Get environment ID")
        .def("set_seed", &CatchEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // CliffWalking Environment
    //==========================================================================

    nb::class_<CliffWalkingState>(m, "CliffWalkingState")
        .def_ro("position", &CliffWalkingState::position, "Encoded position")
        .def("to_int", &CliffWalkingState::toInt, "Convert to int");

    nb::class_<CliffWalkingStepResult>(m, "CliffWalkingStepResult")
        .def_ro("observation", &CliffWalkingStepResult::observation, "Next observation")
        .def_ro("reward", &CliffWalkingStepResult::reward, "Reward")
        .def_ro("done", &CliffWalkingStepResult::done, "Episode done flag")
        .def_ro("truncated", &CliffWalkingStepResult::truncated, "Episode truncated flag");

    nb::class_<CliffWalkingEnv>(m, "CliffWalkingEnv")
        .def(nb::init<int>(),
             nb::arg("env_id") = 0,
             "Create CliffWalking environment")
        .def("reset", &CliffWalkingEnv::reset, "Reset environment")
        .def("step", &CliffWalkingEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &CliffWalkingEnv::isDone, "Check if episode is done")
        .def("id", &CliffWalkingEnv::id, "Get environment ID");

    //==========================================================================
    // FrozenLake Environment
    //==========================================================================

    nb::class_<FrozenLakeState>(m, "FrozenLakeState")
        .def_ro("position", &FrozenLakeState::position, "Encoded position")
        .def("to_int", &FrozenLakeState::toInt, "Convert to int");

    nb::class_<FrozenLakeStepResult>(m, "FrozenLakeStepResult")
        .def_ro("observation", &FrozenLakeStepResult::observation, "Next observation")
        .def_ro("reward", &FrozenLakeStepResult::reward, "Reward")
        .def_ro("done", &FrozenLakeStepResult::done, "Episode done flag")
        .def_ro("truncated", &FrozenLakeStepResult::truncated, "Episode truncated flag");

    nb::class_<FrozenLakeEnv>(m, "FrozenLakeEnv")
        .def(nb::init<int, int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("size") = 4,
             nb::arg("max_episode_steps") = 100,
             "Create FrozenLake environment")
        .def("reset", &FrozenLakeEnv::reset, "Reset environment")
        .def("step", &FrozenLakeEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &FrozenLakeEnv::isDone, "Check if episode is done")
        .def("id", &FrozenLakeEnv::id, "Get environment ID")
        .def("set_seed", &FrozenLakeEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // NChain Environment
    //==========================================================================

    nb::class_<NChainState>(m, "NChainState")
        .def_ro("state", &NChainState::state, "Current state in chain")
        .def("to_int", &NChainState::toInt, "Convert to int");

    nb::class_<NChainStepResult>(m, "NChainStepResult")
        .def_ro("observation", &NChainStepResult::observation, "Next observation")
        .def_ro("reward", &NChainStepResult::reward, "Reward")
        .def_ro("done", &NChainStepResult::done, "Episode done flag")
        .def_ro("truncated", &NChainStepResult::truncated, "Episode truncated flag");

    nb::class_<NChainEnv>(m, "NChainEnv")
        .def(nb::init<int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 1000,
             "Create NChain environment")
        .def("reset", &NChainEnv::reset, "Reset environment")
        .def("step", &NChainEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &NChainEnv::isDone, "Check if episode is done")
        .def("id", &NChainEnv::id, "Get environment ID")
        .def("set_seed", &NChainEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // Taxi Environment
    //==========================================================================

    nb::class_<TaxiState>(m, "TaxiState")
        .def_ro("encoded_state", &TaxiState::encoded_state, "Encoded state")
        .def("to_int", &TaxiState::toInt, "Convert to int");

    nb::class_<TaxiStepResult>(m, "TaxiStepResult")
        .def_ro("observation", &TaxiStepResult::observation, "Next observation")
        .def_ro("reward", &TaxiStepResult::reward, "Reward")
        .def_ro("done", &TaxiStepResult::done, "Episode done flag")
        .def_ro("truncated", &TaxiStepResult::truncated, "Episode truncated flag");

    nb::class_<TaxiEnv>(m, "TaxiEnv")
        .def(nb::init<int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 200,
             "Create Taxi environment")
        .def("reset", &TaxiEnv::reset, "Reset environment")
        .def("step", &TaxiEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &TaxiEnv::isDone, "Check if episode is done")
        .def("id", &TaxiEnv::id, "Get environment ID")
        .def("set_seed", &TaxiEnv::setSeed, nb::arg("seed"), "Set random seed");
}
