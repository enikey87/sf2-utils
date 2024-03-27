#!/bin/bash

# Step 1: Call the sf-split binary with the first argument
./sf2-split "$1"
# Step 2: Check if the command succeeded
if [ $? -ne 0 ]; then
    echo "sf2-split command failed."
    exit 1
fi

# Step 3: Create SF2_SPLIT_OUT folder
mkdir -p SF2_SPLIT_OUT

# Step 4: Move all *.sf2 files into the SF2_SPLIT_OUT folder
mv *.sf2 SF2_SPLIT_OUT/

# Step 5: Call sf-split-json.py and write its stdout into SF2_SPLIT_OUT/descriptor.json
python3 sf2-split-json.py SF2_SPLIT_OUT > SF2_SPLIT_OUT/descriptor.json
