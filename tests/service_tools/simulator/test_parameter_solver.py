#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import unittest
from unittest.mock import MagicMock, patch
from itertools import product

from mindiesimulator.parameter_solver import ParameterSolver, PSOSearch, GridSearch
from mindiesimulator.service_simulator import ServiceSimulator
from mindiesimulator.common.utils import Logger


class TestGridSearch(unittest.TestCase):
    def setUp(self):
        #init Logger
        self.logger = Logger.init_logger(save_to_file=True)
        self.grid_search = GridSearch()

    def test_set_parameter_ranges(self):
        param1, param2 = 'param1', 'param2'
        ranges = {
            param1: (1, 3, 1),
            param2: (2, 4, 2)
        }
        expected_params = [
            {param1: 1, param2: 2},
            {param1: 1, param2: 4},
            {param1: 2, param2: 2},
            {param1: 2, param2: 4},
            {param1: 3, param2: 2},
            {param1: 3, param2: 4}
        ]
        self.grid_search.set_parameter_ranges(ranges)
        self.assertEqual(self.grid_search.params_list, expected_params)

        param1, param2, param3 = 'param1', 'param2', 'param3'
        ranges = {
            param1: (1, 3, 1),
            param2: (2, 4, 2),
            param3: (5, 7, 1)
        }
        expected_params = [
            {param1: 1, param2: 2, param3: 5},
            {param1: 1, param2: 2, param3: 6},
            {param1: 1, param2: 2, param3: 7},
            {param1: 1, param2: 4, param3: 5},
            {param1: 1, param2: 4, param3: 6},
            {param1: 1, param2: 4, param3: 7},
            {param1: 2, param2: 2, param3: 5},
            {param1: 2, param2: 2, param3: 6},
            {param1: 2, param2: 2, param3: 7},
            {param1: 2, param2: 4, param3: 5},
            {param1: 2, param2: 4, param3: 6},
            {param1: 2, param2: 4, param3: 7},
            {param1: 3, param2: 2, param3: 5},
            {param1: 3, param2: 2, param3: 6},
            {param1: 3, param2: 2, param3: 7},
            {param1: 3, param2: 4, param3: 5},
            {param1: 3, param2: 4, param3: 6},
            {param1: 3, param2: 4, param3: 7}
        ]
        self.grid_search.set_parameter_ranges(ranges)
        self.assertEqual(self.grid_search.params_list, expected_params)

    def test_set_constraints(self):
        constraints = {
            'metric1': 10,
            'metric2': 20
        }
        self.grid_search.set_constraints(constraints)
        self.assertEqual(self.grid_search.constraints, constraints)

    def test_has_next(self):
        param1 = 'param1'
        ranges = {
            param1: (1, 1, 1)
        }
        self.grid_search.set_parameter_ranges(ranges)
        self.assertTrue(self.grid_search.has_next())
        self.grid_search.current_index += 1
        self.assertFalse(self.grid_search.has_next())

    def test_get_next_params_1(self):
        param1, param2, param3 = 'param1', 'param2', 'param3'
        ranges = {
            param1: (1, 3, 1),
            param2: (2, 4, 2),
            param3: (5, 7, 1)
        }
        expected_params_sequence = [
            {param1: 1, param2: 2, param3: 5},
            {param1: 1, param2: 2, param3: 6},
            {param1: 1, param2: 2, param3: 7},
            {param1: 1, param2: 4, param3: 5},
            {param1: 1, param2: 4, param3: 6},
            {param1: 1, param2: 4, param3: 7},
            {param1: 2, param2: 2, param3: 5},
            {param1: 2, param2: 2, param3: 6},
            {param1: 2, param2: 2, param3: 7},
            {param1: 2, param2: 4, param3: 5},
            {param1: 2, param2: 4, param3: 6},
            {param1: 2, param2: 4, param3: 7},
            {param1: 3, param2: 2, param3: 5},
            {param1: 3, param2: 2, param3: 6},
            {param1: 3, param2: 2, param3: 7},
            {param1: 3, param2: 4, param3: 5},
            {param1: 3, param2: 4, param3: 6},
            {param1: 3, param2: 4, param3: 7}
        ]
        self.grid_search.set_parameter_ranges(ranges)
        for expected_params in expected_params_sequence:
            self.assertEqual(self.grid_search.get_next_params(), expected_params)
            self.grid_search.current_index += 1

    def test_get_next_params_2(self):
        param1, param2 = 'param1', 'param2'
        ranges = {
            param1: (1, 2, 1),
            param2: (3, 4, 1)
        }
        expected_params_sequence = [
            {param1: 1, param2: 3},
            {param1: 1, param2: 4},
            {param1: 2, param2: 3},
            {param1: 2, param2: 4}
        ]
        self.grid_search.set_parameter_ranges(ranges)
        for expected_params in expected_params_sequence:
            self.assertEqual(self.grid_search.get_next_params(), expected_params)
            self.grid_search.current_index += 1


