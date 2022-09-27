#!/bin/bash
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
DATASET_DIR=$1
DATASET_TYPE=$2
DEVICE=$3

dir="../output"
if [ ! -d "$dir" ];then
mkdir $dir
fi

python ../train.py --datasetdir $DATASET_DIR --datasetType $DATASET_TYPE --device $DEVICE &> ../output/standalone_log &