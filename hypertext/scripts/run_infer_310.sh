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
# ============================================================================

if [[ $# -lt 3 || $# -gt 4 ]]; then 
    echo "Usage: bash run_infer_310.sh [MINDIR_PATH] [DATASET] [DATA_PATH] [DEVICE_ID] (optional)
    DEVICE_ID is optional, it can be set by environment variable device_id, otherwise the value is zero"
exit 1
fi

get_real_path(){
  if [ "${1:0:1}" == "/" ]; then
    echo "$1"
  else
    echo "$(realpath -m $PWD/$1)"
  fi
}

model=$(get_real_path $1)
dataset=$2
data_path=$(get_real_path $3)

device_id=0

if [ $# == 4 ]; then
    device_id=$4
fi

echo $model
echo $dataset
echo $data_path
echo $device_id

export ASCEND_HOME=/usr/local/Ascend/
if [ -d ${ASCEND_HOME}/ascend-toolkit ]; then
    export PATH=$ASCEND_HOME/ascend-toolkit/latest/fwkacllib/ccec_compiler/bin:$ASCEND_HOME/ascend-toolkit/latest/atc/bin:$PATH
    export LD_LIBRARY_PATH=/usr/local/lib:$ASCEND_HOME/ascend-toolkit/latest/atc/lib64:$ASCEND_HOME/ascend-toolkit/latest/fwkacllib/lib64:$ASCEND_HOME/driver/lib64:$ASCEND_HOME/add-ons:$LD_LIBRARY_PATH
    export TBE_IMPL_PATH=$ASCEND_HOME/ascend-toolkit/latest/opp/op_impl/built-in/ai_core/tbe
    export PYTHONPATH=${TBE_IMPL_PATH}:$ASCEND_HOME/ascend-toolkit/latest/fwkacllib/python/site-packages:$PYTHONPATH
    export ASCEND_OPP_PATH=$ASCEND_HOME/ascend-toolkit/latest/opp
else
    export ASCEND_HOME=/usr/local/Ascend/latest/
    export PATH=$ASCEND_HOME/atc/ccec_compiler/bin:$ASCEND_HOME/atc/bin:$PATH
    export LD_LIBRARY_PATH=/usr/local/lib:$ASCEND_HOME/atc/lib64:$ASCEND_HOME/acllib/lib64:$ASCEND_HOME/driver/lib64:$ASCEND_HOME/add-ons:$LD_LIBRARY_PATH
    export PYTHONPATH=$ASCEND_HOME/atc/python/site-packages:$PYTHONPATH
    export ASCEND_OPP_PATH=$ASCEND_HOME/opp
fi

function preprocess_data()
{
    if [ -d preprocess_Result ]; then
        rm -rf ./preprocess_Result
    fi
    mkdir preprocess_Result
    python ../preprocess.py --datasetdir=$data_path --batch_size=1 --datasetType=$dataset --device=Ascend
}

function compile_app()
{
    cd ../ascend310_infer || exit
    if [ -f "Makefile" ]; then
        make clean
    fi
    sh build.sh &> build.log

    if [ $? -ne 0 ]; then
        echo "compile app code failed"
        exit 1
    fi
    cd - || exit
}

function infer()
{
    if [ -d result_Files ]; then
        rm -rf ./result_Files
    fi
     if [ -d time_Result ]; then
        rm -rf ./time_Result
    fi
    mkdir result_Files
    mkdir time_Result
    ../ascend310_infer/out/main --mindir_path=$model --input0_path=./preprocess_Result/00_ids/ --input1_path=./preprocess_Result/01_ngrad/ --device_id=$device_id &> infer.log

    if [ $? -ne 0 ]; then
        echo "execute inference failed"
        exit 1
    fi
}

function cal_acc()
{
    python ../postprocess.py --result_Path=./result_Files \
        --label_Path=./preprocess_Result/02_label &> acc.log
    if [ $? -ne 0 ]; then
        echo "calculate accuracy failed"
        exit 1
    fi
}

preprocess_data
compile_app
infer
cal_acc