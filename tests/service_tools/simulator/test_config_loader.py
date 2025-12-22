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
import json
import os
import tempfile
from unittest.mock import patch, mock_open, MagicMock
import subprocess
import re
from pathlib import Path

from mindiesimulator.common.config_loader import ConfigLoader, input_validate, HBM_32G, HBM_64G

HARDWARE_MODEL_CONFIG = "hardware_model_config"
WORLD_SIZE = 8


class TestInputValidate(unittest.TestCase):
    def test_valid_input(self):
        self.assertEqual(input_validate("123abc"), 123)
    
    def test_long_input(self):
        with self.assertRaises(ValueError):
            input_validate("a" * 101)
    
    def test_no_digits(self):
        with self.assertRaises(ValueError):
            input_validate("abc")


class TestConfigLoader(unittest.TestCase):
    @patch.object(ConfigLoader, "extract_hbm_size")
    @patch.object(ConfigLoader, "extract_world_size")
    def setUp(self, mock_extract_world_size, mock_extract_hbm_size):
        # Create temporary files for testing
        self.temp_dir = tempfile.TemporaryDirectory()
        
        # Create version.info file
        self.version_file = os.path.join(self.temp_dir.name, "../version.info")
        with open(self.version_file, "w") as f:
            f.write("mindie-service: 1.0.0\n")
        
        # Create config.json file
        os.makedirs(os.path.join(self.temp_dir.name, "conf"), exist_ok=True)
        self.config_file = os.path.join(self.temp_dir.name, "conf/config.json")
        config_data = {
            "BackendConfig": {
                "ModelDeployConfig": {
                    "ModelConfig": [{
                        "worldSize": 8,
                        "modelWeightPath": self.temp_dir.name,
                        "modelName": "DeepSeek_R1_671B",
                        "tp": 2,
                        "dp": 4
                    }]
                }
            }
        }
        with open(self.config_file, "w") as f:
            json.dump(config_data, f)
        
        # Create model config.json
        self.model_config_file = os.path.join(self.temp_dir.name, "config.json")
        model_config_data = {
            "hidden_size": 4096,
            "num_hidden_layers": 32,
            "num_attention_heads": 32,
            "num_key_value_heads": 8,
            "quantize": "w8a8"
        }
        with open(self.model_config_file, "w") as f:
            json.dump(model_config_data, f)
        
        # Patch environment variables
        self.env_patch = patch.dict(os.environ, {"MIES_INSTALL_PATH": self.temp_dir.name})
        self.env_patch.start()
        
        # Patch subprocess.run for npu-smi
        self.subprocess_patch = patch('subprocess.run')
        self.mock_subprocess = self.subprocess_patch.start()
        mock_result = MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = "HBM: 0 / 64"
        self.mock_subprocess.return_value = mock_result
        
        # Patch shutil.which
        self.which_patch = patch('shutil.which', return_value="/usr/bin/npu-smi")
        self.mock_which = self.which_patch.start()

        mock_extract_hbm_size.return_value = HBM_64G
        mock_extract_world_size.return_value = WORLD_SIZE
        self.loader = ConfigLoader()

    def tearDown(self):
        self.temp_dir.cleanup()
        Path(self.version_file).unlink(missing_ok=True)
        self.env_patch.stop()
        self.subprocess_patch.stop()
        self.which_patch.stop()

    def test_init(self):
        self.assertEqual(self.loader.version_info, "1.0.0")
        self.assertEqual(self.loader.world_size, 8)
        self.assertEqual(self.loader.model_name, "DeepSeek_R1_671B")
        self.assertEqual(self.loader.llm_size, 671)
        self.assertEqual(self.loader.hbm_size, HBM_64G)
        self.assertEqual(self.loader.bytes_per_element, 1)
        self.assertEqual(self.loader.tp, 2)
        self.assertEqual(self.loader.dp, 4)
        
        expected_params = {
            "hidden_size": 4096,
            "num_hidden_layers": 32,
            "num_attention_heads": 32,
            "num_key_value_heads": 8,
            "model_name": "DeepSeek_R1_671B",
            "llm_size": 671,
            "tp": 2,
            "dp": 4,
            "bytes_per_element": 1
        }
        self.assertEqual(self.loader.model_params, expected_params)

    def test_extract_version(self):
        version = self.loader.extract_version()
        self.assertEqual(version, "1.0.0")

    def test_extract_version_not_found(self):
        version = self.loader.extract_version("nonexistent-key")
        self.assertIsNone(version)

    def test_load_service_config(self):
        config = self.loader.load_service_config()
        self.assertIn("BackendConfig", config)

    def test_load_service_config_invalid(self):
        with open(self.config_file, "w") as f:
            f.write("invalid json")
        
        with self.assertRaises(ValueError):
            self.loader.load_service_config()

    def test_load_service_config_not_found(self):
        os.remove(self.config_file)
        with self.assertRaises(FileNotFoundError):
            self.loader.load_service_config()

    def test_extract_world_size(self):
        with patch('builtins.input', return_value=""):
            world_size = self.loader.extract_world_size()
            self.assertEqual(world_size, 8)

    def test_extract_world_size_user_input(self):
        with patch('builtins.input', return_value="16"):
            world_size = self.loader.extract_world_size()
            self.assertEqual(world_size, 16)

    def test_extract_world_size_invalid_input(self):
        with patch('builtins.input', return_value="invalid"):
            with self.assertRaises(ValueError):
                self.loader.extract_world_size()

    def test_extract_model_config_path(self):
        path = self.loader.extract_model_config_path()
        self.assertEqual(path, self.temp_dir.name)

    def test_extract_model_name(self):
        name = self.loader.extract_model_name()
        self.assertEqual(name, "DeepSeek_R1_671B")

    def test_extract_llm_size_from_name(self):
        self.loader.model_name = "Model_7B"
        size = self.loader.extract_llm_size()
        self.assertEqual(size, 7)

    def test_extract_llm_size_user_input(self):
        self.loader.model_name = "Model_without_size"
        with patch('builtins.input', return_value="13"):
            size = self.loader.extract_llm_size()
            self.assertEqual(size, 13)

    def test_extract_model_params(self):
        params = self.loader.extract_model_params()
        self.assertEqual(params["hidden_size"], 4096)
        self.assertEqual(params["num_hidden_layers"], 32)

    def test_extract_bytes_per_element_8bit(self):
        self.loader.model_config = {"quantize": "w8a8"}
        self.assertEqual(self.loader.extract_bytes_per_element(), 1)

    def test_extract_bytes_per_element_default(self):
        self.loader.model_config = {"quantize": "w16a16"}
        self.assertEqual(self.loader.extract_bytes_per_element(), 2)

    def test_extract_tp_when_dp_1(self):
        self.loader.dp = 1
        self.loader.world_size = 8
        tp = self.loader.extract_tp()
        self.assertEqual(tp, 8)

    def test_extract_tp_from_config(self):
        self.loader.dp = 4
        tp = self.loader.extract_tp()
        self.assertEqual(tp, 2)

    def test_open_config_file_invalid(self):
        with self.assertRaises(FileNotFoundError):
            ConfigLoader.open_config_file("/invalid/path")

    def test_updata_config(self):
        temp_config = os.path.join(self.temp_dir.name, "temp_config.json")
        with open(temp_config, "w") as f:
            json.dump({HARDWARE_MODEL_CONFIG: {}}, f)
        
        self.loader.updata_config(temp_config)
        
        with open(temp_config, "r") as f:
            updated = json.load(f)
            self.assertIn("world_size", updated[HARDWARE_MODEL_CONFIG])
            self.assertIn("model_name", updated[HARDWARE_MODEL_CONFIG])


if __name__ == '__main__':
    unittest.main()