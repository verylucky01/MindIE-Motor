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
from unittest.mock import patch

import numpy as np
from mock_object import MockBenchmarkTokenizer
from mindiebenchmark.datasets.truthfulqa_eval import (
    run_bleu_and_rouge,
    format_prompt,
    parse_answers,
    split_multi_answer,
    find_subsequence,
    bleu,
    rouge,
    cal_bleu_and_rouge,
    BenchmarkTokenizer,
    QA_PRIMER,
    ANSWER_COL,
    INCORRECT_COL
)


class TestQA(unittest.TestCase):
    def setUp(self):
        # Setup data for each test
        self.loader = None  # You can mock this if needed

        self.patcher_init = patch.object(BenchmarkTokenizer, 'init', MockBenchmarkTokenizer.init)
        self.patcher_encode = patch.object(BenchmarkTokenizer, 'encode', MockBenchmarkTokenizer.encode)
        self.patcher_decode = patch.object(BenchmarkTokenizer, 'decode', MockBenchmarkTokenizer.decode)

        # Start patching the methods
        self.patcher_init.start()
        self.patcher_encode.start()
        self.patcher_decode.start()
        MockBenchmarkTokenizer.init("MockPath")
        
        mock_ref_true = "Telescopes use mirrors to focus light and make objects appear closer."
        mock_pred = "Telescopes use lenses or mirrors to focus light and make objects appear closer."
        mock_ref_false = "I have no comment."
        
        self.out_token_ids = np.array(
            BenchmarkTokenizer.encode("A: Telescopes use lenses to focus light and make objects appear closer."))
        self.answers = {
            "Correct Answers": ["Telescopes use lenses to focus light and make objects appear closer."],
            "Incorrect Answers": [mock_ref_false]
        }

        self.row_data = {
            "Correct Answers": "Paris; Berlin.",
            "Incorrect Answers": mock_ref_false
        }
        self.question_data = {
            "Question": "What is the capital of France?"
        }
        self.full_array = np.array([1, 2, 3, 4, 5, 6])
        self.sub_array = np.array([3, 4])
        
        self.references = [[mock_ref_true]]
        self.predictions = [mock_pred]
        self.sequence = mock_ref_true
        self.ref_true = [mock_ref_true]
        self.ref_false = [mock_ref_false]

    def test_run_bleu_and_rouge(self):
        result = run_bleu_and_rouge(self.loader, self.out_token_ids, self.answers)
        self.assertEqual(len(result), 2)  # It should return two values: BLEU and ROUGE
        self.assertIsInstance(result[0], float)  # BLEU score should be a float
        self.assertIsInstance(result[1], float)  # ROUGE score should be a float

    def test_format_prompt(self):
        prompt = format_prompt(self.question_data)
        self.assertTrue(prompt.startswith(QA_PRIMER))  # It should start with the QA_PRIMER string
        self.assertIn("Q: What is the capital of France?", prompt)  # The question should be included in the prompt

    def test_parse_answers(self):
        answers = parse_answers(self.row_data)
        self.assertEqual(len(answers[ANSWER_COL]), 2)  # Should split into two answers for correct answers
        self.assertEqual(len(answers[INCORRECT_COL]), 1)  # Should only have one incorrect answer
        self.assertTrue(answers[ANSWER_COL][0].endswith('.'))  # Answers should end with a period

    def test_split_multi_answer(self):
        answers = split_multi_answer(self.row_data["Correct Answers"], sep=";")
        self.assertEqual(len(answers), 2)  # Should split into two answers
        self.assertEqual(answers[0], "Paris.")  # Should append period to each answer
        self.assertEqual(answers[1], "Berlin.")  # Last answer should also have period

    def test_find_subsequence(self):
        start_idx = find_subsequence(self.full_array, self.sub_array, start=True)
        end_idx = find_subsequence(self.full_array, self.sub_array, start=False)
        self.assertEqual(start_idx, 4)  # The subsequence starts at index 4
        self.assertEqual(end_idx, 0)  # The subsequence ends at index 0 from the end (5th element)

    def test_bleu(self):
        bleu_score = bleu(self.references, self.predictions)
        self.assertIn('bleu', bleu_score)  # BLEU score should be present
        self.assertGreaterEqual(bleu_score['bleu'], 0)  # BLEU score should be non-negative

    def test_rouge(self):
        rouge_scores = rouge(self.references[0], self.predictions)
        self.assertIn('rouge1', rouge_scores)  # Rouge1 score should be present
        self.assertGreaterEqual(rouge_scores['rouge1'], 0)  # Rouge score should be non-negative

    def test_cal_bleu_and_rouge(self):
        result = cal_bleu_and_rouge(self.sequence, self.ref_true, self.ref_false)
        self.assertEqual(len(result), 2)  # It should return two scores: BLEU and ROUGE
        self.assertIsInstance(result[0], float)  # BLEU score should be float
        self.assertIsInstance(result[1], float)  # ROUGE score should be float

    def tearDown(self):
        patch.stopall()


if __name__ == '__main__':
    unittest.main()
