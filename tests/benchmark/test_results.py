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

import queue
import unittest
from unittest.mock import patch, MagicMock
from mock_object import MockBenchmarkTokenizer
from mindiebenchmark.common.textvqa_eval import VQAEvalMethod
from benchmark.module import Config, ResultData, DatasetLoaderFactory


TEST_MOD_PATH = "mindiebenchmark.common.results."
TEXT_OUTPUT_DIR = "text_output"
OUTPUT_TOKEN_ID_DIR = "output_token_id"
LOGPROBS_DIR = "logprobs"
OUTPUT_DIR = "output"


class TestResultData(unittest.TestCase):
    def setUp(self):
        self.tokenize_encode_mocker = patch(
            'mindiebenchmark.common.tokenizer.BenchmarkTokenizer.encode',
            return_value=1
        )
        self.tokenize_encode_mocker.start()
        self.tokenize_mocker = patch(
            'mindiebenchmark.common.tokenizer.BenchmarkTokenizer',
            return_value=MockBenchmarkTokenizer
        )
        self.tokenize_mocker.start()
        patch("mindieclient.python.common.config.Config._check_file_permission",
                    return_value=(True, None)).start()
        self.config = Config()
        self.config.command_line_args.update(
            {
                'TaskKind': "stream",
                'ModelName': "Llama2",
                'DatasetType': "ceval",
                'Http': "http://127.0.0.1:1025",
                'MaxInputLen': 1024,
                'MaxOutputLen': 1024,
                "MAX_LINK_NUM": 1,
                "RequestRate": "",
                "WarmupSize": 10,
                'ManagementHttp': "http://127.0.0.2:1026",
            }
        )
        self.result = ResultData(self.config.command_line_args)
        self.config.config = {"MAX_LINK_NUM": 1}

        result = ResultData(self.config.command_line_args)

    def test_cleanup_code(self):
        code = " I am from Chinese.\nMy name is Tom."
        self.assertEqual(self.result.cleanup_code(code), code.split("\n")[0])
        code = ""
        self.assertEqual(self.result.cleanup_code(code), "")

    def test_calculate_metrics(self):
        self.result.add_success_req()
        self.assertEqual(self.result.success_req, 1)
        self.result.add_returned_req()
        self.assertEqual(self.result.returned_req, 1)
        self.result.add_empty_req()
        self.assertEqual(self.result.empty_req, 1)
        self.result.add_total_req()
        self.assertEqual(self.result.total_req, 1)
        self.result.add_total_time(1)
        self.assertEqual(self.result.total_time_elapsed, 1)
        self.result.add_total_tokens(1)
        self.assertEqual(self.result.total_tokens, 1)

    def test_process_openai_stream_normal_case(self):
        i_req_res = {
            OUTPUT_DIR: [
                {
                    TEXT_OUTPUT_DIR: {'0': 'Hello', '1': 'World'},
                    LOGPROBS_DIR: 0.8
                },
                {
                    TEXT_OUTPUT_DIR: {'0': ', ', '1': '!'},
                    LOGPROBS_DIR: 0.6
                }
            ]
        }

        expected_output = {
            OUTPUT_DIR: ['Hello, ', 'World!'],
            LOGPROBS_DIR: [0.8, 0.6],
            OUTPUT_TOKEN_ID_DIR: [1, 1]
        }

        self.result.process_openai_stream(i_req_res)
        self.assertEqual(i_req_res[OUTPUT_DIR], expected_output[OUTPUT_DIR])
        self.assertEqual(i_req_res.get(LOGPROBS_DIR), expected_output[LOGPROBS_DIR])
        self.assertEqual(i_req_res.get(OUTPUT_TOKEN_ID_DIR), expected_output[OUTPUT_TOKEN_ID_DIR])
    
    def test_process_openai_stream_when_no_logprobs(self):
        i_req_res = {
            OUTPUT_DIR: [
                {
                    TEXT_OUTPUT_DIR: {'0': 'Hello', '1': 'World'},
                },
                {
                    TEXT_OUTPUT_DIR: {'0': ', ', '1': '!'},
                }
            ]
        }

        expected_output = {
            OUTPUT_DIR: ['Hello, ', 'World!'],
            OUTPUT_TOKEN_ID_DIR: [1, 1]
        }

        self.result.process_openai_stream(i_req_res)
        self.assertEqual(i_req_res[OUTPUT_DIR], expected_output[OUTPUT_DIR])
        self.assertNotIn(LOGPROBS_DIR, i_req_res)
        self.assertIn(OUTPUT_TOKEN_ID_DIR, i_req_res)
        self.assertEqual(i_req_res.get(OUTPUT_TOKEN_ID_DIR), expected_output[OUTPUT_TOKEN_ID_DIR])
    
    def test_process_openai_text_normal_case(self):
        i_req_res = {
            OUTPUT_DIR: [
                {
                    'message': {'content': 'Hello, world!'},
                    LOGPROBS_DIR: 0.8
                },
                {
                    'message': {'content': 'How are you?'},
                    LOGPROBS_DIR: 0.6
                }
            ]
        }
        expected_output = {
            OUTPUT_DIR: ['Hello, world!', 'How are you?'],
            OUTPUT_TOKEN_ID_DIR: [1, 1],
            LOGPROBS_DIR: [0.8, 0.6]
        }
        self.result.process_openai_text(i_req_res)
        self.assertEqual(i_req_res[OUTPUT_DIR], expected_output[OUTPUT_DIR])
        self.assertEqual(i_req_res.get(OUTPUT_TOKEN_ID_DIR), expected_output[OUTPUT_TOKEN_ID_DIR])
        self.assertEqual(i_req_res.get(LOGPROBS_DIR), expected_output[LOGPROBS_DIR])
    
    def test_process_vllm_stream_normal_case(self):
        i_req_res = {
            OUTPUT_DIR: [
                ['a', 'b'],
                ['c', 'd']
            ]
        }
        expected_output = {
            OUTPUT_DIR: ['ac', 'bd'],
            OUTPUT_TOKEN_ID_DIR: [1, 1]
        }
        self.result.process_vllm_stream(i_req_res)
        self.assertEqual(i_req_res[OUTPUT_DIR], expected_output[OUTPUT_DIR])
        self.assertEqual(i_req_res.get(OUTPUT_TOKEN_ID_DIR), expected_output[OUTPUT_TOKEN_ID_DIR])

    def test_add_req_res(self):
        q = queue.Queue(1000)
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.config.command_line_args, q, self.result)
        self.result.loader = loader
        self.result.cal_acc = True
        self.result.test_type = "engine"

        data = [
            {
                "id": 1,
                "input_data": [1],
                'prefill_latencies': 1,
                'decode_token_latencies': [1],
                'decode_max_token_latencies': 1,
                'seq_latencies': 1,
                'input_tokens_len': 1,
                'generate_tokens_len': 1,
                'generate_characters_len': 1,
                'prefill_batch_size': 1,
                'decode_batch_size': [1],
                'queue_wait_time': [1],
                'post_processing_time': 1,
                'tokenizer_time': 1,
                'detokenizer_time': 1,
                'output': [1],
                'characters_per_token': 1,
                'request_id': 1,
            }
        ]
        self.assertIsNone(self.result.add_req_res(data))

    def test_convert_result(self):
        self.assertIsNotNone(self.result.convert_result())

    def test_cal_acc_gsm8k(self):
        # correct and false answer examples
        self.result.gt_answer = {"1": "6", "2": "7"}
        correct_id = "1"
        false_id = "2"
        res_data = '''
        A) 3 B) 4 C) 5 D) 6 E) 7
        Answer:
        Harry slept 9 hours.
        James slept 3 hours.
        9 - 3 = 6 hours.
        The answer is D.)
        '''

        # assert
        result = self.result.cal_acc_gsm8k(correct_id, res_data)
        self.assertEqual(result, 1)

        result = self.result.cal_acc_gsm8k(false_id, res_data)
        self.assertEqual(result, 0)

    def test_cal_acc_vocalsound(self):
        # correct and false answer examples
        self.result.gt_answer = {
            "001": "sigh",
            "002": "laughter",
            "003": "cough",
            # more entries...
        }

        correct_id = "001"
        # assert
        self.assertEqual(self.result.cal_acc_vocalsound(correct_id, "answer is B"), 1)
        self.assertEqual(self.result.cal_acc_vocalsound(correct_id, "answer is sigh"), 1)
        self.assertEqual(self.result.cal_acc_vocalsound(correct_id, "answer is Sigh"), 1)
        self.assertEqual(self.result.cal_acc_vocalsound(correct_id, "answer isSighhhh"), 1)
        self.assertEqual(self.result.cal_acc_vocalsound(correct_id, "answer is C"), 0)
        self.assertEqual(self.result.cal_acc_vocalsound(correct_id, "answer is laughter"), 0)

    def test_cal_acc_textvqa(self):
        # Define reusable string variables in snake_case
        answer_id = "answer_id"
        answer = "answer"
        answer_confidence = "answer_confidence"
        confidence_yes = "yes"
        koala = "koala"
        bear = "bear"
        koala_bear = "koala bear"

        # correct and false answer examples
        self.result.gt_answer = [[
            {answer_id: 0, answer: koala_bear, answer_confidence: confidence_yes},
            {answer_id: 1, answer: bear, answer_confidence: confidence_yes},
            {answer_id: 2, answer: koala, answer_confidence: confidence_yes},
            {answer_id: 3, answer: koala, answer_confidence: confidence_yes},
            {answer_id: 4, answer: koala, answer_confidence: confidence_yes},
            {answer_id: 5, answer: koala, answer_confidence: confidence_yes},
            {answer_id: 6, answer: koala, answer_confidence: confidence_yes},
            {answer_id: 7, answer: koala_bear, answer_confidence: confidence_yes},
            {answer_id: 8, answer: bear, answer_confidence: confidence_yes},
            {answer_id: 9, answer: koala, answer_confidence: confidence_yes}
        ]]

        self.result.vqa_eval = VQAEvalMethod()
        correct_id = 0

        # assert
        self.assertEqual(self.result.cal_acc_textvqa(correct_id, koala), 1)
        self.assertEqual(self.result.cal_acc_textvqa(correct_id, bear), 0.6)


    def test_cal_acc_videobench(self):
        # correct and false answer examples
        self.result.gt_answer = {
            0: "A",
            1: "B",
            2: "C",
            3: "D"
        }
        self.result.vqa_eval = VQAEvalMethod()
        # assert
        self.assertEqual(self.result.cal_acc_videobench(0, "A"), 1)
        self.assertEqual(self.result.cal_acc_videobench(1, "B"), 1)
        self.assertEqual(self.result.cal_acc_videobench(2, "C"), 1)
        self.assertEqual(self.result.cal_acc_videobench(3, "D"), 1)

        self.assertEqual(self.result.cal_acc_videobench(1, "A"), 0)
        self.assertEqual(self.result.cal_acc_videobench(2, "A"), 0)
        self.assertEqual(self.result.cal_acc_videobench(3, "A"), 0)

    def test_save_eval_results_success(self):
        # Test case: Successful calculation and saving to file
        eval_model = self.result
        eval_model.dataset_type = "truthfulqa"
        eval_model.model_name = "TestModel"
        eval_model.eval_res = {"BLEU": [0.5, 0.6], "ROUGE": [0.7, 0.8]}  # Valid BLEU and ROUGE values
        
        with patch(TEST_MOD_PATH + 'PathCheck.check_path_full') as mock_check_path, \
             patch(TEST_MOD_PATH + 'os.open') as mock_os_open, \
             patch(TEST_MOD_PATH + 'os.fdopen') as mock_fdopen, \
             patch(TEST_MOD_PATH + 'json.dump') as mock_json_dump, \
             patch(TEST_MOD_PATH + 'Logger') as mock_logger, \
             patch(TEST_MOD_PATH + 'os.path.exists') as mock_exists:
            # Mock the behaviors
            mock_exists.return_value = False  # Simulate that the file does not exist
            mock_check_path.return_value = (True, "")  # Path check passes
            mock_os_open.return_value = 1  # Simulate successful file open (file descriptor)
            
            # Mock the context manager behavior of fdopen
            mock_fdopen.return_value.__enter__.return_value = MagicMock()
            
            # No return from json.dump
            mock_json_dump.return_value = None  
        
            # Call the method
            eval_model.save_eval_results()
        
            # Assert the calculations for BLEU and ROUGE
            avg_bleu = sum([0.5, 0.6]) / 2  # 0.55
            avg_rouge = sum([0.7, 0.8]) / 2  # 0.75
        
            # Check if the json.dump was called with correct arguments
            mock_json_dump.assert_called_once_with({
                "ModelName": "TestModel",
                "Dataset": "truthfulqa",
                "BLEU": avg_bleu,
                "ROUGE": avg_rouge,
            }, mock_fdopen.return_value.__enter__.return_value)
        
    @patch(TEST_MOD_PATH + 'os.path.exists')
    @patch(TEST_MOD_PATH + 'PathCheck.check_path_full')
    @patch(TEST_MOD_PATH + 'Logger')
    def test_zero_division_error_bleu(self, mock_logger, mock_check_path, mock_exists):
        # Test case: ZeroDivisionError for BLEU calculation (empty list)
        eval_model = self.result
        eval_model.dataset_type = "truthfulqa"
        eval_model.model_name = "TestModel"
        eval_model.eval_res = {"BLEU": [], "ROUGE": [0.7, 0.8]}  # BLEU is empty, ROUGE is valid

        # Mock behaviors
        mock_exists.return_value = False
        mock_check_path.return_value = (True, "")  # Path check passes

        # Call the method and expect a ZeroDivisionError for BLEU calculation
        with self.assertRaises(ZeroDivisionError):
            eval_model.save_eval_results()

            # Assert error is logged for BLEU calculation
            mock_logger.error.assert_called_with("[MIE01E000005] zero division error occurs while calculating BLEU")

    @patch(TEST_MOD_PATH + 'os.path.exists')
    @patch(TEST_MOD_PATH + 'PathCheck.check_path_full')
    @patch(TEST_MOD_PATH + 'Logger')
    def test_zero_division_error_rouge(self, mock_logger, mock_check_path, mock_exists):
        # Test case: ZeroDivisionError for ROUGE calculation (empty list)
        eval_model = self.result
        eval_model.dataset_type = "truthfulqa"
        eval_model.model_name = "TestModel"
        eval_model.eval_res = {"BLEU": [0.5, 0.6], "ROUGE": []}  # ROUGE is empty, BLEU is valid

        # Mock behaviors
        mock_exists.return_value = False
        mock_check_path.return_value = (True, "")  # Path check passes

        # Call the method and expect a ZeroDivisionError for ROUGE calculation
        with self.assertRaises(ZeroDivisionError):
            eval_model.save_eval_results()

            # Assert error is logged for ROUGE calculation
            mock_logger.error.assert_called_with("[MIE01E000005] zero division error occurs while calculating ROUGE")

    @patch(TEST_MOD_PATH + 'os.path.exists')
    @patch(TEST_MOD_PATH + 'PathCheck.check_path_full')
    @patch(TEST_MOD_PATH + 'Logger')
    def test_path_check_fail(self, mock_logger, mock_check_path, mock_exists):
        # Test case: PathCheck fails
        eval_model = self.result
        eval_model.dataset_type = "truthfulqa"
        eval_model.model_name = "TestModel"
        eval_model.eval_res = {"BLEU": [0.5, 0.6], "ROUGE": [0.7, 0.8]}  # Valid data

        # Mock the behaviors
        mock_exists.return_value = True  # Simulate that the file exists
        mock_check_path.return_value = (False, "Path is invalid")  # Simulate path check failure

        # Call the method and expect a ValueError due to the failed path check
        with self.assertRaises(ValueError):
            eval_model.save_eval_results()
            # Assert the correct error message is logged
            mock_logger.error.assert_called_with(
                "[MIE01E000005] Check ./truthfulqa_eval.json failed because Path is invalid")


    def tearDown(self):
        del self.result


if __name__ == "__main__":
    unittest.main()
