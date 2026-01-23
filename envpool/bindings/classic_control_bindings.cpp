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
 * @file classic_control_bindings.cpp
 * @brief Nanobind Python bindings for classic control environments
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/array.h>

import envpool.env.classic_control.cartpole;
import envpool.env.classic_control.pendulum;
import envpool.env.classic_control.mountain_car;
import envpool.env.classic_control.mountain_car_continuous;
import envpool.env.classic_control.acrobot;

namespace nb = nanobind;
using namespace envpool::classic_control;

NB_MODULE(envpool2_classic_control, m) {
    m.doc() = "EnvPool2 Classic Control Environments - C++20 Modules with Nanobind";

    //==========================================================================
    // CartPole Environment
    //==========================================================================

    nb::class_<CartPoleState>(m, "CartPoleState")
        .def_ro("x", &CartPoleState::x, "Cart position")
        .def_ro("x_dot", &CartPoleState::x_dot, "Cart velocity")
        .def_ro("theta", &CartPoleState::theta, "Pole angle (radians)")
        .def_ro("theta_dot", &CartPoleState::theta_dot, "Pole angular velocity")
        .def("to_array", &CartPoleState::toArray, "Convert to array");

    nb::class_<CartPoleStepResult>(m, "CartPoleStepResult")
        .def_ro("observation", &CartPoleStepResult::observation, "Next observation")
        .def_ro("reward", &CartPoleStepResult::reward, "Reward")
        .def_ro("done", &CartPoleStepResult::done, "Episode done flag")
        .def_ro("truncated", &CartPoleStepResult::truncated, "Episode truncated flag");

    nb::class_<CartPoleEnv>(m, "CartPoleEnv")
        .def(nb::init<int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 500,
             "Create CartPole environment")
        .def("reset", &CartPoleEnv::reset, "Reset environment")
        .def("step", &CartPoleEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &CartPoleEnv::isDone, "Check if episode is done")
        .def("id", &CartPoleEnv::id, "Get environment ID")
        .def("elapsed_step", &CartPoleEnv::elapsedStep, "Get current step count")
        .def("max_episode_steps", &CartPoleEnv::maxEpisodeSteps, "Get maximum episode steps")
        .def("set_seed", &CartPoleEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // Pendulum Environment
    //==========================================================================

    nb::class_<PendulumState>(m, "PendulumState")
        .def_ro("cos_theta", &PendulumState::cos_theta, "Cosine of angle")
        .def_ro("sin_theta", &PendulumState::sin_theta, "Sine of angle")
        .def_ro("theta_dot", &PendulumState::theta_dot, "Angular velocity")
        .def("to_array", &PendulumState::toArray, "Convert to array");

    nb::class_<PendulumStepResult>(m, "PendulumStepResult")
        .def_ro("observation", &PendulumStepResult::observation, "Next observation")
        .def_ro("reward", &PendulumStepResult::reward, "Reward")
        .def_ro("done", &PendulumStepResult::done, "Episode done flag")
        .def_ro("truncated", &PendulumStepResult::truncated, "Episode truncated flag");

    nb::class_<PendulumEnv>(m, "PendulumEnv")
        .def(nb::init<int, int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 200,
             nb::arg("version") = 0,
             "Create Pendulum environment")
        .def("reset", &PendulumEnv::reset, "Reset environment")
        .def("step", &PendulumEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &PendulumEnv::isDone, "Check if episode is done")
        .def("id", &PendulumEnv::id, "Get environment ID")
        .def("elapsed_step", &PendulumEnv::elapsedStep, "Get current step count")
        .def("max_episode_steps", &PendulumEnv::maxEpisodeSteps, "Get maximum episode steps")
        .def("set_seed", &PendulumEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // MountainCar Environment
    //==========================================================================

    nb::class_<MountainCarState>(m, "MountainCarState")
        .def_ro("position", &MountainCarState::position, "Car position")
        .def_ro("velocity", &MountainCarState::velocity, "Car velocity")
        .def("to_array", &MountainCarState::toArray, "Convert to array");

    nb::class_<MountainCarStepResult>(m, "MountainCarStepResult")
        .def_ro("observation", &MountainCarStepResult::observation, "Next observation")
        .def_ro("reward", &MountainCarStepResult::reward, "Reward")
        .def_ro("done", &MountainCarStepResult::done, "Episode done flag")
        .def_ro("truncated", &MountainCarStepResult::truncated, "Episode truncated flag");

    nb::class_<MountainCarEnv>(m, "MountainCarEnv")
        .def(nb::init<int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 200,
             "Create MountainCar environment")
        .def("reset", &MountainCarEnv::reset, "Reset environment")
        .def("step", &MountainCarEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &MountainCarEnv::isDone, "Check if episode is done")
        .def("id", &MountainCarEnv::id, "Get environment ID")
        .def("elapsed_step", &MountainCarEnv::elapsedStep, "Get current step count")
        .def("max_episode_steps", &MountainCarEnv::maxEpisodeSteps, "Get maximum episode steps")
        .def("set_seed", &MountainCarEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // MountainCarContinuous Environment
    //==========================================================================

    nb::class_<MountainCarContinuousState>(m, "MountainCarContinuousState")
        .def_ro("position", &MountainCarContinuousState::position, "Car position")
        .def_ro("velocity", &MountainCarContinuousState::velocity, "Car velocity")
        .def("to_array", &MountainCarContinuousState::toArray, "Convert to array");

    nb::class_<MountainCarContinuousStepResult>(m, "MountainCarContinuousStepResult")
        .def_ro("observation", &MountainCarContinuousStepResult::observation, "Next observation")
        .def_ro("reward", &MountainCarContinuousStepResult::reward, "Reward")
        .def_ro("done", &MountainCarContinuousStepResult::done, "Episode done flag")
        .def_ro("truncated", &MountainCarContinuousStepResult::truncated, "Episode truncated flag");

    nb::class_<MountainCarContinuousEnv>(m, "MountainCarContinuousEnv")
        .def(nb::init<int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 999,
             "Create MountainCarContinuous environment")
        .def("reset", &MountainCarContinuousEnv::reset, "Reset environment")
        .def("step", &MountainCarContinuousEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &MountainCarContinuousEnv::isDone, "Check if episode is done")
        .def("id", &MountainCarContinuousEnv::id, "Get environment ID")
        .def("elapsed_step", &MountainCarContinuousEnv::elapsedStep, "Get current step count")
        .def("max_episode_steps", &MountainCarContinuousEnv::maxEpisodeSteps, "Get maximum episode steps")
        .def("set_seed", &MountainCarContinuousEnv::setSeed, nb::arg("seed"), "Set random seed");

    //==========================================================================
    // Acrobot Environment
    //==========================================================================

    nb::class_<AcrobotState>(m, "AcrobotState")
        .def_ro("cos_theta1", &AcrobotState::cos_theta1, "Cosine of first joint angle")
        .def_ro("sin_theta1", &AcrobotState::sin_theta1, "Sine of first joint angle")
        .def_ro("cos_theta2", &AcrobotState::cos_theta2, "Cosine of second joint angle")
        .def_ro("sin_theta2", &AcrobotState::sin_theta2, "Sine of second joint angle")
        .def_ro("theta1_dot", &AcrobotState::theta1_dot, "Angular velocity of first joint")
        .def_ro("theta2_dot", &AcrobotState::theta2_dot, "Angular velocity of second joint")
        .def_ro("theta1", &AcrobotState::theta1, "First joint angle")
        .def_ro("theta2", &AcrobotState::theta2, "Second joint angle")
        .def("to_array", &AcrobotState::toArray, "Convert to array");

    nb::class_<AcrobotStepResult>(m, "AcrobotStepResult")
        .def_ro("observation", &AcrobotStepResult::observation, "Next observation")
        .def_ro("reward", &AcrobotStepResult::reward, "Reward")
        .def_ro("done", &AcrobotStepResult::done, "Episode done flag")
        .def_ro("truncated", &AcrobotStepResult::truncated, "Episode truncated flag");

    nb::class_<AcrobotEnv>(m, "AcrobotEnv")
        .def(nb::init<int, int, int>(),
             nb::arg("env_id") = 0,
             nb::arg("seed") = 42,
             nb::arg("max_episode_steps") = 500,
             "Create Acrobot environment")
        .def("reset", &AcrobotEnv::reset, "Reset environment")
        .def("step", &AcrobotEnv::step, nb::arg("action"), "Execute one step")
        .def("is_done", &AcrobotEnv::isDone, "Check if episode is done")
        .def("id", &AcrobotEnv::id, "Get environment ID")
        .def("elapsed_step", &AcrobotEnv::elapsedStep, "Get current step count")
        .def("max_episode_steps", &AcrobotEnv::maxEpisodeSteps, "Get maximum episode steps")
        .def("set_seed", &AcrobotEnv::setSeed, nb::arg("seed"), "Set random seed");
}
