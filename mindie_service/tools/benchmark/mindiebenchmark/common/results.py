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

import csv
import json

import math
import os
import re
import stat
from decimal import Decimal, InvalidOperation
from typing import List

import numpy as np
import mindiebenchmark.datasets.truthfulqa_eval as tqa
from .file_utils import PathCheck
from .logging import Logger
from .tokenizer import BenchmarkTokenizer


# 初始化某个请求数据字典，其中decode相关的时延和batch_size是多个值，对应空列表
request_result_template = {
    "prefill_latencies": 0,
    "decode_token_latencies": [],
    "decode_max_token_latencies": 0,
    "seq_latencies": 0,
    "input_tokens_len": 0,
    "generate_tokens_len": 0,
    "generate_characters_len": 0,
    "prefill_batch_size": 0,
    "decode_batch_size": [],
    "queue_wait_time": [],
    "post_processing_time": 0,
    "tokenizer_time": 0,
    "detokenizer_time": 0,
    "characters_per_token": 0.0,
    "request_id": ""
}

TEXT_OUTPUT_STR = "text_output"
LOGPROBS_STR = "logprobs"
OUTPUT_STR = "output"


class ResultData:
    """
    定义BenchMark压测中间输出结果。

    成员变量说明:
        self.prefill_latencies: 存储首token时延，结构为[]每个prefill包含多个token
        self.decode_token_latencies: 存储非首token时延，结构为[],每个decode包含多个token，更新时平铺
        self.decode_max_token_latencies: 存储非首token当前批次最大时延，结构为[]
        self.seq_latencies: 存储推理seq时延，结构为[],每个响应请求包含多个seq
        self.seq_throughput: 存储推理seq吞吐，结构为[],,每个响应请求包含多个seq
        self.input_tokens_len: 存储输入tokens长度，结构为[],每个请求包含多个token
        self.generate_tokens_len: 存储输出tokens长度，结构为[],每个请求包含多个token
        self.generate_characters_len: 存储输出characters长度，结构为[],,每个请求包含多个characters
        self.prefill_batch_size: 存储prefilld阶段处理时的batchsize，结构为[]
        self.decode_batch_size: 存储decode阶段处理时的batchsize，结构为[]，,每个decode包含多个token，更新时平铺
        self.queue_wait_time: 存储每个请求处理时的等待时间，结构为[]
        self.post_processing_time: 存储每个请求后处理时的时间，结构为[]
        self.tokenizer_time: 存储每个请求tokenizer时间，结构为[]，涉及北向接口模式
        self.detokenizer_time: 存储每个请求detokenizer时间，结构为[]，涉及北向接口模式
        self.log_post_processing_time: 存储每个token的后处理时间，结构为[]
        self.forward_time: 存储每个token的模型推理时间，结构为[]
        self.total_time_elapsed: 存储测试总耗时
        self.total_throughput: 存储测试总吞吐
        self.total_tokens: 存储测试总tokens
        self.total_characters: 存储测试总characters
        self.success_req: 存储测试总成功请求数
        self.total_req: 存储测试总请求数
        self.request_id_dict: 存储所有请求request_id和data_id映射关系的字典
    """

    def __init__(self, command_line_args: dict):
        self.logger = Logger().logger
        self.dataset_type = command_line_args.get("DatasetType")
        self.dataset_path = command_line_args.get("DatasetPath")
        self.test_type = command_line_args.get("TestType")
        self.cal_acc = command_line_args.get("TestAccuracy")
        self.tokenizer = command_line_args.get("Tokenizer")
        self.task_kind = command_line_args.get("TaskKind")
        self.model_name = command_line_args.get("ModelName")
        self.save_tokens_path = command_line_args.get("SaveTokensPath")
        self.save_output_file = command_line_args.get("SaveHumanEvalOutputFile")
        self.save_path = command_line_args.get("SavePath")
        self.prefill_latencies = []
        self.decode_token_latencies = []
        self.last_decode_latencies = []
        self.decode_max_token_latencies = []
        self.seq_latencies = []
        self.input_tokens_len = []
        self.generate_tokens_len = []
        self.generate_characters_len = []
        self.prefill_batch_size = []
        self.decode_batch_size = []
        self.queue_wait_time = []
        self.post_processing_time = []
        self.tokenizer_time = []
        self.detokenizer_time = []
        self.generated_characters = []
        self.characters_per_token = []
        self.log_post_processing_time = []
        self.forward_time = []
        self.total_time_elapsed = 0
        self.total_throughput = 0
        self.total_tokens = 0
        self.total_characters = 0
        self.success_req = 0
        self.returned_req = 0
        self.empty_req = 0
        self.total_req = 0
        self.generate_tokens_speed = []
        self.gt_answer = {}
        self.eval_res = {}
        self.infer_res = {}
        self.correct_num = 0
        self.acc_result = 0.0
        self.engine_infer_time = 0.0
        self.result_id = 0
        self.results_per_request = {}
        self.request_id_dict = dict()
        from mindiebenchmark.common.datasetloaders.base_loader import BaseLoader

        self.loader: BaseLoader

        if self.dataset_type == "textvqa":
            from .textvqa_eval import VQAEvalMethod
            parts = self.dataset_path.split('/')
            path_prefix = '/'.join(parts[:-1])
            path_suffix = parts[-1].split('.')[0] + "_annotations.json"
            textvqa_gt_json_path = os.path.join(path_prefix, path_suffix)

            ret, infos = PathCheck.check_path_full(textvqa_gt_json_path)
            if not ret:
                err_msg = f"Failed to check path textvqa_gt_json_path because {infos}."
                self.logger.error(err_msg)
                raise ValueError(infos)

            with os.fdopen(os.open(textvqa_gt_json_path, os.O_RDONLY, 0o640), 'r', encoding='utf-8') as f:
                vqa_gt_answer = json.load(f)['annotations']
                for item in vqa_gt_answer:
                    self.gt_answer[item.get('question_id')] = item.get('answers')
            self.vqa_eval = VQAEvalMethod()

        if self.dataset_type == "videobench":
            videobench_gt_json_path = os.path.join(
                os.path.abspath(self.dataset_path).split('.')[0], "answer/ANSWER.json"
                )

            ret, infos = PathCheck.check_path_full(videobench_gt_json_path)
            if not ret:
                err_msg = f"Failed to check path videobench_gt_json_path because {infos}."
                self.logger.error(err_msg)
                raise ValueError(infos)
            with os.fdopen(os.open(videobench_gt_json_path, os.O_RDONLY, 0o640), 'r', encoding='utf-8') as f:
                self.vb_gt_answer = json.load(f)

    @staticmethod
    def cleanup_code(code: str) -> str:
        code_splits = code.split("\n")
        is_empty_line = False
        ind_empty_line = None
        for i, line in enumerate(code_splits):
            if len(line.strip()) > 0 and line[0] != " " and line[0] != "\t":
                is_empty_line = True
                ind_empty_line = i
                break
        if is_empty_line:
            code = "\n".join(code_splits[:ind_empty_line])
        else:
            end_words = [
                "\ndef",
                "\nclass",
                "\n#",
                "\nassert",
                '\n"""',
                "\nprint",
                "\nif",
                "\n\n\n",
            ]
            for w in end_words:
                if w in code:
                    code = code[: code.rfind(w)]
        return code

    @staticmethod
    def process_qvq_results(in_text):
        patterns = [
            r"\\boxed{\\text{([^}]+)}}",
            r"\\boxed{([^}]+)}"
        ]
        for pattern in patterns:
            match = re.search(pattern, in_text)
            if match:
                return match.group(1)
        return in_text


    def process_openai_stream(self, i_req_res):
        i_req_res_output = i_req_res['output']
        output = [''] * len(i_req_res_output[0][TEXT_OUTPUT_STR])
        logprobs = []
        output_token_id = []
        for token in i_req_res_output:
            for key in token[TEXT_OUTPUT_STR].keys():
                output[int(key)] += token[TEXT_OUTPUT_STR][key]
        if "logprobs" in i_req_res_output[0].keys():
            for token in i_req_res_output:
                logprobs.append(token[LOGPROBS_STR])
            i_req_res['logprobs'] = logprobs
        if self.save_tokens_path:
            for i in range(len(i_req_res_output[0][TEXT_OUTPUT_STR])):
                output_token_id.append(BenchmarkTokenizer.encode(output[i]))
        i_req_res['output_token_id'] = output_token_id
        i_req_res['output'] = output

    def process_openai_text(self, i_req_res):
        i_req_res_output = i_req_res['output']
        output_token_id = []
        output = []
        logprobs = []
        message_key = 'message'
        for res in i_req_res_output:
            full_text = ""
            if 'reasoning_content' in res[message_key]:
                full_text += res[message_key]['reasoning_content']
            if 'content' in res[message_key]:
                full_text += res[message_key]['content']
            output.append(full_text)
            if self.save_tokens_path:
                output_token_id.append(BenchmarkTokenizer.encode(full_text))
            if LOGPROBS_STR in res.keys():
                logprobs.append(res[LOGPROBS_STR])
        i_req_res['output_token_id'] = output_token_id
        i_req_res['logprobs'] = logprobs
        i_req_res['output'] = output

    def process_vllm_stream(self, i_req_res):
        if not i_req_res[OUTPUT_STR]:
            self.logger.warning(f"req res has None output, req res info:{i_req_res}")
            return
        output = [''] * len(i_req_res[OUTPUT_STR][0])
        output_token_id = []
        for token in i_req_res[OUTPUT_STR]:
            for i, token_i in enumerate(token):
                output[i] += token_i
        if self.save_tokens_path:
            for i in range(len(i_req_res[OUTPUT_STR][0])):
                output_token_id.append(BenchmarkTokenizer.encode(output[i]))
        i_req_res['output_token_id'] = output_token_id
        i_req_res['output'] = output

    def set_loader(self, loader):
        self.loader = loader

    def add_success_req(self, req_count=1):
        self.success_req += req_count

    def add_returned_req(self, req_count=1):
        self.returned_req += req_count

    def add_empty_req(self, req_count=1):
        self.empty_req += req_count

    def add_total_req(self, req_count=1):
        self.total_req += req_count

    def add_total_time(self, time_elapsed):
        self.total_time_elapsed += time_elapsed

    def add_total_tokens(self, tokens):
        self.total_tokens += tokens

    def add_req_res(self, req_res_data: List, sub_process=True):
        # 推理后处理
        decode_key = "decode_token_latencies"
        id_key = "id"
        for i_req_res in req_res_data:
            if sub_process:
                if self.test_type == "openai" and self.task_kind == "stream":
                    self.process_openai_stream(i_req_res)
                if self.test_type == "openai" and self.task_kind == "text":
                    self.process_openai_text(i_req_res)
                if self.test_type == "vllm_client":
                    self.process_vllm_stream(i_req_res)
                self._record_detokenizer_time(i_req_res)
            seq_latencies = "seq_latencies"
            self.prefill_latencies.append(i_req_res["prefill_latencies"])
            self.decode_token_latencies.extend(i_req_res[decode_key])
            if i_req_res["decode_max_token_latencies"] > 0:
                self.decode_max_token_latencies.append(
                    i_req_res["decode_max_token_latencies"]
                )

            if len(i_req_res[decode_key]) > 0:
                self.last_decode_latencies.append(i_req_res[decode_key][-1])
            else:
                self.last_decode_latencies.append(0)

            self.seq_latencies.append(i_req_res[seq_latencies])
            if i_req_res[seq_latencies] > 0:
                self.generate_tokens_speed.append(
                    i_req_res["generate_tokens_len"] / i_req_res[seq_latencies] * 1000
                )
            self.input_tokens_len.append(i_req_res["input_tokens_len"])
            self.generate_tokens_len.append(i_req_res["generate_tokens_len"])
            self.prefill_batch_size.append(i_req_res["prefill_batch_size"])
            self.decode_batch_size.extend(i_req_res["decode_batch_size"])
            self.queue_wait_time.extend(i_req_res["queue_wait_time"])
            self.post_processing_time.append(i_req_res["post_processing_time"])
            self.characters_per_token.append(i_req_res["characters_per_token"])
            self.generate_characters_len.append(i_req_res["generate_characters_len"])
            output = i_req_res["output"]

            if self.save_tokens_path:
                self.save_tokens_to_csv(self.save_tokens_path, i_req_res)

            if self.dataset_type == "truthfulqa":
                out_ids = output
                if isinstance(output, str):
                    out_ids = BenchmarkTokenizer.encode(output, add_special_tokens=False)
                self.gen_eval_results(i_req_res[id_key], out_ids)

            # acc process
            if self.cal_acc and not sub_process:
                if isinstance(output, list) and len(output) > 0 and isinstance(output[0], int):
                    output = BenchmarkTokenizer.decode(output)
                elif isinstance(output, list) and len(output) > 0 and isinstance(output[0], str):
                    output = output[0]
                try:
                    res = self.compare_res_with_gt(i_req_res[id_key], output)
                    if res > 0:
                        self.correct_num += res
                except RuntimeError as run_error:
                    raise RuntimeError("Run compare_res_with_gt error") from run_error
                except Exception as err:
                    self.logger.error(f'[MIE01E000005] {err}')

            self.request_id_dict[i_req_res["request_id"]] = i_req_res[id_key]
        if self.test_type == "client":
            self.save_request_id_dict()

    def save_request_id_dict(self):
        file_path = os.path.join(self.save_path, "req_to_data_map.json")
        ret, infos = PathCheck.check_path_link(file_path)
        if not ret:
            raise ValueError(f"failed to save to `req_to_data_filepath`. {infos}")
        fd = os.open(file_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o640)
        with os.fdopen(fd, "w") as json_file:
            json.dump(self.request_id_dict, json_file)

    def gen_eval_results(self, req_id, out_token_ids):
        bleu = "BLEU"
        rouge = "ROUGE"
        if not self.eval_res:
            self.eval_res[bleu] = []
            self.eval_res[rouge] = []
        bleu_rouge = tqa.run_bleu_and_rouge(self.loader, out_token_ids, self.gt_answer.get(req_id))
        if bleu_rouge:
            self.eval_res[bleu].append(bleu_rouge[0])
            self.eval_res[rouge].append(bleu_rouge[1])

    def save_eval_results(self):
        bleu = "BLEU"
        rouge = "ROUGE"
        if self.dataset_type == "truthfulqa":
            try:
                avg_bleu = sum(self.eval_res.get(bleu)) / len(self.eval_res.get(bleu))
            except ZeroDivisionError as e:
                self.logger.error(f"[MIE01E000005] zero division error occurs while calculating BLEU")
                raise e
            try:
                avg_rouge = sum(self.eval_res.get(rouge)) / len(self.eval_res.get(rouge))
            except ZeroDivisionError as e:
                self.logger.error(f"[MIE01E000005] zero division error occurs while calculating ROUGE")
                raise e

            self.logger.info(f"{bleu}: {avg_bleu}, {rouge}: {avg_rouge}.")
            flags = (
                    os.O_WRONLY | os.O_CREAT
            )  # 注意根据具体业务的需要，通过改变os的对应常量来设置相应文件读写方式。
            modes = stat.S_IWUSR | stat.S_IRUSR  # 注意根据具体业务的需要设置文件权限
            filename = "./truthfulqa_eval.json"
            if os.path.exists(filename):
                ret, infos = PathCheck.check_path_full(filename)
                if not ret:
                    err_msg = f"Check {filename} failed because " + infos
                    self.logger.error(f'[MIE01E000005] {err_msg}')
                    raise ValueError(err_msg)
            with os.fdopen(
                    os.open(filename, flags, modes), "w"
            ) as file:
                json.dump({
                    "ModelName": self.model_name,
                    "Dataset": self.dataset_type,
                    bleu: avg_bleu,
                    rouge: avg_rouge,
                }, file)

    def save_tokens_to_csv(self, path, i_req_res: dict):
        def check_tokens(tokens):
            for token in tokens:
                if not isinstance(token, (int, np.int32, np.int64)) or token < 0:
                    raise ValueError(f"check tokens failed, token should be a positive number but got {token}")

        out_str = "output"
        output_token_id_str = "output_token_id"
        input_key = "input_data"
        req_id = i_req_res["id"]
        input_tokens = i_req_res[input_key]
        output_tokens = i_req_res[out_str]
        if isinstance(input_tokens, str):
            input_tokens = BenchmarkTokenizer.encode(i_req_res[input_key])

        if i_req_res.get(output_token_id_str):
            output_tokens = i_req_res.get(output_token_id_str)
        elif isinstance(output_tokens, str):
            add_special_tokens = self.dataset_type != "boolq"
            output_tokens = BenchmarkTokenizer.encode(output_tokens,
                                            add_special_tokens=add_special_tokens)

        if self.test_type in ["openai", "vllm_client", "engine"]:
            output_tokens = [i for i in i_req_res[output_token_id_str]]
            for i in output_tokens:
                check_tokens(i)
        else:
            check_tokens(output_tokens)

        check_tokens(input_tokens)
        row = [req_id, input_tokens, output_tokens]
        if LOGPROBS_STR in i_req_res.keys() and i_req_res[LOGPROBS_STR]:
            row.append(i_req_res[LOGPROBS_STR])

        flags = (
                os.O_WRONLY | os.O_CREAT
        )  # 注意根据具体业务的需要，通过改变os的对应常量来设置相应文件读写方式。
        modes = stat.S_IWUSR | stat.S_IRUSR  # 注意根据具体业务的需要设置文件权限
        with os.fdopen(
                os.open(path, flags, modes), "a" if os.path.isfile(path) else "w"
        ) as file:
            write = csv.writer(file)
            write.writerow(row)

    def update_gt_answer(self, gt_answer):
        if self.dataset_type == 'textvqa':
            return
        self.gt_answer = gt_answer

    def save_humaneval_output(self, data_id, res_data):
        flags = (
                os.O_WRONLY | os.O_CREAT
        )  # 注意根据具体业务的需要，通过改变os的对应常量来设置相应文件读写方式。
        modes = stat.S_IWUSR | stat.S_IRUSR  # 注意根据具体业务的需要设置文件权限
        if os.path.exists(self.save_output_file):
            ret, infos = PathCheck.check_path_full(self.save_output_file, is_size_check=True)
            if not ret:
                raise ValueError(infos)
        try:
            with os.fdopen(
                    os.open(self.save_output_file, flags, modes), "a+"
            ) as humaneval_outifile:
                completion_result = {
                    "task_id": data_id,
                    "completion": self.cleanup_code(res_data),
                }
                json.dump(completion_result, humaneval_outifile)
                humaneval_outifile.write("\n")
        except FileNotFoundError as e:
            raise ValueError(f"Failed to save HumanEval output to {self.save_output_file}") from e

    def compare_res_with_gt(self, data_id, res_data):
        from .datasetloaders.base_loader import extract_dataset_type_info
        dataset_name, _ = extract_dataset_type_info(self.dataset_type)
        acc_func_map = {
            "ceval": "cal_acc_ceval_mmlu",
            "mmlu": "cal_acc_ceval_mmlu",
            "gsm8k": "cal_acc_gsm8k",
            "textvqa": "cal_acc_textvqa",
            "videobench": "cal_acc_videobench",
            "vocalsound": "cal_acc_vocalsound"
        }
        if dataset_name == "humaneval":
            self.save_humaneval_output(data_id, res_data)
            return 1
        else:
            func_name = acc_func_map.get(dataset_name)
            supported_dataset_type = "humaneval"
            for key in acc_func_map.keys():
                supported_dataset_type += ", " + key
            if func_name is None:
                self.logger.error(
                f"[MIE01E000005] Unsupported dataset. Please choose [{supported_dataset_type}]."
                )
                return 0
            func = getattr(self, func_name)
            return func(data_id, res_data)

    def cal_acc_ceval_mmlu(self, data_id, res_data):
        if len(res_data.lstrip()) == 0:
            return 0
        gt = self.gt_answer.get(data_id)
        extract_res = res_data.lstrip()[0]
        if gt == extract_res:
            return 1
        else:
            return 0

    def cal_acc_gsm8k(self, data_id, res_data):
        # 取出文本最后一个数
        def extract_last_digit(res):
            re_str = r"([+-])?(?=([0-9]|\.[0-9]))(0|([1-9](\d{0,2}(,\d{3})*)|\d*))?(\.\d*)?(?=\D|$)"
            _pat_last_digit = re.compile(re_str)
            match = list(_pat_last_digit.finditer(res))
            if match:
                last_digit = match[-1].group().replace(",", "").replace("+", "").strip()
            else:
                last_digit = None
            return last_digit

        answer = self.gt_answer.get(data_id)
        res_last_digit = extract_last_digit(res_data)
        gt = extract_last_digit(answer)
        if res_last_digit is None or gt is None:
            return 0
        try:
            res_last_digit = Decimal(res_last_digit)
            gt = Decimal(gt)
            return math.isclose(res_last_digit, gt, rel_tol=0, abs_tol=Decimal('1e-4'))
        except (InvalidOperation, ValueError, TypeError, SyntaxError) as e:
            self.logger.error("[MIE01E000005] Error evaluating expression: %s. "
                              "Please check the predict result or the answer.", str(e))
            return 0
        except OverflowError as e:
            self.logger.error("[MIE01E000005] OverflowError: %s.", str(e))
            return 0

    def cal_acc_textvqa(self, data_id, res_data):

        res_data = res_data.replace('\n', ' ')
        res_data = res_data.replace('\t', ' ')
        res_data = res_data.strip()
        if self.model_name == 'vita':
            res_data = self.vqa_eval.remove_special_characters(res_data)
        if self.model_name == "qvq":
            res_data = self.process_qvq_results(res_data)
        res_data = self.vqa_eval.process_punctuation(res_data)
        res_data = self.vqa_eval.process_digit_article(res_data)
        gt_acc = []
        answer_ = "answer"
        gt_answers = self.gt_answer[data_id]
        gt_answer_list = [ans[answer_] for ans in gt_answers]
        if len(set(gt_answer_list)) > 1:
            for ans_dic in gt_answers:
                ans_dic[answer_] = self.vqa_eval.process_punctuation(ans_dic[answer_])
        for gt_ans in gt_answers:
            other_gt_ans = [item for item in gt_answers if item != gt_ans]
            matched_ans = [item for item in other_gt_ans if item[answer_] == res_data]
            acc = min(1, len(matched_ans) / 3)
            gt_acc.append(acc)

        avg_acc = sum(gt_acc) / len(gt_acc)

        return avg_acc

    def cal_acc_videobench(self, data_id, res_data):
        def find_choice(result):
            choice_list = ['A', 'B', 'C', 'D', 'E', 'F']
            for choice in choice_list:
                if choice in result:
                    return choice
            return ""
        if self.model_name == "qvq":
            res_data = self.process_qvq_results(res_data)
        choice = find_choice(res_data)
        gt_choice = self.gt_answer.get(int(data_id))
        return int(choice == gt_choice)

    def cal_acc_vocalsound(self, data_id, res_data):
        def find_choice(result):
            choose_map = {
                "A": "Laughter",
                "B": "Sigh",
                "C": "Cough",
                "D": "Throat clearing",
                "E": "Sneeze",
                "F": "Sniff"
            }
            for choose in choose_map:
                choose_ = choose + '.'
                if (choose_map[choose].lower() in result.lower()) or (choose_ in result) or (choose in result):
                    return choose
            return ""

        gt_answer_map = {
            "laughter": "A",
            "sigh": "B",
            "cough": "C",
            "throatclearing": "D",
            "sneeze": "E",
            "sniff": "F"
        }
        choice = find_choice(res_data)
        gt_answer = self.gt_answer.get(data_id)
        gt_choice = gt_answer_map.get(gt_answer)

        return int(choice == gt_choice)

    def convert_result(self):
        mapping = {
            "prefill_latencies": "FirstTokenTime",
            "decode_token_latencies": "DecodeTime",
            "last_decode_latencies": "LastDecodeTime",
            "decode_max_token_latencies": "MaxDecodeTime",
            "seq_latencies": "GenerateTime",
            "input_tokens_len": "InputTokens",
            "generate_tokens_len": "GeneratedTokens",
            "generate_tokens_speed": "GeneratedTokenSpeed",
            "generate_characters_len": "GeneratedCharacters",
            "tokenizer_time": "Tokenizer",
            "detokenizer_time": "Detokenizer",
            "characters_per_token": "CharactersPerToken",
            "log_post_processing_time": "PostProcessingTime",
            "forward_time": "ForwardTime",
        }

        if self.prefill_batch_size and self.decode_batch_size and self.queue_wait_time:
            mapping["prefill_batch_size"] = "PrefillBatchsize"
            mapping["decode_batch_size"] = "DecoderBatchsize"
            mapping["queue_wait_time"] = "QueueWaitTime"

        ans = {mapping_value: [] for mapping_value in mapping.values()}

        # Use a dictionary comprehension to populate the values
        for mapping_key, mapping_value in mapping.items():
            ans[mapping_value].extend(getattr(self, mapping_key))

        return ans

    def update(self, new_res: dict):
        self.total_time_elapsed = max(
            self.total_time_elapsed, new_res.get("total_time_elapsed", 0)
        )
        self.total_throughput += new_res.get("total_throughput", 0)
        self.total_tokens += new_res.get("total_tokens", 0)
        self.total_characters += new_res.get("total_characters", 0)
        self.success_req += new_res.get("success_req", 0)
        self.returned_req += new_res.get("returned_req", 0)
        self.empty_req += new_res.get("empty_req", 0)
        self.total_req += new_res.get("total_req", 0)
        self.tokenizer_time.extend(new_res.get("tokenizer_time", []))
        self.detokenizer_time.extend(new_res.get("detokenizer_time", []))
        self.add_req_res(new_res.get("all_results", []), False)
        self.extract_results_per_request(new_res.get("req_result_data", {}))

    def extract_results_per_request(self, req_res_data: dict):
        for key, value in req_res_data.items():
            if key in self.results_per_request:
                if isinstance(self.results_per_request[key], dict) and isinstance(value, dict):
                    self.extract_results_per_request(self.results_per_request[key], value)
                elif isinstance(self.results_per_request[key], list) and isinstance(value, list):
                    self.results_per_request[key].extend(value)
                else:
                    self.results_per_request[key] = value
            else:
                self.results_per_request[key] = value

    def _record_detokenizer_time(self, req_res):

        if req_res["tokenizer_time"] > 0 and req_res["detokenizer_time"] > 0:
            # use server return as record
            self.tokenizer_time.append(req_res["tokenizer_time"])
            self.detokenizer_time.append(req_res["detokenizer_time"])
        else:
            # calculate from benchmark side
            text_output = req_res['output']
            if isinstance(text_output, list) and isinstance(text_output[0], str):
                BenchmarkTokenizer.decode(BenchmarkTokenizer.encode(text_output[0]), self.detokenizer_time)
            elif isinstance(text_output, list) and isinstance(text_output[0], int):
                BenchmarkTokenizer.decode(text_output, self.detokenizer_time)
            elif isinstance(text_output, str): # normal process
                BenchmarkTokenizer.decode(BenchmarkTokenizer.encode(text_output), self.detokenizer_time)
            else:
                err_msg = "Unexpected Text Output Type of text_output :{} !".format(type(text_output))
                raise ValueError(err_msg)
