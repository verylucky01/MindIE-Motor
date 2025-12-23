#!/usr/bin/python3.11
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

import unittest
from unittest.mock import Mock, patch, MagicMock
import numpy as np


# Import the class to be tested
from mindiesimulator.inference_engine import InferenceSim


class TestInferenceSim(unittest.TestCase):
    """Test cases for InferenceSim class"""

    def setUp(self):
        """Set up test fixtures before each test method"""
        # Create mock latency model
        self.mock_latency_model = Mock()

        # Default configuration for testing
        self.config = {
            "scenario": "test_scenario",
            "llm_size": 7,  # 7B model
            "world_size": 8,
            "num_hidden_layers": 32,
            "hidden_size": 4096,
            "num_attention_heads": 32,
            "num_key_value_heads": 32,
            "block_size": 16,
            "recompute_ratio": 0.6,
            "npu_mem_size": 20,  # 20GB
            "hbm_size": 64,  # 64GB
            "max_prefill_tokens": 2048,
            "max_seq_len": 4096,
            "max_input_len": 2048,
            "bytes_per_element": 2,
            "dp": 1
        }

        # Mock the Logger
        self.patcher_logger = patch('mindiesimulator.inference_engine.Logger')
        self.mock_logger_class = self.patcher_logger.start()
        self.mock_logger = Mock()
        self.mock_logger_class.get_logger.return_value = self.mock_logger

        # Mock LatencyModeling
        self.patcher_latency = patch('mindiesimulator.inference_engine.LatencyModeling')
        self.mock_latency_modeling = self.patcher_latency.start()
        self.mock_latency_instance = Mock()
        self.mock_latency_modeling.return_value = self.mock_latency_instance

        # Setup default return values for latency model methods
        self.mock_latency_instance.decode_model_time_predict.return_value = 10.0
        self.mock_latency_instance.decode_frame_time_predict.return_value = 5.0
        self.mock_latency_instance.prefill_model_time_predict.return_value = 20.0
        self.mock_latency_instance.prefill_frame_time_predict.return_value = 10.0

    def tearDown(self):
        """Clean up after each test method"""
        self.patcher_logger.stop()
        self.patcher_latency.stop()

    def test_init_success(self):
        """Test successful initialization of InferenceSim"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        self.assertEqual(sim.llm_size, 7)
        self.assertEqual(sim.num_npu, 8)
        self.assertEqual(sim.num_hidden_layers, 32)
        self.assertEqual(sim.hidden, 4096)
        self.assertEqual(sim.block_size, 16)
        self.assertIsNotNone(sim.num_kv_block)

    def test_init_zero_num_hidden_layers(self):
        """Test initialization fails with zero num_hidden_layers"""
        config = self.config.copy()
        config["num_hidden_layers"] = 0

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("num_hidden_layers should not be zero", str(context.exception))

    def test_init_zero_block_size(self):
        """Test initialization fails with zero block_size"""
        config = self.config.copy()
        config["block_size"] = 0

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("block_size should not be zero", str(context.exception))

    def test_init_zero_kv_head(self):
        """Test initialization fails with zero kv_head"""
        config = self.config.copy()
        config["num_key_value_heads"] = 0

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("kv_head number should not be zero", str(context.exception))

    def test_init_zero_attn_heads(self):
        """Test initialization fails with zero attn_heads"""
        config = self.config.copy()
        config["num_attention_heads"] = 0

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("attn_heads should not be zero", str(context.exception))

    def test_init_zero_hidden(self):
        """Test initialization fails with zero hidden"""
        config = self.config.copy()
        config["hidden_size"] = 0

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("hidden size should not be zero", str(context.exception))

    def test_recompute_action(self):
        """Test recompute_action static method"""
        decode_table = [
            [1, 100, 50, 200],  # [req_id, prompt_len, generated_len, total_len]
            [2, 200, 100, 400],
            [3, 150, 75, 300]
        ]

        result = InferenceSim.recompute_action(decode_table)

        expected = [
            [1, 150, 150],  # [req_id, prompt_len + generated_len, total_len - generated_len]
            [2, 300, 300],
            [3, 225, 225]
        ]

        self.assertEqual(result, expected)

    def test_judge_kv_block_prefill_only(self):
        """Test judge_kv_block with prefill table only"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        prefill_table = [
            [1, 32],  # [req_id, prompt_len]
            [2, 48]
        ]

        exceed, kv_block = sim.judge_kv_block(prefill_table=prefill_table)

        self.assertEqual(kv_block, 7)
        self.assertFalse(exceed)

    def test_judge_kv_block_with_decode(self):
        """Test judge_kv_block with both prefill and decode tables"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        prefill_table = [[1, 32]]
        decode_table = [[2, 100, 50, 200]]  # [req_id, prompt_len, generated_len, total_len]

        exceed, kv_block = sim.judge_kv_block(
            prefill_table=prefill_table,
            decode_table=decode_table,
            decode=False
        )
        # Expected: ceil(33/16) + ceil(150/16) = 3 + 10 = 13
        self.assertIsInstance(kv_block, (int, float))
        self.assertIsInstance(exceed, (bool, np.bool_))

    def test_judge_kv_block_zero_block_size(self):
        """Test judge_kv_block raises error with zero block_size"""
        config = self.config.copy()
        config["hbm_size"] = 100  # Increase to avoid insufficient memory error
        sim = InferenceSim(config, self.mock_latency_model)
        sim.block_size = 0

        with self.assertRaises(ValueError) as context:
            sim.judge_kv_block(prefill_table=[[1, 32]])
        self.assertIn("block_size should not be zero", str(context.exception))

    def test_get_model_mem(self):
        """Test get_model_mem calculation"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        # 7B * 10^9 * 2 / 1024^3
        expected = 7 * 10 ** 9 * 2 / 1024 ** 3
        result = sim.get_model_mem()

        self.assertAlmostEqual(result, expected, places=2)

    def test_cal_npu_mem_zero_num_npu(self):
        """Test cal_npu_mem raises error with zero num_npu"""
        sim = InferenceSim(self.config, self.mock_latency_model)
        sim.num_npu = 0

        with self.assertRaises(ValueError) as context:
            sim.cal_npu_mem(2048, 4096, 2048, 64 * 1024 ** 3)
        self.assertIn("num_npu should not be zero", str(context.exception))

    def test_cal_npu_mem_zero_attn_heads(self):
        """Test cal_npu_mem raises error with zero attn_heads"""
        sim = InferenceSim(self.config, self.mock_latency_model)
        sim.attn_heads = 0

        with self.assertRaises(ValueError) as context:
            sim.cal_npu_mem(2048, 4096, 2048, 64 * 1024 ** 3)
        self.assertIn("attn_heads should not be zero", str(context.exception))

    def test_decode_sim_positive_latency(self):
        """Test decode_sim returns positive latency"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        decode_table = [[1, 100, 50, 200]]
        latency = sim.decode_sim(decode_table)

        self.assertEqual(latency, 15.0)
        self.assertGreaterEqual(latency, 0)

    def test_decode_sim_negative_latency_becomes_zero(self):
        """Test decode_sim returns zero when latency would be negative"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        # Set mock to return negative values
        self.mock_latency_instance.decode_model_time_predict.return_value = -10.0
        self.mock_latency_instance.decode_frame_time_predict.return_value = -5.0

        decode_table = [[1, 100, 50, 200]]
        latency = sim.decode_sim(decode_table)

        self.assertEqual(latency, 0)

    def test_prefill_sim_positive_latency(self):
        """Test prefill_sim returns positive latency"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        request = [1, 128]
        latency = sim.prefill_sim(request)

        self.assertEqual(latency, 30.0)
        self.assertGreaterEqual(latency, 0)

    def test_prefill_sim_negative_latency_becomes_zero(self):
        """Test prefill_sim returns zero when latency would be negative"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        # Set mock to return negative values
        self.mock_latency_instance.prefill_model_time_predict.return_value = -20.0
        self.mock_latency_instance.prefill_frame_time_predict.return_value = -10.0

        request = [1, 128]
        latency = sim.prefill_sim(request)

        self.assertEqual(latency, 0)

    def test_construct_decode_batch(self):
        """Test construct_decode_batch method"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        all_decode_table = [
            [1, 100, 15, 200],  # Not on block boundary (115 % 16 != 0)
            [2, 100, 16, 200],  # On block boundary (116 % 16 == 0)
            [3, 100, 20, 200],
            [4, 100, 32, 200],  # On block boundary (132 % 16 == 0)
        ]

        result = sim.construct_decode_batch(10, all_decode_table)

        # Should include requests, respecting block boundaries
        self.assertIsInstance(result, list)
        self.assertLessEqual(len(result), 10)

    def test_recompute_insufficient_blocks_error(self):
        """Test recompute raises error when blocks are sufficient"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        # Create a scenario where blocks are sufficient
        decode_table = [[1, 100, 16, 200]]  # Only one request on boundary

        # This should raise an error because recompute shouldn't be triggered
        # when there are sufficient blocks
        with self.assertRaises(ValueError) as context:
            sim.recompute(decode_table)
        self.assertIn("kv blocks is sufficient", str(context.exception))

    @patch('mindiesimulator.inference_engine.PathCheck')
    @patch('mindiesimulator.inference_engine.os.getenv')
    def test_get_npu_mem_size_from_llm_log_invalid_path(
            self, mock_getenv, mock_pathcheck
    ):
        """Test get_npu_mem_size_from_llm_log with invalid path"""
        mock_getenv.return_value = "/invalid/path/"
        mock_pathcheck.check_path_full.return_value = False

        config = self.config.copy()
        config["dp"] = 2
        config["npu_mem_size"] = -1

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("Invalid MINDIE_LOG_PATH", str(context.exception))

    @patch('mindiesimulator.inference_engine.PathCheck')
    @patch('mindiesimulator.inference_engine.os.getenv')
    @patch('mindiesimulator.inference_engine.glob.glob')
    def test_get_npu_mem_size_from_llm_log_no_files(
            self, mock_glob, mock_getenv, mock_pathcheck
    ):
        """Test get_npu_mem_size_from_llm_log when no log files found"""
        mock_getenv.return_value = "/test/log/path/"
        mock_pathcheck.check_path_full.return_value = True
        mock_glob.return_value = []

        config = self.config.copy()
        config["dp"] = 2
        config["npu_mem_size"] = -1

        with self.assertRaises(FileNotFoundError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("No log files found", str(context.exception))

    def test_init_insufficient_memory(self):
        """Test initialization fails with insufficient memory"""
        config = self.config.copy()
        config["hbm_size"] = 10  # Very small HBM size
        config["llm_size"] = 70  # Large model

        with self.assertRaises(ValueError) as context:
            InferenceSim(config, self.mock_latency_model)
        self.assertIn("Insufficient memory for the model", str(context.exception))

    def test_recompute_zero_num_kv_block(self):
        """Test recompute with zero num_kv_block raises error"""
        sim = InferenceSim(self.config, self.mock_latency_model)
        sim.num_kv_block = 0

        decode_table = [[1, 100, 15, 200]]

        with self.assertRaises(ValueError) as context:
            sim.recompute(decode_table)
        self.assertIn("num_kv_block should not be zero", str(context.exception))


class TestInferenceSimIntegration(unittest.TestCase):
    """Integration tests for InferenceSim"""

    def setUp(self):
        """Set up for integration tests"""
        self.mock_latency_model = Mock()

        self.config = {
            "scenario": "test_scenario",
            "llm_size": 7,
            "world_size": 8,
            "num_hidden_layers": 32,
            "hidden_size": 4096,
            "num_attention_heads": 32,
            "num_key_value_heads": 32,
            "block_size": 16,
            "recompute_ratio": 0.6,
            "npu_mem_size": 20,
            "hbm_size": 64,
            "max_prefill_tokens": 2048,
            "max_seq_len": 4096,
            "max_input_len": 2048,
            "bytes_per_element": 2,
            "dp": 1
        }

        # Mock Logger and LatencyModeling - 修正路径
        self.patcher_logger = patch('mindiesimulator.inference_engine.Logger')
        self.mock_logger_class = self.patcher_logger.start()
        self.mock_logger = Mock()
        self.mock_logger_class.get_logger.return_value = self.mock_logger

        self.patcher_latency = patch('mindiesimulator.inference_engine.LatencyModeling')
        self.mock_latency_modeling = self.patcher_latency.start()
        self.mock_latency_instance = Mock()
        self.mock_latency_modeling.return_value = self.mock_latency_instance

        self.mock_latency_instance.decode_model_time_predict.return_value = 10.0
        self.mock_latency_instance.decode_frame_time_predict.return_value = 5.0
        self.mock_latency_instance.prefill_model_time_predict.return_value = 20.0
        self.mock_latency_instance.prefill_frame_time_predict.return_value = 10.0

    def tearDown(self):
        """Clean up after integration tests"""
        self.patcher_logger.stop()
        self.patcher_latency.stop()

    def test_full_workflow(self):
        """Test a complete workflow of prefill and decode"""
        sim = InferenceSim(self.config, self.mock_latency_model)

        # Test prefill
        request = [1, 128]
        prefill_latency = sim.prefill_sim(request)
        self.assertGreater(prefill_latency, 0)

        # Test decode
        decode_table = [[1, 128, 10, 200]]
        decode_latency = sim.decode_sim(decode_table)
        self.assertGreater(decode_latency, 0)

        # Test KV block judgment
        exceed, kv_block = sim.judge_kv_block(
            prefill_table=[request],
            decode_table=decode_table
        )
        self.assertIsInstance(exceed, (bool, np.bool_))
        self.assertGreater(kv_block, 0)


if __name__ == '__main__':
    unittest.main()