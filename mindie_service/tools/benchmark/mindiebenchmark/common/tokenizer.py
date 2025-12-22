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

import time
import os
import glob
from abc import ABC, abstractmethod


os.environ["TOKENIZERS_PARALLELISM"] = "true"


class Tokenizer(ABC):
    @abstractmethod
    def encode(self, ques_string: str, add_special_tokens=True):
        """Encode the input text to tokens."""
        pass

    @abstractmethod
    def decode(self, token_ids: list, skip_special_tokens=False):
        """Decode the tokens back to text."""
        pass


class HuggingfaceTokenizer(Tokenizer):
    def __init__(self, model_name_or_path: str, trust_remote_code: bool = False):
        import transformers
        from transformers import AutoTokenizer
        try:
            self.tokenizer_model = AutoTokenizer.from_pretrained(
                model_name_or_path, trust_remote_code=trust_remote_code, use_fast=True, local_files_only=True
            )
        except RecursionError as e:
            raise RecursionError(
                f"Failed to load the Tokenizer using AutoTokenizer. The current transformers "
                f"version is {transformers.__version__}. Please visit the HuggingFace official "
                "website to update to the latest weights and Tokenizer."
            ) from e
        except Exception as excepion:
            raise Exception("Please check your tokenizer") from excepion

    def encode(self, ques_string: str, add_special_tokens=True) -> list:
        return self.tokenizer_model.encode(ques_string, add_special_tokens=add_special_tokens)

    def decode(self, token_ids: list, skip_special_tokens=False) -> str:
        return self.tokenizer_model.decode(token_ids, skip_special_tokens=skip_special_tokens)


class MindformersTokenizer(Tokenizer):
    def __init__(self, model_name_or_path: str, trust_remote_code: bool = False):
        import mindformers
        from mindformers import get_model
        try:
            self.tokenizer_model, _ = get_model(
                model_name_or_path, trust_remote_code=trust_remote_code, use_fast=True, local_files_only=True)
        except RecursionError as e:
            raise RecursionError(
                f"Failed to load the Tokenizer using AutoTokenizer. The current mindformers "
                f"version is {mindformers.__version__}. Please visit the Mindformers official "
                "website to update to the latest weights and Tokenizer."
            ) from e
        except Exception as excepion:
            raise Exception("Please check your model") from excepion

    def encode(self, ques_string: str, add_special_tokens=True) -> list:
        return self.tokenizer_model.encode(ques_string, add_special_tokens=add_special_tokens)

    def decode(self, token_ids: list, skip_special_tokens=False) -> str:
        return self.tokenizer_model.decode(token_ids, skip_special_tokens=skip_special_tokens)


class BenchmarkTokenizer:
    """
    Factory class that provides static methods to initialize, encode,
    and decode text with a singleton tokenizer instance.

    Supported tokenizer types:
        - 'transformers': Uses `HuggingfaceTokenizer`.
        - 'mindformers': Uses `MindformersTokenizer`.
    """

    _tokenizer_instance = None

    @staticmethod
    def init(model_path: str, tokenizer_type: str = None, trust_remote_code: bool = False, **kwargs):
        """
        Initializes a singleton tokenizer instance based on the specified `tokenizer_type`.

        Args:
            model_path (str): Path to the pre-trained model for the tokenizer.
            tokenizer_type (str, optional): Type of the tokenizer. Defaults to 'transformers'.
                - 'transformers': Loads using `HuggingfaceTokenizer`.
                - 'mindformers': Loads using `MindformersTokenizer`.
            result (ResultData, optional): Timing data structure for recording encode/decode times.
            **kwargs: Additional keyword arguments for tokenizer initialization.

        Raises:
            ValueError: If the `tokenizer_type` is not supported.
        """
        if tokenizer_type is None:
            yaml_files = glob.glob(os.path.join(model_path, "*.yaml"))
            if len(yaml_files) > 0:
                tokenizer_type = 'mindformers'
            else:
                tokenizer_type = 'transformers'

        if BenchmarkTokenizer._tokenizer_instance is None:
            if tokenizer_type == 'transformers':
                BenchmarkTokenizer._tokenizer_instance = \
                    HuggingfaceTokenizer(model_path, trust_remote_code=trust_remote_code)
            elif tokenizer_type == 'mindformers':
                BenchmarkTokenizer._tokenizer_instance = \
                    MindformersTokenizer(model_path, trust_remote_code=trust_remote_code)
            else:
                raise ValueError("Tokenizer Type Not Supported")

    @staticmethod
    def is_initialized() -> bool:
        """
        Checks if the tokenizer instance has been initialized.

        Returns:
            bool: True if the tokenizer instance is initialized, False otherwise.
        """
        return BenchmarkTokenizer._tokenizer_instance is not None

    @staticmethod
    def reset_instance():
        """
        Uninitializes the tokenizer instance, allowing re-initialization if needed.
        """
        BenchmarkTokenizer._tokenizer_instance = None

    @staticmethod
    def encode(text: str, tokenizer_time=None, add_special_tokens=True, **kwargs):
        """
        Encodes the input text to tokens using the singleton tokenizer instance.

        Args:
            text (str): The input text to encode.
            **kwargs: Additional keyword arguments for the encoding method.

        Returns:
            list: The tokenized representation of the input text.
        """
        if BenchmarkTokenizer._tokenizer_instance is None:
            raise RuntimeError("Tokenizer has not been initialized. Call `init` first.")
        start_time = time.perf_counter()
        token_ids = BenchmarkTokenizer._tokenizer_instance.encode(text, add_special_tokens)
        if tokenizer_time is not None:
            tokenizer_time.append((time.perf_counter() - start_time) * 1000)
        return token_ids

    @staticmethod
    def decode(tokens, detokenizer_time=None, skip_special_tokens=False, **kwargs):
        """
        Decodes the input tokens back to text using the singleton tokenizer instance.

        Args:
            tokens (list): The list of tokens to decode.
            **kwargs: Additional keyword arguments for the decoding method.

        Returns:
            str: The decoded text.
        """
        if BenchmarkTokenizer._tokenizer_instance is None:
            raise RuntimeError("Tokenizer has not been initialized. Call `init` first.")
        start_time = time.perf_counter()
        output_str = BenchmarkTokenizer._tokenizer_instance.decode(tokens, skip_special_tokens)
        if detokenizer_time is not None:
            detokenizer_time.append((time.perf_counter() - start_time) * 1000)
        return output_str