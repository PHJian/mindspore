# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""
The MSAdvisor analyzer.
"""

import os
import stat
import subprocess

from mindspore import log as logger
from mindspore.profiler.common.validator.validate_path import validate_and_normalize_path
from mindspore.profiler.parser.msadvisor_parser import MsadvisorParser


class Msadvisor:
    """
    The interface to call MSAdvisor(CANN) by command line.
    """
    def __init__(self, job_id, rank_id, output_path):
        self._job_id, self._device_id = job_id.split("/")
        self._rank_id = rank_id
        self._output_path = output_path

    def call_msadvisor(self):
        """
        Call MSAdvisor by command line.
        """
        output_path = os.path.join(self._output_path, "msadvisor")
        output_path = os.path.join(output_path, "device_" + self._rank_id)
        output_path = validate_and_normalize_path(output_path)
        logger.info("MSAdvisor is running. Log and result files are saved in %s", output_path)

        try:
            running_result = subprocess.run(["msadvisor", "-d", output_path, "-c", "all"], capture_output=True)
        except FileNotFoundError as err:
            logger.warning("MSAdvisor: command not found,"
                           "please check if installed ascend-toolkit and set environment path correctly.")
            raise err
        except OSError as err:
            logger.warning("Cannot execute binary file: Exec format error.")
            raise err
        finally:
            pass

        if running_result.returncode == 0:
            logger.info("MSAdvisor process finished.")

            recommendation_path = os.path.join(output_path, "recommendation")
            recommendation_path = validate_and_normalize_path(recommendation_path)
            os.chmod(recommendation_path, stat.S_IRWXU)
            file_list = os.listdir(recommendation_path)
            file_list.sort(key=lambda fn: os.path.getmtime(os.path.join(recommendation_path, fn)))
            recommendation_path = os.path.join(recommendation_path, file_list[-1])
            recommendation_path = validate_and_normalize_path(recommendation_path)
            os.chmod(recommendation_path, stat.S_IRUSR | stat.S_IRUSR)
        else:
            logger.warning("MSAdvisor running failed,"
                           "please check MSAdvisor running log.")

        log_path = os.path.join(output_path, "log")
        log_path = validate_and_normalize_path(log_path)
        os.chmod(log_path, stat.S_IRWXU)
        file_list = os.listdir(log_path)
        file_list.sort(key=lambda fn: os.path.getmtime(os.path.join(log_path, fn)))
        log_path = os.path.join(log_path, file_list[-1])
        log_path = validate_and_normalize_path(log_path)
        os.chmod(log_path, stat.S_IRWXU)
        file_list = os.listdir(log_path)
        log_path = os.path.join(log_path, file_list[0])
        log_path = validate_and_normalize_path(log_path)
        os.chmod(log_path, stat.S_IRUSR | stat.S_IRUSR)

    def analyse(self):
        """
        Execute the MSAdvisor parser, generate timeline file and call MSAdvisor by command line.
        """
        reformater = MsadvisorParser(self._job_id, self._device_id, self._rank_id, self._output_path)
        reformater.parse()
        self.call_msadvisor()