class TestPSOSearch(unittest.TestCase):
    def setUp(self):
        self.n_particles = 5
        self.n_iterations = 3
        self.pso_search = PSOSearch(self.n_particles, self.n_iterations)
        desc = 'desc'
        self.params_ranges = {
            'param1': (1, 10, desc),
            'param2': (0.1, 1.0, desc),
            'param3': (1, 5, desc),
            'param4': (10, 50, desc),
            'param5': (5, 10, desc)
        }
        self.constraints = {
            'avg_prefill_time_thld': 10,
            'avg_decode_time_thld': 20,
            'SLOP90_prefill_time_thld': 15,
            'SLOP90_decode_time_thld': 25
        }
        self.pso_search.set_parameter_ranges(self.params_ranges)
        self.pso_search.set_constraints(self.constraints)

    def test_init_particles(self):
        self.pso_search.init_particles()
        self.assertEqual(len(self.pso_search.particles), self.n_particles)
        self.assertEqual(len(self.pso_search.velocities), self.n_particles)
        self.assertEqual(len(self.pso_search.pbest_positions), self.n_particles)
        self.assertEqual(len(self.pso_search.pbest_values), self.n_particles)

        for particle in self.pso_search.particles:
            self.assertLessEqual(particle[2], particle[4])

    def test_get_next_params(self):
        self.pso_search.init_particles()
        params = self.pso_search.get_next_params()
        self.assertIsInstance(params, dict)
        self.assertIn('concurrency', params)
        self.assertIn('request_rate', params)
        self.assertIn('max_prefill_batch_size', params)
        self.assertIn('prefill_time_ms_per_request', params)
        self.assertIn('max_batch_size', params)

    def test_update(self):
        self.pso_search.init_particles()
        params = self.pso_search.get_next_params()
        result = {
            'performance': 100,
            'avg_prefill_time': 8,
            'avg_decode_time': 15,
            'p90_prefill_time': 12,
            'p90_decode_time': 22
        }
        self.pso_search.update(params, result)

        self.assertIsNotNone(self.pso_search.gbest_params)
        self.assertEqual(self.pso_search.gbest_thp, result['performance'])

    def test_has_next(self):
        self.pso_search.init_particles()
        self.assertTrue(self.pso_search.has_next())
        self.pso_search.current_iteration = self.n_iterations - 1 
        self.pso_search.current_particle = self.n_particles 
        self.assertFalse(self.pso_search.has_next())

    def test_get_best_params(self):
        self.pso_search.init_particles()
        params = self.pso_search.get_next_params()
        result = {
            'performance': 100,
            'avg_prefill_time': 8,
            'avg_decode_time': 15,
            'p90_prefill_time': 12,
            'p90_decode_time': 22
        }
        self.pso_search.update(params, result)
        best_params, best_thp = self.pso_search.get_best_params()
        self.assertEqual(best_params, params)
        self.assertEqual(best_thp, result['performance'])

    def test_constraint_violation(self):
        self.pso_search.init_particles()
        params = self.pso_search.get_next_params()
        result = {
            'performance': 100,
            'avg_prefill_time': 12,  
            'avg_decode_time': 15,
            'p90_prefill_time': 16,  
            'p90_decode_time': 22
        }
        self.pso_search.update(params, result)

        self.assertIsNone(self.pso_search.gbest_params)


if __name__ == '__main__':
    unittest.main()